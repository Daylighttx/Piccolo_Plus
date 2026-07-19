# Piccolo_Plus 渲染模块学习笔记 & 现代引擎化改造路线

> 本文是渲染模块的系统讲解 + 改造路线的汇总，配合源码中的逐行中文注释（位于
> `engine/source/runtime/function/render/` 下各 `.h` / `.cpp`）一起看。
> 阅读顺序建议：先看「1 整体架构」建立地图，再按「3 阅读路线图」分组精读源码，
> 最后用「6 改造路线」作为改造时的 checklist。

---

## 1. 渲染模块整体架构（三层设计）

整个渲染层代码集中在 `engine/source/runtime/function/render/`，采用标准的三层分层：

```
┌──────────────────────────────────────────────────────────────┐
│  逻辑层 (Logic / Game)                                         │
│  world_manager 描述"世界里有什么"（物体、相机、灯光、变换）      │
└───────────────────────────┬──────────────────────────────────┘
                            │  RenderSwapContext（双缓冲桥，唯一数据通道）
┌───────────────────────────┴──────────────────────────────────┐
│  渲染层 (Render)                                              │
│  RenderSystem → RenderScene(世界状态) → RenderPipeline(各Pass) │
└───────────────────────────┬──────────────────────────────────┘
                            │  RHI 抽象接口
┌───────────────────────────┴──────────────────────────────────┐
│  图形 API 层                                                  │
│  RHI(interface/rhi.h) → VulkanRHI(interface/vulkan/...)       │
│  → Vulkan + swapchain 上屏                                     │
└──────────────────────────────────────────────────────────────┘
```

**分层的好处**：逻辑层代码完全不碰任何 Vulkan API；将来要换/自定义后端（你的愿景），
只需新增一个 `RHI` 实现，上层无感知。`RHI` 是所有图形调用的抽象接缝。

---

## 2. 一帧是怎么画出来的：`RenderSystem::tick()`

入口 `render_system.cpp:101` `RenderSystem::tick(delta_time)`，每帧固定 6 步：

| 步骤 | 代码 | 作用 |
|------|------|------|
| ① 消费数据 | `processSwapData()` (`:104`, 实现 `:285`) | 把 swap context 里的逻辑层数据转成渲染层对象 |
| ② 准备命令上下文 | `m_rhi->prepareContext()` (`:107`) | 取当帧 command buffer |
| ③ 更新逐帧缓冲 | `updatePerFrameBuffer(...)` (`:110`) | 相机/视图矩阵等上传到 per-frame uniform |
| ④ 收集灯光 | `updateLights()` (`:113`, 实现 `:139`) | 把 `LightComponent` 收集进 `RenderScene` |
| ⑤ 剔除 | `updateVisibleObjects(...)` (`:116`) | 视锥 + 光源剔除，产出每帧可见列表 |
| ⑥ 渲染 | `forwardRender()` / `deferredRender()` (`:125-132`) | 交给 pipeline 跑各 Pass |

**数据流向**：逻辑层每帧产生数据 → 经 `RenderSwapContext` 甩给渲染层 →
`processSwapData` 加载资源、生成 `RenderEntity` → 进 `RenderScene` →
剔除 → pipeline 各 Pass 消费 → RHI 提交 → swapchain 上屏。

---

## 3. 阅读路线图（由底向上，4 组）

> 先懂"数据长什么样"，再看"怎么上 GPU、怎么调显卡"，最后看"怎么串成一帧"。

| 组 | 文件 | 职责 |
|----|------|------|
| **① 数据结构** | `render_swap_context.h/.cpp`、`render_camera.h`、`render_entity.h`、`render_object.h`、`light.h`、`render_scene.h` | 逻辑↔渲染的数据模样 |
| **② 资源与接口** | `render_resource_base.h` → `render_resource.h/.cpp`、`interface/rhi.h`、`interface/vulkan/vulkan_rhi.cpp` | 数据用 VMA 上传 GPU + 图形 API 抽象 |
| **③ 管线与 Pass** | `render_pipeline.h/.cpp`、`main_camera_pass.*`、`ui_pass.*`、`pick_pass.*`、`combine_ui_pass.*`、`tonemapping_pass.*`、`fxaa_pass.*` | 一帧各渲染阶段 |
| **④ 入口串联** | `render_system.h/.cpp`、`render_scene.cpp` 的剔除实现 | 把上述全部串起来 |

> 第 ① 组源码已带逐行中文注释（含本次补的 `render_swap_context.cpp`）。
> 后续 ②③④ 按此顺序继续加注释 / 讲解。

---

## 4. 第 ① 组数据结构要点

（字段/函数已注释在源码，这里给"速记卡片"，精确含义以源码注释为准）

- **`RenderSwapContext`**：逻辑↔渲染唯一通道。两块 `RenderSwapData` + 两个索引 ping-pong，
  无锁隔离两层。GameObject 在缓冲内是 `deque` 队列（逻辑 push / 渲染 pop），其余是 `optional` 最新值。
- **`RenderCamera`**：渲染层相机。位置/旋转/FOV/视图矩阵、`forward()/up()/right()`、透视投影矩阵。
- **`RenderEntity`**：一个渲染实例 = mesh + 材质 + 模型矩阵 + 骨骼矩阵 + 包围盒。
- **`GameObjectDesc`**（`render_object.h`，逻辑层侧，可反射/序列化，来自关卡 JSON）：
  `GameObjectPartDesc`（mesh+material+transform+动画）打包成"一个游戏对象"。
  它经 swap → `processSwapData` 加载资源后变成 `RenderEntity`。
- **`LightList`**（`light.h`）：点光/方向光/环境光 + GPU 16 字节对齐顶点结构。
- **`RenderScene`**：渲染层的"世界状态"。`updateVisibleObjects()`（`:116` 调用）每帧做剔除，
  内含 5 个私有 `updateVisibleObjects*()`（DirectionalLight / PointLight / MainCamera / Axis / Particle）。

---

## 5. `RenderSwapContext` 双缓冲机制详解（重点）

### 5.1 不是"一块共享内存 + 一个队列"

代码里是**固定两个缓冲**（ping-pong），不是一块内存大家抢：

```cpp
RenderSwapData m_swap_data[2];          // Buffer[0] / Buffer[1]
uint8_t m_logic_swap_data_index;        // 逻辑层当前写哪块
uint8_t m_render_swap_data_index;       // 渲染层当前读哪块
```

- 逻辑层永远写 `m_swap_data[m_logic_swap_data_index]`（初值 Buffer[0]）
- 渲染层永远读 `m_swap_data[m_render_swap_data_index]`（初值 Buffer[1]）
- **同一时刻两层动的是不同物理内存 → 根本不需要锁。**

### 5.2 缓冲内部：GameObject 是队列，其余是最新值

只有物体是真正的 FIFO 队列（`render_swap_context.h` 内 `GameObjectResourceDesc`）：

```cpp
struct GameObjectResourceDesc {
    std::deque<GameObjectDesc> m_game_object_descs;  // 队列
    void add(...)          { m_game_object_descs.push_back(desc); }   // 逻辑：入队
    auto& getNextProcessObject() { return m_game_object_descs.front(); }
    void pop()             { m_game_object_descs.pop_front(); }       // 渲染：出队
};
```

相机/光照/粒子等是 `optional` 的"最新值覆盖"快照，逻辑层每次直接覆盖写。

### 5.3 交换（swap）是唯一同步点

渲染层把它的那块缓冲**彻底消费空**后才允许交换：

```cpp
bool isReadyToSwap() const {      // 渲染侧所有 optional 都为空 = 已消费完
    return !(render_slot.m_level_resource_desc.has_value() || ...);
}
void swap() {
    reset渲染侧字段();                                            // 清空，准备给逻辑层下回写
    std::swap(m_logic_swap_data_index, m_render_swap_data_index); // 交换索引
}
```

交换后，下一帧逻辑写"原渲染读的那块"，渲染读"逻辑刚写满的那块"。如此往复。

### 5.4 它解决的是什么问题

**`RenderSwapContext` 的双缓冲 = 逻辑/渲染解耦 + 免锁。它并不是用来防画面撕裂的。**
（防撕裂是 swapchain 的活，见第 6 节。）

---

## 6. 双缓冲 / 三缓冲 / 现代解决方案

> 注意：这里说的"双缓冲 vs 三缓冲"指的是**屏幕呈现层**的经典机制，
> 与第 5 节的 `RenderSwapContext` 是**两套完全不相干的代码路径**。

### 6.1 防撕裂靠 swapchain，不靠 RenderSwapContext

- **双缓冲（屏幕呈现）**：GPU 往"后缓冲"画，画完等 vsync 信号，再交换前后缓冲。
  若没有双缓冲，GPU 可能在屏幕扫描到一半时写正在显示的缓冲 → 上下两半不同帧 → **撕裂**。
- Piccolo 的 swapchain 由 `VulkanRHI` 管理：`createSwapchain()`（`vulkan_rhi.cpp:3056`）填
  `VkSwapchainCreateInfoKHR`，`presentMode` 来自 `chooseSwapchainPresentModeFromDetails()`
  （`:3497`，**优先 MAILBOX，否则 FIFO**），呈现在 `submitRendering()`（`:443`）里 `vkQueuePresentKHR`。
- **结论**：present 层我们引擎只填一个 create-info 结构体，防撕裂那套一行都不用写。
  我们真正维护的是 **CPU→GPU 的数据通道（提交部分）**。

### 6.2 双缓冲 vs 三缓冲（屏幕呈现层）

- **双缓冲**：后缓冲画完必须干等下一个 vsync 才能上屏 → 期间 GPU 空转 →
  **输入延迟变大**，帧时间一波动就更易卡。
- **三缓冲**：多一块"第三缓冲"。后缓冲在等 vsync 时，GPU 不闲着，马上去画第三块；
  vsync 一到取"最新画完的那块"上屏。
  - 好：GPU 不空转、延迟更低、更平滑。
  - 代价：多占一块显存；上屏的可能不是"刚画的那帧"而是"最新的那帧"（晚一帧，通常可忽略）。

> "三缓冲"是被"双缓冲会空转等 vsync"逼出来的优化——既要不撕裂、又要低延迟。

### 6.3 现代引擎 / API 的解决方案

**A. 呈现层（防撕裂 + 低延迟）**
- Vulkan / D3D12 / Metal 的 present mode：
  - `FIFO` = 强制 vsync，不撕裂但可能空转；
  - `MAILBOX` = 三缓冲 + 取最新，不撕裂且低延迟（**现代游戏首选，Piccolo 已默认**）；
  - `IMMEDIATE` = 关 vsync，最低延迟但会撕裂（竞技游戏有时用）。
- **自适应同步（G-Sync / FreeSync / VRR）**：显示器刷新率跟着 GPU 走，无固定 vsync 节奏
  → 既无撕裂也无空转，当前最理想方案。

**B. 逻辑↔渲染这层（即 `RenderSwapContext` 干的事）**
- 无锁 SPSC ring buffer（单生产者单消费者环形队列）：比 ping-pong 更通用，
  每帧分配一个 slot 即可，零锁零分配；
- "渲染晚 1 帧消费"：像 Unreal 的 Game 线程 / Render 线程，渲染永远读 1~2 帧前的快照，
  游戏逻辑永不被渲染卡住；
- 三缓冲变体：逻辑产数据远快于渲染消费又不丢数据时，把 sim 状态做成三缓冲，
  但代价是渲染看到更旧的数据（延迟↑），一般只在特殊场景用。

---

## 7. 提交部分的现代引擎化差距分析 & 改造路线

### 7.1 现状核查（代码为证）

1. **单线程 + 双缓冲形同装饰**
   `engine.cpp:131` 主循环：`world_manager.tick()`（逻辑）紧接着 `render_system->tick()`（渲染）
   **同一线程顺序跑**，全程无 `std::thread`。`isReadyToSwap` 永远为真 → 双缓冲没发挥解耦作用。
   （这也意味着：present 层你已"现代"——默认 MAILBOX 三缓冲；真正落伍的是提交层。）

2. **同步阻塞上传在帧热路径上**
   `render_system.cpp:381/386`：某 mesh/材质未缓存时，直接在 `processSwapData` 里调
   `uploadGameObjectRenderResource`，其内部 `vkQueueSubmit` + **等 fence**。一个新建物体 = 这一帧卡在上传。

3. **静态 / 动态完全不分**
   移动一个物体，逻辑层把**整个 `GameObjectDesc`（mesh+material+transform）重新入队**，
   `processSwapData`（`:308-405`）走完整 per-part 路径（分配器查找、包围盒、骨骼矩阵）
   只为更新一个 `m_model_matrix`（`:318`）。现代引擎只把 model matrix 写进一个 GPU buffer。

4. **每帧堆分配 / 拷贝**
   局部 `RenderEntity`、整个 `GameObjectDesc` 拷贝（`:306`）、`m_render_entities.push_back`、
   swap 用 `std::deque`。现代用帧分配器 / 空闲表，零 per-object 分配。

5. **`updateLights()` 每帧全量重建，且绕过 swap context**
   `render_system.cpp:139-165`：每帧 `clear()` + 从 `getAllLights()` 重建点光列表，
   直接写 `RenderScene`，与双缓冲设计不一致，也无 dirty 标记。

6. **无 frame graph / 无显式 barrier**（偏 pipeline 层，顺带提）
   Pass 序列写死，同步手动。现代用 frame graph 自动插 barrier。

### 7.2 改造路线图（按杠杆排序）

| 优先级 | 改造 | 收益 | 风险 |
|--------|------|------|------|
| **P0** | 拆"动态变换通道"：mesh/材质绑定只做一次，移动物体只更新 model matrix | 立刻消掉"动一个物体每帧重跑整段"的浪费 | 零（不动 GPU/多线程） |
| **P1** | 上传异步化：把 `uploadGameObjectRenderResource` 移出帧热路径，进 staging ring，未就绪先用占位 | 新建物体不再卡帧 | 低 |
| **P2** | 起真正的渲染/RHI 线程，`RenderSwapContext` 当 SPSC ring 用，N 帧在途（per-frame command buffer + fence） | 双缓冲名副其实，逻辑永不被渲染卡 | 中（线程安全） |
| **P3** | frame graph 自动 barrier + GPU-driven 实例/灯光缓冲（按 dirty 更新） | 超越点：渲染层完全数据驱动 | 高 |

### 7.3 建议的第一刀（P0，可立即动手）

给 `RenderSwapContext` 加 `m_transform_updates`（map<instance_id, Matrix4x4>），
逻辑层只对"变化的物体" push；`processSwapData` 命中已存在实例时只改 `m_model_matrix`
（`:318`、`:396-403`），**不重跑** mesh/材质绑定（`:323-386`）。
这一刀不动 GPU、不动多线程，立刻见效，也正好把双缓冲"队列只装变化"的设计用起来。

---

## 8. 与人交流 / 复核时用的一句话总结

> `RenderSwapContext` 的双缓冲 = **逻辑/渲染解耦 + 免锁**（不是防撕裂）；防撕裂是 swapchain 的活。
> 屏幕呈现层双缓冲会"等 vsync 时空转"，三缓冲用第三块缓冲消除空转、降延迟；
> 现代靠 `Mailbox` 三缓冲或自适应同步解决，而逻辑/渲染间现代更爱用无锁 ring buffer 或"晚一帧消费"。
> Piccolo 的 present 层已现代（默认 MAILBOX），真正落伍的是**提交层**——单线程跑、同步阻塞上传、
> 动一个物体就重跑完整路径。最高杠杆的三刀：拆变换通道 / 上传异步化 / 真渲染线程+帧在途。

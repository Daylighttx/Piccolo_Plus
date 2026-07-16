# Piccolo 引擎底层学习路线（读源码 + 动手实践并行）

> 目标：以游戏开发者视角，从「能编译运行」走到「能读懂并改造引擎」。
> 方法：**每章先读指定源码 → 立刻动手改一处 / 加一个功能 / 做一次优化**，改完就编译验证，形成「读一章、练一章」的肌肉记忆。
>
> 当前仓库：**Daylighttx/Piccolo_Plus**（BoomingTech/Piccolo 的 fork），目录仍在 `Piccolo/`（即 `engine/source/runtime` 的上级）。所有源码路径相对 `engine/source/runtime/`。
> 仓库自带两份学习资料，强烈建议配合本计划一起看：
> - `Piccolo/docs/engine-architecture.svg` —— 引擎架构图（鸟瞰全貌）
> - `Piccolo/docs/learning-plan.md` —— 作者写的分模块学习路线（含物理「它到底怎么算」讲解，比本计划更细）
> 工作流：读源码 → 改代码 → `cd Piccolo && ./deploy_editor.sh` + 重新编译 → 看启动日志 / 渲染窗口验证。

---

## 第 1 章 · 主循环与子系统调度

**读什么**
- `engine.cpp` —— 主循环 `run()` / `tickOneFrame()`，理解一帧里「逻辑 → 交换数据 → 渲染 → 事件轮询」的顺序
- `engine.h` —— 引擎类成员（`m_last_tick_time_point`、`m_fps`、`calculateFPS`）
- `function/global/global_context.cpp` —— `startSystems()` 里所有子系统的**创建顺序**（Config → File → Log → Asset → Physics → World → Window → Input → Particle → Render → DebugDraw），顺序即依赖关系

**关键概念**
- 单线程主循环：`logicalTick` 改逻辑，`swapLogicRenderData` 把逻辑数据交给渲染，`rendererTick` 消费
- `calculateDeltaTime` 用 `steady_clock` 算帧间隔；`calculateFPS` 用指数滑动平均平滑 FPS（不是裸 1/dt）
- 标题栏已经在显示 FPS（`setTitle("Piccolo - N FPS")`）

**动手实践（读已有 + 扩展）**：帧耗时剖析
- 这个 fork 的 `engine.cpp:75/85` **已经原生打印** `logicTime` / `renderTime`（每帧毫秒），比旧版 Piccolo 更全。先读懂它怎么用 `steady_clock` 分别卡 `logicalTick` 和 `rendererTick`。
- 扩展练习（任选其一，改完编译看日志）：
  1. 加**滑动平均**：维护 `m_logic_ms_avg` / `m_render_ms_avg`，每 120 帧打印一次均值，滤掉单帧抖动，更容易看出瓶颈在哪一侧；
  2. 把 `logicTime`/`renderTime` 接到后面第 8 章的 ImGui 面板里，做成实时 FPS / 帧耗时 HUD。
- 验证：启动后日志出现 `logicTime: x ms` / `renderTime: y ms`（已自带），扩展后额外出现均值行。
- `engine/source/runtime/engine.cpp`：在 `tickOneFrame()` 用 `std::chrono::steady_clock` 分别测量 `logicalTick()` 与 `rendererTick()` 的耗时（毫秒），做指数滑动平均，每 120 帧用 `LOG_INFO` 打印一次

**编译验证（本会话 Bash 工具故障，需你手动跑）**
```bash
cd /Users/tal/Documents/Piccolo/Piccolo
export CMAKE_POLICY_VERSION_MINIMUM=3.5
cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$(pwd)/engine/bin/MacOS"
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"
./deploy_editor.sh && cd engine/bin/MacOS && ./PiccoloEditor
```
启动后日志应每隔约 120 帧打印：`frame profile: logical=..ms render=..ms total=..ms`，立刻能看出瓶颈在逻辑侧还是渲染侧。

---

## 第 2 章 · 数学库与 Transform

**读什么**
- `core/math/vector3.h`（已含 `lerp`/`clamp`/`crossProduct`/`getRotationTo`）
- `core/math/quaternion.h`
- `core/math/matrix4x4.h`
- `core/math/transform.h`（位置 / 旋转 / 缩放的组合，引擎里物体的空间姿态）

**关键概念**
- `Vector3` 是 Ogre 风格的右手系实现；`getRotationTo` 用 Stan Melax 算法从向量算最短弧四元数
- `Transform` 把 位姿（position/rotation/scale）封装成可组合的对象，是组件系统操作物体姿态的入口
- 引擎里旋转用**四元数**而非欧拉角（避免万向锁）

**动手实践（待做）**：给数学库补一个实用工具
- 方案 A（推荐）：给 `Quaternion` 加 `slerp(a, b, t)` 球面插值静态方法（注意先 `dot<0` 时取反保证走最短弧、夹角极小时退化到 `lerp`）
- 方案 B：给 `Transform` 加 `rotateAround(const Vector3& point, const Vector3& axis, const Radian& angle)` —— 绕空间任意轴公转（很有用，且能直接被第 3 章的组件调用）
- 写一个最小 `static_assert` 或临时 `LOG_INFO` 验证插值结果正确（如 `slerp(IDENTITY, 90°, 0.5)` 应是 45°）

---

## 第 3 章 · 组件系统与 GameObject

**读什么**
- `function/framework/component/component.h`（基类：`virtual tick(delta_time)` + `m_parent_object` 弱引用 + `postLoadResource`）
- `function/framework/object/game_object.h`（GameObject 如何持有多个 Component）
- `function/framework/world/world_manager.cpp`（每帧 `tick` 遍历 Level 里的 GameObject → 调各 Component 的 `tick`）
- `function/framework/component/transform/transform_component.h`（把 `Transform` 挂到 GameObject 上的组件）

**关键概念**
- ECS-lite：GameObject 是 Component 的容器；每帧由 World 驱动所有 Component 的 `tick`
- `m_parent_object` 是 `weak_ptr`，组件通过它拿到自己挂在哪个物体上、读写其 Transform
- `m_tick_in_editor_mode` 控制组件在编辑器下是否 tick（注意 `g_editor_tick_component_types`）

**动手实践（待做）**：实现 `SpinComponent`
- 新建 `function/framework/component/spin/spin_component.h/.cpp`，继承 `Component`
- 重写 `tick(delta_time)`：把自己的 `m_parent_object` 的 Transform 绕 Y 轴旋转 `speed * delta_time`
- 用 `REFLECTION_TYPE(SpinComponent)` / `CLASS(SpinComponent, Fields)` 挂上反射宏，使其能被资源/编辑器识别
- 在示例 Level 的某个 GameObject 上挂这个组件，编译运行后看物体自转
- 进阶：把 `speed` 做成可反射字段，从 `.level.json` 里配

---

## 第 4 章 · 输入系统

**读什么**
- `function/input/input_system.h`（已含 `GameCommand` 位标志枚举 WASD/SPACE/SHIFT/F、`onKey`、`getGameCommand`、`m_cursor_delta_yaw/pitch`）
- `function/input/input_system.cpp`（`onKeyInGameMode` 如何把 GLFW key 映射成 `GameCommand`、`calculateCursorDeltaAngles` 如何把鼠标位移转成 yaw/pitch）
- 看 `window_system` 如何把 GLFW 回调接到 `InputSystem::onKey` / `onCursorPos`

**关键概念**
- 输入用**位标志命令字**（`GameCommand`），逻辑层 `getGameCommand() & forward` 判断，与具体按键解耦
- 鼠标增量被转成相机 yaw/pitch（弧度），供相机组件消费
- `squat` / `fire` 枚举里写了 `// not implemented yet` —— 留了扩展位

**动手实践（待做）**：新增一个输入命令并接入
- 加 `pause = 1 << 9`（或 `reset = 1 << 10`）到 `GameCommand`，绑定到 `P`（或 `R`）键
- 在 `onKeyInGameMode` 里处理该键的 press/release，设置/清除对应位
- 在 `logicalTick` 或某个组件里读取该命令，做「暂停主循环 / 重置物体位置」之类的可见效果
- 验证：按 P 后日志/行为有反应

---

## 第 5 章 · 渲染系统（重头戏）

**读什么**
- `function/render/render_system.h`（顶层 `tick` / `swapLogicRenderData` / `initialize`）
- `function/render/window_system.h`（GLFW 窗口 + Vulkan surface）
- `function/render/vulkan/vulkan_rhi.cpp`（Vulkan 设备/交换链/命令缓冲；`PICCOLO_VK_ICD_FILENAMES` 指 MoltenVK）
- `function/render/render_pipeline / render_pass / render_resource / render_scene`（现代渲染管线四层：场景描述 → 资源 → Pass → 管线）

**关键概念**
- 渲染是**延迟/多 Pass**结构：先填 `RenderScene`/`RenderResource`（逻辑侧数据），渲染侧消费
- `swapLogicRenderData` 是逻辑↔渲染的边界，理解它就读懂了「游戏逻辑如何变成画面」
- 每个 `RenderPass` 是一个独立绘制阶段（GBuffer / 光照 / 后期…）

**动手实践（待做）**：改一处渲染表现（二选一）
- 方案 A：把渲染 Clear Color 改成可配置（从 `config_manager` 读一个颜色），顺手在窗口里看到背景色变化
- 方案 B：加一个 `wireframe` 调试开关（用 `VK_POLYGON_MODE_LINE`），通过第 4 章的输入命令或 `RenderDebugConfig` 切换
- 验证：编译运行，背景色/线框模式按预期变化

---

## 第 6 章 · 物理系统

**读什么**
- `function/physics/physics_manager.h`（基于 Jolt 物理；`initialize` / `tick` / `createRigidBody` / `removeRigidBody`）
- 启动日志里看到的 `createRigidBody` ×28 就是它干的 —— 关卡里的碰撞体在 `world load` 时批量创建
- 物理场景 `PhysicsScene` 如何与渲染侧的 Transform 双向同步

**关键概念**
- 物理与渲染**解耦**：物理算刚体，渲染读结果；每帧物理 `tick` 后把位姿写回 GameObject 的 Transform
- Jolt 是现代物理引擎（和 PhysX 同级），Piccolo 把它包了一层 `PhysicsManager`

**动手实践（待做）**：暴露一个物理参数
- 给 `PhysicsManager` 或某个全局配置加 `gravity` / `fixedTimeStep` 可调字段
- 或：在示例关卡里加一个新的碰撞体形状（如一个球/盒），验证 `createRigidBody` 路径
- 验证：日志出现新刚体；运行时物体受重力/碰撞符合预期

---

## 第 7 章 · 资源与反射系统（之前卡住的那套）

**读什么**
- `core/meta/reflection/reflection.h`（`REFLECTION_TYPE` / `CLASS` / `Fields` 宏）
- `core/meta/reflection/reflection_register.h`（运行时类型注册表）
- `meta_parser/`（`PiccoloParser` 用 libclang 解析带反射宏的头，生成 `*.reflection.gen.h` 序列化代码；`precompile.cmake` 在构建期调用）
- `_generated/reflection/*.reflection.gen.h`（自动生成的产物，看它长什么样）

**关键概念**
- 反射系统是 Piccolo 的「黑魔法」：用宏标注类→构建期生成 JSON 序列化/反序列化代码→资源（`.level.json`/`.world.json`）能被直接加载成 C++ 对象
- 这是它「改一处数据，编辑器里立刻生效」的基础

**动手实践（待做）**：给现有类加一个可序列化字段
- 挑一个简单的资源类（如某个 Render 配置或组件数据），加一个 `float m_new_field {default}` 并用 `Fields` 宏登记
- 重新编译（会触发 precompile 重新生成反射代码），在对应 `.json` 里配这个值
- 验证：加载后字段被正确读取（`LOG_INFO` 打印一下）；理解「加字段→自动序列化」的闭环

---

## 第 8 章 · UI 系统

**读什么**
- `function/ui/`（基于 Dear ImGui 的调试 UI 层）
- 看编辑器如何把 ImGui 接到 Vulkan 后端、每帧 `render` 时画调试面板
- `function/render/debugdraw/debug_draw_manager.h`（几何调试绘制：线框、包围盒、坐标轴）

**关键概念**
- 引擎调试 UI 与游戏 UI 是两套：调试 UI（ImGui）给开发者看内部状态；游戏 UI 是另一回事
- `DebugDrawManager` 把调试几何塞进渲染管线，和正常物体一起画

**动手实践（待做）**：加一个调试面板
- 用 ImGui 加一个 `Engine Stats` 面板，显示实时 FPS、第 1 章的「逻辑/渲染平均耗时」、当前 Level 的 GameObject 数量
- 把第 1 章 profiling 的结果接到这个面板（数据贯通：引擎内部 → UI 显示）
- 验证：运行时面板出现且数值随帧更新

---

## 第 9 章 · 渲染管线现代化（进阶渲染）

> 前置：第 5 章渲染系统已读完，理解 Piccolo 的 pass-based 延迟渲染管线（render_pipeline → render_pass → 资源 → 场景四层）。
> 本章按**依赖链顺序**推进：Frame Graph 是基础设施 → 在它之上构建 GPU-Driven → 再叠 Visibility Buffer → 最后加 GI。VSM 和体渲染不依赖 Frame Graph，可先行。
>
> 参考资料：《Games104 引擎新技术实验部署方案》（`article/Games104引擎新技术实验部署方案.html`），含实测数据和论文链接。

### 9.1 Virtual Shadow Maps — 虚拟阴影贴图

**读什么**
- `function/render/passes/directional_light_pass.{h,cpp}`（现有 CSM 级联阴影实现）
- `function/render/render_resource.{h,cpp}`（RT 资源分配、depth texture 管理）
- UE5 VSM 文档：虚拟纹理分页管理、Page-In 机制、Clipmap 方向光

**关键概念**
- 虚拟分辨率 16K×16K，分 128×128 Page 管理；物理显存只分配固定大小按需 Page-In
- 从 G-Buffer 提取屏幕像素世界位置 → 投影到光源视角 → 标记需要的虚拟 Page
- 只渲染被标记的 Page；未变化的 Page 复用上一帧（深度缓存机制）
- 方向光用 Clipmap：每级覆盖半径翻倍，分辨率不变

**动手实践**：把 CSM 升级为简化版 VSM
- 不必一步到位 16K。先用 4K 虚拟分辨率 + 64×64 Page，验证 Page 标记 → 按需渲染流程
- 对比 CSM 的阴影质量（近处物体边缘锯齿改善）
- 进阶：加 Page 深度缓存（物体未移动时跳过光栅化），用帧耗时对比收益

**实测参考（团结引擎 Tower Valley）**：VSM GPU 2.33ms vs CSM+VG 18.16ms，且覆盖更远。

---

### 9.2 体渲染 — 大气散射 + 体积雾

**读什么**
- `function/render/passes/`（看现有 pass 列表，找到插入点）
- `function/render/render_pipeline.{h,cpp}`（pass 注册和执行顺序）
- Frostbite 论文："A Scalable and Production Ready Sky and Atmosphere Rendering" + "Physically-based & Unified Volumetric Rendering"

**关键概念**
- 大气散射：低成本实现真实天空和远距离雾（Rayleigh + Mie 散射）
- 体积雾：Frustum-aligned 3D 网格，逐体素计算雾照明，时域累积
- 体积光照：将太阳级联光注入体积系统，产生 God Rays 效果
- 与渲染管线集成：声明为独立 Pass，自动管理资源

**动手实践**：加一个体积雾 Pass
- 新建 `volumetric_fog_pass`，在 GBuffer 之后、光照之前执行
- 实现 Frustum-aligned 3D 网格（如 64×64×32），逐体素计算雾密度 × 光照
- 时域累积：上一帧结果重投影，减少噪声
- 验证：场景中出现体积雾和 God Rays 效果；性能控制在 ~1ms（参考 Steam Deck 数据）

---

### 9.3 Frame Graph — 声明式渲染管线

> 这是后续 GPU-Driven / VisBuffer / DDGI 的**基础设施**，优先级最高。

**读什么**
- `function/render/render_pipeline_base.{h,cpp}`（现有硬编码 pass 序列：Geometry → Lighting → PostProcess → Output）
- `function/render/render_pass_base.{h,cpp}`（pass 基类、RT 资源声明）
- Frostbite GDC 2017："FrameGraph: Extensible Rendering Architecture"
- UE5 RDG 文档：Rendering Dependency Graph

**关键概念**
- 每帧渲染抽象为**有向无环图（DAG）**，节点 = Pass，边 = 资源依赖
- 每个 Pass 声明输入（读）和输出（写/创建）资源，系统自动推导生命周期
- 编译期：拓扑排序 → 资源活跃区间 → 自动内存别名（Aliasing）→ 自动屏障
- 执行期：按排序结果执行各 Pass lambda，自动创建/释放临时资源

**动手实践**：实现 FrameGraphBuilder + 迁移一个 Pass
- Phase 1：定义 `FrameGraphBuilder` 接口（`createTexture` / `createBuffer` / `addPass`）
- Phase 2：实现拓扑排序 + 资源活跃区间计算
- Phase 3：实现内存别名复用 + 自动屏障插入
- Phase 4：把现有 `directional_light_pass` 迁移到 FrameGraph 声明式 API
- 验证：新增 pass 只需 ~5 行声明（替代原来 ~60 行手动 RT 管理）；显存占用下降

**实测参考**：显存降 24%、Vulkan 分配数减少 98%（2341→47）、同步屏障 bug 归零。

---

### 9.4 GPU-Driven Rendering Pipeline — GPU 驱动渲染

> 前置：9.3 Frame Graph 已完成（GPU-Driven 的 pass 管理依赖 FrameGraph）

**读什么**
- `function/render/render_scene.{h,cpp}`（CPU 端如何组织可见物体、提交 draw call）
- `function/render/render_entity.{h,cpp}`（实体 → mesh → draw call 的链路）
- Ubisoft《刺客信条》GPU-Driven 实践；UE5 Nanite 深度解析

**关键概念**
- 从 `DrawPrimitive`（CPU 逐物体组织）到 `DrawScene`（GPU 一次提交全部）
- Compute Shader 在 GPU 端完成：视锥剔除 → HZB 遮挡剔除 → LOD 选择
- Indirect Draw：GPU 生成 draw call 参数表，CPU 只提交一次「执行这段参数表」
- Mesh Cluster Rendering：几何体拆成 64-128 三角形/簇

**动手实践**：GPU 端视锥剔除 + Indirect Draw
- 用 Compute Shader 在 GPU 端做视锥剔除（替代 CPU `render_scene` 里的遍历）
- 生成 indirect draw 参数 buffer
- 用 `vkCmdDrawIndirect` 一次调用渲染全部可见物体
- 对比：CPU 端帧耗时下降、draw call 数量变化
- 进阶：加 HZB 遮挡剔除（生成层次化 Z buffer，GPU 端判断物体是否被遮挡）

---

### 9.5 Visibility Buffer — 可见性缓冲渲染

> 前置：9.4 GPU-Driven 已完成

**读什么**
- `function/render/passes/main_camera_pass.{h,cpp}`（现有 G-Buffer 填充逻辑）
- Wolfgang Engel Visibility Buffer 论文；Nanite Visibility Buffer 详解

**关键概念**
- 两阶段渲染：先只记录「谁可见」，再对着色
- Pass 1（Visibility）：硬件光栅化大三角形 + 软件光栅化小三角形（<32px），输出 TriangleID + ClusterID + Depth
- Pass 2（Deferred Material）：读取 Visibility Buffer → 加载顶点数据 → 插值 G-Buffer 属性 → 执行材质着色
- 渲染消耗与屏幕像素相关，而非场景复杂度（关键突破）

**动手实践**：用 Visibility Buffer 替换 G-Buffer 直接渲染
- 实现 Visibility Pass（64-bit/pixel：30-bit Depth + 27-bit Cluster ID + 7-bit Triangle ID）
- 实现 Deferred Material Pass（只对可见像素着色）
- 对比：overdraw 浪费消除、材质绑定开销下降
- 验证：同样场景下渲染正确，帧耗时变化

---

### 9.6 DDGI + SDF — 实时动态全局光照

> 前置：9.3 Frame Graph + 9.5 Visibility Buffer（GI 需要干净的 G-Buffer 采样）

**读什么**
- `function/render/passes/`（现有光照 pass，理解直接光照 + 简单环境光）
- DDGI 论文（浙大 SDFDDGI）；Flax 引擎 DDGI 实现文档

**关键概念**
- DDGI 不依赖硬件光追，用 SDF 做软件光线求交
- 离线：对每个物体生成 SDF 数据
- 运行时每帧：组合场景级 Global Distance Field (GDF) → 8×8 Tile 生成 Probe → Probe 用 GDF 光线求交 → 生成球谐光照数据
- 后处理：G-Buffer 还原世界位置 → 采样 Probe 球谐数据 → 叠加 GI

**动手实践**：实现简化版 DDGI
- 先给一个物体生成 SDF（如示例场景的静态 mesh）
- 实现 Probe 网格（8×8 Tile），用 SDF 做光线求交
- 生成球谐光照数据，在 G-Buffer 后处理阶段采样叠加
- 验证：间接光出现（暗处有反射光），性能控制在 ~1.3ms（参考 Sponza 场景数据）
- 进阶：多弹射漫反射 GI、Probe 自动重定位

---

## 第 10 章 · 物理与架构进阶

> 前置：第 6 章物理系统 + 第 3 章组件系统已读完。

### 10.1 XPBD 物理仿真 — 布料与柔体

**读什么**
- `function/physics/physics_manager.{h,cpp}`（现有 Jolt 刚体集成）
- `function/physics/physics_scene.{h,cpp}`（物理场景管理）
- PBD/XPBD 理论综述；SIGGRAPH 2025 AVBD 论文；《恋与深空》StrayCloth 实践

**关键概念**
- PBD：直接操作粒子位置满足约束，无条件稳定，但刚度受迭代次数影响
- XPBD：引入拉格朗日乘子 λ（约束张力），约束硬度可控且与时间步长无关
- 工业实践：StrayCloth 用 XPBD + Substep（1/200~1/300），骨骼作为模拟粒子，Cosserat Rod 约束

**动手实践**：在 Piccolo 里加一个 XPBD 布料组件
- 新建 `cloth_component`，用 XPBD 框架模拟一块布（粒子网格 + 距离约束 + 弯曲约束）
- 子步 substep（动态调整 1/120~1/240），保证稳定性
- 用 DebugDrawManager 画出粒子位置，验证仿真正确
- 把布料 mesh 挂到粒子上，渲染侧看到布料形变
- 进阶：参考 StrayCloth 用骨骼作为模拟粒子，或加 Cosserat Rod 约束做毛发/线缆

---

### 10.2 ECS / DOD 混合架构改造

**读什么**
- `function/framework/object/game_object.h`（现有 GameObject-Component 容器模型）
- `function/framework/component/component.h`（Component 基类、虚函数 tick）
- `function/framework/world/world_manager.cpp`（每帧遍历 → tick 所有 Component）
- Unity DOTS 文档（ECS 性能对比实测）；Piccolo 官方论坛 DOP 理论帖

**关键概念**
- GameObject-Component 是 OOP 架构：内存碎片化、缓存不友好、单线程瓶颈
- ECS 三要素：Entity（轻量 ID）/ Component（纯数据 SoA）/ System（处理特定组件组合，天然并行）
- 混合架构：保留 GameObject 用于编辑器和高层逻辑，性能热点用 ECS 重写
- DOTS 实测：1000 NPC 从 45fps→130fps；10000 子弹从 30fps→200fps

**动手实践**：把一个性能热点组件改造成 ECS 风格
- 挑一个简单但量大的场景（如粒子系统或 Boids 群集）
- 把 Component 数据从 `std::vector<GameObject*>` 改成 SoA 布局（`std::vector<Position>` + `std::vector<Velocity>`）
- 用 Job System（或 `std::for_each + std::execution::par`）多线程遍历
- 对比改造前后的帧耗时和缓存命中率
- 进阶：参考 Unity DOTS Baking 流程，做编辑器数据 → 运行时 Entity 的转换

---

### 10.3 碰撞事件系统

**读什么**
- `function/physics/physics_scene.{h,cpp}`（Jolt 物理场景的 contact 回调）
- `function/physics/jolt/utils.{h,cpp}`（Jolt ↔ Piccolo 类型转换）
- Jolt Physics 文档：ContactListener、碰撞事件回调

**关键概念**
- Jolt 的 `ContactListener` 提供 `OnContactAdded` / `OnContactPersisted` / `OnContactRemoved`
- 碰撞事件可用于：触发音效、游戏逻辑（开门/拾取）、物理效果（爆炸冲击力）

**动手实践**：实现碰撞事件回调系统
- 在 `PhysicsScene` 注册 `ContactListener`，转发碰撞事件到 Piccolo 的事件系统
- 设计 `CollisionEvent` 结构（两个实体 ID + 接触点 + 法线 + 冲击力）
- 在 `PhysicsManager` 暴露 `onCollision(callback)` 注册接口
- 在示例关卡里验证：两个物体碰撞时触发 `LOG_INFO`，或触发音效播放
- 进阶：用 DebugDrawManager 在接触点画标记，直观看到碰撞发生位置

---

## 第 11 章 · 前沿实验（持续探索）

> 这些方向更适合独立分支实验，不阻塞主学习线。作为长期跟踪目标记录。

### 11.1 3D Gaussian Splatting 混合渲染

**读什么**
- 3DGS 原始论文（SIGGRAPH 2023）
- Unigine 2.20 / Unity / UE 的 3DGS 集成方案
- Mesh-Inclusive 混合管线设计

**关键概念**
- 场景由数百万个 3D 高斯原语表示（非传统三角形网格）
- 每个高斯有位置、旋转、缩放、不透明度、球谐系数
- 混合管线：Mesh 负责逻辑和物理，3DGS 负责视觉保真（远景、照片级场景）
- 未来方向：4D Gaussian Splatting（加入时间属性，动态光照和形变）

**实验方向**
- 从 .ply 格式导入 3DGS 数据
- 实现基础高斯光栅化 Pass（作为 FrameGraph 中的一个节点）
- 与现有 G-Buffer 深度合成
- 远景 LOD：远处用 Impostor 替代

---

### 11.2 AI 辅助渲染管线

**读什么**
- NVIDIA DLSS 5 官方博客（GTC 2026）
- Arm 移动端神经渲染架构（NSS/NSSD/NFRU）
- TAA（Temporal Anti-Aliasing）基础

**关键概念**
- 神经超级采样（NSS）：基于 CNN 的像素级增强，输入色彩/深度/运动矢量/历史帧
- 神经降噪（NSSD）：光追输出低分辨率噪点画面 → 神经网络精准消噪
- 神经帧率提升（NFRU）：利用运动数据智能合成中间帧，30fps→60fps

**实验方向**
- 引入 TAA 作为基础时域累积框架
- 研究轻量级 CNN 超分模型替代传统 Upsample Pass
- 光追降噪器集成（SVGF 或神经降噪）
- Frame Generation 插帧实验

---

### 11.3 SIGGRAPH 论文跟踪

保持跟踪以下方向的最新论文，有合适的就纳入实验：
- Augmented Vertex Block Descent（AVBD，SIGGRAPH 2025 Best）— 超快速物理求解器
- Stable Cosserat Rods — 毛发/线缆仿真
- Offset Geometric Contact — 碰撞检测精度提升
- Implicit Position-Based Fluids — PBD 流体仿真
- Vector-Valued Monte Carlo Integration — 蒙特卡洛渲染降噪
- Sample Space Partitioning for Specular Manifold Sampling — 镜面路径采样优化

---

## 进度追踪

### 基础阶段（第 1-8 章）
- [x] 第 1 章 主循环与子系统（环形缓冲滑动平均已实现并提交）
- [ ] 第 2 章 数学库与 Transform
- [ ] 第 3 章 组件系统（SpinComponent）
- [ ] 第 4 章 输入系统
- [ ] 第 5 章 渲染系统
- [ ] 第 6 章 物理系统
- [ ] 第 7 章 资源与反射
- [ ] 第 8 章 UI 系统

### 进阶阶段（第 9-11 章）
- [ ] 第 9 章 渲染管线现代化
  - [ ] 9.1 VSM 虚拟阴影贴图
  - [ ] 9.2 体渲染（大气散射 + 体积雾）
  - [ ] 9.3 Frame Graph 声明式管线
  - [ ] 9.4 GPU-Driven Rendering
  - [ ] 9.5 Visibility Buffer
  - [ ] 9.6 DDGI + SDF 全局光照
- [ ] 第 10 章 物理与架构进阶
  - [ ] 10.1 XPBD 物理仿真
  - [ ] 10.2 ECS / DOD 混合架构
  - [ ] 10.3 碰撞事件系统
- [ ] 第 11 章 前沿实验
  - [ ] 11.1 3D Gaussian Splatting
  - [ ] 11.2 AI 辅助渲染
  - [ ] 11.3 SIGGRAPH 论文跟踪

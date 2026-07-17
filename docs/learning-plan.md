# Piccolo 引擎学习路线图（全模块 · 渲染重点）

> 目标：从「只会写业务逻辑」进阶到「理解游戏引擎底层」。
> 路线：**先整体过一遍引擎全貌 → 分模块逐个深入 → 边读边改，改动推送到 GitHub**。
> 当前进度：已克隆并成功编译（Release 可用，亦可编 Debug），`bin/PiccoloEditor.exe` 可运行。

---

## 🎯 总目标（North Star）—— 把教学引擎改造成「现代引擎」，最终用它做游戏 / 酷效果

> 这是整个学习路线的最高优先级约束。**后面所有阶段的学习和每一次动手，都先问「这一处离现代引擎还差什么 / 怎么往目标推进一步」，再动手。**

- **终极愿景**：不是停在"学引擎"，而是把这个从 GAMES104 教学引擎 fork 出来的 Piccolo，改造成**功能上对齐现代商业引擎（Unity / Unreal 级别的标准件）**、甚至在某些点上**超越它们**的自研引擎。
- **为什么能超越**：我们对引擎有 100% 控制权（源码、反射、渲染管线全在自己手里）。商业引擎为了通用和稳定，会限制很多底层能力（自定义渲染 pass、运行时改管线、深度定制组件 / 序列化、跨模块黑科技）。我们完全可以为特定酷效果绕开这些限制。
- **最终产出**：用**我们自己改造的引擎**做一个小游戏，或实现若干「Unity 这类商业引擎不好做」的酷效果——自定义渲染管线、程序化 / 全屏特效、深度 ECS 玩法、与物理 / 动画的非常规组合等。
- **对学习的约束（重要）**：
  1. **补标准件优先于纯玩法件**：Light / Audio / Collider / Trigger / Navigation 这类现代引擎标配，优先于 health 之类纯玩法组件。
  2. **先"像现代引擎"，再"超越现代引擎"**：先把引擎补齐到现代引擎的基本盘，再去碰商业引擎不好做的酷效果。
  3. **每个动手任务都要能"看到效果 / 用数据证明"**，并直接提交推 GitHub（见 §0 工作流）。
- **提交文化不变**：所有改动仍走 main 单线、小步提交推 GitHub（见 §0）。

---

## 0. 学习方法与 Git 工作流

### 学习方法（每个模块固定四步循环）
1. **读源码**：带着问题读，先抓「这个模块管什么、输入是什么、输出去哪」。
2. **理数据流**：画出「数据从哪来、到哪去」。例：物理 = `rigidbody` 组件 → `physics_scene` → Jolt → 写回 `transform`。
3. **改一处**：哪怕只改一个参数 / 加一行日志，立刻编译看效果。
4. **提交**：每完成一个小改动就提交到对应分支并推 GitHub。

### Git 工作流（单人单线，只用 main）
- **只维护 `main` 一条分支**，所有学习改动直接提交到 main —— 一个人学习维护，不额外开分支，最省心、最不容易乱。
- 远程只推自己的 fork：`origin = Daylighttx/Piccolo_Plus`。**与官方 `BoomingTech/Piccolo` 完全解耦，不提 PR**，纯作个人学习仓库。
- 提交信息用中文，格式：`[模块] 做了什么`，如 `[physics] 加碰撞事件日志`。
- 每完成一个「能编译 / 能看到效果」的小改动就提交并推送一次，保持提交粒度小、可回溯。

---

## 1. 阶段一：整体概览（先过一遍全引擎）

目标：钻进任何模块前，先建立「引擎长什么样、模块怎么连、一帧怎么跑完」的整体地图。

### 1.1 引擎架构鸟瞰
Piccolo 是**组件-系统（ECS 风格）**的延迟渲染引擎。核心分层：

- **core（地基）**：`math`（向量/矩阵/四元数）、`color`、`log`、`meta`（反射 + 序列化 + json）
- **function（功能）**：`framework`（ECS 核心）、`render`、`physics`、`animation`、`character`、`controller`、`input`、`particle`、`ui`、`global`
- **resource（资源）**：`asset_manager`（加载/缓存）、`config_manager`、`res_type`（资源类型）
- **platform**：`file_service`、`path`
- **editor**：`PiccoloEditor`（基于 ImGui，靠反射系统自动生成属性面板）
- **3rdparty**：VulkanSDK、JoltPhysics（物理）、glfw、imgui、lua、sol2、spdlog、stb、vulkanmemoryallocator

### 1.2 三个核心机制（必须理解）
1. **ECS 模式**：`Object` = 一组 `Component` 的容器；每种 Component 有对应的 `System`（`render_system`、`physics_manager`、`animation_system`…）；`world_manager` 每帧按序 tick 所有 system。
   - 文件：`function/framework/object/object.h`、`component/component.h`、`world/world_manager.cpp`
2. **Tick 主循环**：`engine.cpp` 的 `tick()` → 更新逻辑 → 各 system 更新 → 渲染提交。
   - 文件：`runtime/engine.cpp`
3. **反射系统（meta）**：Piccolo 的「灵魂」。通过 `core/meta/reflection` 在运行时知道类的字段，从而自动生成编辑器属性面板、做序列化、绑定 Lua。
   - 文件：`core/meta/reflection/reflection.h`、`reflection_register.h`（反射 API + 注册宏；该目录无单独 example 文件）

### 1.3 一帧的生命周期
```
engine.tick()
  → input_system 收集输入
  → world_manager 遍历所有 object
      → 各 component 的 system 更新（physics 积分、animation 采样、controller 移动…）
  → render_system 收集可见物体
      → render_pipeline 跑延迟渲染管线 → 输出到屏幕
  → ui/editor 画 ImGui 面板
```

### 1.4 本阶段动手
- [ ] 读 `engine.cpp`、`world_manager.cpp`、`global_context.cpp`、`reflection.h`，各写 2 行注释说明它做什么。
- [ ] 在主循环加帧耗时日志（逻辑耗时 vs 渲染耗时），跑起来看数字。
- [ ] 画一张自己的引擎架构图（可用 level/object 当例子）。

---

## 2. 阶段二：分模块逐个深入

> 难度：★ 入门 / ★★ 中等 / ★★★ 进阶。渲染部分最细（你的重点），物理部分配了「它到底怎么算」的讲解（你的盲区）。

### 2.1 引擎地基 core（★）
- 关键文件：`core/math/*`、`core/meta/reflection/*`、`core/meta/serializer/*`、`core/meta/json.h`、`core/log/*`
- 核心概念：数学库（向量/矩阵/四元数/变换）、反射注册宏、序列化（对象 ↔ JSON）、日志。
- 动手：用反射系统给一个已有类加一个新字段，看它是否自动出现在编辑器面板 + 序列化里。

### 2.2 框架与 ECS（★）
- 关键文件：`framework/object/*`、`framework/component/*`（transform / mesh / camera / animation / lua / motor / particle / rigidbody）、`framework/level/*`、`framework/world/*`、`global/global_context.*`
- 核心概念：Object 如何持有 Component；System 如何遍历 Component；Level 是场景容器；World 驱动一切；GlobalContext 是模块间拿彼此入口的全局表。
- 动手：写一个新 Component（如 `health_component`），并让它被某个 system 每帧打印一次 —— 验证你理解了 ECS 套路。

### 2.3 渲染 render（★★★ 重点）
分 5 个子阶段：
- **RHI 抽象层**：`function/render/interface/rhi.h` + `rhi_struct.h`（图形接口）+ `interface/vulkan/*`（Vulkan 实现，含 VMA 显存分配）。
- **渲染管线**：`render_system.cpp` → `render_pipeline*.{h,cpp}` → `render_pass*.{h,cpp}`（基类）。
- **Pass 与 Shader**：`passes/*` 对照 `engine/shader/*` 的 GLSL（完整 pass 列表：`main_camera_pass` 写 G-buffer；`directional_light_pass` / `point_light_pass` 光照；`particle_pass` 粒子合成；`tone_mapping_pass` / `color_grading_pass` / `fxaa_pass` 后处理；`pick_pass` 鼠标拾取；`combine_ui_pass` / `ui_pass` 叠 UI）。
- **场景与资源**：`render_scene`、`render_camera`、`render_entity`、`render_mesh`、`window_system`（GLFW + Vulkan Surface）。
- **动手（按收益排序）**：① 加渲染统计 HUD（draw call / 显存）；② 加开关跳过某后处理 pass；③ 改一个 shader 看画面变化；④ 视锥剔除减少 draw call。

### 2.4 物理 physics（★★ 你的盲区，重点讲）
Piccolo 用 **JoltPhysics**（开源刚体物理引擎，与 PhysX / Havok 同类）。

**它到底怎么算（一帧里物理做的事）：**
1. **数据接入**：`rigidbody_component` 定义刚体的形状（盒 / 球 / 胶囊）、质量、是否静态；`physics_scene` 据此在 Jolt 里创建一个 `Body`。
2. **积分 Integration**：根据受力 / 扭矩，用时间步更新每个刚体的速度、角速度、位置、朝向（常用半隐式欧拉 + 子步 substep 提高稳定性）。
3. **宽相 Broadphase**：用加速结构（BVH / Sweep & Prune）快速筛出「可能碰撞」的刚体对，避免 O(n²) 两两检测。
4. **窄相 Narrowphase**：对候选对做精确碰撞检测，算出接触点（contact point）、法线、穿透深度。
5. **约束求解 Solver**：迭代求解接触约束和关节约束，把物体「推开」到不穿透、并按物理规律反弹 / 滑动。这是物理稳定与否的关键。
6. **写回**：把算完的 transform 写回 `transform_component`，渲染和游戏逻辑就能读到新位置。

- 关键文件：`function/physics/physics_manager.{h,cpp}`、`physics_scene.{h,cpp}`、`physics_config.h`、`jolt/utils.{h,cpp}`、`framework/component/rigidbody/*`、`engine/3rdparty/JoltPhysics/`
- 核心概念：RigidBody、CollisionShape、Broadphase、Narrowphase、Contact、Constraint、Solver、Substep、Static vs Dynamic。
- 动手：① 在 `physics_scene` 加碰撞事件回调（A 碰到 B 打日志）；② 用 `debugdraw` 把碰撞体线框画出来，直观看物理形状；③ 调 `physics_config` 的 substep / 迭代次数，观察稳定性变化。

### 2.5 动画 animation（★★）
- 关键文件：`function/animation/animation_system.*`、`skeleton.*`、`node.*`、`animation_loader.*`、`utilities.*`、`component/animation/*`
- 核心概念：骨骼（Skeleton）、骨骼节点（Node）、动画采样、骨骼变换层级、动画混合（后续可深入）。
- 动手：加载一个已有动画，在编辑器里调播放速度 / 循环，观察 skeleton 节点变换如何驱动 mesh。

### 2.6 角色与控制器 character / controller（★★）
- 关键文件：`function/character/character.*`、`function/controller/character_controller.*`、`component/rigidbody/*`、`component/motor/*`
- 核心概念：角色 = 动画 + 控制器 + 刚体 的组合；角色控制器处理移动 / 跳跃 / 碰撞响应（常基于胶囊体 + 射线 / 扫掠）。
- 动手：调 `character_controller` 的移动速度 / 跳跃力，或给角色加一个简单的状态（走 / 跑）。

### 2.7 输入 input（★）
- 关键文件：`function/input/input_system.{h,cpp}`
- 核心概念：输入设备抽象、按键映射、帧内输入快照。
- 动手：加一个自定义按键，绑定到一个简单的编辑器动作。

### 2.8 粒子 particle（★★）
- 关键文件：`function/particle/particle_manager.*`、`particle_desc.*`、`particle_common.h`、`emitter_id_allocator.*`、`component/particle/*`、`render/passes/particle_pass.*`
- 核心概念：发射器（Emitter）、粒子描述、CPU / GPU 粒子更新、渲染如何画粒子。
- 动手：改粒子参数（数量 / 生命周期 / 速度），看画面变化。

### 2.9 UI 与编辑器 ui / editor（★★）
- 关键文件：`function/ui/window_ui.*`、`source/editor/*`（ImGui）、`core/meta`（属性面板靠反射）
- 核心概念：ImGui 即时模式 UI、编辑器如何靠反射自动生成组件属性面板、关卡保存 / 加载。
- 动手：在编辑器加一个自定义面板（如显示当前 draw call 数，配合 2.3 的统计接口）。

### 2.10 资源 resource（★）
- 关键文件：`resource/asset_manager/*`、`config_manager/*`、`res_type/*`
- 核心概念：资源加载 / 缓存 / 引用计数、资源类型（mesh / texture / material / level）、异步加载。
- 动手：跟一遍「一个 mesh 从磁盘到渲染」的完整路径。

---

## 3. 阶段三：综合改造与优化（跨模块，推 GitHub）

> 前置：阶段二 2.1–2.10 已读完，理解各模块数据流。
> 这些改造是阶段四（渲染管线现代化）的**直接前置**，建议按序号顺序推进。

把前面学到的东西用起来，做能「看到效果 / 用数据证明」的改造。每个改动直接提交到 main、推送。

**渲染相关（你最熟，先上手）**
- [ ] 方向光阴影 Shadow Map（加 shadow pass）→ 为 4.1 VSM 打基础
- [ ] 抗锯齿升级 FXAA → SMAA / TAA → 为 6.2 AI 渲染的 TAA 打基础
- [ ] 视锥剔除（减少 draw call，配合统计 HUD 看数字）→ 为 4.4 GPU-Driven 打基础
- [ ] 后处理合并（tone_mapping + color_grading + fxaa 合一，省带宽）
- [ ] SSAO / Bloom（扩战后处理）→ 为 4.6 DDGI 的 GI 概念打基础

**物理相关（补盲 + 实用）**
- [ ] 碰撞体调试可视化（debugdraw 画线框）→ 为 5.3 碰撞事件打基础
- [ ] 碰撞事件系统（触发音效 / 逻辑）→ 对应 5.3
- [ ] 物理性能剖析（刚体数 vs 耗时）→ 为 5.2 ECS 改造提供基准

**跨模块**
- [ ] 用反射给所有组件加统一调试面板
- [ ] 自定义 Component 端到端跑通（2.2 的延伸）

---

## 4. 阶段四：渲染管线现代化（进阶渲染）

> 前置：阶段三「方向光阴影 Shadow Map」「视锥剔除」已完成，理解 pass-based 延迟渲染管线。
> 本章按**依赖链顺序**推进：Frame Graph 是基础设施 → 在它之上构建 GPU-Driven → 再叠 Visibility Buffer → 最后加 GI。VSM 和体渲染不依赖 Frame Graph，可先行。

### 4.1 Virtual Shadow Maps — 虚拟阴影贴图

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

### 4.2 体渲染 — 大气散射 + 体积雾

**读什么**
- `function/render/passes/`（看现有 pass 列表，找到插入点）
- `function/render/render_pipeline.{h,cpp}`（pass 注册和执行顺序）
- Frostbite 论文："A Scalable and Production Ready Sky and Atmosphere Rendering" + "Physically-based & Unified Volumetric Rendering"

**关键概念**
- 大气散射：低成本实现真实天空和远距离雾（Rayleigh + Mie 散射）
- 体积雾：Frustum-aligned 3D 网格，逐体素计算雾照明，时域累积
- 体积光照：将太阳级联光注入体积系统，产生 God Rays 效果

**动手实践**：加一个体积雾 Pass
- 新建 `volumetric_fog_pass`，在 GBuffer 之后、光照之前执行
- 实现 Frustum-aligned 3D 网格（如 64×64×32），逐体素计算雾密度 × 光照
- 时域累积：上一帧结果重投影，减少噪声
- 验证：场景中出现体积雾和 God Rays 效果

---

### 4.3 Frame Graph — 声明式渲染管线

> 这是后续 GPU-Driven / VisBuffer / DDGI 的**基础设施**，优先级最高。

**读什么**
- `function/render/render_pipeline_base.{h,cpp}`（现有硬编码 pass 序列）
- `function/render/render_pass_base.{h,cpp}`（pass 基类、RT 资源声明）
- Frostbite GDC 2017："FrameGraph: Extensible Rendering Architecture"
- UE5 RDG 文档：Rendering Dependency Graph

**关键概念**
- 每帧渲染抽象为**有向无环图（DAG）**，节点 = Pass，边 = 资源依赖
- 编译期：拓扑排序 → 资源活跃区间 → 自动内存别名 → 自动屏障
- 执行期：按排序结果执行各 Pass lambda，自动创建/释放临时资源

**动手实践**：实现 FrameGraphBuilder + 迁移一个 Pass
- Phase 1：定义 `FrameGraphBuilder` 接口（`createTexture` / `createBuffer` / `addPass`）
- Phase 2：实现拓扑排序 + 资源活跃区间计算
- Phase 3：实现内存别名复用 + 自动屏障插入
- Phase 4：把现有 `directional_light_pass` 迁移到 FrameGraph 声明式 API
- 验证：新增 pass 只需 ~5 行声明（替代原来 ~60 行手动 RT 管理）；显存占用下降

**实测参考**：显存降 24%、Vulkan 分配数减少 98%（2341→47）、同步屏障 bug 归零。

---

### 4.4 GPU-Driven Rendering Pipeline — GPU 驱动渲染

> 前置：4.3 Frame Graph 已完成

**读什么**
- `function/render/render_scene.{h,cpp}`（CPU 端如何组织可见物体、提交 draw call）
- `function/render/render_entity.{h,cpp}`（实体 → mesh → draw call 的链路）
- Ubisoft《刺客信条》GPU-Driven 实践；UE5 Nanite 深度解析

**关键概念**
- 从 `DrawPrimitive`（CPU 逐物体组织）到 `DrawScene`（GPU 一次提交全部）
- Compute Shader 在 GPU 端完成：视锥剔除 → HZB 遮挡剔除 → LOD 选择
- Indirect Draw：GPU 生成 draw call 参数表，CPU 只提交一次

**动手实践**：GPU 端视锥剔除 + Indirect Draw
- 用 Compute Shader 在 GPU 端做视锥剔除（替代 CPU 遍历）
- 生成 indirect draw 参数 buffer，用 `vkCmdDrawIndirect` 一次调用渲染全部
- 对比：CPU 端帧耗时下降、draw call 数量变化
- 进阶：加 HZB 遮挡剔除

---

### 4.5 Visibility Buffer — 可见性缓冲渲染

> 前置：4.4 GPU-Driven 已完成

**读什么**
- `function/render/passes/main_camera_pass.{h,cpp}`（现有 G-Buffer 填充逻辑）
- Wolfgang Engel Visibility Buffer 论文；Nanite Visibility Buffer 详解

**关键概念**
- 两阶段渲染：先只记录「谁可见」，再对着色
- Pass 1：硬件光栅化大三角形 + 软件光栅化小三角形，输出 TriangleID + ClusterID + Depth
- Pass 2（Deferred Material）：读取 Visibility Buffer → 加载顶点数据 → 插值 G-Buffer → 材质着色

**动手实践**：用 Visibility Buffer 替换 G-Buffer
- 实现 Visibility Pass（64-bit/pixel：30-bit Depth + 27-bit Cluster ID + 7-bit Triangle ID）
- 实现 Deferred Material Pass（只对可见像素着色）
- 验证：同样场景下渲染正确，overdraw 浪费消除

---

### 4.6 DDGI + SDF — 实时动态全局光照

> 前置：4.3 Frame Graph + 4.5 Visibility Buffer

**读什么**
- `function/render/passes/`（现有光照 pass，理解直接光照 + 简单环境光）
- DDGI 论文（浙大 SDFDDGI）；Flax 引擎 DDGI 实现文档

**关键概念**
- DDGI 不依赖硬件光追，用 SDF 做软件光线求交
- 运行时每帧：组合场景级 Global Distance Field → Probe 用 SDF 光线求交 → 球谐光照数据

**动手实践**：实现简化版 DDGI
- 先给一个物体生成 SDF（如示例场景的静态 mesh）
- 实现 Probe 网格（8×8 Tile），用 SDF 做光线求交
- 生成球谐光照数据，在 G-Buffer 后处理阶段采样叠加
- 验证：间接光出现（暗处有反射光），性能控制在 ~1.3ms

---

## 5. 阶段五：物理与架构进阶

> 前置：阶段二 2.4 物理 + 2.2 ECS 已读完，阶段三「碰撞事件系统」已完成或并行。

### 5.1 XPBD 物理仿真 — 布料与柔体

**读什么**
- `function/physics/physics_manager.{h,cpp}`（现有 Jolt 刚体集成）
- `function/physics/physics_scene.{h,cpp}`（物理场景管理）
- PBD/XPBD 理论综述；SIGGRAPH 2025 AVBD 论文；《恋与深空》StrayCloth 实践

**关键概念**
- PBD：直接操作粒子位置满足约束，无条件稳定
- XPBD：引入拉格朗日乘子 λ，约束硬度可控且与时间步长无关
- 工业实践：StrayCloth 用 XPBD + Substep（1/200~1/300），骨骼作为模拟粒子

**动手实践**：在 Piccolo 里加一个 XPBD 布料组件
- 新建 `cloth_component`，用 XPBD 框架模拟一块布（粒子网格 + 距离约束 + 弯曲约束）
- 子步 substep（动态调整 1/120~1/240），保证稳定性
- 用 DebugDrawManager 画出粒子位置，验证仿真正确
- 把布料 mesh 挂到粒子上，渲染侧看到布料形变

---

### 5.2 ECS / DOD 混合架构改造

**读什么**
- `function/framework/object/game_object.h`（现有 GameObject-Component 容器模型）
- Unity DOTS 文档；Piccolo 官方论坛 DOP 理论帖

**关键概念**
- GameObject-Component 是 OOP 架构：内存碎片化、缓存不友好
- ECS 三要素：Entity（轻量 ID）/ Component（纯数据 SoA）/ System（处理特定组合，天然并行）
- 混合架构：保留 GameObject 用于编辑器和高层逻辑，性能热点用 ECS 重写

**动手实践**：把一个性能热点组件改造成 ECS 风格
- 挑一个简单但量大的场景（如粒子系统或 Boids 群集）
- 把 Component 数据从 `std::vector<GameObject*>` 改成 SoA 布局
- 用 `std::for_each + std::execution::par` 多线程遍历
- 对比改造前后的帧耗时和缓存命中率

---

### 5.3 碰撞事件系统

**读什么**
- `function/physics/physics_scene.{h,cpp}`（Jolt 物理场景的 contact 回调）
- `function/physics/jolt/utils.{h,cpp}`（Jolt ↔ Piccolo 类型转换）
- Jolt Physics 文档：ContactListener 回调

**关键概念**
- Jolt 的 `ContactListener` 提供 `OnContactAdded` / `OnContactPersisted` / `OnContactRemoved`

**动手实践**：实现碰撞事件回调系统
- 在 `PhysicsScene` 注册 `ContactListener`，转发碰撞事件到事件系统
- 设计 `CollisionEvent` 结构（两个实体 ID + 接触点 + 法线 + 冲击力）
- 在示例关卡里验证：两个物体碰撞时触发 `LOG_INFO`
- 进阶：用 DebugDrawManager 在接触点画标记

---

## 6. 阶段六：前沿实验（持续探索）

> 这些方向更适合独立实验，不阻塞主学习线。作为长期跟踪目标记录。

### 6.1 3D Gaussian Splatting 混合渲染

**读什么**
- 3DGS 原始论文（SIGGRAPH 2023）
- Unigine 2.20 / Unity / UE 的 3DGS 集成方案

**关键概念**
- 场景由数百万个 3D 高斯原语表示（非传统三角形网格）
- 混合管线：Mesh 负责逻辑和物理，3DGS 负责视觉保真

**实验方向**
- 从 .ply 格式导入 3DGS 数据
- 实现基础高斯光栅化 Pass（作为 FrameGraph 中的一个节点）
- 与现有 G-Buffer 深度合成

---

### 6.2 AI 辅助渲染管线

**读什么**
- NVIDIA DLSS 5 官方博客（GTC 2026）
- Arm 移动端神经渲染架构（NSS/NSSD/NFRU）

**关键概念**
- 神经超级采样（NSS）、神经降噪（NSSD）、神经帧率提升（NFRU）

**实验方向**
- 引入 TAA 作为基础时域累积框架
- 研究轻量级 CNN 超分模型替代传统 Upsample Pass
- 光追降噪器集成（SVGF 或神经降噪）
- Frame Generation 插帧实验

---

### 6.3 SIGGRAPH 论文跟踪

保持跟踪以下方向的最新论文，有合适的就纳入实验：
- AVBD（SIGGRAPH 2025 Best）— 超快速物理求解器
- Stable Cosserat Rods — 毛发/线缆仿真
- Offset Geometric Contact — 碰撞检测精度提升
- Implicit Position-Based Fluids — PBD 流体仿真
- Vector-Valued Monte Carlo Integration — 蒙特卡洛渲染降噪

---

## 7. 进度追踪

### 基础阶段（阶段一 ~ 阶段三）
- [x] 阶段一：整体概览（架构注释 + 引擎架构图均已完成推送 main）
- [x] 主循环帧耗时日志（2eda7a9：环形缓冲滑动平均已实现并推送 main）
- [ ] 2.1 引擎地基 core（反射/序列化已读，实验字段已加，待编译验证）
- [ ] 2.2 框架与 ECS（Light 组件已完成，待编译验证后提交推送）
- [ ] 2.3–2.10 各模块（渲染 / 物理 / 动画 / 角色 / 输入 / 粒子 / UI / 资源）
- [ ] 阶段三：综合改造清单

### 进阶阶段（阶段四 ~ 阶段六）
- [ ] 4. 渲染管线现代化
  - [ ] 4.1 VSM 虚拟阴影贴图
  - [ ] 4.2 体渲染（大气散射 + 体积雾）
  - [ ] 4.3 Frame Graph 声明式管线
  - [ ] 4.4 GPU-Driven Rendering
  - [ ] 4.5 Visibility Buffer
  - [ ] 4.6 DDGI + SDF 全局光照
- [ ] 5. 物理与架构进阶
  - [ ] 5.1 XPBD 物理仿真
  - [ ] 5.2 ECS / DOD 混合架构
  - [ ] 5.3 碰撞事件系统
- [ ] 6. 前沿实验
  - [ ] 6.1 3D Gaussian Splatting
  - [ ] 6.2 AI 辅助渲染
  - [ ] 6.3 SIGGRAPH 论文跟踪

---

## 8. 推荐节奏
- 第 1 周：阶段一（整体概览）+ 2.1 / 2.2（core + ECS）。
- 第 2–4 周：2.3 渲染（重点，最细）。
- 第 5–6 周：2.4 物理（补盲）+ 2.5 / 2.6 动画 / 角色。
- 第 7 周：2.7–2.10 输入 / 粒子 / UI / 资源 快速过。
- 第 8–9 周：阶段三综合改造（影子 / 抗锯齿 / 剔除 / 碰撞调试等）。
- 第 10–14 周：阶段四渲染管线现代化（VSM → Frame Graph → GPU-Driven → VisBuffer → DDGI，逐节推进）。
- 第 15–16 周：阶段五物理与架构进阶（XPBD / ECS 混合 / 碰撞事件）。
- 第 17 周起：阶段六前沿实验 & 持续跟踪论文。

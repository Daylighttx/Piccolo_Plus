# A01：Piccolo 的一帧从哪里开始，到哪里结束

> 状态：进行中。CPU 调用链、构建和运行基线已验证；GPU Capture 尚未完成，因为本机暂未找到 RenderDoc。

## 本节结果

Piccolo 当前可以用 Release 配置完整构建，`PiccoloEditor.exe` 能初始化 Vulkan 并持续渲染。日志中的稳定渲染耗时约为 1.9 ms。

一帧的主链已经定位为：

```text
PiccoloEditor::run
  → PiccoloEngine::tickOneFrame
      → WorldManager::tick                 逻辑更新
      → RenderSystem::swapLogicRenderData  交换逻辑/渲染数据
      → RenderSystem::tick                 渲染入口
          → processSwapData
          → RHI::prepareContext
          → updatePerFrameBuffer
          → updateLights
          → RenderScene::updateVisibleObjects
          → RenderPipeline::preparePassData
          → RenderPipeline::deferredRender / forwardRender
              → VulkanRHI::waitForFences
              → VulkanRHI::prepareBeforePass
              → Shadow Passes
              → MainCameraPass
              → VulkanRHI::submitRendering
                  → vkQueueSubmit
                  → vkQueuePresentKHR
```

这条链分成三个不同层级：

```text
Engine Tick       决定这一帧什么时候开始
Render Pipeline   决定这一帧画哪些 Pass、顺序是什么
Vulkan RHI        决定命令如何提交并显示到窗口
```

## 1. 编辑器主循环

程序入口是：

```text
engine/source/editor/source/main.cpp
  → PiccoloEditor::run()
```

`PiccoloEditor::run()` 每轮计算 `delta_time`，然后调用：

```cpp
m_engine_runtime->tickOneFrame(delta_time);
```

运行时版本的循环在 `PiccoloEngine::run()` 中做相同的事情。因此编辑器和独立运行时最后都会汇入 `PiccoloEngine::tickOneFrame()`。

关键文件：

- `engine/source/editor/source/main.cpp`
- `engine/source/editor/source/editor.cpp`
- `engine/source/runtime/engine.cpp`

## 2. 逻辑帧与渲染帧的交界

`PiccoloEngine::tickOneFrame()` 先运行世界逻辑，随后执行：

```cpp
g_runtime_global_context.m_render_system->swapLogicRenderData();
g_runtime_global_context.m_render_system->tick(delta_time);
```

这里有两个容易混淆的“交换”：

1. `swapLogicRenderData()` 交换的是 CPU 侧逻辑/渲染数据缓冲。
2. Vulkan Swapchain交换的是最终用于显示的窗口图像。

前者解决游戏逻辑与渲染数据的所有权边界；后者解决 GPU 渲染结果如何送到窗口。它们不是同一件事，也都不是简单的“防止画面撕裂开关”。

当前代码仍在一个主循环里先后调用逻辑和渲染，所以 `RenderSwapContext` 具备结构解耦，但还不代表逻辑线程和渲染线程已经并行。

## 3. RenderSystem 的六个步骤

`RenderSystem::tick()` 是渲染总入口：

### 3.1 `processSwapData()`

消费逻辑层提交的数据，将物体增删、Transform、相机和粒子变化更新到 RenderScene 与 GPU 资源。

### 3.2 `m_rhi->prepareContext()`

准备 RHI 每帧上下文。这里不是开始一个具体 RenderPass，而是让底层后端进入当前帧可工作的状态。

### 3.3 `updatePerFrameBuffer()`

把相机矩阵、环境光、方向光等逐帧数据写入动态 Ring Buffer。之后多个 Pass 通过 Descriptor引用这些数据。

### 3.4 `updateLights()`

遍历 `LightComponent`，重建 RenderScene 中的点光源列表，并更新方向光。

### 3.5 `updateVisibleObjects()`

执行视锥和灯光相关剔除，产出不同 Pass 使用的可见节点集合。这里建立“场景里存在”与“本帧实际绘制”之间的边界。

### 3.6 `preparePassData()` 与绘制

把可见对象、逐帧数据和资源引用交给各 Pass，随后调用 Forward 或 Deferred Pipeline。

关键文件：

- `engine/source/runtime/function/render/render_system.cpp`
- `engine/source/runtime/function/render/render_scene.cpp`
- `engine/source/runtime/function/render/render_resource.cpp`
- `engine/source/runtime/function/render/render_swap_context.cpp`

## 4. RenderPipeline 如何组织一帧

Piccolo 当前默认使用 Deferred Pipeline。主顺序可以概括为：

```text
等待当前 Frame Slot 可复用
  → 获取 Swapchain Image并开始 Command Buffer
  → Point Light Shadow
  → Directional Light Shadow
  → Main Camera RenderPass
      ├─ GBuffer
      ├─ Deferred Lighting
      ├─ Forward Lighting
      ├─ Skybox
      ├─ Tone Mapping
      ├─ Color Grading
      ├─ FXAA
      └─ UI / Combine UI
  → 结束并提交 Command Buffer
  → Present
  → 粒子相关拷贝与模拟
```

`MainCameraPass` 把多个阶段放进同一个传统 Vulkan RenderPass 的 Subpass 中。独立的 Shadow Pass 则在 MainCameraPass 之前执行并产出阴影贴图。

关键文件：

- `engine/source/runtime/function/render/render_pipeline.cpp`
- `engine/source/runtime/function/render/passes/main_camera_pass.cpp`
- `engine/source/runtime/function/render/passes/point_light_pass.cpp`
- `engine/source/runtime/function/render/passes/directional_light_pass.cpp`

## 5. 一帧为什么先等 Fence

CPU 可能远快于 GPU。如果当前 Frame Slot 上一轮提交的 GPU 工作还没结束，CPU 就覆盖它使用的 Command Buffer、Uniform Buffer或同步对象，会产生数据竞争。

因此管线开始时调用：

```cpp
vulkan_rhi->waitForFences();
```

它表达的是：

> 等到 GPU 完成了这个 Frame Slot 上一次关联的提交，CPU 才能复用该 Slot 的资源。

这不是等待显示器刷新，也不是等待所有 GPU 工作清空；它等待的是当前多帧飞行槽对应的 Fence。

## 6. `prepareBeforePass()` 做什么

这一阶段负责取得本帧要画的 Swapchain Image，并开始录制主 Command Buffer。若检测到窗口尺寸变化或 Swapchain 失效，会重建 Swapchain，并回调：

```cpp
RenderPipeline::passUpdateAfterRecreateSwapchain()
```

所有依赖 Swapchain 尺寸、Image View 或 RenderPass兼容性的 Framebuffer资源，都必须在这里同步更新。这也是以后添加 `ToonCharacterPass` 时必须通过的一项验收。

## 7. `submitRendering()` 做什么

渲染命令录制完成后，`VulkanRHI::submitRendering()` 执行三件核心工作：

```text
结束 Command Buffer
  → vkQueueSubmit
  → vkQueuePresentKHR
```

`vkQueueSubmit` 把命令提交到 Graphics Queue。提交结构中包含：

- 等待 Swapchain Image可用的 Semaphore。
- 当前帧录制好的 Command Buffer。
- 渲染完成后发信号的 Semaphore。
- 标记 Frame Slot完成的 Fence。

`vkQueuePresentKHR` 再等待“渲染完成”Semaphore，把对应 Swapchain Image交给显示系统。

这形成一个最小同步链：

```text
Acquire Image
  → image_available Semaphore
  → Graphics Queue执行 Command Buffer
  → render_finished Semaphore
  → Present Queue显示

Graphics Queue完成
  → in_flight Fence
  → 下一次复用该 Frame Slot
```

关键文件：

- `engine/source/runtime/function/render/interface/rhi.h`
- `engine/source/runtime/function/render/interface/vulkan/vulkan_rhi.cpp`

## 8. 本次构建与运行证据

构建命令：

```powershell
cmake --build build --config Release
```

结果：构建成功，生成：

```text
build/engine/source/editor/Release/PiccoloEditor.exe
```

运行测试使用 `bin/PiccoloEditor.exe`，工作目录为 `bin`。编辑器成功初始化 Vulkan并持续输出帧日志，渲染耗时稳定在约 1.9 ms。

当前警告主要是 MSVC `C4819`：源文件中的部分字符不能用代码页 936 表示。它不阻止构建，但后续应统一工程源码编码；本节不修改该问题，以免把编码整理和帧链分析混在一起。

## 9. 尚缺的证据

本机当前没有发现 `qrenderdoc` 或 `renderdoccmd`，所以本节还缺一次 GPU Capture。安装或提供 RenderDoc 后，需要补齐：

- GPU Event层级截图。
- Shadow Pass与 MainCameraPass 的 Attachment。
- Subpass切换。
- 最终 `vkQueuePresentKHR` 前的输出图像。
- 一次 Mesh Draw对应的 Pipeline、Descriptor和顶点/索引缓冲。

完成这些证据后，A01 才标记为完成。

## 10. 与 Godot 和 UE5 的对应关系

| Piccolo | Godot | UE5 |
|---|---|---|
| `RenderSystem::tick` | `RendererSceneRenderRD::render_scene` | Scene Renderer入口 |
| `RenderPipeline` | `RenderForwardClustered` | RDG Pass编排 |
| `RenderPass` 子类 | `PassMode + RenderList` | `FMeshPassProcessor` / RDG Pass |
| `RHI` | `RenderingDevice` | RHI |
| `VulkanRHI` | Vulkan RD Driver | VulkanRHI / D3D12RHI |
| Visible Nodes | `RenderGeometryInstance` | Primitive / MeshDrawCommand |
| `submitRendering` | RenderingDevice提交 | RHI Command List提交 |

下一节 A02 会选中一个真实 Mesh，从 `MeshComponent` 产生渲染数据开始，一直跟踪到 `MainCameraPass` 中的 `cmdDrawIndexedPFN()`，列清每一个 Descriptor绑定的数据。

# A05：用 RenderDoc 把 Draw Call 和 Vulkan 管线逐项对上

本章最终会留下一个可复现的 RenderDoc 帧捕获流程、一份自动生成的事件与管线报告，以及一张从 RenderDoc Event 回到 Piccolo 源码的路线图。重点不是“看到模型了”，而是确认 A04 送到 GPU 的 Toon 数据确实落在一条真实的 Vulkan Draw Call 上。

本次实测的捕获中，Toon 描边位于 `MainCameraPass` 的 **Forward Lighting 子通道（subpass 2）**；Event Browser 中的 `Toon Outline` 是 Vulkan Debug Utils 调试标记，不是另开的一条 `VkRenderPass`。标记下有一个 `vkCmdDrawIndexed`：55,500 个索引、1 个实例；管线为三角形列表、剔除正面、深度测试开启、深度写入关闭、比较函数 `LESS`。这些值来自本次 RenderDoc 捕获，不是根据预期填出来的。

## 1. 本章要回答的问题

A04 已经把 `toon_outline_width` 和 `toon_outline_color` 送进每实例 Storage Buffer。但仅看源码，只能证明 CPU 有写数据；仅看画面，也无法证明渲染时绑定了哪条 Pipeline、哪些 Buffer 或哪次 Draw。

RenderDoc 让我们沿着一帧 GPU 命令检查：

```text
Vulkan 帧捕获
  → Event Browser：哪一条 GPU 命令画了描边？
  → Pipeline State：它使用什么 shader、顶点输入、光栅和深度状态？
  → Descriptor：shader 读取了哪些 buffer？
  → Piccolo 源码：这些 GPU 命令由哪段 C++ 录制？
```

RenderDoc 不会替我们猜效果对不对。它给的是 GPU 命令及状态证据；视觉效果仍需结合最终画面和后续像素/顶点调试判断。

## 2. 捕获前的准备

本章在 Windows、Vulkan、RenderDoc 1.46 和 Piccolo Release 构建上完成实测。不同 RenderDoc 版本的界面文字会有变化，但 Event、Pipeline State 和 Descriptor 的核对方法相同。

### 2.1 安装 RenderDoc 并确认 Vulkan 层可用

RenderDoc 必须在 Piccolo 创建 Vulkan Instance 时已经作为 Vulkan 层加载。程序启动后才打开 RenderDoc，并不能 retroactively 把该进程变成已注入状态。本机通过 RenderDoc 安装器注册了 Vulkan Layer；捕获脚本会在启动编辑器前设置 `ENABLE_VULKAN_RENDERDOC_CAPTURE=1`。

如果安装器询问是否注册 Vulkan Layer，请启用它。若捕获失败，先确认 RenderDoc 已安装、Vulkan Layer 已注册，再重新启动 Piccolo；不要只反复点 RenderDoc 的 Capture 按钮。

### 2.2 构建 Release

在仓库根目录执行：

```powershell
cd D:\Code\AI_Only\Piccolo_Plus
cmake -S . -B build
cmake --build build --config Release
```

本机 Release 构建已成功生成 `PiccoloRuntime.lib` 和 `PiccoloEditor.exe`。编辑器必须从 `bin` 目录作为工作目录启动，因为资源路径按此工作目录解析。

### 2.3 为什么 Release 还要显式启用 GPU Event 名称

Piccolo 在 Debug 构建中默认打开 Vulkan Debug Utils labels；Release 构建会关闭它们以避免额外调试开销。A05 增加了一个单独的环境开关：

```text
PICCOLO_VULKAN_DEBUG_LABELS=1
```

它让 Release 创建 Vulkan Instance 时启用 Debug Utils 扩展并记录事件名称；不会因此打开 Vulkan Validation Layers。开关在 `engine/source/runtime/function/render/interface/vulkan/vulkan_rhi.cpp` 的 `VulkanRHI::initialize()` 读取。

## 3. 启动 Piccolo 并捕获指定帧

从仓库根目录运行：

```powershell
.\docs\render-learning\tools\capture_piccolo_a05.ps1
```

默认捕获引擎帧 120。想改帧号时：

```powershell
.\docs\render-learning\tools\capture_piccolo_a05.ps1 -Frame 180
```

脚本临时设置四个进程环境变量，再从 `bin` 目录启动编辑器；编辑器退出后会恢复调用 PowerShell 原本的环境变量：

| 变量 | 用途 |
|---|---|
| `ENABLE_VULKAN_RENDERDOC_CAPTURE=1` | 请求 Vulkan RenderDoc Layer 在应用启动时介入 |
| `PICCOLO_VULKAN_DEBUG_LABELS=1` | Release 下打开 Debug Utils Event 标签 |
| `PICCOLO_RENDERDOC_CAPTURE_FRAME=120` | 在 Piccolo 的第 120 个引擎帧启动捕获 |
| `PICCOLO_RENDERDOC_CAPTURE_FILE=...\piccolo-a05` | 指定 RenderDoc 输出文件名模板 |

Piccolo 在 `engine.cpp` 的帧循环中先增加 `m_frame_count`，再检查目标帧；匹配后在 `rendererTick()` 前调用 `StartFrameCapture()`，并在它返回后调用 `EndFrameCapture()`。所以这里的 120 是 Piccolo 帧循环计数，不是 RenderDoc 文件编号，也不是 GPU 命令编号。

运行日志应包含类似信息：

```text
RenderDoc capture start requested for frame 120: active=1
RenderDoc capture end requested for frame 120: saved=1
```

看到 `active=1` 和 `saved=1`，说明 RenderDoc API 接受了本次帧捕获。捕获结束后可以关闭编辑器。`.rdc` 文件会出现在 `docs/render-learning/evidence/A05/`；它约 65 MB，且包含机器相关的 GPU 资源数据，因此 `.gitignore` 会让它保留在本机而不进入 Git。仓库只提交可读的文本报告和复现脚本。

## 4. 从 Event Browser 找到描边 Draw

RenderDoc 打开捕获后，在 Event Browser 中展开主相机录制的命令。关键层级如下：

```text
vkCmdBeginRenderPass
  BasePass
  vkCmdNextSubpass() => 1
  Deferred Lighting
  vkCmdNextSubpass() => 2
  Forward Lighting
    Toon Outline
      vkCmdDrawIndexed()
    ParticleBillboard
      vkCmdDraw()
```

本次捕获中描边 Draw 是 **Event ID 153 / Action ID 21**。ID 是这一份捕获里的临时编号；换一台机器、改一帧或改命令顺序，ID 都可能变化。判断目标时以父级 `Toon Outline` 调试标记为准，不要把 153 写死在自己的分析脚本中。

`Toon Outline` 这个树节点来自 `MainCameraPass::drawToonOutline()` 的 `pushEvent()`。它的用途是把 GPU 命令分组并命名，便于在人眼和工具中定位；它不创建 Vulkan Render Pass，不做资源屏障，也不改变子通道。真正决定归属的是 Graphics Pipeline 创建信息中的 `pipeline_info.subpass = _main_camera_subpass_forward_lighting`。

这一区别很重要：本实验添加的是 **新的 Graphics Pipeline 和 Draw**，并将它们插入现有 `MainCameraPass` 的 Forward Lighting 子通道；不是新增一个独立 `VkRenderPass`。在事件树中看到一个有名字的组，不代表引擎底层多创建了一条 Pass。

## 5. 查看 Pipeline State

选中 `Toon Outline` 下的 `vkCmdDrawIndexed()`，查看 Pipeline State。也可以用仓库里的自动报告脚本读取这个 Draw 的状态：

```powershell
$capture = (Resolve-Path .\docs\render-learning\evidence\A05\piccolo-a05_capture.rdc).Path
$env:PICCOLO_RENDERDOC_REPORT = (Join-Path (Resolve-Path .\docs\render-learning\evidence\A05).Path 'capture-report.txt')
& 'C:\Program Files\RenderDoc\qrenderdoc.exe' --ui-python `
  (Resolve-Path .\docs\render-learning\tools\analyze_renderdoc_capture.py).Path $capture
```

如果 RenderDoc 安装在其他目录，把 `qrenderdoc.exe` 路径改成实际位置。脚本会遍历 Event 名称，动态找到 `Toon Outline` 下面的 indexed draw，再调用 RenderDoc replay API 查询该事件状态；它不会假设 Event ID 永远是 153。

本次捕获的关键报告：

```text
API: Vulkan
Event ID: 153; Action ID: 21
Draw: vkCmdDrawIndexed(); indices/vertices=55500; instances=1; firstIndex=0; baseVertex=0
Topology: TriangleList
Raster: fill=Solid; cull=Front; frontCCW=True
Depth: test=True; write=False; compare=Less
Index buffer: ... stride=2
Vulkan pass: subpass=2; cullMode=Front; depthTest=True; depthWrite=False; depthCompare=Less
```

逐项读这份状态：

1. `TriangleList` 与 `55500` 个索引表示这次调用按三角形列表取索引；若索引没有被 primitive restart 分隔，就是 18,500 个三角形。
2. 索引 stride 为 2 字节，和源码中 `RHI_INDEX_TYPE_UINT16` 相符。`instances=1` 表示本次 Draw 画一个 mesh 实例；不是说整个角色只有一个三角形或只有一个子网格。
3. `cull=Front` 与壳层描边算法相配：顶点 Shader 将顶点沿法线外推，Rasterizer 丢弃扩张壳的正面，让背面轮廓露出。若改成 `Back`，通常会让被遮挡的正面壳覆盖模型而不是得到预期边线。
4. `frontCCW=True` 说明 Vulkan 将逆时针绕序作为正面判定；剔除哪一面必须和模型三角形绕序、Vulkan viewport/Y 方向设置一起看。
5. `Depth test=True; write=False; compare=Less` 表示轮廓片元仍需通过已有场景深度测试，但不会覆写深度缓冲，避免描边壳改变后续物体的深度结果。此设置也要求 Forward Lighting 子通道确实有深度附件；源码的 subpass 描述和 RenderDoc 状态均能核对这一点。

Shader 输入和 Buffer stride 在同一份报告中也能对上 `MeshVertex`：位置槽 stride 12 字节；法线/切线槽 stride 24 字节；UV 槽 stride 8 字节。描边顶点 Shader 从 location 0 读取 `in_position`，从 location 1 读取 `in_normal`；公共 `MeshVertex` 布局还声明了 location 2 的 tangent 和 location 3 的 UV，但描边 Shader 本身没有读取它们。骨骼索引和权重不是额外的顶点属性，它们从 set 1 的 Storage Buffer 读取。

## 6. 检查 Descriptor：Shader 从哪里读数据

`toon_outline.vert` 声明的资源与 C++ 管线布局对应如下：

| Set / Binding | 数据 | C++ 创建和绑定位置 |
|---|---|---|
| set 0 / binding 0 | 每帧投影、相机等数据 | `_mesh_global` 布局；动态 Storage Buffer |
| set 0 / binding 1 | 每实例模型矩阵、描边宽度、描边颜色等 | `_mesh_global` 布局；动态 Storage Buffer |
| set 0 / binding 2 | 每实例骨骼矩阵数组 | `_mesh_global` 布局；动态 Storage Buffer |
| set 1 / binding 0 | 每顶点骨骼索引和权重 | `_per_mesh` 布局；Storage Buffer |

对应 Shader 里的 `layout(set = ..., binding = ...)` 与 `setupDescriptorSetLayout()`、`setupPipelines()`、`drawToonOutline()` 三处 C++。draw 之前先绑定 set 1 的 mesh joint 数据，再绑定 set 0 的三个全局动态 Storage Buffer 和三项 dynamic offsets。Descriptor Set 的编号不是“全局/局部”的固定语义，而是 Pipeline Layout 数组中的位置；检查时应从 `pSetLayouts` 的顺序追到绑定调用。

本次 RenderDoc 报告确实列出了四项被 Vertex Shader 使用的 Storage Buffer Descriptor：三项来自全局上传环形缓冲区，一项来自当前 Mesh 的骨骼索引/权重 Buffer。报告还给出每个 Descriptor 的资源 ID、偏移和范围。资源 ID 和绝对偏移只对本次捕获有效，不应复制成程序常量。

本章核对到“描述符在目标 Draw 时已绑定，Shader 使用了这些 Storage Buffer”。**本章没有用 Structured Buffer Viewer 解码 GPU 内存中的单个 `toon_outline_width` / `toon_outline_color` 数值。**参数沿 CPU 数据链的传递在 A04 检查；如果要证明 GPU 字节值和 JSON 完全一致，下一次可单开一个 Buffer Viewer 实验，而不应把“Descriptor 已绑定”夸大成“所有字段字节都已读回”。

## 7. 把 GPU Event 对回 Piccolo 源码

在 `engine/source/runtime/function/render/passes/main_camera_pass.cpp` 的 `MainCameraPass::drawToonOutline()` 中依次可以找到：

1. 从 `m_visiable_nodes.p_main_camera_visible_toon_mesh_nodes` 读取可见 Toon 节点；
2. 按 `VulkanMesh*` 分批，避免把不同 Mesh 的 Buffer 混在同一次 Draw；
3. 绑定描边 Graphics Pipeline、Viewport、Scissor；
4. 绑定 mesh vertex/index buffers 与 set 1 的每 Mesh 骨骼数据；
5. 为当前批次写入每实例模型矩阵、蒙皮开关、宽度和颜色；
6. 绑定 set 0 和 dynamic offsets；
7. 调用 `cmdDrawIndexed(mesh.mesh_index_count, current_instance_count, 0, 0, 0)`；
8. 退出 `Toon Outline` Debug Utils label。

因此 RenderDoc 的 Event 153 并非“Shader 名称猜测”出来的：C++ 在那段函数周围写了命名标记，标记下紧跟的 indexed draw 使用的正是该 Graphics Pipeline 与 descriptors。Capture report 能证明 GPU 命令，源码能解释命令为何按此顺序生成。

## 8. 这次核对后，哪里做错了吗？

**本次捕获流程和目标 Draw 的 Pipeline 状态是正确的，但代码审阅确实找到并修复了一个条件性错误。**如果同一个 `VulkanMesh` 批次混有蒙皮和不蒙皮实例，旧逻辑要求“整批都蒙皮”才分配并上传骨骼矩阵；与此同时，每实例标记仍会让蒙皮实例进入 Shader 蒙皮分支。该实例于是会用 dynamic offset 0 读取无关的环形缓冲区数据。

现在主相机 GBuffer、主相机 Mesh Lighting、Toon 描边和方向光阴影路径都改为：批里**至少一个**实例蒙皮时分配骨骼矩阵块，并逐实例只写入实际有骨骼矩阵的条目。不蒙皮实例的 Shader 分支不会读取矩阵。该修复在代码路径上覆盖了同 Mesh 混合批次；本章捕获只有一个蒙皮实例，尚未用专门的混合实例场景触发并录制这个边界案例。

曾经最像另一个 bug 的地方是 Shader 对骨骼索引使用 `in_indices.x > 0` 这类判断，看起来像漏掉骨骼 0。追到 Piccolo 的约定后可确认它是有意的：`animation_loader.cpp::addBoneBind()` 把真实骨骼索引加一，0 保留为“未使用”；`mesh_component.cpp` 又先把 Identity 矩阵放在 joint 数组的第 0 项。因此真正的骨骼 0 存在 joint matrix 索引 1，条件 `> 0` 不会误删它。此处不应只看 GLSL 就草率改成 `>= 0`。

不过当前 Shader 用 `mat3(model_matrix) * normal`（蒙皮时也直接取 joint matrix 的 `mat3`）变换法线。这对角色当前使用的旋转/平移与均匀缩放成立；若后续允许非均匀缩放，法线应该使用逆转置矩阵。它是通用法线变换的适用边界，不是这次 RenderDoc 捕获失败，也不宜在 A05 随手引入每顶点矩阵求逆而不测性能。

另一个需要纠正的是证据表述：只看到 Event 标记不能证明独立 RenderPass，也不能证明所有 Descriptor 里的 CPU 参数值。本章现在明确区分了 Event、子通道、Pipeline 和 Buffer 内容；之前尝试的 target-control 触发脚本不再保留，避免读者走一条本机未验证的辅助路径。推荐复现路径是启动前注册 Vulkan 层，再由 Piccolo 的 in-app RenderDoc API 精确包住一帧。

## 9. 排错表

| 现象 | 常见原因 | 检查办法 |
|---|---|---|
| 日志显示 `renderdoc.dll is not loaded` | Vulkan 层没有在进程启动时注入 | 检查 RenderDoc 安装及 Vulkan Layer 注册；关掉 Piccolo 后用脚本重新启动 |
| 捕获文件没生成，`active=0` 或 `saved=0` | Layer 未接管进程，或 API 没开始捕获 | 看日志；确认脚本启动时设了 `ENABLE_VULKAN_RENDERDOC_CAPTURE=1`；不要启动后再设环境变量 |
| Event Tree 只有 `vkCmdDraw...`，没有 `Toon Outline` 等名称 | Release 没启用 Debug Utils 标签 | 设置 `PICCOLO_VULKAN_DEBUG_LABELS=1` 后重新捕获；它必须在 Vulkan Instance 创建前设置 |
| 捕获有 Event 树但没 Toon Draw | Toon 节点未进入可见列表、角色不在视锥内，或对应 Draw 未执行 | 在 `RenderScene::updateVisibleObjects()` 检查 Toon Visible List；回到 A02/A03 看过滤链 |
| 报告没有 `Toon Outline` indexed draw | 打开的 `.rdc` 不是目标帧，或本帧没有可见 Toon Mesh | 重新捕获目标关卡，确认 Event Browser 内的 Forward Lighting 下有标记 |
| Release 编译 `engine.cpp` 报 Windows API 冲突 | 直接 include `Windows.h` 可能带入 `min/max` 宏 | 当前实现只声明所需 Kernel32 符号，避免把 Windows 宏泄露进引擎头文件 |

## 10. 验收清单与证据边界

| 等级 | 本章结果 | 证据 |
|---|---|---|
| L1 静态链路 | 通过 | 能从 `drawToonOutline()` 指到 Pipeline、Descriptor、Vertex/Index Buffer 与 Draw |
| L2 构建 | 通过 | Release 生成 `PiccoloRuntime.lib` 和 `PiccoloEditor.exe` |
| L3 运行 | 通过 | Piccolo 成功运行到捕获帧，RenderDoc API 返回 `active=1 / saved=1` |
| L4 画面 | 本章不单独打勾 | 本章没有保存稳定的像素对比图；Draw 被录制不等于轮廓质量已经通过美术验收 |
| L5 GPU 证据 | 通过（状态层面） | [capture-report.txt](../evidence/A05/capture-report.txt) 记录真实 Event、Pipeline、Vertex Input、Descriptor 与 Draw 参数 |

`.rdc` 二进制捕获文件刻意不提交。需要重建时运行本章脚本；如果要分享可复核的单帧原始数据，可以后续把压缩后的捕获作为 GitHub Release 附件，而不是让仓库每次 clone 都下载 65 MB。

## 本章文件索引

- `engine/source/runtime/engine.cpp`：按帧号调用 RenderDoc in-app API。
- `engine/source/runtime/function/render/interface/vulkan/vulkan_rhi.cpp`：Release 环境下打开 Vulkan Debug Utils labels。
- `docs/render-learning/tools/capture_piccolo_a05.ps1`：启动编辑器并设置一次性捕获环境。
- `docs/render-learning/tools/analyze_renderdoc_capture.py`：自动找到 Outline Draw 并输出 Event/Pipeline/Descriptor 摘要。
- `docs/render-learning/evidence/A05/capture-report.txt`：本机实测的文本化 GPU 证据。

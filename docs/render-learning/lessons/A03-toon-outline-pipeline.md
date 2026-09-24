# A03：第一个真正可见的 Toon Outline Pipeline

> 状态：已完成。Player Robot 通过独立 Graphics Pipeline 执行带蒙皮的倒壳描边，并在 GPU 调试事件中显示为 `Toon Outline`。

## 1. 本节完成了什么

这一节第一次真正改变最终画面，不再只是准备数据。完整链路如下：

```text
player.object.json: is_toon_character = true
  → RenderEntity
  → 主相机视锥裁剪
  → Toon Visible List
  → MainCameraPass::drawToonOutline()
  → 绑定 Toon Outline Graphics Pipeline
  → 上传相机、实例矩阵和骨骼矩阵
  → 绑定 Vertex / Index Buffer
  → vkCmdDrawIndexed
  → 顶点沿法线外扩
  → 只保留外壳背面
  → 输出深色轮廓
```

这里实现的是几何倒壳描边（inverted hull outline），不是屏幕空间边缘检测。

## 2. 为什么没有新建一个完全独立的 VkRenderPass

Piccolo 的 `MainCameraPass` 名字虽然叫 Pass，内部实际是一个 Vulkan `VkRenderPass` 加八个 subpass：

```text
subpass 0  BasePass / GBuffer
subpass 1  Deferred Lighting
subpass 2  Forward Lighting
subpass 3  Tone Mapping
subpass 4  Color Grading
subpass 5  FXAA
subpass 6  UI
subpass 7  Combine UI / Swapchain
```

描边需要读已经生成的深度，并写入 HDR Scene Color。`forward_lighting` subpass 恰好同时拥有这两个条件：

- BasePass 已经写完角色主体深度。
- Deferred Lighting 已经生成 HDR 场景色。
- Forward subpass 可以继续做带深度测试的几何绘制。
- 后续 Tone Mapping、Color Grading 和 FXAA 会自然处理轮廓。

如果现在建立独立 `VkRenderPass`，还需要额外处理 Attachment layout transition、load/store、pipeline barrier、Framebuffer 和 Swapchain 重建。那些是值得学习的内容，但并不是完成第一个描边所需的最小闭环。因此本节新增的是独立 Graphics Pipeline 和 Draw Stage，复用主相机 RenderPass 的 forward subpass。

## 3. 倒壳描边的几何原理

角色主体正常绘制一次后，再绘制一次相同 Mesh：

```glsl
world_position += world_normal * outline_width;
```

第二次绘制的模型比原模型略大。随后 Pipeline 使用：

```text
cullMode = FRONT
```

正常模型一般剔除背面，只看正面；倒壳则反过来剔除正面，只画膨胀外壳的背面。原角色主体会遮住外壳的大部分区域，只有轮廓边缘露出来，于是看起来像一圈描边。

在 A03 的最初版本中，实验值写在 Shader 内：

```glsl
outline_width = 0.025
outline_color = vec4(0.015, 0.02, 0.03, 1.0)
```

Robot 的模型单位接近米，因此 `0.025` 约等于 2.5 厘米的世界空间外扩。它是用于验证管线的临时常量。当前源码已在 A04 中把宽度和颜色迁移成资产参数；这里保留的是实现演进记录。

## 4. Vertex Shader 每一步

文件：`engine/shader/glsl/toon_outline.vert`

### 4.1 Descriptor Set 0：每帧与每次 Draw 数据

它复用 Piccolo 原有 `_mesh_global` Descriptor Set：

```text
binding 0：相机 proj_view、灯光等每帧数据
binding 1：每个 Instance 的 model_matrix、是否蒙皮
binding 2：每个 Instance 的骨骼矩阵数组
```

描边只真正使用 `proj_view_matrix`、`model_matrix` 和骨骼数据，但布局必须和已有 Descriptor Set 兼容，才能直接复用已经分配的 Descriptor Set。

### 4.2 Descriptor Set 1：每个 Mesh 的骨骼索引和权重

Robot 是 Skinned Mesh。每个顶点需要读取四组骨骼索引和权重：

```text
indices_and_weights[gl_VertexIndex]
```

如果描边不执行同样的蒙皮，主体在播放动画时移动，轮廓仍停在绑定姿势，二者会彻底错位。因此 Outline Vertex Shader 重用了主体 Shader 的 Linear Blend Skinning 计算。

### 4.3 为什么先蒙皮，再沿法线外扩

正确顺序是：

```text
模型空间顶点
  → 骨骼蒙皮得到动画姿势
  → Model Matrix 变换到世界空间
  → 使用动画后的世界法线外扩
  → Projection × View
  → Clip Space
```

如果先外扩再蒙皮，关节附近的轮廓宽度和方向可能出现错误；如果完全不变换法线，角色旋转后描边会朝错误方向膨胀。

## 5. Fragment Shader 为什么很简单

文件：`engine/shader/glsl/toon_outline.frag`

```glsl
out_scene_color = vec4(0.015, 0.02, 0.03, 1.0);
```

描边阶段不需要 PBR 材质贴图、法线贴图、灯光或阴影。它只输出一种颜色，因此 Pipeline Layout 也不包含 Material Descriptor Set。这减少了绑定次数，并明确表达了该阶段的职责：只产生轮廓颜色。

## 6. Graphics Pipeline 每个状态为什么这样设置

Pipeline 在 `MainCameraPass::setupPipelines()` 中创建。

### Shader Stages

```text
TOON_OUTLINE_VERT
TOON_OUTLINE_FRAG
```

CMake 会把 GLSL 编译成 SPIR-V，再生成 C++ 头文件；运行时通过 `createShaderModule()` 创建 Vulkan Shader Module。Pipeline 创建完即可销毁 Module，因为 Pipeline 已经保存编译结果。

### Vertex Input

继续使用 `MeshVertex::getBindingDescriptions()` 和 `getAttributeDescriptions()`，保证输入 Buffer 的绑定方式与原 Mesh 完全相同。描边 Vertex Shader 只消费 Position 和 Normal，额外提供但未使用的 Tangent/UV 不影响兼容性。

### Input Assembly

```text
topology = TRIANGLE_LIST
```

因为 Robot 的 Index Buffer 本身就是三角形列表。描边不是画 Vulkan 线段，而是画一个放大的实心三角形外壳。

### Rasterization

```text
polygonMode = FILL
cullMode = FRONT
frontFace = COUNTER_CLOCKWISE
```

`FRONT` 是倒壳成立的关键。使用 `LINE` Polygon Mode 只能显示三角网格边，不会得到角色剪影。

### Depth

```text
depthTestEnable = true
depthWriteEnable = false
depthCompareOp = LESS
```

- 开启深度测试：外壳被更近的角色主体和场景物体遮挡，只在合理位置出现。
- 关闭深度写入：描边是附加颜色，不应污染后续粒子、透明物体等阶段使用的主深度。
- 使用 `LESS`：与 Piccolo 现有主相机 Pipeline 的非反转深度约定保持一致。

### Blend

```text
blendEnable = false
```

当前输出的是不透明深色轮廓，直接覆盖目标颜色。未来若增加半透明或彩色边缘，再单独设计混合方程。

### RenderPass 与 Subpass

```text
renderPass = MainCameraPass 的 VkRenderPass
subpass = forward_lighting
```

Vulkan Graphics Pipeline 与 RenderPass/Subpass 的 Attachment 接口必须兼容。这里 Fragment Shader 只输出一个 Scene Color，正好匹配 forward subpass 的单颜色附件。

## 7. `drawToonOutline()` 在 CPU 侧做了什么

### 7.1 空列表快速退出

没有可见 Toon 角色时立即返回，不绑定 Pipeline、不申请 Ring Buffer，也不产生空 Draw Call。

### 7.2 按 VulkanMesh 分批

```text
Toon RenderMeshNode
  → map<VulkanMesh*, vector<OutlineMeshNode>>
```

同一个 Mesh 的多个实例可以合并为一次 Instanced Draw。描边不需要材质，所以不像原 GBuffer 路径那样先按 Material、再按 Mesh 分组。

### 7.3 上传 Per-frame 数据

从 Piccolo 每帧 Upload Ring Buffer 中按设备要求的 alignment 分配空间，写入 `MeshPerframeStorageBufferObject`。Vertex Shader 通过动态偏移读取当前相机的 `proj_view_matrix`。

### 7.4 上传 Per-drawcall 数据

每个 Draw Call 最多处理 `s_mesh_per_drawcall_max_instance_count` 个实例。CPU 写入：

```text
model_matrix
enable_vertex_blending
```

然后通过 Descriptor Set 0 的动态偏移告诉 GPU 本次 Draw 应读取 Ring Buffer 的哪一段。

### 7.5 上传骨骼矩阵

如果这一批实例使用蒙皮，就再分配一块 `MeshPerdrawcallVertexBlendingStorageBufferObject`，按实例槽位复制骨骼矩阵。这样 Outline Shader 和主体 Shader 使用同一帧动画姿势。

### 7.6 绑定真正的几何资源

```text
Position Buffer
Varying/Normal Buffer
Joint Binding Buffer
Index Buffer
```

它们全部复用已经上传到 GPU 的 `VulkanMesh`，没有为描边复制第二套 Mesh。

### 7.7 发出 Draw Call

最终调用：

```cpp
cmdDrawIndexedPFN(index_count, instance_count, ...)
```

在 Vulkan 后端继续映射到 `vkCmdDrawIndexed()`。Robot 的 `index_count` 为 55,500。

## 8. 在整帧中的执行位置

延迟路径现在是：

```text
BasePass / GBuffer
  → Deferred Lighting
  → Forward Lighting
       → drawToonOutline()
       → particle_pass.draw()
  → Tone Mapping
  → Color Grading
  → FXAA
  → UI
```

Forward 路径中则在正常 Mesh 与 Skybox 之后、Particle 之前调用描边。两条主相机路径都能看到一致结果。

## 9. GPU 调试标记

绘制函数增加了：

```text
Toon Outline
```

GPU Event。以后用 RenderDoc Capture 时，可以直接展开 `Forward Lighting`，定位 Toon Pipeline、Descriptor、Vertex Buffer 与 `vkCmdDrawIndexed`，而不需要在大量 Draw Call 中猜哪一个是描边。

## 10. 验证结果

- 两个 GLSL Shader 均由 `glslangValidator` 成功编译为 SPIR-V。
- Release C++ 编译、链接成功。
- Graphics Pipeline 在真实 Vulkan 运行时创建成功。
- 默认关卡正常加载，编辑器持续响应；2026-09-19 再次启动验证时约 `298 FPS`。
- 最新运行日志未出现 `error`、`fatal`、`assert` 或 `failed`。
- 当前电脑操作接口没有暴露 Piccolo 原生窗口截图，因此本节完成了编译、Pipeline 创建和持续运行验证；最终轮廓宽度的目视验收仍以屏幕上的 Piccolo 窗口为准，不把“进程正常”冒充为图像质量验收。

## 11. A03 完成时的实现边界

这是一个已经完整跑通的第一版几何描边，但还不是最终生产版本：

- 宽度目前是世界空间常量，远近视觉宽度会变化。
- A03 完成时颜色和宽度尚未成为资产参数；该限制已在 A04 解决。
- 非均匀缩放应使用 inverse-transpose normal matrix，而当前沿用 Piccolo 主 Mesh Shader 的简化矩阵。
- 硬边模型的法线断裂可能让轮廓不连续，后续可加入平滑描边法线。
- 透明部件、眼睛、头发和武器还没有独立的描边策略。

下一节 A04 给出从 JSON 到 GPU Storage Buffer 的完整参数链路和手工复现实验。RenderDoc 核验作为后续 GPU 证据章节单独完成。

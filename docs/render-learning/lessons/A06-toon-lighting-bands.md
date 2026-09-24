# A06 · 从 GBuffer 开始做 Toon 明暗分阶

上一章我们用 RenderDoc 确认了“描边壳”确实走过了指定的 Vulkan Pipeline。这一章回到渲染算法本身：让 Player Robot 的表面不再使用连续变化的 PBR 漫反射，而是按光照强度切成几块清晰的色阶。

完成后应看到：Toon 角色的受光面出现明暗分界，投影阴影仍然可见；未标记为 Toon 的网格保持原样；A03 的黑色描边仍在。实现不新增 Vulkan Render Pass，而是在 GBuffer 中写入一个 Shading Model ID，让后续光照阶段选择不同的着色公式。

## 本章目标与范围

| 内容 | A06 的选择 |
|---|---|
| Toon 分类粒度 | 每个 Mesh 实例，不按材质分组 |
| 光照 | 一盏方向光的 Lambert 漫反射 |
| 明暗阶 | `N·L < 0.30` 为暗部，`0.30–0.70` 为中间调，`≥ 0.70` 为亮部 |
| 阴影 | 沿用 Piccolo 现有方向光硬阴影采样 |
| 环境 | 保留 `ambient_light` 作为底色 |
| 暂不处理 | 点光源、IBL 高光、Toon 高光、边缘光、可调材质阈值、柔和阴影 |

这些限制是有意的：先让一条简单、可测量的光照公式走完整个 GBuffer 链路；下一步再把阴影和参数拆开调。它不是“完整复刻 Unity Toon Shader”的终点。

## 1. 先看输入：连续光照为什么会变成色带

Piccolo 原来的 PBR 光照在 `engine/shader/include/mesh_lighting.inl` 中计算方向光漫反射。核心量是表面法线和光线方向的点积：

```glsl
float NoL = max(dot(N, L), 0.0);
```

`N`、`L` 都是单位向量，所以 `N·L` 大致从 0 到 1：法线背向光时接近 0，正对光时接近 1。PBR 把这个连续数值乘进 BRDF，于是光照沿着曲面平滑变化。

Toon 分阶保留这个测量，但不直接把它当连续亮度，而是先量化：

```glsl
float band = 0.0;
if (NoL >= 0.70)
    band = 1.0;
else if (NoL >= 0.30)
    band = 0.55;
```

```text
N·L:  0.0 -------- 0.30 -------- 0.70 -------- 1.0
band:      0.0          0.55          1.0
```

最终只把这一个方向光的漫反射替换成色带：

```text
颜色 = Albedo × (环境底色 + 方向光颜色 × 色带 × 阴影)
```

这里的 `0.30`、`0.70` 和 `0.55` 暂时是教学常量，不是材质参数。它们让同一个网格上的像素跳到有限的几个亮度，不再逐渐变亮。暗向区域的方向光项是 0，但环境光仍在，所以模型不会因为法线背光而必然变成纯黑。

## 2. A06 的数据与 Pass 链路

```text
player.object.json 的 Toon 标记
  → RenderSystem / RenderEntity
  → RenderScene 中的 RenderMeshNode.is_toon_character
  → drawMeshGbuffer 按材质、Mesh 批处理，但逐实例保留 Toon 标记
  → VulkanMeshInstance.toon_shading_enabled
  → mesh.vert 输出 flat Shading Model ID
  → mesh_gbuffer.frag 把 ID 写入 GBuffer B.a
  → deferred_lighting.frag 解码 ID 并选择 Toon 分支
  → toon_lighting.inl 计算分阶方向光
  → 后续 Tone Mapping / Color Grading / FXAA
```

这条链把两个概念分开了：**GBuffer Pass 记录表面属性和材质模型类型；Deferred Lighting Pass 根据这些数据算最终光照。**因此 A06 没有新增 Draw Pass，也没有要求 Toon 角色换一套材质。

还有一个关键点：Piccolo 按 `材质 → Mesh` 批处理。同一个 Mesh 的一个批次理论上可以同时包含普通实例和 Toon 实例。不能把“这一批是否 Toon”存在批次外层；标记必须跟每个 `gl_InstanceIndex` 对齐。本章用 flat varying 传递每实例的 Shading Model ID，避免插值。

## 3. 保持 CPU 与 GLSL 的实例布局一致

CPU 端 `VulkanMeshInstance` 与 Shader 端 `VulkanMeshInstance` 是同一段 Storage Buffer 的两种解释，字段顺序和大小必须一致。

A05 中这段 16 字节头部原本是：

```text
float enable_vertex_blending
float toon_outline_width
float padding
float padding
```

A06 把其中一个 padding 槽复用为 `toon_shading_enabled`，没有在中间插字段，所以后面的 `model_matrix` 和描边颜色偏移保持不变：

```cpp
// engine/source/runtime/function/render/render_common.h
struct VulkanMeshInstance
{
    float     enable_vertex_blending;
    float     toon_outline_width;
    float     toon_shading_enabled; // 原 padding 槽，保持 ABI 尺寸不变
    float     _padding_enable_vertex_blending_3;
    Matrix4x4 model_matrix;
    Vector4   toon_outline_color;
};
```

`engine/shader/include/structures.h` 中的 GLSL struct 必须同步改成相同顺序。这里用 32 位 float 存 0/1，和原 padding 一样占 4 字节；这不是为了数学精度，而是为了不改变既有 Buffer 布局。

**练习检查：**打开两份结构定义，逐字段确认前 16 字节、矩阵位置和最后的颜色位置相同。不要只看字段名称，更不要只改 GLSL 或只改 C++。

## 4. 在实例批处理中保留 Toon 身份

`RenderScene::updateVisibleObjects()` 已经把对象上的 `is_toon_character` 复制到 `RenderMeshNode`，A03/A04 也用它建立 Toon 描边列表。现在同一个标记还必须进入正常表面着色。

在 `MainCameraPass::drawMeshGbuffer()` 的本地 `MeshNode` 批处理结构中增加 `is_toon_character`，从可见节点复制它；填写每个 `VulkanMeshInstance` 时写入：

```cpp
perdrawcall_storage_buffer_object.mesh_instances[i].toon_shading_enabled =
    mesh_nodes[drawcall_max_instance_count * drawcall_index + i].is_toon_character ? 1.0f : 0.0f;
```

同样的字段也在 `drawMeshLighting()` 中写入。这个函数服务于 forward 路径，使用相同的 `mesh.vert` / `mesh.frag`；如果只改 Deferred 路径，Forward 配置就会出现 Toon 标记丢失、画面退回 PBR 的不一致。

这一层没有改材质批次键。材质仍负责 Albedo、金属度、粗糙度和贴图；Toon 是每个渲染实例选择的 Shading Model。

## 5. 从顶点阶段把模型类型送到 GBuffer Fragment

`mesh.vert` 已经按 `gl_InstanceIndex` 读取模型矩阵。A06 再从同一实例结构读取 Toon 标记，并输出一个 flat `uint`：

```glsl
layout(location = 4) flat out highp uint out_shading_model_id;

out_shading_model_id = toon_shading_enabled > 0.0
    ? SHADINGMODELID_TOON_LIT
    : SHADINGMODELID_DEFAULT_LIT;
```

`flat` 表示该 primitive 不对模型 ID 做顶点插值。一个三角形的三个顶点属于同一个实例，三个顶点输出值相同；即使以后不同实例被放进同一 indexed draw，也不会把 1 和 2 插值成一个无效的 1.4。

`mesh_gbuffer.frag` 在 location 4 接收这个 ID：

```glsl
layout(location = 4) flat in highp uint in_shading_model_id;
...
gbuffer.shadingModelID = in_shading_model_id;
```

`SHADINGMODELID_DEFAULT_LIT` 是原有 PBR 类型 1；A06 新增 `SHADINGMODELID_TOON_LIT`，值为 2。`gbuffer.h` 会把 Shading Model ID 编码进 GBuffer B 的 Alpha 通道：

```glsl
OutGBufferB.a = float(ShadingModelId) / 255.0;
```

因此 Toon 像素在归一化 8 位附件中的 Alpha 预期为 `2 / 255`，普通 Lit 像素为 `1 / 255`。GBuffer B 的四个通道依次是 Metallic、Specular、Roughness、Shading Model ID；它的格式是 `R8G8B8A8_UNORM`。

## 6. 在 Deferred Lighting 选择分阶公式

`deferred_lighting.frag` 读取三个 GBuffer input attachment，`DecodeGBufferData()` 解码 Shading Model ID。原分支继续调用 `mesh_lighting.inl`；新分支才调用 `toon_lighting.inl`：

```glsl
else if (SHADINGMODELID_DEFAULT_LIT == gbuffer.shadingModelID)
{
#include "mesh_lighting.inl"
}
else if (SHADINGMODELID_TOON_LIT == gbuffer.shadingModelID)
{
#include "toon_lighting.inl"
}
```

`toon_lighting.inl` 在 Deferred 与 Forward Shader 之间复用。它先算 `NoL`，再套三阶阈值；如果方向光项非零，就沿用当前方向光阴影贴图做硬阴影比较；最后把方向光色带和环境底色乘回 Albedo：

```glsl
highp float toon_NoL          = max(dot(N, toon_L), 0.0);
highp float toon_diffuse_band = 0.0;

if (toon_NoL >= 0.70)
    toon_diffuse_band = 1.0;
else if (toon_NoL >= 0.30)
    toon_diffuse_band = 0.55;

result_color = basecolor *
    (ambient_light + scene_directional_light.color * toon_diffuse_band * toon_shadow);
```

注意 `toon_shadow` 只压低方向光，不压低 `ambient_light`。因此投影阴影区会留下环境底色。方向光阴影采样的投影矩阵、UV 转换与比较偏置沿用现有 PBR 代码约定；如果 A06 同时重写阴影系统，就很难分清色阶错误和阴影坐标错误。

Forward `mesh.frag` 用同一个 flat Shading Model ID 选择这份 include；普通实例继续走 `mesh_lighting.inl`。这让开关只影响被标记的实例，而不改其它物体。

## 7. 本章实际改动的文件

| 文件 | 作用 |
|---|---|
| `engine/source/runtime/function/render/render_common.h` | 复用实例结构 padding 槽，CPU / GPU 共享数据加 Toon 标记 |
| `engine/source/runtime/function/render/passes/main_camera_pass.cpp` | GBuffer 与 Forward 批次保留逐实例 Toon 标记 |
| `engine/shader/include/structures.h` | GLSL 侧同步 `VulkanMeshInstance` 布局 |
| `engine/shader/include/gbuffer.h` | 新增 Toon Lit Shading Model ID 2 |
| `engine/shader/glsl/mesh.vert` | 按实例标记输出 flat Shading Model ID |
| `engine/shader/glsl/mesh_gbuffer.frag` | 把 Shading Model ID 写进 GBuffer |
| `engine/shader/glsl/deferred_lighting.frag` | Deferred Lighting 新增 Toon 分支 |
| `engine/shader/glsl/mesh.frag` | Forward 渲染保留相同 Toon 选择行为 |
| `engine/shader/include/toon_lighting.inl` | 可复用的三阶方向光漫反射与现有硬阴影公式 |

Shader 的 SPIR-V 和嵌入头文件由构建过程生成，不需要手工编辑 `engine/shader/generated`。

## 8. 按步骤复现

从已完成 A05 的干净仓库开始。先确认当前工作区状态；如果里面有自己的修改，先妥善保存，不要为了跟教程而丢掉它们：

```powershell
cd D:\Code\AI_Only\Piccolo_Plus
git status --short
```

确认处于 A05 基线且工作区干净后，再新建实验分支：

```powershell
git switch -c lesson/a06-toon-lighting-bands
```

如果你要从干净的 A05 基线重做，应使用 A05 完成时的提交作为基线。接下来按顺序做，每步检查一次。先改 CPU 结构 `engine/source/runtime/function/render/render_common.h` 和 GLSL 结构 `engine/shader/include/structures.h`，两边都将第二个 padding（第三个字段）改名为 `toon_shading_enabled`，不要插入新字段；然后在 `main_camera_pass.cpp` 的 GBuffer 与 Forward 两个 `MeshNode` 批次结构各加一个 `bool is_toon_character`，从 `RenderMeshNode` 复制，并在各自写 `VulkanMeshInstance` 的循环里逐项填入 0/1。先搜准函数与写入位置：

```powershell
rg -n "drawMeshGbuffer|drawMeshLighting|mesh_instances\[i\]\.enable_vertex_blending" engine/source/runtime/function/render/passes/main_camera_pass.cpp
```

逐实例写入的形态如下（GBuffer 和 Forward 各有一处）：

```cpp
temp.is_toon_character = node.is_toon_character;
// ...
perdrawcall_storage_buffer_object.mesh_instances[i].toon_shading_enabled =
    mesh_nodes[drawcall_max_instance_count * drawcall_index + i].is_toon_character ? 1.0f : 0.0f;
```

**检查点：**同一材质/Mesh 批次内，每个实例都有独立 0/1；不能依据批次中第一个对象来设置整批的值。之后再接 Shader：

1. 在 `engine/shader/include/gbuffer.h` 定义 Toon ID 2。
2. 在 `mesh.vert` 输出 flat Shading Model ID；在 `mesh_gbuffer.frag` 接收并写入 GBuffer。
3. 在 `deferred_lighting.frag` 根据 ID 分支到 `toon_lighting.inl`。
4. 将同一光照 include 接到 `mesh.frag`，让 Forward 路径也能运行。
5. 保存 `engine/shader/include/toon_lighting.inl`，然后构建：

```powershell
cmake --build build --config Release --parallel 8
```

正常结果包含 `PiccoloRuntime.lib` 和 `PiccoloEditor.exe`；构建日志应显示 `mesh.vert`、`mesh_gbuffer.frag`、`mesh.frag`、`deferred_lighting.frag` 的 SPIR-V 与嵌入头文件被生成。不要把 MSVC 对已有中文注释的 C4819 编码警告误判成 Shader 编译失败；真正的失败会以 `error` / 非零退出码结束。

运行：

```powershell
cd D:\Code\AI_Only\Piccolo_Plus\build\engine\source\editor\Release
.\PiccoloEditor.exe
```

观察默认关卡的 Player Robot：身体受光区应该呈现清楚的暗部、中间调、亮部边界；原黑色描边还在；场景里未标记 Toon 的网格继续使用渐变 PBR。把角色转向光线时，色带边界应随法线与方向光的夹角变化，而不是固定贴在模型表面。

## 9. 用 RenderDoc 验证 Shading Model ID

A05 的 capture 脚本输出固定写到 A05 证据目录，不要拿它抓 A06（可能覆盖 A05 捕获）。本章提供单独脚本，默认把捕获保存到 `docs/render-learning/evidence/A06/piccolo-a06_capture.rdc`，并在目标文件已存在时拒绝覆盖：

```powershell
cd D:\Code\AI_Only\Piccolo_Plus
.\docs\render-learning\tools\capture_piccolo_a06.ps1 -Frame 120
```

脚本启动编辑器后会自动请求第 120 帧捕获。确认 `.rdc` 已生成，再关闭编辑器返回 PowerShell。要保留多帧/多次试验，用独立输出目录：

```powershell
.\docs\render-learning\tools\capture_piccolo_a06.ps1 -Frame 240 -OutputDirectory "$env:TEMP\piccolo-a06-frame240"
```

用 RenderDoc 打开新 `.rdc`，然后：

1. 在 Event Browser 展开 `MainCameraPass → BasePass → Mesh GBuffer`，选择 Player Robot 对应的 indexed draw。
2. 查看该 Draw 的 Color Outputs，定位附件 GBuffer B（源码格式 `R8G8B8A8_UNORM`）。
3. 在 Texture Viewer 选择角色表面像素并查看 Alpha：Toon 像素应解码为约 `0.007843`（8 位归一化值 2）；普通 Lit 像素应为约 `0.003922`（值 1）。避免点到描边、背景或清屏像素。
4. 再到 `Deferred Lighting` 的全屏 Draw，确认 GBuffer B 是输入附件，并查看输出颜色；同一角色上应有台阶状亮度，而非原来的连续 PBR 高光/漫反射。
5. 将鼠标移到亮部和中间调各选一个像素，核对 GBuffer ID 相同（两者都是 Toon，ID=2），但法线与 `N·L` 不同。这样能区分“选了 Toon 模型”和“分阶阈值实际生效”这两个判断。

如果 RenderDoc Texture Viewer 只显示 RGBA 颜色，不显示十进制数值，可以用颜色通道数值/像素调试器查看原始 UNORM 值；不要从最终 tone-mapped 屏幕颜色反推 ID。

### 当前实测记录（2026-09-25）

本次 Release 运行生成了一帧新的 Vulkan RenderDoc capture。Event Browser 中实际出现 `BasePass → Mesh GBuffer`，随后是 `Deferred Lighting`，再后面是 `Toon Outline`；描边 draw 为一个 indexed draw，`instances=1`，轮廓管线仍为正面剔除、深度测试开启、深度写入关闭。这确认 A06 没有破坏原来的 GBuffer/Deferred/Outline pass 顺序。此记录不代替上面的 GBuffer B.a 像素检查：本次还没有把 Player Robot 的 ID=2 和亮度台阶逐像素核实。

本机启动日志还出现材质加载器尝试读取 `bin/` 空路径的纹理错误；引擎仍完成关卡加载并持续绘制。它可能影响默认角色的材质观感，先作为运行环境问题记录，不能用当前小视口截图判断最终明暗带质量。

## 10. 常见错误

| 现象 | 优先检查 |
|---|---|
| Toon 角色仍是平滑 PBR | `RenderMeshNode.is_toon_character` 是否为 true；`drawMeshGbuffer()` 是否写入 `toon_shading_enabled`；flat varying location 4 两端是否一致；GBuffer ID 是否为 2 |
| 所有物体都变成 Toon | 是否把标记存在材质批次级，而不是 `gl_InstanceIndex` 对应的实例；每个非 Toon 实例是否明确写 0 |
| 描边消失或颜色乱码 | 有没有挪动结构字段而非复用 padding；CPU 与 GLSL 字段顺序是否完全一致 |
| 编译报 `no matching input` 或 location interface mismatch | `mesh.vert` location 4 flat 输出与两个 Fragment Shader 输入的 location、类型、flat qualifier 是否一致 |
| GBuffer B Alpha 显示 0 | 当前选中的 draw/pixel 可能是 clear 或普通默认值，也可能 Forward 路径不写 GBuffer；确认选中 BasePass 中 Player 的 draw |
| 色带正确但投影阴影没有 | 检查 Toon 分支是否采样 `directional_light_shadow`，以及默认关卡方向光阴影是否开启；本章不包括点光源阴影 |
| 暗面仍然可见 | 这是 `ambient_light` 的预期贡献；A06 暂不做阴影颜色和环境光的独立 Toon 控制 |

## 11. 验收与下一步

| 等级 | A06 验收方式 |
|---|---|
| L1 静态链路 | 能从 `is_toon_character` 指到 per-instance Buffer、flat ID、GBuffer B.a 和 Deferred Lighting 分支 |
| L2 构建 | Release 中四个相关 Shader 编译，Runtime 与 Editor 编译链接通过 |
| L3 运行 | 已实测：默认关卡启动；新 capture 中存在 Mesh GBuffer、Deferred Lighting、Toon Outline 顺序；启动日志另有空纹理路径错误 |
| L4 画面 | 待手动确认：Toon 角色出现 3 阶方向光明暗、保留原描边，非 Toon 角色不变 |
| L5 GPU 证据 | 新捕获里 GBuffer B.a 为 ID 2，Deferred Draw 使用它并输出台阶式光照 |

下一章 A07 会把“有硬阴影”进一步拆成 Toon 阴影的控制问题：阴影阈值/颜色如何作用于色带，如何保留普通物体 PBR 的现状，以及哪些参数应该属于对象、材质或全局光照。A06 的四级阈值故意先留在 Shader 常量里，等我们确认参数归属后再暴露给资产。

## 本章源码索引

- `render_scene.cpp::updateVisibleObjects()`：从 RenderEntity 填充可见 Mesh Node。
- `main_camera_pass.cpp::drawMeshGbuffer()`：默认 Deferred 路径的实例标记写入点。
- `main_camera_pass.cpp::drawMeshLighting()`：Forward 路径的实例标记写入点。
- `mesh.vert` → `mesh_gbuffer.frag`：顶点阶段的逐实例 ID 到 GBuffer 分类。
- `deferred_lighting.frag` → `toon_lighting.inl`：读取分类并执行分阶公式。

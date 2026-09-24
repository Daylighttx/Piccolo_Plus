# A04：把描边宽度和颜色从资产送进 GPU

## 本章产物

完成后，不再修改 Shader 中的常量，而是在 `player.object.json` 中配置：

```json
"is_toon_character": true,
"toon_outline_width": 0.025,
"toon_outline_color": {
    "x": 0.015,
    "y": 0.02,
    "z": 0.03,
    "w": 1.0
}
```

这两个参数经过 CPU 的五层数据表示，最终写入每实例 GPU Storage Buffer。一个 Draw Call 中即使存在多个实例，每个实例也可以拥有不同的宽度与颜色。

## 1. 修改前为什么不够

A03 在 Shader 中写死：

```glsl
const float outline_width = 0.025;
out_scene_color = vec4(0.015, 0.02, 0.03, 1.0);
```

这种写法能证明 Pipeline 通了，却不是引擎功能：美术人员不能调参，不同角色不能使用不同轮廓，修改任何数值都要重新编译 SPIR-V。A04 的目标是把“程序常量”变成“资产数据”。

## 2. 完整数据路线

```text
player.object.json
  → SubMeshRes
  → MeshComponent::postLoadResource()
  → GameObjectPartDesc
  → RenderSwapData 双缓冲
  → RenderSystem::processSwapData()
  → RenderEntity
  → RenderMeshNode
  → OutlineMeshNode
  → VulkanMeshInstance
  → GLSL Storage Buffer
  → Vertex Shader / Fragment Shader
```

每一次复制都跨越了一个职责或生命周期边界。不要省略中间层让渲染线程回头读取 JSON；那会破坏逻辑/渲染线程分离。

## 3. 第一步：扩展可序列化资源类型

文件：`engine/source/runtime/resource/res_type/components/mesh.h`

在 `SubMeshRes` 中加入：

```cpp
bool    m_is_toon_character {false};
float   m_toon_outline_width {0.025f};
Vector4 m_toon_outline_color {0.015f, 0.02f, 0.03f, 1.0f};
```

默认值有两个作用：旧资产没有新字段时仍能加载；新角色只开启 Toon 标记而未配置参数时也能得到可见结果。

构建时 PiccoloParser 会重新扫描反射类型。成功后在生成文件中应能找到：

```powershell
rg -n "toon_outline_(width|color)" engine/source/_generated
```

预期同时出现 Reflection 和 Serializer 的 `read/write` 代码。如果只改 C++ 头文件却没有生成序列化代码，JSON 数值不会进入运行时。

## 4. 第二步：进入逻辑到渲染的交换描述

文件：`engine/source/runtime/function/render/render_object.h`

在 `GameObjectPartDesc` 加入相同字段：

```cpp
float   m_toon_outline_width {0.025f};
Vector4 m_toon_outline_color {0.015f, 0.02f, 0.03f, 1.0f};
```

文件：`engine/source/runtime/function/framework/component/mesh/mesh_component.cpp`

在 `postLoadResource()` 中复制：

```cpp
meshComponent.m_toon_outline_width = sub_mesh.m_toon_outline_width;
meshComponent.m_toon_outline_color = sub_mesh.m_toon_outline_color;
```

为什么必须进入 `GameObjectPartDesc`：`RenderSwapContext` 搬运的是 `GameObjectDesc/GameObjectPartDesc`，不是 `SubMeshRes`。字段如果停在资源层，就会在逻辑线程与渲染线程交界处丢失。

## 5. 第三步：成为 RenderScene 的长期状态

文件：`engine/source/runtime/function/render/render_entity.h`

在 `RenderEntity` 中加入参数。随后在 `RenderSystem::processSwapData()` 创建或更新实体时复制：

```cpp
render_entity.m_toon_outline_width = game_object_part.m_toon_outline_width;
render_entity.m_toon_outline_color = game_object_part.m_toon_outline_color;
```

`RenderEntity` 是参数的长期所有者。交换数据消费完会被重置，因此 `RenderMeshNode` 不能指向交换区中的临时对象。

## 6. 第四步：进入本帧可见节点

文件：`engine/source/runtime/function/render/render_common.h`

在 `RenderMeshNode` 加入值类型字段：

```cpp
float   toon_outline_width {0.025f};
Vector4 toon_outline_color {0.015f, 0.02f, 0.03f, 1.0f};
```

文件：`engine/source/runtime/function/render/render_scene.cpp`

主相机视锥裁剪通过后复制：

```cpp
temp_node.toon_outline_width = entity.m_toon_outline_width;
temp_node.toon_outline_color = entity.m_toon_outline_color;
```

只有可见 Toon 节点进入 Outline 列表，所以屏幕外角色不会申请本帧 Upload Ring Buffer 空间。

## 7. 第五步：扩展 CPU/GPU 共享结构

CPU 文件：`engine/source/runtime/function/render/render_common.h`

GLSL 文件：`engine/shader/include/structures.h`

两端的 `VulkanMeshInstance` 必须保持同样的字段顺序：

```text
float enable_vertex_blending
float toon_outline_width
float padding 2
float padding 3
mat4  model_matrix
vec4  toon_outline_color
```

C++ 版本：

```cpp
struct VulkanMeshInstance
{
    float     enable_vertex_blending;
    float     toon_outline_width;
    float     _padding_enable_vertex_blending_2;
    float     _padding_enable_vertex_blending_3;
    Matrix4x4 model_matrix;
    Vector4   toon_outline_color;
};
```

这里保留前四个 float 组成 16 字节块，随后是 64 字节矩阵和 16 字节颜色，总计预期 96 字节。CPU 和 SPIR-V 的数组 stride 一旦不一致，第二个实例开始就会读取错位数据。

本次没有新增 Descriptor Binding，而是扩展已有 per-drawcall Storage Buffer。优点是继续复用 `_mesh_global` Descriptor Set；代价是所有 Mesh Instance 的 stride 都增加了 16 字节。学习引擎时先选择最清晰的路径，后续再测量是否值得拆分专用 Buffer。

## 8. 第六步：Draw 阶段填写每实例参数

在 `drawToonOutline()` 的轻量 `OutlineMeshNode` 中保存宽度和颜色。构造批次时从 `RenderMeshNode` 复制，填写 Upload Ring Buffer 时写入：

```cpp
perdrawcall_data.mesh_instances[instance_index].toon_outline_width = node.outline_width;
perdrawcall_data.mesh_instances[instance_index].toon_outline_color = node.outline_color;
```

为什么放在每实例数据而不是 Per-frame：Per-frame 会让整帧所有角色只能共用一个值；放在 Material Descriptor 又会迫使当前不需要材质的 Outline Pipeline 绑定整套 PBR 材质。每实例数据最符合当前需求。

## 9. 第七步：Shader 消费参数

Vertex Shader 读取：

```glsl
float outline_width = mesh_instances[gl_InstanceIndex].toon_outline_width;
out_outline_color = mesh_instances[gl_InstanceIndex].toon_outline_color;
world_position += world_normal * outline_width;
```

颜色用 `flat` varying 传给 Fragment Shader：

```glsl
layout(location = 0) out flat vec4 out_outline_color;
layout(location = 0) in  flat vec4 in_outline_color;
```

`flat` 表示三角形内部不插值。一个实例的轮廓颜色本来就是常量，不需要 GPU 对三个顶点的相同颜色做插值。

Fragment Shader 最终只做：

```glsl
out_scene_color = in_outline_color;
```

## 10. 构建与运行

```powershell
cd D:\Code\AI_Only\Piccolo_Plus
cmake --build build --config Release
cd bin
.\PiccoloEditor.exe
```

构建日志应该依次出现：

```text
PiccoloParser / Precompile finished
toon_outline.vert.spv
toon_outline.frag.spv
PiccoloRuntime.lib
PiccoloEditor.exe
```

本次实际验证：Release 构建成功；默认关卡持续运行，窗口约 `382 FPS`；最新日志没有 `error/fatal/assert/failed`。

## 11. 手工可见性实验

为了证明参数确实来自 JSON，而不是 Shader 残留常量，建议做两次实验，每次修改 JSON 后重启关卡：

实验一：宽轮廓。

```json
"toon_outline_width": 0.08
```

预期：Robot 外轮廓明显变粗。若完全没变化，优先检查 JSON 字段拼写和生成 Serializer。

实验二：红色轮廓。

```json
"toon_outline_color": { "x": 1.0, "y": 0.0, "z": 0.0, "w": 1.0 }
```

预期：轮廓变红，角色主体 PBR 材质颜色不变。若主体也变红，说明参数错误地进入了普通材质路径。

实验结束后恢复本章默认值。

## 12. 常见失败与定位顺序

### JSON 修改无效

先在 `_generated/serializer/all_serializer.ipp` 搜索字段，再检查 `MeshComponent::postLoadResource()` 是否复制，最后检查是否重启了已加载的关卡。

### 所有模型爆炸或闪烁

优先怀疑 CPU/GLSL `VulkanMeshInstance` 布局不一致。逐字段核对顺序、类型和 padding，并确认所有 Shader 都重新生成 SPIR-V。

### 只有第一实例正确

通常是数组 stride 不一致。检查 C++ `sizeof(VulkanMeshInstance)` 与 SPIR-V 反射结果，而不是只检查第一个元素。

### 动画主体正常但描边错位

检查 Outline 批次是否上传骨骼矩阵，以及 `enable_vertex_blending` 是否为正值。

### 程序运行但没有轮廓

依次检查资产标记、Toon Visible List 是否非空、`drawToonOutline()` 是否被调用、Pipeline 是否使用 `FRONT` culling、深度比较是否与主相机一致。

## 13. 验收清单

- [ ] 能从 JSON 指出宽度和颜色经过的每一个 C++ 类型。
- [ ] 生成 Serializer 包含两个新字段。
- [ ] CPU 与 GLSL `VulkanMeshInstance` 字段顺序一致。
- [ ] Shader 编译为 SPIR-V，C++ Release 构建成功。
- [ ] 默认关卡持续运行且日志无错误。
- [ ] 将宽度改为 `0.08` 后轮廓明显变粗。
- [ ] 将颜色改为红色后只有轮廓改变。
- [ ] 恢复默认 JSON 参数。

完成前五项代表数据链与运行链通过；完成全部项目才算本章可以由读者独立复现。

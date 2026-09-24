# A02：跟踪 Player Robot Mesh 到 Draw Call

> 状态：已选定实验资产，并完成 Toon 分类数据从资产到主相机可见节点的贯通。

## 选定资产

本节使用 Piccolo 默认关卡中的 `Player` 机器人，而不是临时导入新的 FBX/VRM。

```text
asset/objects/character/player/player.object.json
  → MeshComponent
  → asset/objects/character/player/components/animation/data/robot.mesh_bind.json
  → asset/objects/character/player/components/mesh/robot.material.json
```

选择它的原因：

- 默认 `1-1.level.json` 明确实例化了这个 Player，运行时一定进入场景。
- 它是真实 Skinned Mesh，可以同时跟踪几何、材质、骨骼绑定和动态蒙皮。
- 它有完整 BaseColor、Normal、ORM 与 Emissive 输入。
- 后续可以直接作为第一个 Toon Character Pass 的目标角色。

## 资产数据

`robot.mesh_bind.json` 的实测数据：

| 项目 | 数值 |
|---|---:|
| 文件大小 | 21,624,026 bytes |
| 顶点数 | 55,500 |
| 索引数 | 55,500 |
| 索引范围 | 0–55,499 |
| 骨骼绑定记录 | 55,500 |
| 每顶点最大影响骨骼数 | 4 |

每个顶点包含：

```text
Position: px, py, pz
Normal:   nx, ny, nz
Tangent:  tx, ty, tz
UV:       u, v
Skin:     最多四组 bone index + weight
```

材质 `robot.material.json` 绑定：

```text
xiaobairen1k_BaseColor.tga
xiaobairen1k_Normal.tga
xiaobairen1k_OcclusionRoughnessMetallic.tga
xiaobairen1k_Emissive.tga
```

## 即将跟踪的真实路径

```text
1-1.level.json
  → player.object.json
  → MeshComponent::postLoadResource()
  → RenderSwapData中的 GameObjectResourceDesc
  → RenderSystem::processSwapData()
  → RenderResourceBase::loadMeshData()
  → AssetManager::loadAsset<MeshData>()
  → RenderResource::uploadGameObjectRenderResource()
  → RenderScene::updateVisibleObjectsMainCamera()
  → MainCameraPass::drawMeshGbuffer()
  → RHI::cmdDrawIndexedPFN()
  → VulkanRHI::cmdDrawIndexedPFN()
  → vkCmdDrawIndexed()
```

下一步要把这条路径中的对象类型、资源句柄、GPU Buffer 和 Descriptor逐项对上，并确认 Draw Call 的 `indexCount` 是否为 55,500。

## 第一个源码改点：渲染分类数据

在增加 Render Pass 以前，引擎必须可靠地知道哪些几何体属于 Toon 角色。此次增加的字段沿以下路径传播：

```text
SubMeshRes::m_is_toon_character
  → GameObjectPartDesc::m_is_toon_character
  → RenderEntity::m_is_toon_character
  → RenderMeshNode::is_toon_character
```

Player 资产通过下面的序列化字段启用：

```json
"is_toon_character": true
```

其他旧资产没有这个字段时使用 `false` 默认值，因此保持向后兼容。该标记现在只传递数据，不改变任何绘制结果。下一步将由它生成独立的 Toon Visible List；不在 Vulkan Draw函数中通过文件名或 Asset ID硬编码角色。

## 本步验证

- Release 配置重新编译成功，新的 `PiccoloEditor.exe` 已生成。
- 代码生成器已把 `is_toon_character` 写入 `SubMeshRes` 的反射信息以及 JSON `read/write` 函数。
- 用新程序加载默认关卡后，编辑器持续运行并正常响应；实测窗口标题为 `Piccolo - 443 FPS`。
- 本次启动后的日志未出现 `error`、`fatal`、`assert` 或 `failed`。
- 复核可见性阶段时发现，最初只在方向光阴影列表复制了该标记；现已补齐主相机可见列表。未来的 Toon Pass 应消费主相机列表，阴影列表是否使用该标记则由阴影设计决定。

所以，本节建立的是一条已经过真实资源加载验证的“资产标签通道”。它还不是 Toon Pass，也不会改变画面；下一节才开始让该标签影响 Render Scene 的绘制列表。

# A02 补充：建立 Toon Visible List

> 状态：已实现，现已由 A03 的 Toon Outline 绘制阶段消费。

## 本节目标

上一节把 `is_toon_character` 从资产传到了 `RenderEntity`。本节让这个标记第一次改变渲染数据的组织方式：主相机完成视锥裁剪后，额外生成一份仅包含 Toon 角色的可见节点列表。

```text
RenderEntity
  → 主相机视锥裁剪
  → m_main_camera_visible_mesh_nodes       （所有可见网格）
  → m_main_camera_visible_toon_mesh_nodes  （其中的 Toon 子集）
```

## 为什么保留两份列表

当前阶段计划先做附加式倒壳描边。角色主体仍由已有 GBuffer Pass 绘制，描边则由新的 Outline Pass 再绘制一次：

```text
所有可见网格 → 原 GBuffer Pass → 正常主体
Toon 可见子集 → Outline Pass   → 外扩轮廓
```

因此不能把 Toon 角色从原列表移走，否则在 Outline Pass 接入之后可能只看到轮廓、看不到角色主体。

`RenderMeshNode` 中保存的是指向长期渲染资源和实体数据的非拥有指针。Toon 列表复制的是轻量节点，不复制 Mesh Buffer、材质贴图或骨骼数组，也不取得它们的所有权。

## 数据如何进入所有 Render Pass

`RenderScene::setVisibleNodesReference()` 把两个列表的地址写入 `RenderPass::m_visiable_nodes`：

```text
RenderScene 拥有 vector
  → VisiableNodes 保存 vector 指针
  → MainCameraPass / 后续 ToonOutlinePass 读取
```

这里沿用 Piccolo 现有的共享可见节点接口。下一节新增 Pass 时，不需要重新遍历整个 RenderScene，只需要读取 `p_main_camera_visible_toon_mesh_nodes`。

## 每帧生命周期

每帧开始主相机可见性更新时，两份列表都会先 `clear()`。随后只对通过主相机视锥测试的实体创建节点；若节点的 `is_toon_character` 为 `true`，再将该轻量节点复制进 Toon 子集。

这意味着屏幕外的 Toon 角色不会进入 Outline Pass，基础的 CPU 视锥裁剪结果得到了复用。

## 下一步

创建第一个真正改变画面的 `ToonOutlinePass`：独立 Graphics Pipeline、顶点沿法线外扩、正面剔除、纯色输出，并绘制 `p_main_camera_visible_toon_mesh_nodes`。

## 本步验证

- Release 配置编译、链接成功。
- 新版编辑器加载默认关卡后持续运行并正常响应，验证时窗口约为 `294 FPS`。
- 最新 250 行运行日志未出现 `error`、`fatal`、`assert` 或 `failed`。
- 本节没有 Pass 消费 Toon 列表，因此画面保持不变是预期结果，不代表列表无效。

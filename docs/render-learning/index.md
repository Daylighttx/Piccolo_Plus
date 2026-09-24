---
layout: home

hero:
  name: "从源码学实时渲染"
  text: "Piccolo 渲染管线实验手册"
  tagline: 从一帧的生命周期开始，沿着真实角色资产一路走到 Vulkan Draw Call，再亲手添加 Toon 渲染能力。
  actions:
    - theme: brand
      text: 从 A01 开始
      link: /lessons/A01-frame-lifecycle
    - theme: alt
      text: 查看学习路线
      link: /guide/roadmap

features:
  - title: 源码驱动
    details: 每个结论都落到 Piccolo 的真实文件、类型、函数和数据流，不用抽象名词代替源码。
  - title: 可以复现
    details: 每章包含修改步骤、关键代码、构建命令、预期现象、失败排查和验收清单。
  - title: 逐级验证
    details: 区分静态链路、构建、运行、画面与 GPU 证据，不把“能编译”误当成“渲染正确”。
---

## 这本教程要解决什么问题

我们不是只为 Piccolo 加一个描边效果。真正的目标，是借一个足够小、又确实使用 Vulkan 的引擎，把现代实时渲染的完整链路拆开：

```text
资产文件 → 资源反序列化 → 游戏对象 → 逻辑/渲染交换数据
        → RenderScene → 可见性分类 → RenderPass
        → Pipeline / Descriptor → Shader → Draw Call → GPU
```

学完一条完整链路以后，再去阅读 Godot 或 UE5，面对的是规模更大的同类问题，而不是一套完全陌生的知识。

## 当前进度

| 阶段 | 结果 | 状态 |
|---|---|---|
| A01 | 找到 Piccolo 一帧的入口、提交与呈现 | 已完成 |
| A02 | 从 Player Robot 资产追踪到 Draw Call | 已完成 |
| A02 补充 | 建立 Toon 专用可见节点列表 | 已完成 |
| A03 | 添加支持蒙皮的倒壳描边 Pipeline | 已完成 |
| A04 | 将每个角色的描边宽度和颜色送入 GPU | 已完成 |
| A05 | 用 RenderDoc 验证 Event、Pipeline、Descriptor 和 Draw | 已完成：捕获报告可复现；逐像素检查留待后续 |
| A06 | 让 Toon 角色的方向光漫反射分成 3 个色阶 | 已实现；Release 构建、启动与 GBuffer→Deferred→Outline 捕获顺序已验证；像素级明暗效果待核 |

::: tip 阅读建议
第一次按顺序阅读并亲手复现；第二次从右侧目录随机定位概念；遇到错误时先查每章末尾的排错表。
:::

## 下一章

[A06：从 GBuffer 开始做 Toon 明暗分阶](/lessons/A06-toon-lighting-bands)

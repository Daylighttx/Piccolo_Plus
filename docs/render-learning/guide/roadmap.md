# 学习路线与章节约定

这套教程采用“小步修改、逐层取证”的方式。每一章既是知识章节，也是一次能够独立验收的源码实验。

## 第一阶段：读懂 Piccolo 的链路

1. A01 建立帧生命周期地图。
2. A02 选择真实角色，跟踪资产到 Draw Call。
3. A02 补充建立 Toon 分类与可见列表。
4. A03 添加第一个新的 Graphics Pipeline。
5. A04 打通资产参数到 Shader 的数据通道。
6. A05 使用 RenderDoc 给前四章补齐 GPU 证据。
   - 交付：可复现的一帧捕获脚本、Event 到 C++ Draw 的映射和 Pipeline/Descriptor 状态报告。

## 第二阶段：补全 Toon 光照

7. A06：基础明暗分阶——从 per-instance Shading Model ID 走入 GBuffer，再让 Deferred / Forward 光照分别选择 Toon 公式。
8. A07：阴影控制——把已有硬阴影与 Toon 暗部、阴影色分开调，并确定参数属于对象、材质还是全局光照。
9. 后续按依赖顺序扩展边缘光、高光、面部阴影、MatCap、描边细节和后处理；每个效果先拆解 Unity 版本的输入、Pass 和数学，再映射到 Piccolo。

## 第三阶段：迁移阅读方法

同一个问题分别在 Piccolo、Godot 和 UE5 中定位：谁创建 Pipeline、谁收集可见物、谁准备 Descriptor、谁决定 Pass 顺序。代码规模会变化，但问题结构基本一致。

## 五级验收

| 等级 | 必须拿到的证据 |
|---|---|
| L1 静态链路 | 能指出字段或调用在每一层的位置 |
| L2 构建 | Shader、反射生成、C++ 编译与链接成功 |
| L3 运行 | 默认关卡持续运行，日志没有新增错误 |
| L4 画面 | 屏幕上出现章节规定的可观察变化 |
| L5 GPU 证据 | RenderDoc 中找到对应 Event、Pipeline、Descriptor 与 Draw Call |

一章没有达到 L5 不一定代表失败，但文档必须明确写出当前证据边界。

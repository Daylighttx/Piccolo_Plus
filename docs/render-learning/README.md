# 从 Piccolo 源码学习实时渲染：可复现实验手册

这不是改动日志，而是一套可以从源码基线重新手工完成的课程。每章都必须包含：本章产物、前置知识、原始调用链、逐文件修改、完整关键代码、构建命令、运行方法、预期现象、失败排查和验收清单。

## 使用方式

建议为练习建立独立 Git 分支。每完成一个小节先编译，再继续下一小节；不要一次复制整章后才排错。

Windows 基础构建命令：

```powershell
cd D:\Code\AI_Only\Piccolo_Plus
cmake -S . -B build
cmake --build build --config Release
cd bin
.\PiccoloEditor.exe
```

看到 `Piccolo - <FPS> FPS` 只证明程序在运行，不等于画面已经正确。图像功能还必须完成章节中的目视验收；涉及 GPU 状态时再使用 RenderDoc 验证。

## 课程顺序

1. [A01：一帧从逻辑线程到 Vulkan](lessons/A01-frame-lifecycle.md)
2. [A02：真实 Player Robot 从资产到 Draw Call](lessons/A02-player-robot-mesh.md)
3. [A02 补充：建立 Toon Visible List](lessons/A02-supplement-toon-visible-list.md)
4. [A03：第一个蒙皮倒壳描边 Pipeline](lessons/A03-toon-outline-pipeline.md)
5. [A04：把描边参数从资产送进 GPU](lessons/A04-toon-outline-parameters.md)
6. [A05：用 RenderDoc 核验 Event、Pipeline、Descriptor 和 Draw](lessons/A05-renderdoc-capture.md)

## 每章统一验收等级

| 等级 | 含义 |
|---|---|
| L1 静态链路 | 能指出字段或调用在每一层的位置 |
| L2 构建 | Shader、反射生成、C++ 编译和链接成功 |
| L3 运行 | 默认关卡持续运行，日志没有错误 |
| L4 画面 | 屏幕上出现章节规定的可观察变化 |
| L5 GPU 证据 | RenderDoc 中找到对应 Event、Pipeline、Descriptor 和 Draw Call |

文档会明确标注当前实际达到哪一级，不把编译成功写成图像正确，也不把运行成功写成 GPU 状态已经核验。

## 在线教材站

本目录同时是 VitePress 教材站的源文件。章节 Markdown 只维护一份，网页导航、全文搜索、上一篇/下一篇和本页目录由站点自动生成。

本地阅读：

```powershell
cd D:\Code\AI_Only\Piccolo_Plus\docs\render-learning
npm ci
npm run docs:dev
```

浏览器打开终端显示的本地地址。检查正式构建：

```powershell
npm run docs:build
npm run docs:preview
```

仓库中的 `.github/workflows/deploy-render-learning.yml` 已配置 GitHub Pages。推送后，在 GitHub 仓库的 **Settings → Pages → Build and deployment → Source** 中选择 **GitHub Actions**；随后每次修改本目录并推送，网页都会自动重新构建与发布。

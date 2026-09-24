import { defineConfig } from 'vitepress'

const isGitHubPages = process.env.GITHUB_ACTIONS === 'true'

export default defineConfig({
  lang: 'zh-CN',
  title: 'Piccolo 渲染源码学习',
  description: '从 Piccolo 源码学习 Vulkan 实时渲染，并逐步实现 Toon Shader。',
  base: isGitHubPages ? '/Piccolo_Plus/' : '/',
  cleanUrls: true,
  lastUpdated: true,
  head: [
    ['link', { rel: 'icon', type: 'image/svg+xml', href: isGitHubPages ? '/Piccolo_Plus/favicon.svg' : '/favicon.svg' }],
    ['meta', { name: 'theme-color', content: '#0f766e' }]
  ],
  themeConfig: {
    logo: '/logo.svg',
    siteTitle: '渲染源码实验室',
    nav: [
      { text: '教程', link: '/lessons/A01-frame-lifecycle' },
      { text: '学习路线', link: '/guide/roadmap' },
      { text: 'Piccolo 源码', link: 'https://github.com/Daylighttx/Piccolo_Plus' }
    ],
    sidebar: [
      {
        text: '开始阅读',
        items: [
          { text: '教程首页', link: '/' },
          { text: '学习路线与验收标准', link: '/guide/roadmap' }
        ]
      },
      {
        text: '第一阶段 · 打通渲染链路',
        collapsed: false,
        items: [
          { text: 'A01 · 一帧的生命周期', link: '/lessons/A01-frame-lifecycle' },
          { text: 'A02 · 角色资产到 Draw Call', link: '/lessons/A02-player-robot-mesh' },
          { text: 'A02 补充 · Toon Visible List', link: '/lessons/A02-supplement-toon-visible-list' },
          { text: 'A03 · Toon Outline Pipeline', link: '/lessons/A03-toon-outline-pipeline' },
          { text: 'A04 · 描边参数进入 GPU', link: '/lessons/A04-toon-outline-parameters' },
          { text: 'A05 · RenderDoc 核验 Draw 与管线', link: '/lessons/A05-renderdoc-capture' }
        ]
      },
      {
        text: '第二阶段 · Toon 光照',
        collapsed: false,
        items: [
          { text: 'A06 · Toon 明暗分阶与 GBuffer', link: '/lessons/A06-toon-lighting-bands' }
        ]
      }
    ],
    outline: { level: [2, 3], label: '本页目录' },
    docFooter: { prev: '上一篇', next: '下一篇' },
    lastUpdated: { text: '最后更新' },
    returnToTopLabel: '返回顶部',
    sidebarMenuLabel: '章节目录',
    darkModeSwitchLabel: '外观',
    search: {
      provider: 'local',
      options: {
        translations: {
          button: { buttonText: '搜索教程', buttonAriaLabel: '搜索教程' },
          modal: {
            noResultsText: '没有找到相关内容',
            resetButtonTitle: '清除查询',
            footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' }
          }
        }
      }
    },
    socialLinks: [
      { icon: 'github', link: 'https://github.com/Daylighttx/Piccolo_Plus' }
    ]
  }
})

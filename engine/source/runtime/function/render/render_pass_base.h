// [RenderPassBase] 所有渲染 Pass 的抽象基类（极薄）。
// 只定义生命周期接口：initialize（构造 GPU 资源）、preparePassData（每帧前从 RenderResource 取数据）、
// setCommonInfo（注入 RHI 与 RenderResource 两把“钥匙”）、initializeUIRenderBackend（UI 后端）。
// 除 setCommonInfo 外默认空实现，由子类按需重写。这是“渲染层每个 Pass 的统一骨架”。
#pragma once

#include "runtime/function/render/interface/rhi.h"

namespace Piccolo
{
    class RHI;
    class RenderResourceBase;
    class WindowUI;

    struct RenderPassInitInfo
    {};

    struct RenderPassCommonInfo
    {
        std::shared_ptr<RHI>                rhi;
        std::shared_ptr<RenderResourceBase> render_resource;
    };

    class RenderPassBase
    {
    public:
        virtual void initialize(const RenderPassInitInfo* init_info) = 0;
        virtual void postInitialize();
        virtual void setCommonInfo(RenderPassCommonInfo common_info);
        virtual void preparePassData(std::shared_ptr<RenderResourceBase> render_resource);
        virtual void initializeUIRenderBackend(WindowUI* window_ui);

    protected:
        std::shared_ptr<RHI>                m_rhi;
        std::shared_ptr<RenderResourceBase> m_render_resource;
    };
} // namespace Piccolo

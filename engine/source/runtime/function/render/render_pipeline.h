#pragma once

// ============================================================================
// RenderPipeline —— 渲染管线编排者（把各 Pass 串成一帧）
// ----------------------------------------------------------------------------
// 角色：持有全部 Pass 实例，并定义"一帧的绘制顺序"。
//   - initialize()：new 出 10 个 Pass、互相 setCommonInfo / 串好帧缓冲视图依赖
//   - deferredRender()/forwardRender()：每帧的绘制编排
//      阴影(方向光/点光) → MainCameraPass(8 个 subpass，见 main_camera_pass) →
//      粒子(拷贝深度 + simulate)
//   - passUpdateAfterRecreateSwapchain()：swapchain 重建后刷新各 Pass 的帧缓冲视图
// 它依赖 ③ 组的所有 Pass，是 ④ 总成里"真正下令画画"的那一层。
// ============================================================================

#include "runtime/function/render/render_pipeline_base.h"

namespace Piccolo
{
    class RenderPipeline : public RenderPipelineBase
    {
    public:
        virtual void initialize(RenderPipelineInitInfo init_info) override final;

        virtual void forwardRender(std::shared_ptr<RHI>                rhi,
                                   std::shared_ptr<RenderResourceBase> render_resource) override final;

        virtual void deferredRender(std::shared_ptr<RHI>                rhi,
                                    std::shared_ptr<RenderResourceBase> render_resource) override final;

        void passUpdateAfterRecreateSwapchain();

        virtual uint32_t getGuidOfPickedMesh(const Vector2& picked_uv) override final;

        void setAxisVisibleState(bool state);

        void setSelectedAxis(size_t selected_axis);
    };
} // namespace Piccolo

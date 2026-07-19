// [FXAAPass] FXAA 快速近似抗锯齿 subpass。
// 对上一阶段（color grading 后）的整屏图做边缘抗锯齿。可选（m_enable_fxaa 控制），不开则整个 subpass 跳过。
#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    class WindowUI;

    struct FXAAPassInitInfo : RenderPassInitInfo
    {
        RHIRenderPass* render_pass;
        RHIImageView*  input_attachment;
    };

    class FXAAPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;

        void updateAfterFramebufferRecreate(RHIImageView* input_attachment);

    private:
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
    };
} // namespace Piccolo

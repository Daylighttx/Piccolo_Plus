// [ColorGradingPass] 色彩分级 subpass（LUT / 美术调色）。
// 输入 tone mapping 后的图（backup_even）：做色相/饱和度/对比度等调色。
// 若开 FXAA 则输出到 post_process_odd（再交给 FXAA），否则直接写回 backup_odd。
#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct ColorGradingPassInitInfo : RenderPassInitInfo
    {
        RHIRenderPass* render_pass;
        RHIImageView* input_attachment;
    };

    class ColorGradingPass : public RenderPass
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

// [CombineUIPass] 合成“场景图 + UI 图”到 swapchain 的 subpass（管线最后一棒）。
// 输入两张 input attachment（scene / ui），叠加后输出到最终呈现的 swapchain image（finalLayout = PRESENT_SRC_KHR）。
#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct CombineUIPassInitInfo : RenderPassInitInfo
    {
        RHIRenderPass* render_pass;
        RHIImageView*  scene_input_attachment;
        RHIImageView*  ui_input_attachment;
    };

    class CombineUIPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;

        void updateAfterFramebufferRecreate(RHIImageView* scene_input_attachment, RHIImageView* ui_input_attachment);

    private:
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
    };
} // namespace Piccolo

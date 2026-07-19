// [ToneMappingPass] 色调映射（HDR→LDR）subpass。
// 输入是延迟光照产出的 HDR 缓冲（backup_odd），输出到 backup_even。把高动态范围亮度压缩到可显示范围（如 ACES 曲线）。
#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct ToneMappingPassInitInfo : RenderPassInitInfo
    {
        RHIRenderPass* render_pass;
        RHIImageView*  input_attachment;
    };

    class ToneMappingPass : public RenderPass
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

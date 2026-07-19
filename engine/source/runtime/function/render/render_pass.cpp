// [RenderPass 实现] 帧缓冲/描述符/管线容器的默认行为：把 attachment 收集成 image view 列表、暴露
// render_pass / descriptor set layout 供外部取用。静态可见节点 m_visiable_nodes 在此定义一次（所有 Pass 共享）。
#include "runtime/function/render/render_pass.h"

#include "runtime/core/base/macro.h"

#include "runtime/function/render/render_resource.h"
#include "runtime/function/render/interface/vulkan/vulkan_rhi.h"

Piccolo::VisiableNodes Piccolo::RenderPass::m_visiable_nodes;

namespace Piccolo
{
    void RenderPass::initialize(const RenderPassInitInfo* init_info)
    {
        m_global_render_resource =
            &(std::static_pointer_cast<RenderResource>(m_render_resource)->m_global_render_resource);
    }
    void RenderPass::draw() {}

    void RenderPass::postInitialize() {}

    RHIRenderPass* RenderPass::getRenderPass() const { return m_framebuffer.render_pass; }

    std::vector<RHIImageView*> RenderPass::getFramebufferImageViews() const
    {
        std::vector<RHIImageView*> image_views;
        for (auto& attach : m_framebuffer.attachments)
        {
            image_views.push_back(attach.view);
        }
        return image_views;
    }

    std::vector<RHIDescriptorSetLayout*> RenderPass::getDescriptorSetLayouts() const
    {
        std::vector<RHIDescriptorSetLayout*> layouts;
        for (auto& desc : m_descriptor_infos)
        {
            layouts.push_back(desc.layout);
        }
        return layouts;
    }
} // namespace Piccolo

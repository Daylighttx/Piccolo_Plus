// [RenderPass] 带“帧缓冲/描述符集/管线”容器 + Vulkan 原生 subpass 框架的 Pass 基类。
// MainCameraPass 继承自它，把“几何→延迟光照→前向→后处理→UI”全部塞进一个 VkRenderPass 的 8 个 subpass。
// 关键：静态成员 m_visiable_nodes（原拼写 typo）是所有 Pass 共享的“本帧可见节点”指针集合，绘制时从这里取数据。
#pragma once

#include "runtime/function/render/render_common.h"
#include "runtime/function/render/render_pass_base.h"
#include "runtime/function/render/render_resource.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace Piccolo
{
    class VulkanRHI;

    enum
    {
        _main_camera_pass_gbuffer_a                     = 0,
        _main_camera_pass_gbuffer_b                     = 1,
        _main_camera_pass_gbuffer_c                     = 2,
        _main_camera_pass_backup_buffer_odd             = 3,
        _main_camera_pass_backup_buffer_even            = 4,
        _main_camera_pass_post_process_buffer_odd       = 5,
        _main_camera_pass_post_process_buffer_even      = 6,
        _main_camera_pass_depth                         = 7,
        _main_camera_pass_swap_chain_image              = 8,
        _main_camera_pass_custom_attachment_count       = 5,
        _main_camera_pass_post_process_attachment_count = 2,
        _main_camera_pass_attachment_count              = 9,
    };

    enum
    {
        _main_camera_subpass_basepass = 0,
        _main_camera_subpass_deferred_lighting,
        _main_camera_subpass_forward_lighting,
        _main_camera_subpass_tone_mapping,
        _main_camera_subpass_color_grading,
        _main_camera_subpass_fxaa,
        _main_camera_subpass_ui,
        _main_camera_subpass_combine_ui,
        _main_camera_subpass_count
    };

    struct VisiableNodes
    {
        std::vector<RenderMeshNode>*              p_directional_light_visible_mesh_nodes {nullptr};
        std::vector<RenderMeshNode>*              p_point_lights_visible_mesh_nodes {nullptr};
        std::vector<RenderMeshNode>*              p_main_camera_visible_mesh_nodes {nullptr};
        RenderAxisNode*                           p_axis_node {nullptr};
    };

    class RenderPass : public RenderPassBase
    {
    public:
        struct FrameBufferAttachment
        {
            RHIImage*        image;
            RHIDeviceMemory* mem;
            RHIImageView*    view;
            RHIFormat       format;
        };

        struct Framebuffer
        {
            int           width;
            int           height;
            RHIFramebuffer* framebuffer;
            RHIRenderPass*  render_pass;

            std::vector<FrameBufferAttachment> attachments;
        };

        struct Descriptor
        {
            RHIDescriptorSetLayout* layout;
            RHIDescriptorSet*       descriptor_set;
        };

        struct RenderPipelineBase
        {
            RHIPipelineLayout* layout;
            RHIPipeline*       pipeline;
        };

        GlobalRenderResource*      m_global_render_resource {nullptr};

        std::vector<Descriptor>         m_descriptor_infos;
        std::vector<RenderPipelineBase> m_render_pipelines;
        Framebuffer                     m_framebuffer;

        void initialize(const RenderPassInitInfo* init_info) override;
        void postInitialize() override;

        virtual void draw();

        virtual RHIRenderPass*                       getRenderPass() const;
        virtual std::vector<RHIImageView*>           getFramebufferImageViews() const;
        virtual std::vector<RHIDescriptorSetLayout*> getDescriptorSetLayouts() const;

        static VisiableNodes m_visiable_nodes;

    private:
    };
} // namespace Piccolo

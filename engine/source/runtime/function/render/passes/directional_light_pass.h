// [DirectionalLightShadowPass] 方向光阴影贴图 Pass（正交投影，可扩展为级联 CSM）。
// 从平行光方向渲染深度到 shadow map；主相机延迟光照阶段会采样它（m_directional_light_shadow_color_image_view）。
// 与 PointLightShadowPass 一样，每帧由 RenderPipeline 在 MainCameraPass 之前单独 draw()。
#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    class RenderResourceBase;

    class DirectionalLightShadowPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void postInitialize() override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void draw() override final;

        void setPerMeshLayout(RHIDescriptorSetLayout* layout) { m_per_mesh_layout = layout; }

    private:
        void setupAttachments();
        void setupRenderPass();
        void setupFramebuffer();
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
        void drawModel();

    private:
        RHIDescriptorSetLayout* m_per_mesh_layout;
        MeshDirectionalLightShadowPerframeStorageBufferObject
            m_mesh_directional_light_shadow_perframe_storage_buffer_object;
    };
} // namespace Piccolo

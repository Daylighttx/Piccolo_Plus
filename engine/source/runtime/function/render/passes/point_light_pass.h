// [PointLightShadowPass] 点光源阴影贴图 Pass。
// 从点光源视角（立方体 6 面）渲染场景深度到一张 shadow map，供主相机延迟光照阶段采样做阴影判定。
// 它只“产”阴影贴图，不进主相机的 8-subpass 链；由 RenderPipeline 在每帧最前面单独 draw()。
#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    class RenderResourceBase;

    class PointLightShadowPass : public RenderPass
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
        MeshPointLightShadowPerframeStorageBufferObject m_mesh_point_light_shadow_perframe_storage_buffer_object;
    };
} // namespace Piccolo

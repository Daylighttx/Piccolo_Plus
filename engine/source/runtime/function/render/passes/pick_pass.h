// [PickPass] 鼠标拾取 Pass（屏幕坐标 → 物体 ID）。
// 把场景按“物体 ID”渲到一张 offscreen 图（_object_id_image），再 readback 像素得到鼠标下的 GameObject。
// 与主相机 8-subpass 链完全独立，按需（点击时）触发。
#pragma once

#include "runtime/core/math/vector2.h"
#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    class RenderResourceBase;

    struct PickPassInitInfo : RenderPassInitInfo
    {
        RHIDescriptorSetLayout* per_mesh_layout;
    };

    class PickPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void postInitialize() override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void draw() override final;

        uint32_t pick(const Vector2& picked_uv);
        void     recreateFramebuffer();

        MeshInefficientPickPerframeStorageBufferObject _mesh_inefficient_pick_perframe_storage_buffer_object;

    private:
        void setupAttachments();
        void setupRenderPass();
        void setupFramebuffer();
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();

    private:
        RHIImage*        _object_id_image = nullptr;
        RHIDeviceMemory* _object_id_image_memory = nullptr;
        RHIImageView*      _object_id_image_view = nullptr;

        RHIDescriptorSetLayout* _per_mesh_layout = nullptr;
    };
} // namespace Piccolo

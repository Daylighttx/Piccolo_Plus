#pragma once

// ============================================================================
// RenderSystem —— 渲染系统的"总成"入口（CPU 端调度者）
// ----------------------------------------------------------------------------
// 角色：逻辑层与主渲染循环之间的唯一桥梁。
//   - initialize()：装配 RHI(VulkanRHI) + RenderResource + RenderCamera +
//                    RenderScene + RenderPipeline，并上传全局资源(IBL/色彩分级)。
//   - tick()：每帧 6 步（消费 swap 数据 → 准备命令上下文 → 更新逐帧缓冲 →
//             收集灯光 → 剔除可见物体 → 调用 pipeline 画一帧）。
//   - processSwapData()：把 RenderSwapContext 里逻辑层塞进来的数据，转成渲染层
//                        能用的 RenderEntity / 上传 GPU / 重建灯光。
// 它是阅读路线图的最后一站（④ 总成），把 ①②③ 组的所有概念串成一条线。
// ============================================================================

#include "runtime/function/render/render_entity.h"
#include "runtime/function/render/render_guid_allocator.h"
#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_type.h"

#include <array>
#include <memory>
#include <optional>

namespace Piccolo
{
    class WindowSystem;
    class RHI;
    class RenderResourceBase;
    class RenderPipelineBase;
    class RenderScene;
    class RenderCamera;
    class WindowUI;
    class DebugDrawManager;

    struct RenderSystemInitInfo
    {
        std::shared_ptr<WindowSystem> window_system;
        std::shared_ptr<DebugDrawManager> debugdraw_manager;
    };

    struct EngineContentViewport
    {
        float x { 0.f};
        float y { 0.f};
        float width { 0.f};
        float height { 0.f};
    };

    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem();

        void initialize(RenderSystemInitInfo init_info);
        void tick(float delta_time);
        void clear();

        void                          swapLogicRenderData();
        RenderSwapContext&            getSwapContext();
        std::shared_ptr<RenderCamera> getRenderCamera() const;
        std::shared_ptr<RenderScene>  getRenderScene() const { return m_render_scene; }
        std::shared_ptr<RHI>          getRHI() const;

        void      setRenderPipelineType(RENDER_PIPELINE_TYPE pipeline_type);
        void      initializeUIRenderBackend(WindowUI* window_ui);
        void      updateEngineContentViewport(float offset_x, float offset_y, float width, float height);
        uint32_t  getGuidOfPickedMesh(const Vector2& picked_uv);
        GObjectID getGObjectIDByMeshID(uint32_t mesh_id) const;

        EngineContentViewport getEngineContentViewport() const;

        void createAxis(std::array<RenderEntity, 3> axis_entities, std::array<RenderMeshData, 3> mesh_datas);
        void setVisibleAxis(std::optional<RenderEntity> axis);
        void setSelectedAxis(size_t selected_axis);
        GuidAllocator<GameObjectPartId>& getGOInstanceIdAllocator();
        GuidAllocator<MeshSourceDesc>&   getMeshAssetIdAllocator();

        void clearForLevelReloading();

    private:
        RENDER_PIPELINE_TYPE m_render_pipeline_type {RENDER_PIPELINE_TYPE::DEFERRED_PIPELINE};

        RenderSwapContext m_swap_context;

        std::shared_ptr<RHI>                m_rhi;
        std::shared_ptr<RenderCamera>       m_render_camera;
        std::shared_ptr<RenderScene>        m_render_scene;
        std::shared_ptr<RenderResourceBase> m_render_resource;
        std::shared_ptr<RenderPipelineBase> m_render_pipeline;

        void processSwapData();
        void updateLights();   // 阶段二 2.2：把 LightComponent 收集进渲染场景
    };
} // namespace Piccolo

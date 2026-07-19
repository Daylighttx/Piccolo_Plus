#pragma once

#include "runtime/function/framework/object/object_id_allocator.h"  // ID 分配器
#include "runtime/function/render/light.h"            // 光照类型
#include "runtime/function/render/render_common.h"   // 渲染公共类型(RenderMeshNode 等)
#include "runtime/function/render/render_entity.h"   // RenderEntity
#include "runtime/function/render/render_guid_allocator.h"  // GUID 分配器
#include "runtime/function/render/render_object.h"   // GameObjectDesc 等

#include <optional>   // optional(坐标轴 gizmo)
#include <vector>     // 动态数组

namespace Piccolo
{
    class RenderResource;  // 前向声明
    class RenderCamera;    // 前向声明

    // 渲染层的"世界状态"：所有 Pass 真正读取的场景数据
    class RenderScene
    {
    public:
        // —— 光照 ——
        AmbientLight      m_ambient_light;        // 环境光
        PDirectionalLight m_directional_light;    // 方向光(由 updateLights() 覆盖)
        PointLightList    m_point_light_list;     // 点光列表

        // —— 渲染实体 ——
        std::vector<RenderEntity> m_render_entities;  // 全部渲染实体

        // —— 编辑器坐标轴 ——
        std::optional<RenderEntity> m_render_axis;     // 坐标轴 gizmo(可选)

        // —— 每帧可见列表(剔除后产出) ——
        std::vector<RenderMeshNode> m_directional_light_visible_mesh_nodes;  // 方向光可见物体(阴影用)
        std::vector<RenderMeshNode> m_point_lights_visible_mesh_nodes;       // 点光可见物体(阴影用)
        std::vector<RenderMeshNode> m_main_camera_visible_mesh_nodes;        // 主相机可见物体
        RenderAxisNode              m_axis_node;                             // 坐标轴节点

        void clear();  // 清空场景

        // ★每帧更新可见物体：做视锥 + 光源剔除，产出上面各可见列表
        void updateVisibleObjects(std::shared_ptr<RenderResource> render_resource,
                                  std::shared_ptr<RenderCamera>   camera);

        void setVisibleNodesReference();  // 把可见节点指针设到各 Pass 中

        // 各类 ID 分配器
        GuidAllocator<GameObjectPartId>&   getInstanceIdAllocator();
        GuidAllocator<MeshSourceDesc>&     getMeshAssetIdAllocator();
        GuidAllocator<MaterialSourceDesc>& getMaterialAssetdAllocator();

        void      addInstanceIdToMap(uint32_t instance_id, GObjectID go_id);  // 记录 实例ID→对象ID
        GObjectID getGObjectIDByMeshID(uint32_t mesh_id) const;               // 由 mesh 反查对象(拾取用)
        void      deleteEntityByGObjectID(GObjectID go_id);                  // 按对象删除实体

        void clearForLevelReloading();  // 重新加载关卡时清空

    private:
        // ID 分配器实例
        GuidAllocator<GameObjectPartId>   m_instance_id_allocator;
        GuidAllocator<MeshSourceDesc>     m_mesh_asset_id_allocator;
        GuidAllocator<MaterialSourceDesc> m_material_asset_id_allocator;

        std::unordered_map<uint32_t, GObjectID> m_mesh_object_id_map;  // mesh ID → GameObject ID 映射

        // 各来源可见物体计算(被 updateVisibleObjects 调用)
        void updateVisibleObjectsDirectionalLight(std::shared_ptr<RenderResource> render_resource,
                                                  std::shared_ptr<RenderCamera>   camera);
        void updateVisibleObjectsPointLight(std::shared_ptr<RenderResource> render_resource);
        void updateVisibleObjectsMainCamera(std::shared_ptr<RenderResource> render_resource,
                                            std::shared_ptr<RenderCamera>   camera);
        void updateVisibleObjectsAxis(std::shared_ptr<RenderResource> render_resource);
        void updateVisibleObjectsParticle(std::shared_ptr<RenderResource> render_resource);
    };
} // namespace Piccolo

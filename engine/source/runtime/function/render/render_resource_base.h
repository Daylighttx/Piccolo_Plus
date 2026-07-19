#pragma once

#include "runtime/function/render/render_scene.h"
#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_type.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Piccolo
{
    class RHI;
    class RenderScene;
    class RenderCamera;

    // 渲染资源管理的抽象基类：定义"把 CPU 端数据上传到 GPU"的接口。
    // 真正实现是 RenderResource（见 render_resource.h/.cpp）。
    // 抽这一层的目的：将来若换图形后端，只要实现这些接口即可，
    // 上层的 RenderSystem / Pass 代码无需改动（这是"超越商业引擎"的可替换接缝）。
    class RenderResourceBase
    {
    public:
        virtual ~RenderResourceBase() {}

        // 释放所有已上传的 GPU 资源
        virtual void clear() = 0;

        // 上传关卡级全局资源（IBL 环境贴图、色彩分级 LUT 等），通常只调一次
        virtual void uploadGlobalRenderResource(std::shared_ptr<RHI> rhi, LevelResourceDesc level_resource_desc) = 0;

        // 上传一个游戏对象渲染实例的资源（mesh + 材质 一起）
        virtual void uploadGameObjectRenderResource(std::shared_ptr<RHI> rhi,
                                                    RenderEntity         render_entity,
                                                    RenderMeshData       mesh_data,
                                                    RenderMaterialData   material_data) = 0;

        // 仅上传 mesh（材质已缓存时走这个重载）
        virtual void uploadGameObjectRenderResource(std::shared_ptr<RHI> rhi,
                                                    RenderEntity         render_entity,
                                                    RenderMeshData       mesh_data) = 0;

        // 仅上传材质（mesh 已缓存时走这个重载）
        virtual void uploadGameObjectRenderResource(std::shared_ptr<RHI> rhi,
                                                    RenderEntity         render_entity,
                                                    RenderMaterialData   material_data) = 0;

        // 每帧把相机矩阵 / 灯光等动态数据写入逐帧 UBO/SSBO（ring buffer）
        virtual void updatePerFrameBuffer(std::shared_ptr<RenderScene>  render_scene,
                                          std::shared_ptr<RenderCamera> camera) = 0;

        // 下面这些是 CPU 端的资源"加载器"（从磁盘读图/读 mesh），不是 GPU 上传本身
        // TODO: data caching
        std::shared_ptr<TextureData> loadTextureHDR(std::string file, int desired_channels = 4);
        std::shared_ptr<TextureData> loadTexture(std::string file, bool is_srgb = false);
        RenderMeshData               loadMeshData(const MeshSourceDesc& source, AxisAlignedBox& bounding_box);
        RenderMaterialData           loadMaterialData(const MaterialSourceDesc& source);
        AxisAlignedBox               getCachedBoudingBox(const MeshSourceDesc& source) const;

    private:
        StaticMeshData loadStaticMesh(std::string mesh_file, AxisAlignedBox& bounding_box);

        // 包围盒缓存：同一 mesh 的包围盒只算一次
        std::unordered_map<MeshSourceDesc, AxisAlignedBox> m_bounding_box_cache_map;
    };
} // namespace Piccolo

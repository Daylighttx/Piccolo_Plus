#pragma once

#include "runtime/function/render/render_resource_base.h"
#include "runtime/function/render/render_type.h"
#include "runtime/function/render/interface/rhi.h"

#include "runtime/function/render/render_common.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <map>
#include <vector>
#include <cmath>

namespace Piccolo
{
    class RHI;
    class RenderPassBase;
    class RenderCamera;

    // IBL（基于图像的光照）相关 GPU 资源：BRDF 查找表 + 辐照度图 + 高光图
    struct IBLResource
    {
        RHIImage* _brdfLUT_texture_image;
        RHIImageView* _brdfLUT_texture_image_view;
        RHISampler* _brdfLUT_texture_sampler;
        VmaAllocation _brdfLUT_texture_image_allocation;       // VMA 显存分配句柄

        RHIImage* _irradiance_texture_image;
        RHIImageView* _irradiance_texture_image_view;
        RHISampler* _irradiance_texture_sampler;
        VmaAllocation _irradiance_texture_image_allocation;

        RHIImage* _specular_texture_image;
        RHIImageView* _specular_texture_image_view;
        RHISampler* _specular_texture_sampler;
        VmaAllocation _specular_texture_image_allocation;
    };

    // IBL 的 CPU 端像素数据（上传前的临时持有结构）
    struct IBLResourceData
    {
        void* _brdfLUT_texture_image_pixels;
        uint32_t             _brdfLUT_texture_image_width;
        uint32_t             _brdfLUT_texture_image_height;
        RHIFormat   _brdfLUT_texture_image_format;
        std::array<void*, 6> _irradiance_texture_image_pixels; // 立方体贴图 6 面
        uint32_t             _irradiance_texture_image_width;
        uint32_t             _irradiance_texture_image_height;
        RHIFormat   _irradiance_texture_image_format;
        std::array<void*, 6> _specular_texture_image_pixels;
        uint32_t             _specular_texture_image_width;
        uint32_t             _specular_texture_image_height;
        RHIFormat   _specular_texture_image_format;
    };

    // 色彩分级用的 3D LUT 贴图资源
    struct ColorGradingResource
    {
        RHIImage* _color_grading_LUT_texture_image;
        RHIImageView* _color_grading_LUT_texture_image_view;
        VmaAllocation _color_grading_LUT_texture_image_allocation;
    };

    struct ColorGradingResourceData
    {
        void* _color_grading_LUT_texture_image_pixels;
        uint32_t           _color_grading_LUT_texture_image_width;
        uint32_t           _color_grading_LUT_texture_image_height;
        RHIFormat _color_grading_LUT_texture_image_format;
    };

    // 全局存储缓冲（UBO/SSBO）相关资源
    struct StorageBuffer
    {
        // 设备对齐限制（来自物理设备 properties）
        uint32_t _min_uniform_buffer_offset_alignment{ 256 };
        uint32_t _min_storage_buffer_offset_alignment{ 256 };
        uint32_t _max_storage_buffer_range{ 1 << 27 };
        uint32_t _non_coherent_atom_size{ 256 };

        // 128MB 常驻映射的 ring buffer（逐帧动态数据写这里，零提交开销）
        RHIBuffer* _global_upload_ringbuffer;
        RHIDeviceMemory* _global_upload_ringbuffer_memory;
        void* _global_upload_ringbuffer_memory_pointer;
        std::vector<uint32_t> _global_upload_ringbuffers_begin;  // 每帧切片起点
        std::vector<uint32_t> _global_upload_ringbuffers_end;    // 每帧切片当前写到的位置
        std::vector<uint32_t> _global_upload_ringbuffers_size;   // 每帧切片大小

        // 不绑定任何有效数据时用的空 descriptor 缓冲
        RHIBuffer* _global_null_descriptor_storage_buffer;
        RHIDeviceMemory* _global_null_descriptor_storage_buffer_memory;

        // 坐标轴 gizmo 用的存储缓冲
        RHIBuffer* _axis_inefficient_storage_buffer;
        RHIDeviceMemory* _axis_inefficient_storage_buffer_memory;
        void* _axis_inefficient_storage_buffer_memory_pointer;
    };

    // 聚合所有全局渲染资源
    struct GlobalRenderResource
    {
        IBLResource          _ibl_resource;
        ColorGradingResource _color_grading_resource;
        StorageBuffer        _storage_buffer;
    };

    // RenderResource：RenderResourceBase 的真正实现，负责把所有 CPU 数据上传 GPU
    class RenderResource : public RenderResourceBase
    {
    public:
        void clear() override final;

        // 上传关卡全局资源（IBL + LUT）
        virtual void uploadGlobalRenderResource(std::shared_ptr<RHI> rhi,
            LevelResourceDesc    level_resource_desc) override final;

        // 上传游戏对象资源（mesh + 材质 一起 / 仅 mesh / 仅材质 三个重载）
        virtual void uploadGameObjectRenderResource(std::shared_ptr<RHI> rhi,
            RenderEntity         render_entity,
            RenderMeshData       mesh_data,
            RenderMaterialData   material_data) override final;

        virtual void uploadGameObjectRenderResource(std::shared_ptr<RHI> rhi,
            RenderEntity         render_entity,
            RenderMeshData       mesh_data) override final;

        virtual void uploadGameObjectRenderResource(std::shared_ptr<RHI> rhi,
            RenderEntity         render_entity,
            RenderMaterialData   material_data) override final;

        // 每帧把相机/灯光写入逐帧存储缓冲
        virtual void updatePerFrameBuffer(std::shared_ptr<RenderScene>  render_scene,
            std::shared_ptr<RenderCamera> camera) override final;

        // 取已上传的 mesh / 材质（Pass 绘制时调用）
        VulkanMesh& getEntityMesh(RenderEntity entity);
        VulkanPBRMaterial& getEntityMaterial(RenderEntity entity);

        // 每帧重置 ring buffer 写指针到切片起点
        void resetRingBufferOffset(uint8_t current_frame_index);

        // 全局渲染资源，包含 IBL 数据 + 全局存储缓冲
        GlobalRenderResource m_global_render_resource;

        // 逐帧存储缓冲对象（UBO/SSBO 的 CPU 镜像，最终写进 ring buffer）
        MeshPerframeStorageBufferObject                 m_mesh_perframe_storage_buffer_object;
        MeshPointLightShadowPerframeStorageBufferObject m_mesh_point_light_shadow_perframe_storage_buffer_object;
        MeshDirectionalLightShadowPerframeStorageBufferObject
                                                       m_mesh_directional_light_shadow_perframe_storage_buffer_object;
        AxisStorageBufferObject                        m_axis_storage_buffer_object;
        MeshInefficientPickPerframeStorageBufferObject m_mesh_inefficient_pick_perframe_storage_buffer_object;
        ParticleBillboardPerframeStorageBufferObject   m_particlebillboard_perframe_storage_buffer_object;
        ParticleCollisionPerframeStorageBufferObject   m_particle_collision_perframe_storage_buffer_object;

        // 已上传 mesh / 材质的缓存，key = asset id，保证同资源只上传一次
        std::map<size_t, VulkanMesh>        m_vulkan_meshes;
        std::map<size_t, VulkanPBRMaterial> m_vulkan_pbr_materials;

        // 上传 mesh / 材质时要用到的 descriptor set layout（由 MainCameraPass 设置进来）
        RHIDescriptorSetLayout* const* m_mesh_descriptor_set_layout {nullptr};
        RHIDescriptorSetLayout* const* m_material_descriptor_set_layout {nullptr};

    private:
        // 创建并映射 128MB 全局存储缓冲 ring buffer
        void createAndMapStorageBuffer(std::shared_ptr<RHI> rhi);
        void createIBLSamplers(std::shared_ptr<RHI> rhi);
        void createIBLTextures(std::shared_ptr<RHI>                        rhi,
                               std::array<std::shared_ptr<TextureData>, 6> irradiance_maps,
                               std::array<std::shared_ptr<TextureData>, 6> specular_maps);

        // 按 asset id 查缓存，没有才真正上传（核心去重逻辑）
        VulkanMesh& getOrCreateVulkanMesh(std::shared_ptr<RHI> rhi, RenderEntity entity, RenderMeshData mesh_data);
        VulkanPBRMaterial&
        getOrCreateVulkanMaterial(std::shared_ptr<RHI> rhi, RenderEntity entity, RenderMaterialData material_data);

        // mesh 上传拆成 buffer 创建 + 顶点填充 + 索引填充三步
        void updateMeshData(std::shared_ptr<RHI>                          rhi,
                            bool                                          enable_vertex_blending,
                            uint32_t                                      index_buffer_size,
                            void*                                         index_buffer_data,
                            uint32_t                                      vertex_buffer_size,
                            struct MeshVertexDataDefinition const*        vertex_buffer_data,
                            uint32_t                                      joint_binding_buffer_size,
                            struct MeshVertexBindingDataDefinition const* joint_binding_buffer_data,
                            VulkanMesh&                                   now_mesh);
        void updateVertexBuffer(std::shared_ptr<RHI>                          rhi,
                                bool                                          enable_vertex_blending,
                                uint32_t                                      vertex_buffer_size,
                                struct MeshVertexDataDefinition const*        vertex_buffer_data,
                                uint32_t                                      joint_binding_buffer_size,
                                struct MeshVertexBindingDataDefinition const* joint_binding_buffer_data,
                                uint32_t                                      index_buffer_size,
                                uint16_t*                                     index_buffer_data,
                                VulkanMesh&                                   now_mesh);
        void updateIndexBuffer(std::shared_ptr<RHI> rhi,
                               uint32_t             index_buffer_size,
                               void*                index_buffer_data,
                               VulkanMesh&          now_mesh);
        // 把 CPU 贴图像素真正建 GPU image（走 staging + 布局切换）
        void updateTextureImageData(std::shared_ptr<RHI> rhi, const TextureDataToUpdate& texture_data);
    };
} // namespace Piccolo

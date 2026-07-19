// 渲染交换上下文：实现逻辑线程与渲染线程之间的双缓冲数据交换
#pragma once

#include "runtime/function/particle/emitter_id_allocator.h"
#include "runtime/function/particle/particle_desc.h"
#include "runtime/function/render/render_camera.h"
#include "runtime/function/render/render_object.h"

#include "runtime/resource/res_type/global/global_particle.h"
#include "runtime/resource/res_type/global/global_rendering.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace Piccolo
{
    // 关卡 IBL 相关资源描述（天空盒辐照度贴图、镜面反射贴图、BRDF 查找表）
    struct LevelIBLResourceDesc
    {
        SkyBoxIrradianceMap m_skybox_irradiance_map; // 天空盒辐照度贴图（用于漫反射 IBL）
        SkyBoxSpecularMap   m_skybox_specular_map;   // 天空盒镜面反射贴图（用于镜面反射 IBL）
        std::string         m_brdf_map;              // BRDF 查找表贴图路径
    };

    // 关卡颜色分级资源描述
    struct LevelColorGradingResourceDesc
    {
        std::string m_color_grading_map; // 颜色分级 LUT 贴图路径
    };

    // 关卡整体资源描述（包含 IBL 与颜色分级）
    struct LevelResourceDesc
    {
        LevelIBLResourceDesc          m_ibl_resource_desc;          // IBL 资源
        LevelColorGradingResourceDesc m_color_grading_resource_desc; // 颜色分级资源
    };

    // 相机交换数据：逻辑线程向渲染线程传递相机参数变更
    struct CameraSwapData
    {
        std::optional<float>            m_fov_x;       // 水平视场角（可选）
        std::optional<RenderCameraType> m_camera_type; // 相机类型（可选）
        std::optional<Matrix4x4>        m_view_matrix; // 视图矩阵（可选）
    };

    // 游戏对象资源描述队列：用于批量提交新增/更新的游戏对象
    struct GameObjectResourceDesc
    {
        std::deque<GameObjectDesc> m_game_object_descs; // 待处理游戏对象描述队列

        void add(GameObjectDesc& desc);       // 向队列尾部添加一个游戏对象描述
        void pop();                           // 弹出队首的游戏对象描述

        bool isEmpty() const;                 // 队列是否为空

        GameObjectDesc& getNextProcessObject(); // 获取队首待处理的游戏对象描述
    };

    // 粒子发射器提交请求：提交需要新建的粒子发射器
    struct ParticleSubmitRequest
    {
        std::vector<ParticleEmitterDesc> m_emitter_descs; // 待提交的粒子发射器描述列表

        void add(ParticleEmitterDesc& desc); // 添加一个粒子发射器描述

        unsigned int getEmitterCount() const; // 获取待提交的发射器总数

        const ParticleEmitterDesc& getEmitterDesc(unsigned int index); // 获取指定索引的发射器描述
    };

    // 发射器 tick 请求：需要在本帧更新的粒子发射器 ID 列表
    struct EmitterTickRequest
    {
        std::vector<ParticleEmitterID> m_emitter_indices; // 待 tick 的发射器 ID 列表
    };

    // 发射器变换更新请求：需要更新位置/旋转等变换信息的粒子发射器
    struct EmitterTransformRequest
    {
        std::vector<ParticleEmitterTransformDesc> m_transform_descs; // 变换描述列表

        void add(ParticleEmitterTransformDesc& desc); // 添加一个变换描述

        void clear(); // 清空所有变换描述

        unsigned int getEmitterCount() const; // 获取待更新的发射器总数

        const ParticleEmitterTransformDesc& getNextEmitterTransformDesc(unsigned int index); // 获取指定索引的变换描述
    };

    // 渲染交换数据：逻辑线程与渲染线程之间一次交换的所有数据的集合
    struct RenderSwapData
    {
        std::optional<LevelResourceDesc>       m_level_resource_desc;       // 关卡资源描述（有变化时才填充）
        std::optional<GameObjectResourceDesc>  m_game_object_resource_desc; // 新增/更新的游戏对象
        std::optional<GameObjectResourceDesc>  m_game_object_to_delete;     // 需要删除的游戏对象
        std::optional<CameraSwapData>          m_camera_swap_data;          // 相机参数变更
        std::optional<ParticleSubmitRequest>   m_particle_submit_request;   // 新建粒子发射器请求
        std::optional<EmitterTickRequest>      m_emitter_tick_request;      // 发射器 tick 请求
        std::optional<EmitterTransformRequest> m_emitter_transform_request; // 发射器变换更新请求

        void addDirtyGameObject(GameObjectDesc&& desc);      // 添加一个需要更新的游戏对象（移动语义）
        void addDeleteGameObject(GameObjectDesc&& desc);     // 添加一个需要删除的游戏对象（移动语义）

        void addNewParticleEmitter(ParticleEmitterDesc& desc);     // 登记一个新建的粒子发射器
        void addTickParticleEmitter(ParticleEmitterID id);         // 登记一个需要 tick 的粒子发射器
        void updateParticleTransform(ParticleEmitterTransformDesc& desc); // 更新粒子发射器变换
    };

    // 双缓冲交换数据类型：区分逻辑线程与渲染线程各自操作的缓冲区索引
    enum SwapDataType : uint8_t
    {
        LogicSwapDataType = 0, // 逻辑线程写入缓冲区索引
        RenderSwapDataType,    // 渲染线程读取缓冲区索引
        SwapDataTypeCount      // 缓冲区总数
    };

    // 渲染交换上下文：以双缓冲（index swap）的方式实现逻辑-渲染线程数据交换
    // 逻辑线程通过 getLogicSwapData() 写入数据，渲染线程通过 getRenderSwapData() 读取数据，
    // 每帧调用 swapLogicRenderData() 交换索引完成数据切换。
    class RenderSwapContext
    {
    public:
        RenderSwapData& getLogicSwapData();   // 获取逻辑线程写入的交换数据引用
        RenderSwapData& getRenderSwapData();  // 获取渲染线程读取的交换数据引用
        void            swapLogicRenderData(); // 交换逻辑/渲染侧的缓冲区索引

        void            resetLevelRsourceSwapData();       // 重置关卡资源交换数据
        void            resetGameObjectResourceSwapData(); // 重置游戏对象资源交换数据
        void            resetGameObjectToDelete();         // 重置待删除游戏对象
        void            resetCameraSwapData();             // 重置相机交换数据
        void            resetPartilceBatchSwapData();      // 重置粒子批处理交换数据
        void            resetEmitterTickSwapData();        // 重置发射器 tick 交换数据
        void            resetEmitterTransformSwapData();   // 重置发射器变换交换数据

    private:
        uint8_t        m_logic_swap_data_index {LogicSwapDataType};  // 逻辑线程当前写入的缓冲区索引
        uint8_t        m_render_swap_data_index {RenderSwapDataType}; // 渲染线程当前读取的缓冲区索引
        RenderSwapData m_swap_data[SwapDataTypeCount];                 // 双缓冲数据数组

        bool isReadyToSwap() const; // 判断是否满足交换条件
        void swap();                // 执行索引交换
    };
} // namespace Piccolo
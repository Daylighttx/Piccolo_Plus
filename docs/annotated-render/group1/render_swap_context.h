#pragma once

#include "runtime/function/particle/emitter_id_allocator.h"  // 粒子发射器 ID 分配器
#include "runtime/function/particle/particle_desc.h"          // 粒子描述结构体
#include "runtime/function/render/render_camera.h"            // 渲染相机
#include "runtime/function/render/render_object.h"            // 逻辑层物体描述(GameObjectDesc 等)

#include "runtime/resource/res_type/global/global_particle.h"    // 全局粒子资源类型
#include "runtime/resource/res_type/global/global_rendering.h"    // 全局渲染资源类型(IBL/色彩分级等)

#include <cstdint>   // 定宽整数类型
#include <deque>     // 双端队列(物体描述用队列累积)
#include <optional>  // optional(各字段"有/无"变化)
#include <string>    // 字符串(资源路径)

namespace Piccolo
{
    // 关卡级 IBL(基于图像的光照)资源描述
    struct LevelIBLResourceDesc
    {
        SkyBoxIrradianceMap m_skybox_irradiance_map;  // 天空盒漫反射辐照度图
        SkyBoxSpecularMap   m_skybox_specular_map;    // 天空盒镜面反射预滤波图
        std::string         m_brdf_map;               // BRDF 查找表(LUT)路径
    };

    // 关卡级色彩分级(调色)资源描述
    struct LevelColorGradingResourceDesc
    {
        std::string m_color_grading_map;  // 色彩分级 LUT 路径
    };

    // 关卡整体资源描述(IBL + 色彩分级)
    struct LevelResourceDesc
    {
        LevelIBLResourceDesc          m_ibl_resource_desc;            // IBL 部分
        LevelColorGradingResourceDesc m_color_grading_resource_desc;  // 色彩分级部分
    };

    // 相机交换数据：逻辑层改了相机才填，用 optional 表示"本帧是否更新"
    struct CameraSwapData
    {
        std::optional<float>             m_fov_x;        // 水平视场角(改了才有)
        std::optional<RenderCameraType>  m_camera_type;  // 相机类型(Editor/Motor)
        std::optional<Matrix4x4>         m_view_matrix;  // 视图矩阵(逻辑层算好传进来)
    };

    // 物体资源描述：一个队列，逻辑层往里塞要新增/更新的物体
    struct GameObjectResourceDesc
    {
        std::deque<GameObjectDesc> m_game_object_descs;  // 待处理的物体描述队列

        void add(GameObjectDesc& desc);                  // 入队
        void pop();                                      // 出队(渲染消费后)

        bool isEmpty() const;                            // 队列是否空
        GameObjectDesc& getNextProcessObject();          // 取队首(下一个要处理的)
    };

    // 粒子提交请求：逻辑层请求新建一批发射器
    struct ParticleSubmitRequest
    {
        std::vector<ParticleEmitterDesc> m_emitter_descs;          // 发射器描述列表
        void add(ParticleEmitterDesc& desc);                       // 加入一个发射器
        unsigned int getEmitterCount() const;                      // 数量
        const ParticleEmitterDesc& getEmitterDesc(unsigned int index);  // 按索引取
    };

    // 发射器 tick 请求：逻辑层请求某些发射器推进模拟
    struct EmitterTickRequest
    {
        std::vector<ParticleEmitterID> m_emitter_indices;  // 要 tick 的发射器 ID 列表
    };

    // 发射器变换更新请求：逻辑层更新发射器位置/朝向
    struct EmitterTransformRequest
    {
        std::vector<ParticleEmitterTransformDesc> m_transform_descs;                 // 变换描述列表
        void add(ParticleEmitterTransformDesc& desc);                               // 加入
        void clear();                                                                 // 清空
        unsigned int getEmitterCount() const;                                        // 数量
        const ParticleEmitterTransformDesc& getNextEmitterTransformDesc(unsigned int index);  // 按索引取
    };

    // ★核心：一次交换的数据总容器。所有字段都是 optional = 只有"变了"才填
    struct RenderSwapData
    {
        std::optional<LevelResourceDesc>       m_level_resource_desc;         // 关卡资源(切换关卡时填)
        std::optional<GameObjectResourceDesc>  m_game_object_resource_desc;   // 新增/更新物体(队列)
        std::optional<GameObjectResourceDesc>  m_game_object_to_delete;       // 待删除物体(队列)
        std::optional<CameraSwapData>          m_camera_swap_data;            // 相机更新
        std::optional<ParticleSubmitRequest>   m_particle_submit_request;     // 粒子提交
        std::optional<EmitterTickRequest>      m_emitter_tick_request;        // 粒子 tick
        std::optional<EmitterTransformRequest> m_emitter_transform_request;   // 粒子变换更新

        void addDirtyGameObject(GameObjectDesc&& desc);      // 逻辑层：标记物体脏(新增/更新)
        void addDeleteGameObject(GameObjectDesc&& desc);     // 逻辑层：标记物体删除
        void addNewParticleEmitter(ParticleEmitterDesc& desc);      // 逻辑层：提交新发射器
        void addTickParticleEmitter(ParticleEmitterID id);         // 逻辑层：请求 tick 发射器
        void updateParticleTransform(ParticleEmitterTransformDesc& desc);  // 逻辑层：更新发射器变换
    };

    // 缓冲槽索引枚举：0=逻辑槽，1=渲染槽
    enum SwapDataType : uint8_t
    {
        LogicSwapDataType = 0,  // 逻辑层写的那块
        RenderSwapDataType,     // 渲染层读的那块
        SwapDataTypeCount       // 总数=2
    };

    // ★核心类：逻辑层与渲染层之间的双缓冲桥
    class RenderSwapContext
    {
    public:
        RenderSwapData& getLogicSwapData();    // 逻辑层拿它写数据
        RenderSwapData& getRenderSwapData();   // 渲染层拿它读数据
        void            swapLogicRenderData(); // 尝试交换(消费完才换)
        void            resetLevelRsourceSwapData();        // 清空关卡资源字段
        void            resetGameObjectResourceSwapData();  // 清空物体新增字段
        void            resetGameObjectToDelete();         // 清空物体删除字段
        void            resetCameraSwapData();             // 清空相机字段
        void            resetPartilceBatchSwapData();      // 清空粒子提交字段
        void            resetEmitterTickSwapData();        // 清空粒子 tick 字段
        void            resetEmitterTransformSwapData();   // 清空粒子变换字段

    private:
        uint8_t        m_logic_swap_data_index {LogicSwapDataType};    // 逻辑当前写哪块(初始0)
        uint8_t        m_render_swap_data_index {RenderSwapDataType};  // 渲染当前读哪块(初始1)
        RenderSwapData m_swap_data[SwapDataTypeCount];                 // 两块缓冲(固定数组，非动态分配)

        bool isReadyToSwap() const;  // 渲染槽是否已被消费空(可交换?)
        void swap();                 // 交换两块索引(并清空渲染侧)
    };
} // namespace Piccolo

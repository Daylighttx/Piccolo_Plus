#include "runtime/function/render/render_swap_context.h"  // 自身头文件

#include <utility>  // std::swap / std::move

namespace Piccolo
{
    // —— GameObjectResourceDesc 的成员实现 ——

    // 入队：把一个物体描述加到队尾
    void GameObjectResourceDesc::add(GameObjectDesc& desc) { m_game_object_descs.push_back(desc); }

    // 队列是否为空
    bool GameObjectResourceDesc::isEmpty() const { return m_game_object_descs.empty(); }

    // 取队首元素(下一个要处理的)，不弹出
    GameObjectDesc& GameObjectResourceDesc::getNextProcessObject() { return m_game_object_descs.front(); }

    // 弹出队首(渲染层消费完后调用)
    void GameObjectResourceDesc::pop() { m_game_object_descs.pop_front(); }

    // —— ParticleSubmitRequest 的成员实现 ——

    // 加入一个发射器描述
    void ParticleSubmitRequest::add(ParticleEmitterDesc& desc) { m_emitter_descs.push_back(desc); }

    // 发射器数量
    unsigned int ParticleSubmitRequest::getEmitterCount() const { return m_emitter_descs.size(); }

    // 按索引取发射器描述
    const ParticleEmitterDesc& ParticleSubmitRequest::getEmitterDesc(unsigned int index)
    {
        return m_emitter_descs[index];
    }

    // —— EmitterTransformRequest 的成员实现 ——

    // 加入一个变换描述
    void EmitterTransformRequest::add(ParticleEmitterTransformDesc& desc) { m_transform_descs.push_back(desc); }

    // 变换数量
    unsigned int EmitterTransformRequest::getEmitterCount() const { return m_transform_descs.size(); }

    // 按索引取变换描述
    const ParticleEmitterTransformDesc& EmitterTransformRequest::getNextEmitterTransformDesc(unsigned int index)
    {
        return m_transform_descs[index];
    }

    // —— RenderSwapContext 的核心实现 ——

    // 返回逻辑层当前应写的缓冲
    RenderSwapData& RenderSwapContext::getLogicSwapData() { return m_swap_data[m_logic_swap_data_index]; }

    // 返回渲染层当前应读的缓冲
    RenderSwapData& RenderSwapContext::getRenderSwapData() { return m_swap_data[m_render_swap_data_index]; }

    // 尝试交换：只有渲染侧消费空了才真正 swap
    void RenderSwapContext::swapLogicRenderData()
    {
        if (isReadyToSwap())  // 渲染槽里还有未消费数据就不换(防止覆盖)
        {
            swap();
        }
    }

    // 判断是否可交换：渲染槽的所有 optional 字段都为空 = 已消费完
    bool RenderSwapContext::isReadyToSwap() const
    {
        return !(m_swap_data[m_render_swap_data_index].m_level_resource_desc.has_value() ||
                 m_swap_data[m_render_swap_data_index].m_game_object_resource_desc.has_value() ||
                 m_swap_data[m_render_swap_data_index].m_game_object_to_delete.has_value() ||
                 m_swap_data[m_render_swap_data_index].m_camera_swap_data.has_value() ||
                 m_swap_data[m_render_swap_data_index].m_particle_submit_request.has_value() ||
                 m_swap_data[m_render_swap_data_index].m_emitter_tick_request.has_value() ||
                 m_swap_data[m_render_swap_data_index].m_emitter_transform_request.has_value());
    }

    // 清空关卡资源字段(在渲染槽上)
    void RenderSwapContext::resetLevelRsourceSwapData()
    {
        m_swap_data[m_render_swap_data_index].m_level_resource_desc.reset();
    }

    // 清空物体新增/更新字段
    void RenderSwapContext::resetGameObjectResourceSwapData()
    {
        m_swap_data[m_render_swap_data_index].m_game_object_resource_desc.reset();
    }

    // 清空物体删除字段
    void RenderSwapContext::resetGameObjectToDelete()
    {
        m_swap_data[m_render_swap_data_index].m_game_object_to_delete.reset();
    }

    // 清空粒子提交字段
    void RenderSwapContext::resetPartilceBatchSwapData()
    {
        m_swap_data[m_render_swap_data_index].m_particle_submit_request.reset();
    }

    // 清空相机字段
    void RenderSwapContext::resetCameraSwapData() { m_swap_data[m_render_swap_data_index].m_camera_swap_data.reset(); }

    // 清空粒子 tick 字段
    void RenderSwapContext::resetEmitterTickSwapData()
    {
        m_swap_data[m_render_swap_data_index].m_emitter_tick_request.reset();
    }

    // 清空粒子变换字段
    void RenderSwapContext::resetEmitterTransformSwapData()
    {
        m_swap_data[m_render_swap_data_index].m_emitter_transform_request.reset();
    }

    // 真正交换：先清空渲染侧所有字段，再交换两个索引
    void RenderSwapContext::swap()
    {
        resetLevelRsourceSwapData();
        resetGameObjectResourceSwapData();
        resetGameObjectToDelete();
        resetCameraSwapData();
        resetEmitterTickSwapData();
        resetEmitterTransformSwapData();
        resetPartilceBatchSwapData();
        std::swap(m_logic_swap_data_index, m_render_swap_data_index);  // 索引互换：逻辑/渲染指向的块对调
    }

    // —— RenderSwapData 的辅助写入函数(逻辑层调用) ——

    // 标记物体脏(新增或更新)：懒初始化后入队
    void RenderSwapData::addDirtyGameObject(GameObjectDesc&& desc)
    {
        if (m_game_object_resource_desc.has_value())
        {
            m_game_object_resource_desc->add(desc);  // 已有容器直接加
        }
        else
        {
            GameObjectResourceDesc go_descs;
            go_descs.add(desc);                       // 首次：建容器再加
            m_game_object_resource_desc = go_descs;
        }
    }

    // 标记物体删除：同上，入"待删除"队列
    void RenderSwapData::addDeleteGameObject(GameObjectDesc&& desc)
    {
        if (m_game_object_to_delete.has_value())
        {
            m_game_object_to_delete->add(desc);
        }
        else
        {
            GameObjectResourceDesc go_descs;
            go_descs.add(desc);
            m_game_object_to_delete = go_descs;
        }
    }

    // 提交新粒子发射器
    void RenderSwapData::addNewParticleEmitter(ParticleEmitterDesc& desc)
    {
        if (m_particle_submit_request.has_value())
        {
            m_particle_submit_request->add(desc);
        }
        else
        {
            ParticleSubmitRequest request;
            request.add(desc);
            m_particle_submit_request = request;
        }
    }

    // 请求 tick 某个发射器
    void RenderSwapData::addTickParticleEmitter(ParticleEmitterID id)
    {
        if (m_emitter_tick_request.has_value())
        {
            m_emitter_tick_request->m_emitter_indices.push_back(id);
        }
        else
        {
            EmitterTickRequest request;
            request.m_emitter_indices.push_back(id);
            m_emitter_tick_request = request;
        }
    }

    // 更新粒子发射器变换
    void RenderSwapData::updateParticleTransform(ParticleEmitterTransformDesc& desc)
    {
        if (m_emitter_transform_request.has_value())
        {
            m_emitter_transform_request->add(desc);
        }
        else
        {
            EmitterTransformRequest request;
            request.add(desc);
            m_emitter_transform_request = request;
        }
    }
} // namespace Piccolo

#pragma once

#include "runtime/core/math/vector3.h"   // 三维向量
#include "runtime/function/render/render_type.h"  // 渲染基础类型(BufferData 等)

#include <vector>   // 动态数组

namespace Piccolo
{
    // 点光源
    struct PointLight
    {
        Vector3 m_position;  // 位置
        Vector3 m_flux;      // 光通量(单位 W)

        // 计算光照影响半径(用于剔除)：衰减到阈值处的距离
        float calculateRadius() const
        {
            const float INTENSITY_CUTOFF    = 1.0f;    // 强度下限
            const float ATTENTUATION_CUTOFF = 0.05f;   // 衰减阈值
            Vector3     intensity           = m_flux / (4.0f * Math_PI);  // 各方向平均强度
            float       maxIntensity        = Vector3::getMaxElement(intensity);  // 最大分量
            // 由阈值反推衰减系数，再求半径
            float       attenuation = Math::max(INTENSITY_CUTOFF, ATTENTUATION_CUTOFF * maxIntensity) / maxIntensity;
            return 1.0f / sqrtf(attenuation);
        }
    };

    // 环境光(全局辐照度)
    struct AmbientLight
    {
        Vector3 m_irradiance;  // 环境辐照度
    };

    // 平行光(方向光)
    struct PDirectionalLight
    {
        Vector3 m_direction;  // 方向(指向光源)
        Vector3 m_color;      // 颜色/强度
    };

    // 光照列表基类(仅含 GPU 上传用的顶点结构)
    struct LightList
    {
        // 点光顶点：16 字节对齐(配合 GPU std140 布局)
        struct PointLightVertex
        {
            Vector3 m_position;  // 位置
            float   m_padding;   // 填充对齐
            Vector3 m_intensity;  // 辐射强度(W/sr)
            float   m_radius;    // 影响半径
        };
    };

    // 点光列表(继承自 LightList，拿到 PointLightVertex 定义)
    class PointLightList : public LightList
    {
    public:
        void init() {}      // 初始化(当前空)
        void shutdown() {}  // 释放(当前空)
        void update() {}    // 上传到 GPU(当前空，待实现)

        std::vector<PointLight> m_lights;               // 点光数组(由 updateLights() 填充)
        std::shared_ptr<BufferData> m_buffer;           // 对应的 GPU 缓冲
    };

} // namespace Piccolo

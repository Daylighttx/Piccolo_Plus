#pragma once
#include "runtime/function/framework/component/component.h"
#include "runtime/core/math/vector3.h"
#include <vector>

namespace Piccolo
{
    // 仅在头文件中前向声明，避免把 TransformComponent 的完整定义拉进这里
    class TransformComponent;

    REFLECTION_TYPE(LightComponent)
    CLASS(LightComponent : public Component, WhiteListFields)
    {
        REFLECTION_BODY(LightComponent)
    public:
        LightComponent() { registerLight(); }
        ~LightComponent() override { unregisterLight(); }

        // === 反射字段访问器（供 render_system 取灯的参数）===
        int            getLightType() const { return m_light_type; } // 0=Point, 1=Directional
        const Vector3& getColor()     const { return m_color; }
        float          getIntensity() const { return m_intensity; }
        float          getRange()     const { return m_range; }

        // 通过所属对象的 TransformComponent 取世界坐标（Component 基类持有 m_parent_object）
        Vector3 getWorldPosition() const;

        // === 全局灯光注册表：组件构造时登记自己，渲染端每帧收集 ===
        void registerLight();
        void unregisterLight();
        static const std::vector<LightComponent*>& getAllLights() { return s_lights; }

    protected:
        // 用 int 而非 enum，反射/序列化对枚举支持不确定，int 最稳
        META(Enable) int     m_light_type {0};          // 0=Point 1=Directional
        META(Enable) Vector3 m_color      {1.f, 1.f, 1.f};
        META(Enable) float   m_intensity  {10.f};
        META(Enable) float   m_range      {20.f};

    private:
        // 静态注册表：所有 LightComponent 实例的指针集合
        inline static std::vector<LightComponent*> s_lights;
    };
} // namespace Piccolo

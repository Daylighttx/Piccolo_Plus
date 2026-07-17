#include "runtime/function/framework/component/light/light_component.h"

#include "runtime/function/framework/object/object.h"
#include "runtime/function/framework/component/transform/transform_component.h"

#include <algorithm>

namespace Piccolo
{
    void LightComponent::registerLight()
    {
        if (std::find(s_lights.begin(), s_lights.end(), this) == s_lights.end())
            s_lights.push_back(this);
    }

    void LightComponent::unregisterLight()
    {
        auto it = std::find(s_lights.begin(), s_lights.end(), this);
        if (it != s_lights.end())
            s_lights.erase(it);
    }

    Vector3 LightComponent::getWorldPosition() const
    {
        auto obj = m_parent_object.lock();
        if (obj)
        {
            const auto* tc = obj->tryGetComponentConst(TransformComponent);
            if (tc)
                return tc->getPosition();
        }
        return Vector3(0.f, 0.f, 0.f);
    }
} // namespace Piccolo

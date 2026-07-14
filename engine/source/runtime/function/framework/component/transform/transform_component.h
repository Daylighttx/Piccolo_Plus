#pragma once

#include "runtime/core/math/matrix4.h"
#include "runtime/core/math/transform.h"

#include "runtime/function/framework/component/component.h"
#include "runtime/function/framework/object/object.h"

namespace Piccolo
{
    REFLECTION_TYPE(TransformComponent)
    CLASS(TransformComponent : public Component, WhiteListFields)
    {
        REFLECTION_BODY(TransformComponent)

    public:
        TransformComponent() = default;

        void postLoadResource(std::weak_ptr<GObject> parent_object) override;

        Vector3    getPosition() const { return m_transform_buffer[m_current_index].m_position; }
        Vector3    getScale() const { return m_transform_buffer[m_current_index].m_scale; }
        Quaternion getRotation() const { return m_transform_buffer[m_current_index].m_rotation; }

        void setPosition(const Vector3& new_translation);

        void setScale(const Vector3& new_scale);

        void setRotation(const Quaternion& new_rotation);

        const Transform& getTransformConst() const { return m_transform_buffer[m_current_index]; }
        Transform&       getTransform() { return m_transform_buffer[m_next_index]; }

        Matrix4x4 getMatrix() const { return m_transform_buffer[m_current_index].getMatrix(); }

        void tick(float delta_time) override;

        void tryUpdateRigidBodyComponent();

    protected:
        META(Enable)
        Transform m_transform;

        // === 阶段二 2.1 反射实验字段 ===
        // 加一行 META(Enable) 就能让反射系统认识这个字段：
        // 跑一次 PiccoloParser 后，会自动生成 get_m_world_matrix/set_m_world_matrix
        // 并 REGISTER_FIELD_TO_MAP 到 TransformComponent 的 TypeMeta 表里。
        // reflection.h 一个字都不用改。
        META(Enable)
        Matrix4x4_ m_world_matrix;

        Transform m_transform_buffer[2];
        size_t    m_current_index {0};
        size_t    m_next_index {1};
    };
} // namespace Piccolo

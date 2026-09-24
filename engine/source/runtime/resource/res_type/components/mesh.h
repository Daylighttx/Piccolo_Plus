#pragma once
#include "runtime/core/math/transform.h"
#include "runtime/core/meta/reflection/reflection.h"

namespace Piccolo
{
    REFLECTION_TYPE(SubMeshRes)
    CLASS(SubMeshRes, Fields)
    {
        REFLECTION_BODY(SubMeshRes);

    public:
        std::string m_obj_file_ref;
        Transform   m_transform;
        std::string m_material;
        bool        m_is_toon_character {false};
        float       m_toon_outline_width {0.025f};
        Vector4     m_toon_outline_color {0.015f, 0.02f, 0.03f, 1.0f};
    };

    REFLECTION_TYPE(MeshComponentRes)
    CLASS(MeshComponentRes, Fields)
    {
        REFLECTION_BODY(MeshComponentRes);

    public:
        std::vector<SubMeshRes> m_sub_meshes;
    };
} // namespace Piccolo

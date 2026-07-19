#pragma once

#include "runtime/core/math/axis_aligned.h"  // AABB 包围盒
#include "runtime/core/math/matrix4.h"        // 4x4 矩阵

#include <cstdint>   // 定宽整数
#include <vector>    // 动态数组

namespace Piccolo
{
    // 渲染层视角的一个可渲染实例(一个 mesh + 一个材质 + 摆放)
    class RenderEntity
    {
    public:
        uint32_t  m_instance_id {0};        // 实例 ID(用于拾取/映射回 GameObject)
        Matrix4x4 m_model_matrix {Matrix4x4::IDENTITY};  // 模型矩阵(世界变换)

        // —— Mesh 部分 ——
        size_t                 m_mesh_asset_id {0};   // mesh 资源 ID
        bool                   m_enable_vertex_blending {false};  // 是否骨骼蒙皮(顶点混合)
        std::vector<Matrix4x4> m_joint_matrices;       // 关节矩阵(蒙皮用)
        AxisAlignedBox         m_bounding_box;         // 包围盒(用于视锥剔除)

        // —— Material 部分 ——
        size_t  m_material_asset_id {0};   // 材质资源 ID
        bool    m_blend {false};           // 是否透明混合
        bool    m_double_sided {false};    // 是否双面渲染
        Vector4 m_base_color_factor {1.0f, 1.0f, 1.0f, 1.0f};  // 基础色系数
        float   m_metallic_factor {1.0f};      // 金属度
        float   m_roughness_factor {1.0f};     // 粗糙度
        float   m_normal_scale {1.0f};         // 法线贴图强度
        float   m_occlusion_strength {1.0f};  // 环境遮蔽强度
        Vector3 m_emissive_factor {0.0f, 0.0f, 0.0f};  // 自发光系数
    };
} // namespace Piccolo

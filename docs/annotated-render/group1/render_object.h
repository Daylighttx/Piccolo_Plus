#pragma once

#include "runtime/core/math/matrix4.h"   // 矩阵
#include "runtime/function/framework/object/object_id_allocator.h"  // GameObject ID 分配器

#include <string>   // 字符串(资源路径)
#include <vector>   // 动态数组

namespace Piccolo
{
    // 物体某部件的 Mesh 描述(逻辑层/可序列化)
    REFLECTION_TYPE(GameObjectMeshDesc)
    STRUCT(GameObjectMeshDesc, Fields)
    {
        REFLECTION_BODY(GameObjectMeshDesc)
        std::string m_mesh_file;  // mesh 文件路径
    };

    // 骨骼绑定描述
    REFLECTION_TYPE(SkeletonBindingDesc)
    STRUCT(SkeletonBindingDesc, Fields)
    {
        REFLECTION_BODY(SkeletonBindingDesc)
        std::string m_skeleton_binding_file;  // 骨骼绑定文件路径
    };

    // 单根骨骼动画结果(一个变换矩阵)
    REFLECTION_TYPE(SkeletonAnimationResultTransform)
    STRUCT(SkeletonAnimationResultTransform, WhiteListFields)
    {
        REFLECTION_BODY(SkeletonAnimationResultTransform)
        Matrix4x4 m_matrix;  // 该骨骼的变换
    };

    // 骨骼动画整体结果(所有骨骼变换)
    REFLECTION_TYPE(SkeletonAnimationResult)
    STRUCT(SkeletonAnimationResult, Fields)
    {
        REFLECTION_BODY(SkeletonAnimationResult)
        std::vector<SkeletonAnimationResultTransform> m_transforms;  // 各骨骼变换列表
    };

    // 物体材质描述(5 张贴图路径 + 是否带贴图)
    REFLECTION_TYPE(GameObjectMaterialDesc)
    STRUCT(GameObjectMaterialDesc, Fields)
    {
        REFLECTION_BODY(GameObjectMaterialDesc)
        std::string m_base_color_texture_file;         // 基础色贴图
        std::string m_metallic_roughness_texture_file; // 金属-粗糙贴图
        std::string m_normal_texture_file;             // 法线贴图
        std::string m_occlusion_texture_file;          // 环境遮蔽贴图
        std::string m_emissive_texture_file;           // 自发光贴图
        bool        m_with_texture {false};            // 是否使用贴图(否则用纯色系数)
    };

    // 物体变换描述
    REFLECTION_TYPE(GameObjectTransformDesc)
    STRUCT(GameObjectTransformDesc, WhiteListFields)
    {
        REFLECTION_BODY(GameObjectTransformDesc)
        Matrix4x4 m_transform_matrix {Matrix4x4::IDENTITY};  // 局部变换矩阵
    };

    // 一个"部件"的完整描述 = mesh + 材质 + 变换 + 可选动画
    REFLECTION_TYPE(GameObjectPartDesc)
    STRUCT(GameObjectPartDesc, Fields)
    {
        REFLECTION_BODY(GameObjectPartDesc)
        GameObjectMeshDesc      m_mesh_desc;            // 该部件的 mesh
        GameObjectMaterialDesc  m_material_desc;        // 该部件的材质
        GameObjectTransformDesc m_transform_desc;       // 该部件的变换
        bool                    m_with_animation {false};     // 是否带动画
        SkeletonBindingDesc     m_skeleton_binding_desc;     // 骨骼绑定
        SkeletonAnimationResult m_skeleton_animation_result; // 动画结果
    };

    // 无效部件 ID 常量
    constexpr size_t k_invalid_part_id = std::numeric_limits<size_t>::max();

    // 部件 ID = (GameObject ID, 部件序号) 组合键
    struct GameObjectPartId
    {
        GObjectID m_go_id {k_invalid_gobject_id};  // 所属 GameObject
        size_t    m_part_id {k_invalid_part_id};   // 部件序号

        bool operator==(const GameObjectPartId& rhs) const  // 相等比较
        { return m_go_id == rhs.m_go_id && m_part_id == rhs.m_part_id; }
        size_t getHashValue() const { return m_go_id ^ (m_part_id << 1); }  // 哈希值(供 unordered_map)
        bool   isValid() const { return m_go_id != k_invalid_gobject_id && m_part_id != k_invalid_part_id; }  // 是否有效
    };

    // 一个游戏对象描述 = ID + 若干部件
    class GameObjectDesc
    {
    public:
        GameObjectDesc() : m_go_id(0) {}
        GameObjectDesc(size_t go_id, const std::vector<GameObjectPartDesc>& parts) :
            m_go_id(go_id), m_object_parts(parts)
        {}

        GObjectID                              getId() const { return m_go_id; }                  // 取对象 ID
        const std::vector<GameObjectPartDesc>& getObjectParts() const { return m_object_parts; } // 取部件列表

    private:
        GObjectID                       m_go_id {k_invalid_gobject_id};  // 对象 ID
        std::vector<GameObjectPartDesc> m_object_parts;                 // 部件列表
    };
} // namespace Piccolo

// 为 GameObjectPartId 特化 std::hash，使其可作 unordered 容器键
template<>
struct std::hash<Piccolo::GameObjectPartId>
{
    size_t operator()(const Piccolo::GameObjectPartId& rhs) const noexcept { return rhs.getHashValue(); }
};

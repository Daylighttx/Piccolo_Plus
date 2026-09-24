#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"
#include "structures.h"

struct DirectionalLight
{
    vec3  direction;
    float _padding_direction;
    vec3  color;
    float _padding_color;
};

struct PointLight
{
    vec3  position;
    float radius;
    vec3  intensity;
    float _padding_intensity;
};

layout(set = 0, binding = 0) readonly buffer _unused_name_perframe
{
    mat4             proj_view_matrix;
    vec3             camera_position;
    float            _padding_camera_position;
    vec3             ambient_light;
    float            _padding_ambient_light;
    uint             point_light_num;
    uint             _padding_point_light_num_1;
    uint             _padding_point_light_num_2;
    uint             _padding_point_light_num_3;
    PointLight       scene_point_lights[m_max_point_light_count];
    DirectionalLight scene_directional_light;
    highp mat4       directional_light_proj_view;
};

layout(set = 0, binding = 1) readonly buffer _unused_name_per_drawcall
{
    VulkanMeshInstance mesh_instances[m_mesh_per_drawcall_max_instance_count];
};

layout(set = 0, binding = 2) readonly buffer _unused_name_per_drawcall_vertex_blending
{
    highp mat4 joint_matrices[m_mesh_vertex_blending_max_joint_count * m_mesh_per_drawcall_max_instance_count];
};

layout(set = 1, binding = 0) readonly buffer _unused_name_per_mesh_joint_binding
{
    VulkanMeshVertexJointBinding indices_and_weights[];
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(location = 0) out flat highp vec4 out_outline_color;

void main()
{
    highp mat4  model_matrix           = mesh_instances[gl_InstanceIndex].model_matrix;
    highp float enable_vertex_blending = mesh_instances[gl_InstanceIndex].enable_vertex_blending;
    highp float outline_width          = mesh_instances[gl_InstanceIndex].toon_outline_width;
    out_outline_color                  = mesh_instances[gl_InstanceIndex].toon_outline_color;

    highp vec3 model_position = in_position;
    highp vec3 model_normal   = in_normal;

    if (enable_vertex_blending > 0.0)
    {
        highp ivec4 in_indices = indices_and_weights[gl_VertexIndex].indices;
        highp vec4  in_weights = indices_and_weights[gl_VertexIndex].weights;

        highp mat4 vertex_blending_matrix = mat4x4(0.0);

        if (in_weights.x > 0.0 && in_indices.x > 0)
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.x] * in_weights.x;
        if (in_weights.y > 0.0 && in_indices.y > 0)
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.y] * in_weights.y;
        if (in_weights.z > 0.0 && in_indices.z > 0)
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.z] * in_weights.z;
        if (in_weights.w > 0.0 && in_indices.w > 0)
            vertex_blending_matrix +=
                joint_matrices[m_mesh_vertex_blending_max_joint_count * gl_InstanceIndex + in_indices.w] * in_weights.w;

        model_position = (vertex_blending_matrix * vec4(in_position, 1.0)).xyz;
        highp mat3 skin_normal_matrix = mat3(vertex_blending_matrix);
        model_normal = normalize(skin_normal_matrix * in_normal);
    }

    highp vec3 world_position = (model_matrix * vec4(model_position, 1.0)).xyz;
    highp vec3 world_normal   = normalize(mat3(model_matrix) * model_normal);
    world_position += world_normal * outline_width;

    gl_Position = proj_view_matrix * vec4(world_position, 1.0);
}

struct VulkanMeshInstance
{
    highp float enable_vertex_blending;
    highp float toon_outline_width;
    highp float _padding_enable_vertex_blending_2;
    highp float _padding_enable_vertex_blending_3;
    highp mat4  model_matrix;
    highp vec4  toon_outline_color;
};

struct VulkanMeshVertexJointBinding
{
    highp ivec4 indices;
    highp vec4  weights;
};

#version 310 es

layout(location = 0) out highp vec4 out_scene_color;
layout(location = 0) in flat highp vec4 in_outline_color;

void main()
{
    out_scene_color = in_outline_color;
}

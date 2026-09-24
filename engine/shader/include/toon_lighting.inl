// A06 baseline cel lighting: quantize one directional Lambert term, then apply the existing hard shadow.
highp vec3  toon_L                   = normalize(scene_directional_light.direction);
highp float toon_NoL                 = max(dot(N, toon_L), 0.0);
highp float toon_diffuse_band        = 0.0;

if (toon_NoL >= 0.70)
{
    toon_diffuse_band = 1.0;
}
else if (toon_NoL >= 0.30)
{
    toon_diffuse_band = 0.55;
}

highp float toon_shadow = 1.0;
if (toon_diffuse_band > 0.0)
{
    highp vec4 toon_position_clip = directional_light_proj_view * vec4(in_world_position, 1.0);
    highp vec3 toon_position_ndc  = toon_position_clip.xyz / toon_position_clip.w;
    highp vec2 toon_shadow_uv     = ndcxy_to_uv(toon_position_ndc.xy);
    highp float toon_closest_depth = texture(directional_light_shadow, toon_shadow_uv).r + 0.000075;
    toon_shadow = toon_closest_depth >= toon_position_ndc.z ? 1.0 : 0.0;
}

result_color = basecolor * (ambient_light + scene_directional_light.color * toon_diffuse_band * toon_shadow);

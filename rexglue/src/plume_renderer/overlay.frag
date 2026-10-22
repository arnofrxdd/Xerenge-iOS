#version 450

// The picture as it is, faded by the opacity (overlay.vert).

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

layout(set = 0, binding = 0) uniform texture2D g_picture;
layout(set = 0, binding = 1) uniform sampler g_sampler;

layout(push_constant) uniform Params {
    vec4 rect;
    float opacity;
} u;

void main() {
    vec4 colour = texture(sampler2D(g_picture, g_sampler), v_uv);
    o_color = vec4(colour.rgb, colour.a * u.opacity);
}

#version 450

// A picture laid over the frame (plume_overlay.cpp): one rectangle, from the
// vertex index alone, where the push constants put it. rect is in clip space,
// left, top, right, bottom; v_uv runs 0..1 across the picture.

layout(push_constant) uniform Params {
    vec4 rect;
    float opacity;
} u;

layout(location = 0) out vec2 v_uv;

void main() {
    const vec2 corners[6] = vec2[6](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
                                    vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
    vec2 corner = corners[gl_VertexIndex];
    v_uv = corner;
    gl_Position = vec4(mix(u.rect.xy, u.rect.zw, corner), 0.0, 1.0);
}

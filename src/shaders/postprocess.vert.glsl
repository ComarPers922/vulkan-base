#version 460
layout(row_major) uniform;

layout(location=0) in vec4 in_position;
layout(location=1) in vec2 in_uv;
layout(location = 0) out vec2 frag_uv;

void main() {
    frag_uv = in_uv;
    gl_Position = in_position;
}

#version 460
layout(row_major) uniform;

layout(location=0) in vec4 in_position;
layout(location=1) in vec2 in_uv;
layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 clipPos;
layout(location = 2) out vec4 history_clipPos;

layout(std140, binding=0) uniform Uniform_Block {
    mat4x4 view_proj;
    mat4x4 history_view_proj;
};

layout(std140, set=1,binding=1) uniform ModelMatrices
{
    mat4x4 currentModelMat;
    mat4x4 previousModelMat;
};

void main() {
    frag_uv = in_uv;
    gl_Position = view_proj * currentModelMat * in_position;

    history_clipPos = history_view_proj * previousModelMat * in_position;
    // history_clipPos = gl_Position;
    clipPos = gl_Position;
}

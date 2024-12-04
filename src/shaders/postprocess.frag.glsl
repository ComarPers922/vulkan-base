#version 460
layout(row_major) uniform;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 color_attachment0;

layout(binding=1) uniform texture2D input_image;
layout(binding=2) uniform sampler input_image_sampler;

void main() {
    color_attachment0 = texture(sampler2D(input_image, input_image_sampler), frag_uv);
}

#version 460
layout(row_major) uniform;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 clipPos;
layout(location = 2) in vec4 history_clipPos;

layout(location = 0) out vec4 color_attachment0;
layout(location = 1) out vec4 motion_vec_attachment;

layout(binding=1) uniform texture2D image;
layout(binding=2) uniform sampler image_sampler;

void main() {
    color_attachment0 = texture(sampler2D(image, image_sampler), frag_uv);

    // vec2 offset = clipPos.xy / clipPos.w - history_clipPos.xy / history_clipPos.w;
    // vec2 offset = clipPos.xy / clipPos.w;
    vec2 offset = history_clipPos.xy;// / history_clipPos.w;
    offset = (offset + 1.) * 0.5;
        
    // (clipPos.xy / clipPos.w + 1.) * 0.5 - (history_clipPos.xy / history_clipPos.w + 1.) * 0.5;

    motion_vec_attachment = vec4(offset, vec2(0.));
    motion_vec_attachment = (history_clipPos + 1.) * 0.5;
}

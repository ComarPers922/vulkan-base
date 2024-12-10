#version 460
layout(row_major) uniform;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 color_attachment0;

layout(binding=1) uniform texture2D input_image;
layout(binding=2) uniform sampler input_image_sampler;
layout(binding=3) uniform texture2D prev_frame_image;

#define texSampler sampler2D(input_image, input_image_sampler)
#define texSamplerPrev sampler2D(prev_frame_image, input_image_sampler)

layout( push_constant ) uniform constants
{
    int enableFXAA;
    float threshold;
} PushConstants;


void main() 
{
    if (PushConstants.enableFXAA == 0)
    {
        color_attachment0 = texture(texSampler, frag_uv);
        return;
    }
    vec2 texSize = textureSize(texSampler, 0);
    vec2 pixelSize = 1.0 / texSize;

    // Sample the four neighboring pixels
    vec3 colorCenter = texture(texSampler, frag_uv).rgb;
    vec3 colorLeft   = texture(texSampler, frag_uv + vec2(-pixelSize.x, 0.0)).rgb;
    vec3 colorRight  = texture(texSampler, frag_uv + vec2(pixelSize.x, 0.0)).rgb;
    vec3 colorUp     = texture(texSampler, frag_uv + vec2(0.0, pixelSize.y)).rgb;
    vec3 colorDown   = texture(texSampler, frag_uv + vec2(0.0, -pixelSize.y)).rgb;

        // Compute luminance (perceived brightness)
    float lumCenter = dot(colorCenter, vec3(0.299, 0.587, 0.114));
    float lumLeft   = dot(colorLeft,   vec3(0.299, 0.587, 0.114));
    float lumRight  = dot(colorRight,  vec3(0.299, 0.587, 0.114));
    float lumUp     = dot(colorUp,     vec3(0.299, 0.587, 0.114));
    float lumDown   = dot(colorDown,   vec3(0.299, 0.587, 0.114));

    // Calculate the luminance difference
    float edgeHorizontal = abs(lumLeft - lumRight);
    float edgeVertical   = abs(lumUp - lumDown);

        // Combine edge detections
    float edgeStrength = max(edgeHorizontal, edgeVertical);

    // Apply FXAA if an edge is detected
    if (edgeStrength > PushConstants.threshold) {
        vec3 averageColor = (colorLeft + colorRight + colorUp + colorDown) / 4.0;
        color_attachment0 = vec4(mix(colorCenter, averageColor, 0.75), 1.0);
    } else {
        color_attachment0 = vec4(colorCenter, 1.0);
    }

    // color_attachment0 = texture(texSampler, frag_uv);
    // color_attachment0 *= vec4(1. , 0., 0., 1.);
}

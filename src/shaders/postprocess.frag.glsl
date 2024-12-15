#version 460
layout(row_major) uniform;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 color_attachment0;

layout(binding=1) uniform texture2D input_image;
layout(binding=2) uniform sampler linear_input_image_sampler;
layout(binding=3) uniform texture2D prev_frame_image;
layout(binding=4) uniform sampler nearest_input_image_sampler;
layout(binding=5) uniform texture2D motion_vec_image;

#define texSampler sampler2D(input_image, nearest_input_image_sampler)
#define texSamplerPrev sampler2D(prev_frame_image, linear_input_image_sampler)
#define texSamplerMotion sampler2D(motion_vec_image, nearest_input_image_sampler)

layout( push_constant ) uniform constants
{
    int aliasingOption;
    float threshold;
    uint frameIndex;
} PushConstants;

const vec2 haltonPoints[16] = vec2[16](
    vec2(0.5, 0.333333),
    vec2(0.25, 0.666667),
    vec2(0.75, 0.111111),
    vec2(0.125, 0.444444),
    vec2(0.625, 0.777778),
    vec2(0.375, 0.222222),
    vec2(0.875, 0.555556),
    vec2(0.0625, 0.888889),
    vec2(0.5625, 0.037037),
    vec2(0.3125, 0.37037),
    vec2(0.8125, 0.703704),
    vec2(0.1875, 0.148148),
    vec2(0.6875, 0.481481),
    vec2(0.4375, 0.814815),
    vec2(0.9375, 0.259259),
    vec2(0.03125, 0.592593)
);

float halton(int index, int base) {
    float result = 0.0;
    float f = 1.0 / float(base);
    int i = index;
    while (i > 0) {
        result += f * float(i % base);
        i = int(i / base);
        f /= float(base);
    }
    return result;
}
vec2 jitterOffset(int frameIndex) {
    return fract(vec2(halton(frameIndex, 2), halton(frameIndex, 3)) * 2.0 - 1.0);
}

void main() 
{
    if (PushConstants.aliasingOption == 0)
    {
        color_attachment0 = texture(texSampler, frag_uv);
        return;
    }

    vec2 texSize = textureSize(texSampler, 0);
    if (PushConstants.aliasingOption == 2)
    {
        // vec2 offset = haltonPoints[PushConstants.frameIndex % 16];
        vec2 offset = jitterOffset(int(PushConstants.frameIndex));
        offset = ((offset - 0.5) / texSize) * 2.;
        vec2 prevOffset = texture(texSamplerMotion, frag_uv).rg * 2. - 1.;
        vec4 prevColor = texture(texSamplerPrev, frag_uv + prevOffset);
        vec4 curColor = texture(texSampler, frag_uv + offset);

        color_attachment0 = mix(curColor, prevColor, 0.9);
        return;
    }

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

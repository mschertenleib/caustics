
precision highp float;

#ifdef COMPUTE_SHADER

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(rgba32f, binding = 0) uniform readonly restrict image2D accumulation_image;
layout(rgba8, binding = 5) uniform writeonly restrict image2D target_image;

#else

uniform sampler2D accumulation_texture;
out vec4 out_color;

#endif


// ACES tone mapping curve from Krzysztof Narkowicz
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACES_tone_map(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Khronos PBR neutral tone mapper from https://modelviewer.dev/examples/tone-mapping
// This should preserve well the hue of bright colors
vec3 PBR_neutral_tone_map(vec3 color)
{
    const float start_compression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < start_compression)
    {
        return color;
    }

    const float d = 1.0 - start_compression;
    float new_peak = 1.0 - d * d / (peak + d - start_compression);
    color *= new_peak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - new_peak) + 1.0);
    return mix(color, new_peak * vec3(1.0, 1.0, 1.0), g);
}

vec3 tone_map(vec3 color)
{
#if 0
    return PBR_neutral_tone_map(color);
#else
    return ACES_tone_map(color);
#endif
}

void main()
{
#ifdef COMPUTE_SHADER
    uvec2 image_size = imageSize(accumulation_image);
    if (gl_GlobalInvocationID.x >= image_size.x || gl_GlobalInvocationID.y >= image_size.y)
    {
        return;
    }

    vec4 color = imageLoad(accumulation_image, ivec2(gl_GlobalInvocationID.xy));
    color = vec4(tone_map(color.rgb), 1.0);
    imageStore(target_image, ivec2(gl_GlobalInvocationID.xy), color);
#else
    vec4 color = texelFetch(accumulation_texture, ivec2(gl_FragCoord.xy), 0);
    out_color = vec4(tone_map(color.rgb), 1.0);
#endif
}

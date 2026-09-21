
precision highp float;

uniform sampler2D accumulation_texture;

uniform float exposure_value;

out vec4 out_color;


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
#if 1
    return ACES_tone_map(color);
#elif 0
    return PBR_neutral_tone_map(color);
#elif 0
    return color / (1.0 + color);
#else
    return clamp(color, 0.0, 1.0);
#endif
}

vec3 sample_count_color(float sample_count)
{
    float scale = exp2(exposure_value) * 0.001;
    float t = clamp(sample_count * scale, 0.0, 1.0);
    
    const vec3 c0 = vec3(0.05, 0.10, 0.50); // blue
    const vec3 c1 = vec3(0.00, 0.75, 0.90); // cyan
    const vec3 c2 = vec3(0.10, 0.85, 0.20); // green
    const vec3 c3 = vec3(1.00, 0.90, 0.00); // yellow
    const vec3 c4 = vec3(0.90, 0.05, 0.02); // red
    if (t < 0.25)
    {
        return mix(c0, c1, t / 0.25);
    }
    else if (t < 0.50)
    {
        return mix(c1, c2, (t - 0.25) / 0.25);
    }
    else if (t < 0.75)
    {
        return mix(c2, c3, (t - 0.50) / 0.25);
    }
    else
    {
        return mix(c3, c4, (t - 0.75) / 0.25);
    }
}

void main()
{
    vec4 accum = texelFetch(accumulation_texture, ivec2(gl_FragCoord.xy), 0);
    vec3 color = accum.rgb / accum.a;
#if 1
    out_color = vec4(tone_map(color * exp2(exposure_value)), 1.0);
#else
    out_color = vec4(sample_count_color(accum.a), 1.0);
#endif
}

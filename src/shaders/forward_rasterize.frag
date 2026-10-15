precision highp float;


flat in vec2 start_px;
flat in vec2 end_px;
flat in vec2 start_world;
flat in vec2 end_world;
flat in float pixel_area;

out vec4 frag_color;


bool clip_axis(float p, float delta, float lo, float hi, inout float t_enter, inout float t_exit)
{
    if (abs(delta) < 1e-8)
    {
        return p >= lo && p < hi;
    }

    float inv_delta = 1.0 / delta;
    float t0 = (lo - p) * inv_delta;
    float t1 = (hi - p) * inv_delta;

    t_enter = max(t_enter, min(t0, t1));
    t_exit = min(t_exit, max(t0, t1));

    return t_enter <= t_exit;
}

void main()
{
    vec2 pixel_min = gl_FragCoord.xy - 0.5;
    vec2 pixel_max = gl_FragCoord.xy + 0.5;

    vec2 delta = end_px - start_px;

    float t_enter = 0.0;
    float t_exit = 1.0;

    if (!clip_axis(start_px.x, delta.x, pixel_min.x, pixel_max.x, t_enter, t_exit))
    {
        discard;
    }
    if (!clip_axis(start_px.y, delta.y, pixel_min.y, pixel_max.y, t_enter, t_exit))
    {
        discard;
    }

    float overlap_length = (t_exit - t_enter) * length(end_world - start_world);

    float weight = 1.0;
    float contribution = weight * overlap_length / pixel_area;
    frag_color = vec4(vec3(contribution), 1.0);
}


precision highp float;

in vec4 local;
in vec3 color;

out vec4 frag_color;

void main()
{
    float dist = length(local.xy);
    float thickness = local.z;
    float y_cutoff = local.w;
    float pixel_size = fwidth(dist);
    float alpha = clamp((1.0 - dist) / pixel_size, 0.0, 1.0)
        - clamp((1.0 - thickness + pixel_size - dist) / pixel_size, 0.0, 1.0);
    
    float radius = 1.0 - 0.5 * thickness;
    float x_cutoff = sqrt(radius * radius - y_cutoff * y_cutoff);
    vec2 p_cutoff_1 = vec2(-x_cutoff, y_cutoff);
    vec2 p_cutoff_2 = vec2(x_cutoff, y_cutoff);
    vec2 dir_cutoff_1 = vec2(-p_cutoff_1.y, p_cutoff_1.x);
    vec2 dir_cutoff_2 = vec2(p_cutoff_2.y, -p_cutoff_2.x);
    bool outside_cutoff_1 = dot(dir_cutoff_1, local.xy) > 0.0;
    bool outside_cutoff_2 = dot(dir_cutoff_2, local.xy) > 0.0;
    // NOTE: if y_cutoff >= 0, the cutoff region is a concave sector,
    // else it is a convex sector.
    if ((y_cutoff >= 0.0 && (outside_cutoff_1 || outside_cutoff_2)) ||
        (y_cutoff < 0.0 && outside_cutoff_1 && outside_cutoff_2))
    {
        float dist = min(distance(local.xy, p_cutoff_1), distance(local.xy, p_cutoff_2));
        alpha = clamp((0.5 * thickness - dist) / pixel_size, 0.0, 1.0);
    }
    frag_color = vec4(color, alpha);
}

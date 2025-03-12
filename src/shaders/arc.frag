
in vec4 local;
in vec3 color;

out vec4 frag_color;

void main()
{
    const float dist = length(local.xy);
    const float thickness = local.z;
    const float y_cutoff = local.w;
    const float pixel_size = fwidth(dist);
    float alpha = clamp((1.0 - dist) / pixel_size, 0.0, 1.0)
        - clamp((1.0 - thickness + pixel_size - dist) / pixel_size, 0.0, 1.0);
    
    const float radius = 1.0 - 0.5 * thickness;
    const float x_cutoff = sqrt(radius * radius - y_cutoff * y_cutoff);
    const vec2 p_cutoff_1 = vec2(-x_cutoff, y_cutoff);
    const vec2 p_cutoff_2 = vec2(x_cutoff, y_cutoff);
    const vec2 dir_cutoff_1 = vec2(-p_cutoff_1.y, p_cutoff_1.x);
    const vec2 dir_cutoff_2 = vec2(p_cutoff_2.y, -p_cutoff_2.x);
    const bool outside_cutoff_1 = dot(dir_cutoff_1, local.xy) > 0.0;
    const bool outside_cutoff_2 = dot(dir_cutoff_2, local.xy) > 0.0;
    // NOTE: if y_cutoff >= 0, the cutoff region is a concave sector,
    // else it is a convex sector.
    if ((y_cutoff >= 0.0 && (outside_cutoff_1 || outside_cutoff_2)) ||
        (y_cutoff < 0.0 && outside_cutoff_1 && outside_cutoff_2))
    {
        const float dist = min(distance(local.xy, p_cutoff_1), distance(local.xy, p_cutoff_2));
        alpha = clamp((0.5 * thickness - dist) / pixel_size, 0.0, 1.0);
    }
    frag_color = vec4(color, alpha);
}

#version 430

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
    if (local.y < y_cutoff)
    {
        // FIXME: we cannot actually have a hard cutoff line if we want rounded corners!
        /*const float radius = 1.0 - 0.5 * thickness;
        const float x_cutoff = sqrt(radius * radius - y_cutoff * y_cutoff);
        const vec2 p1_cutoff = vec2(-x_cutoff, y_cutoff);
        const vec2 p2_cutoff = vec2(x_cutoff, y_cutoff);
        const float dist = min(distance(local.xy, p1_cutoff), distance(local.xy, p2_cutoff));
        alpha = clamp((0.5 * thickness - dist) / pixel_size, 0.0, 1.0);*/
        alpha = 0.0;
    }
    frag_color = vec4(color, alpha);
    frag_color = vec4(local.x*0.5+0.5, local.y*0.5+0.5, 0.0, alpha+0.5);
}

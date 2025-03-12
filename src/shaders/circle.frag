
in vec4 local;
in vec3 color;

out vec4 frag_color;

void main()
{
    const float dist = length(local.xy);
    const float thickness = local.z;
    const float pixel_size = fwidth(dist);
    const float alpha = clamp((1.0 - dist) / pixel_size, 0.0, 1.0)
        - clamp((1.0 - thickness + pixel_size - dist) / pixel_size, 0.0, 1.0);
    frag_color = vec4(color, alpha);
}

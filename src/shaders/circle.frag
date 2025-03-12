
precision highp float;

in vec4 local;
in vec3 color;

out vec4 frag_color;

void main()
{
    float dist = length(local.xy);
    float thickness = local.z;
    float pixel_size = fwidth(dist);
    float alpha = clamp((1.0 - dist) / pixel_size, 0.0, 1.0)
        - clamp((1.0 - thickness + pixel_size - dist) / pixel_size, 0.0, 1.0);
    frag_color = vec4(color, alpha);
}

#version 430

in vec4 local;
in vec3 color;

out vec4 frag_color;

void main()
{
    float dist = abs(local.x);
    if (local.y > 0.0 || local.z > 0.0)
    {
        dist = min(length(local.xy), length(local.xz));
    }
    // NOTE: this assumes that the local x, y and z have the same scale
    const float pixel_size = fwidth(local.x);
    const float alpha = clamp((0.5 - dist) / pixel_size, 0.0, 1.0);
    frag_color = vec4(color, alpha);
}

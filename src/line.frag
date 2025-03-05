#version 430

in vec3 local;
in vec3 color;

out vec4 frag_color;

#define PI 3.1415926535897931

void main()
{
    float dist = abs(local.x);
    if (local.y > 0.0 || local.z > 0.0)
    {
        dist = min(length(local.xy), length(local.xz));
    }
    const float alpha = smoothstep(0.5, 0.5 - fwidth(dist), dist);
    frag_color = vec4(color, alpha);
}

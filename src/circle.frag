#version 430

in vec3 local;
in vec3 color;

out vec4 frag_color;

#define PI 3.1415926535897931

void main()
{
    const float dist = length(local.xy);
    const float thickness = local.z;
    const float alpha = smoothstep(1.0 - thickness, 1.0 - thickness + fwidth(dist), dist)
        - smoothstep(1.0 - fwidth(dist), 1.0, dist);
    frag_color = vec4(color, alpha);
}

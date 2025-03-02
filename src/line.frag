#version 430

in vec3 uvw;
in vec3 color;

out vec4 frag_color;

#define PI 3.1415926535897931

void main()
{
    float dist = abs(uvw.x);
    if (uvw.y > 0.0 || uvw.z > 0.0)
    {
        dist = min(length(uvw.xy), length(uvw.xz));
    }
    const float alpha = smoothstep(0.5, 0.5 - fwidth(dist), dist);
    frag_color = vec4(color, alpha);
}

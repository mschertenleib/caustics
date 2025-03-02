#version 430

in vec2 uv;
in vec3 color;

out vec4 frag_color;

#define PI 3.1415926535897931

void main()
{
    const float dist = abs(uv.x - 0.5);
    const float alpha = smoothstep(0.5, 0.5 - fwidth(dist), dist);
    frag_color = vec4(color, alpha);
}

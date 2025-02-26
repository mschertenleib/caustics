#version 430

in vec2 uv;
in vec3 color;

out vec4 frag_color;

#define PI 3.1415926535897931

void main()
{
    const float f = 10.0;
    const float z = cos(2.0 * PI * f * uv.x) + cos(2.0 * PI * f * uv.y);
    const float alpha = clamp(z * 0.5 + 0.2, 0.0, 1.0);
    frag_color = vec4(uv, 0.0, alpha);
}

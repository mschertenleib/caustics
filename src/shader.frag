#version 430

in vec2 uv;
in vec3 color;

out vec4 frag_color;

#define PI 3.1415926535897931

void main()
{
    const float alpha = step(0.0, cos(2.0 * PI * 10 * uv.y) + 0.5);
    frag_color = vec4(color, alpha);
}

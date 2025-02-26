#version 430

in vec2 uv;
in vec3 color;

out vec4 frag_color;

void main()
{
    frag_color = vec4(uv, 0.0, 1.0);
}

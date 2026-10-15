precision highp float;

layout (location = 0) in vec2 vertex_position;
layout (location = 1) in vec2 vertex_local;
layout (location = 2) in vec4 vertex_params;
layout (location = 3) in vec3 vertex_color;

out vec2 local;
flat out vec4 params;
flat out vec3 color;

uniform vec2 view_position;
uniform vec2 view_size;


void main()
{
    vec2 position = (vertex_position - view_position) / (0.5 * view_size);
    gl_Position = vec4(position, 0.0, 1.0);
    local = vertex_local;
    params = vertex_params;
    color = vertex_color;
}

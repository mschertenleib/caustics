
layout (location = 0) in vec2 vertex_position;
layout (location = 1) in vec4 vertex_local;
layout (location = 2) in vec3 vertex_color;

uniform vec2 view_position;
uniform vec2 view_size;

out vec4 local;
out vec3 color;

void main()
{
    const vec2 position = (vertex_position - view_position) / (0.5 * view_size);
    gl_Position = vec4(position, 0.0, 1.0);
    local = vertex_local;
    color = vertex_color;
}

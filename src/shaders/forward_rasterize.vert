precision highp float;

layout (location = 0) in vec2 ray_origin;
layout (location = 1) in vec2 segment_end;

flat out vec2 start_px;
flat out vec2 end_px;
flat out vec2 start_world;
flat out vec2 end_world;
flat out float pixel_area;

uniform vec2 view_position;
uniform vec2 view_size;
uniform vec2 viewport_size;


void main()
{
    const vec2 vertices[6] = vec2[6](
        vec2(0.0, -1.0),
        vec2(1.0, -1.0),
        vec2(1.0,  1.0),
        vec2(0.0, -1.0),
        vec2(1.0,  1.0),
        vec2(0.0,  1.0)
    );
    vec2 vertex = vertices[gl_VertexID];

    // In pixel coordinates
    vec2 p0 = ((ray_origin - view_position) / view_size + 0.5) * viewport_size;
    vec2 p1 = ((segment_end - view_position) / view_size + 0.5) * viewport_size;

    vec2 delta = p1 - p0;
    float len = length(delta);
    vec2 tangent = len > 1e-6 ? delta * (1.0 / len) : vec2(1.0, 0.0);
    vec2 normal = vec2(-tangent.y, tangent.x);

    float half_width = 0.5 * (abs(tangent.x) + abs(tangent.y)) + 1e-2;
    vec2 position_px =
        mix(p0 - tangent * half_width, p1 + tangent * half_width, vertex.x)
        + normal * vertex.y * half_width;

    vec2 position_ndc = 2.0 * position_px / viewport_size - 1.0;
    gl_Position = vec4(position_ndc, 0.0, 1.0);

    start_px = p0;
    end_px = p1;
    start_world = ray_origin;
    end_world = segment_end;
    pixel_area = view_size.x * view_size.y / (viewport_size.x * viewport_size.y);
}

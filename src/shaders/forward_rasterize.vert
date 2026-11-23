precision highp float;


layout (location = 0) in vec2 a_segment_start;
layout (location = 1) in vec2 a_segment_end;
layout (location = 2) in vec3 a_color;

flat out vec2 v_start_px;
flat out vec2 v_end_px;
flat out float v_pixel_area;
flat out float v_world_length;
flat out vec3 v_color;

uniform vec2 u_view_position;
uniform vec2 u_view_size;
uniform vec2 u_viewport_size;


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
    vec2 p0 = ((a_segment_start - u_view_position) / u_view_size + 0.5) * u_viewport_size;
    vec2 p1 = ((a_segment_end - u_view_position) / u_view_size + 0.5) * u_viewport_size;

    vec2 delta = p1 - p0;
    float len = length(delta);
    vec2 tangent = len > 1e-6 ? delta * (1.0 / len) : vec2(1.0, 0.0);
    vec2 normal = vec2(-tangent.y, tangent.x);

    float half_width = 0.5 * (abs(tangent.x) + abs(tangent.y)) + 1e-2;
    vec2 position_px =
        mix(p0 - tangent * half_width, p1 + tangent * half_width, vertex.x)
        + normal * vertex.y * half_width;

    vec2 position_ndc = 2.0 * position_px / u_viewport_size - 1.0;
    gl_Position = vec4(position_ndc, 0.0, 1.0);

    v_start_px = p0;
    v_end_px = p1;
    v_pixel_area = u_view_size.x * u_view_size.y / (u_viewport_size.x * u_viewport_size.y);
    v_world_length = length(a_segment_end - a_segment_start);
    v_color = a_color;
}

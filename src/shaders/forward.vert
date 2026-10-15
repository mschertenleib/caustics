precision highp float;

layout (location = 0) in vec2 ray_origin;
layout (location = 1) in vec2 ray_direction;
layout (location = 2) in float wavelength;
layout (location = 3) in float radiance;
layout (location = 4) in float throughput;
layout (location = 5) in uint alive;

out vec2 out_ray_origin;
out vec2 out_ray_direction;
out float out_wavelength;
out float out_radiance;
out float out_throughput;
flat out uint out_alive;
out vec2 out_segment_end;


#define PI 3.14159274

void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    out_ray_origin = vec2(0.5, 0.3);
    float angle = float(gl_VertexID) / 4096.0 * 2.0 * PI;
    out_ray_direction = vec2(cos(angle), sin(angle));
    out_wavelength = 0.0;
    out_radiance = 0.0;
    out_throughput = 0.0;
    out_alive = 1u;
    out_segment_end = vec2(out_ray_origin + out_ray_direction * 10.0);
}

precision highp float;

layout (location = 0) in vec2 a_ray_origin;
layout (location = 1) in vec2 a_ray_direction;
layout (location = 2) in float a_wavelength;
layout (location = 3) in float a_radiance;
layout (location = 4) in float a_throughput;
layout (location = 5) in uint a_alive;

out vec2 tf_ray_origin;
out vec2 tf_ray_direction;
out float tf_wavelength;
out float tf_radiance;
out float tf_throughput;
flat out uint tf_alive;

out vec2 tf_segment_start;
out vec2 tf_segment_end;
out vec3 tf_color;

uniform uint u_sample_index;


void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);

    uint rng_state = hash(uint(gl_VertexID)) ^ hash(u_sample_index);
    
    vec2 origin;
    vec2 direction;
    if (a_alive == 1u)
    {
        origin = a_ray_origin;
        direction = a_ray_direction;
        tf_wavelength = a_wavelength;
        tf_radiance = a_radiance;
        tf_throughput = a_throughput;
    }
    else
    {
        origin = vec2(0.8, 0.5);
        float angle = 2.0 * PI * random(rng_state);
        angle = 1.15*PI;
        direction = vec2(cos(angle), sin(angle));
        tf_wavelength = 380.0 + random(rng_state) * 400.0;
        tf_radiance = 0.0;
        tf_throughput = 1.0;
    }

    tf_alive = 1u;
    tf_segment_start = origin;
    tf_color = xyz_to_rgb(tf_throughput * cie_1931_cmf_xyz(tf_wavelength)); // FIXME

    float t;
    vec2 local;
    uint geometry_type;
    uint geometry_index;
    uint volume_mask;
    bool is_hit = intersect(origin, direction, t, local, geometry_type, geometry_index, volume_mask);
    if (is_hit)
    {
        Hit hit = get_hit(origin, direction, t, local, geometry_type, geometry_index);
        if (hit.surface_id != INVALID_ID)
        {
            Surface surface = surfaces[hit.surface_id];

            float pdf;
            bool is_delta;
            evaluate_surface(hit, surface, origin, direction, tf_wavelength, tf_throughput, pdf, is_delta, rng_state);
        }
        else
        {
            vec2 inside_normal = dot(direction, hit.normal) >= 0.0 ? hit.normal : -hit.normal;
            origin = offset_position_along_normal(hit.position, inside_normal);
        }

        tf_ray_origin = origin;
        tf_ray_direction = direction;
        tf_segment_end = vec2(hit.position);
    }
    else
    {
        tf_ray_origin = origin;
        tf_ray_direction = vec2(0.0, 0.0);
        // TODO: clip to visible view
        tf_segment_end = vec2(origin + direction * 10.0);
    }
}

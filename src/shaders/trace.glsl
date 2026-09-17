
precision highp float;



struct Surface
{
    vec3 base_color;
    uint type;
    vec3 emission_color;
    float emission_strength;
    float ior_ratio;
};

struct Volume
{
    vec3 absorption;
    float scattering;
    float phase_anisotropy;
};

struct Line
{
    vec2 vertex_a;
    vec2 vertex_b;
    uint surface_id;
    uint volume_in_id;
    uint volume_out_id;
};

struct Arc
{
    vec2 center;
    float radius;
    float clip_offset;
    vec2 clip_normal;
    uint surface_id;
    uint volume_in_id;
    uint volume_out_id;
};

struct Parabola
{
    vec2 vertex;
    vec2 axis;
    float focal;
    float clip;
    uint surface_id;
    uint volume_in_id;
    uint volume_out_id;
};

struct Hit
{
    vec2 position;
    vec2 normal;
    uint surface_id;
    uint volume_in_id;
    uint volume_out_id;
};



layout(std140) uniform Surfaces { Surface surfaces[MAX_SURFACES]; };
layout(std140) uniform Volumes { Volume volumes[MAX_VOLUMES]; };
layout(std140) uniform Lines { Line lines[MAX_LINES]; };
layout(std140) uniform Arcs { Arc arcs[MAX_ARCS]; };
layout(std140) uniform Parabolas { Parabola parabolas[MAX_PARABOLAS]; };

uniform uint num_lines;
uniform uint num_arcs;
uniform uint num_parabolas;
uniform int sample_index;
uniform int samples_per_frame;
uniform vec2 view_position;
uniform vec2 view_size;
uniform uvec2 image_size;

out vec4 out_color;



#define PI 3.14159274
#define FLOAT_EPSILON 1.1920929e-7
#define FLOAT_MAX 3.40282346e38 // Should be 3.40282347e38, but WebGL flags it as overflowing

#define GEOMETRY_NONE 0u
#define GEOMETRY_LINE 1u
#define GEOMETRY_ARC 2u
#define GEOMETRY_PARABOLA 3u

#define SURFACE_DIFFUSE 0u
#define SURFACE_SPECULAR 1u
#define SURFACE_DIELECTRIC 2u

#define INVALID_ID 0xFFFFFFFFu




float cross2(vec2 a, vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

bool intersect_line(vec2 origin, vec2 direction, Line line, inout float t, inout vec2 local)
{
    vec2 s = line.vertex_b - line.vertex_a;
    vec2 q = line.vertex_a - origin;
    float s_sq = dot(s, s);
    float denom = cross2(direction, s);
    // FIXME: we might want to change this? At least we want to be consistent
    float denom_eps = 4.0 * FLOAT_EPSILON * sqrt(s_sq);
    if (abs(denom) <= denom_eps)
    {
        return false;
    }

    float tr = cross2(q, s) / denom;
    float u  = cross2(q, direction) / denom;
    if (tr <= 0.0 || tr >= t || u < 0.0 || u > 1.0)
    {
        return false;
    }

    t = tr;
    local = vec2(u, 0.0);
    return true;
}

bool intersect_arc(vec2 origin, vec2 direction, Arc arc, inout float t, inout vec2 local)
{
    float radius_sq = arc.radius * arc.radius;
    vec2 m = origin - arc.center;

    // Signed perpendicular distance from the center to the ray
    float perp = cross2(direction, m);

    // Half chord squared
    float h_sq = radius_sq - perp * perp;

    // Allow a small negative error at tangency
    // FIXME: we might want to change this? At least we want to be consistent
    float h_sq_scale = max(radius_sq, perp * perp);
    float h_sq_eps = 4.0 * FLOAT_EPSILON * h_sq_scale;
    if (h_sq < -h_sq_eps)
    {
        return false;
    }

    float h = sqrt(max(h_sq, 0.0));
    float b = dot(m, direction);
    float c = dot(m, m) - radius_sq;
    float q = -(b + (b >= 0.0 ? h : -h));
    if (q == 0.0) // q == 0 implies t = 0
    {
        return false;
    }

    float t0 = q;
    float t1 = c / q;

    // t0 must be the nearest root
    if (t1 < t0)
    {
        float tmp = t0;
        t0 = t1;
        t1 = tmp;
    }

    // Orthonormal basis vector perpendicular to the ray
    vec2 side = vec2(-direction.y, direction.x);

    if (t0 > 0.0 && t0 < t)
    {
        vec2 rel = perp * side - h * direction;
        if (dot(arc.clip_normal, rel) >= arc.clip_offset)
        {
            t = t0;
            local = rel;
            return true;
        }
    }

    if (t1 > 0.0 && t1 < t)
    {
        vec2 rel = perp * side + h * direction;
        if (dot(arc.clip_normal, rel) >= arc.clip_offset)
        {
            t = t1;
            local = rel;
            return true;
        }
    }

    return false;
}

bool intersect_parabola(vec2 origin, vec2 direction, Parabola parabola, inout float t, inout vec2 local)
{
    vec2 axis = parabola.axis;
    vec2 side = vec2(-axis.y, axis.x);

    vec2 ro = origin - parabola.vertex;
    float ox = dot(ro, axis);
    float oy = dot(ro, side);
    float dx = dot(direction, axis);
    float dy = dot(direction, side);
    float f = parabola.focal;
    float a = dy * dy;
    float b = oy * dy - 2.0 * f * dx;
    float c = oy * oy - 4.0 * f * ox;

    // Exactly parallel to the parabola axis -> linear equation
    if (a == 0.0)
    {
        if (b == 0.0)
        {
            return false;
        }

        float tr = -c / (2.0 * b);
        if (tr <= 0.0 || tr >= t)
        {
            return false;
        }

        float y = tr * dy + oy;
        float x = (y * y) / (4.0 * f);
        if (x > parabola.clip)
        {
            return false;
        }

        t = tr;
        local = vec2(x, y);
        return true;
    }

    float dx_sq = dx * dx;
    float dy_sq = dy * dy;
    float dxdy = dx * dy;
    float base = f * dx_sq - oy * dxdy + ox * dy_sq;

    // FIXME: we might want to change this? At least we want to be consistent
    float base_scale = abs(f * dx_sq) + abs(oy * dxdy) + abs(ox * dy_sq);
    float base_eps = 4.0 * FLOAT_EPSILON * base_scale;
    if (base < -base_eps)
    {
        return false;
    }

    float sqrt_disc = 2.0 * sqrt(f * max(base, 0.0));
    float q = -(b + (b >= 0.0 ? sqrt_disc : -sqrt_disc));
    if (q == 0.0) // q == 0 implies t = 0
    {
        return false;
    }

    float t0 = q / a;
    float t1 = c / q;

    // t0 must be the nearest root
    if (t1 < t0)
    {
        float tmp = t0;
        t0 = t1;
        t1 = tmp;
    }

    if (t0 > 0.0 && t0 < t)
    {
        float y = t0 * dy + oy;
        float x = (y * y) / (4.0 * f);
        if (x <= parabola.clip)
        {
            t = t0;
            local = vec2(x, y);
            return true;
        }
    }

    if (t1 > 0.0 && t1 < t)
    {
        float y = t1 * dy + oy;
        float x = (y * y) / (4.0 * f);
        if (x <= parabola.clip)
        {
            t = t1;
            local = vec2(x, y);
            return true;
        }
    }

    return false;
}

bool intersect(vec2 origin, vec2 direction, out float t, out vec2 local, out uint geometry_type, out uint geometry_index)
{
    t = FLOAT_MAX;
    local = vec2(0.0);
    geometry_type = GEOMETRY_NONE;
    geometry_index = INVALID_ID;

    for (uint i = 0u; i < num_lines; ++i)
    {
        if (intersect_line(origin, direction, lines[i], t, local))
        {
            geometry_type = GEOMETRY_LINE;
            geometry_index = i;
        }
    }
    for (uint i = 0u; i < num_arcs; ++i)
    {
        if (intersect_arc(origin, direction, arcs[i], t, local))
        {
            geometry_type = GEOMETRY_ARC;
            geometry_index = i;
        }
    }
    for (uint i = 0u; i < num_parabolas; ++i)
    {
        if (intersect_parabola(origin, direction, parabolas[i], t, local))
        {
            geometry_type = GEOMETRY_PARABOLA;
            geometry_index = i;
        }
    }

    return geometry_type != GEOMETRY_NONE;
}

Hit get_hit(vec2 origin, vec2 direction, float t, vec2 local, uint geometry_type, uint geometry_index)
{
    Hit hit;

    switch(geometry_type)
    {
        case GEOMETRY_LINE:
        {
            Line line = lines[geometry_index];
            vec2 ab = line.vertex_b - line.vertex_a;
            hit.position = line.vertex_a + local.x * ab;
            vec2 line_dir = normalize(ab);
            hit.normal = vec2(line_dir.y, -line_dir.x);
            hit.surface_id = line.surface_id;
            hit.volume_in_id = line.volume_in_id;
            hit.volume_out_id = line.volume_out_id;
            break;
        }
        case GEOMETRY_ARC:
        {
            Arc arc = arcs[geometry_index];
            vec2 geometric_normal = normalize(local);
            hit.position = arc.center + abs(arc.radius) * geometric_normal;
            // Negative radius means normal points towards the center
            hit.normal = sign(arc.radius) * geometric_normal;
            hit.surface_id = arc.surface_id;
            hit.volume_in_id = arc.volume_in_id;
            hit.volume_out_id = arc.volume_out_id;
            break;
        }
        case GEOMETRY_PARABOLA:
        {
            Parabola parabola = parabolas[geometry_index];
            vec2 side = vec2(-parabola.axis.y, parabola.axis.x);
            hit.position = local.x * parabola.axis + local.y * side + parabola.vertex;
            hit.normal = normalize(-2.0 * parabola.focal * parabola.axis + local.y * side);
            hit.surface_id = parabola.surface_id;
            hit.volume_in_id = parabola.volume_in_id;
            hit.volume_out_id = parabola.volume_out_id;
            break;
        }
    }

    return hit;
}

// This uses the technique by Carsten Wächter and
// Nikolaus Binder from "A Fast and Robust Method for Avoiding
// Self-Intersection" from Ray Tracing Gems (version 1.7, 2020).
vec2 offset_position_along_normal(vec2 position, vec2 normal)
{
    // Convert the normal to an integer offset.
    ivec2 of_i = ivec2(256.0 * normal);

    // Offset each component of position using its binary representation.
    // Handle the sign bits correctly.
    vec2 p_i = vec2(
        intBitsToFloat(floatBitsToInt(position.x) + ((position.x < 0.0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(position.y) + ((position.y < 0.0) ? -of_i.y : of_i.y))
    );
    // Use a floating-point offset instead for points near (0,0), the origin.
    const float origin = 1.0 / 32.0;
    const float float_scale = 1.0 / 65536.0;
    return vec2(
        abs(position.x) < origin ? position.x + float_scale * normal.x : p_i.x,
        abs(position.y) < origin ? position.y + float_scale * normal.y : p_i.y
    );
}

uint hash(uint x)
{
    x += x << 10;
    x ^= x >> 6;
    x += x << 3;
    x ^= x >> 11;
    x += x << 15;
    return x;
}

// Returns a value uniformly sampled in [0.0, 1.0)
float random(inout uint rng_state)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;

    const float inv_2_24 = 1.0 / 16777216.0;
    return float(rng_state >> 8u) * inv_2_24;
}

vec2 sample_diffuse(vec2 normal, inout uint rng_state)
{
    float sin_theta = 2.0 * random(rng_state) - 1.0;
    float cos_theta = sqrt(1.0 - sin_theta * sin_theta);
    vec2 tangent = vec2(-normal.y, normal.x);
    return cos_theta * normal + sin_theta * tangent;
}

void evaluate_surface(
    Hit hit,
    Surface surface,
    inout vec2 ray_origin,
    inout vec2 ray_direction,
    inout vec3 throughput,
    inout uint rng_state)
{
    // hit.normal: object normal, defining "inside" and "outside"
    // normal:     opposes the incoming ray
    float cos_theta_i = dot(ray_direction, hit.normal);
    bool is_entering = cos_theta_i < 0.0;
    vec2 normal = is_entering ? hit.normal : -hit.normal;
    cos_theta_i = abs(cos_theta_i);

    if (surface.type == SURFACE_DIFFUSE) 
    {
        ray_origin = offset_position_along_normal(hit.position, normal);
        ray_direction = sample_diffuse(normal, rng_state);
        throughput *= surface.base_color;
    } 
    else if (surface.type == SURFACE_SPECULAR) 
    {
        ray_origin = offset_position_along_normal(hit.position, normal);
        ray_direction = reflect(ray_direction, normal);
        
        vec3 f0 = surface.base_color;
        float c = max(1.0 - cos_theta_i, 0.0);
        vec3 fresnel_reflectance = f0 + (1.0 - f0) * (c * c * c * c * c);
        throughput *= fresnel_reflectance; 
    } 
    else if (surface.type == SURFACE_DIELECTRIC) 
    {
        float eta = is_entering ? 1.0 / surface.ior_ratio : surface.ior_ratio;
        float sin2_theta_t = (eta * eta) * (1.0 - cos_theta_i * cos_theta_i);

        if (sin2_theta_t > 1.0) // Total internal reflection
        {
            ray_origin = offset_position_along_normal(hit.position, normal);
            ray_direction = reflect(ray_direction, normal);
            return;
        }

        float cos_theta_t = sqrt(1.0 - sin2_theta_t);
        float r0 = (eta - 1.0) / (eta + 1.0);
        float f0 = r0 * r0;
        float cos_fresnel = is_entering ? cos_theta_i : cos_theta_t;
        float c = max(1.0 - cos_fresnel, 0.0);
        float fresnel_reflectance = f0 + (1.0 - f0) * c * c * c * c * c;

        if (random(rng_state) < fresnel_reflectance)
        {
            ray_origin = offset_position_along_normal(hit.position, normal);
            ray_direction = reflect(ray_direction, normal);
        }
        else
        {
            ray_origin = offset_position_along_normal(hit.position, -normal);
            ray_direction = eta * ray_direction + (eta * cos_theta_i - cos_theta_t) * normal;
            throughput *= surface.base_color * eta;
        }
    }
}

vec3 compute_radiance(vec2 ray_origin, vec2 ray_direction, inout uint rng_state)
{
    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);

    for (int depth = 0; depth <= 32; ++depth)
    {
        float t;
        vec2 local;
        uint geometry_type;
        uint geometry_index;
        bool is_hit = intersect(ray_origin, ray_direction, t, local, geometry_type, geometry_index);

        if (!is_hit)
        {
            //const vec3 environment_emission = vec3(0.6, 0.6, 0.6);
            //float gate = pow(max(dot(ray_direction, vec2(0.0, 1.0)), 0.0), 5.0);
            //radiance += throughput * environment_emission * gate;
            break;
        }

        Hit hit = get_hit(ray_origin, ray_direction, t, local, geometry_type, geometry_index);

        

// FIXME
// ALso should go before !is_hit check? Maybe unnecessary since we probably don't want
// to allow scattering everywhere, even if this could technically be implied by some
/// ill-formed, non-closed geometries.
#if 1
        bool is_entering = dot(ray_direction, hit.normal) < 0.0;
        uint volume_id = is_entering ? hit.volume_out_id : hit.volume_in_id;
        if (volume_id != INVALID_ID)
        {
            Volume volume = volumes[volume_id];
            vec3 sigma_a = volume.absorption;
            float sigma_s = volume.scattering;
            float g = volume.phase_anisotropy;

            vec3 sigma_t = sigma_a + vec3(sigma_s);
            float sigma_maj = max(sigma_t.x, max(sigma_t.y, sigma_t.z));
            if (sigma_maj > 0.0)
            {
                float distance = -log(1.0 - random(rng_state)) / sigma_maj;
                if (distance < t)
                {
                    ray_origin += ray_direction * distance;

                    vec3 transmittance = exp(-sigma_t * distance);
                    throughput *= transmittance * (sigma_s / sigma_maj) * exp(sigma_maj * distance);

                    float phi = 2.0 * PI * random(rng_state) - PI;
                    float theta = 2.0 * atan((1.0 - g) / (1.0 + g) * tan(0.5 * phi));
                    vec2 forward = ray_direction;
                    vec2 right   = vec2(-forward.y, forward.x);
                    ray_direction = cos(theta) * forward + sin(theta) * right;

                    continue;
                }
            }

            throughput *= exp(-sigma_t * t) * exp(sigma_maj * t);
        }
#endif

        // FIXME: there might not be a surface
        if (hit.surface_id == INVALID_ID) return vec3(1000.0, 0.0, 0.0);
        Surface surface = surfaces[hit.surface_id];

        radiance += throughput * surface.emission_color * surface.emission_strength;

        // Russian Roulette ray termination
        if (depth >= 3)
        {
            float survival_prob = max(throughput.r, max(throughput.g, throughput.b));
            survival_prob = max(survival_prob, 0.05);
            if (random(rng_state) >= survival_prob)
            {
                break;
            }

            throughput /= survival_prob;
        }

        evaluate_surface(hit, surface, ray_origin, ray_direction, throughput, rng_state);
    }

    return radiance;
}

void main()
{
    uvec2 pixel = uvec2(gl_FragCoord.xy);
    uint pixel_index = pixel.y * image_size.x + pixel.x;
    uint rng_state = hash(pixel_index) ^ hash(uint(sample_index));

    vec4 accumulated_color = vec4(0.0);
    for (int i = 0; i < samples_per_frame; ++i)
    {
        vec2 uv = (vec2(pixel) + vec2(random(rng_state), random(rng_state))) / vec2(image_size);
        vec2 ray_origin = view_position + (uv - 0.5) * view_size;
        float angle = 2.0 * PI * random(rng_state);
        vec2 ray_direction = vec2(cos(angle), sin(angle));
        vec3 radiance = compute_radiance(ray_origin, ray_direction, rng_state);
        accumulated_color += vec4(radiance, 1.0);
    }

    out_color = accumulated_color / float(samples_per_frame);
}

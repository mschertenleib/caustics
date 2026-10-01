
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
    float phase_anisotropy;
    vec3 scattering;
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
    float clip_offset;
    vec2 clip_normal;
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

struct AABB
{
    vec2 min;
    vec2 max;
};

struct BVH_node
{
    AABB aabbs[2];
    uint child0;
    uint child1;
};



layout(std140) uniform Surfaces { Surface surfaces[MAX_SURFACES]; };
layout(std140) uniform Volumes { Volume volumes[MAX_VOLUMES]; };
layout(std140) uniform Lines { Line lines[MAX_LINES]; };
layout(std140) uniform Arcs { Arc arcs[MAX_ARCS]; };
layout(std140) uniform Parabolas { Parabola parabolas[MAX_PARABOLAS]; };
layout(std140) uniform BVH_nodes { BVH_node bvh_nodes[MAX_BVH_NODES]; };

uniform uint num_lines;
uniform uint num_arcs;
uniform uint num_parabolas;
uniform uint num_bvh_nodes;
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

#define INVALID_ID 0xffffffffu
#define INVALID_CHILD 0xffffffffu




float cross2(vec2 a, vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

bool intersect_line(vec2 origin, vec2 direction, Line line, inout float t, inout vec2 local, out uint parity)
{
    parity = 0u;
    
    vec2 s = line.vertex_b - line.vertex_a;
    vec2 q = line.vertex_a - origin;
    float denom = cross2(direction, s);
    float denom_sq_eps = 16.0 * FLOAT_EPSILON * FLOAT_EPSILON * dot(s, s);
    if (denom * denom <= denom_sq_eps)
    {
        return false;
    }

    float tr = cross2(q, s) / denom;
    float u  = cross2(q, direction) / denom;
    if (tr <= 0.0 || u < 0.0 || u > 1.0)
    {
        return false;
    }

    parity = 1u;

    if (tr >= t)
    {
        return false;
    }

    t = tr;
    local = vec2(u, 0.0);
    return true;
}

bool intersect_arc(vec2 origin, vec2 direction, Arc arc, inout float t, inout vec2 local, out uint parity)
{
    parity = 0u;

    float radius_sq = arc.radius * arc.radius;
    vec2 m = origin - arc.center;

    // Signed perpendicular distance from the center to the ray
    float perp = cross2(direction, m);

    // Half chord squared
    float perp_sq = perp * perp;
    float h_sq = radius_sq - perp_sq;

    // Allow a small negative error at tangency
    float h_sq_scale = radius_sq + perp_sq;
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

    bool hit = false;

    if (t0 > 0.0)
    {
        vec2 rel = perp * side - h * direction;
        // FIXME: we want to reverse the inequality to stay consistent with the parabolas
        if (dot(arc.clip_normal, rel) >= arc.clip_offset)
        {
            parity ^= 1u;
            if (t0 < t)
            {
                t = t0;
                local = rel;
                hit = true;
            }
        }
    }

    if (t1 > 0.0)
    {
        vec2 rel = perp * side + h * direction;
        if (dot(arc.clip_normal, rel) >= arc.clip_offset)
        {
            parity ^= 1u;
            if (t1 < t)
            {
                t = t1;
                local = rel;
                hit = true;
            }
        }
    }

    return hit;
}

bool intersect_parabola(vec2 origin, vec2 direction, Parabola parabola, inout float t, inout vec2 local, out uint parity)
{
    parity = 0u;

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

    // Exactly parallel to the parabola axis
    if (a == 0.0)
    {
        if (b == 0.0)
        {
            return false;
        }

        float tr = -c / (2.0 * b);
        if (tr <= 0.0)
        {
            return false;
        }

        float y = tr * dy + oy;
        float x = (y * y) / (4.0 * f);
        if (dot(parabola.clip_normal, vec2(x, y)) > parabola.clip_offset)
        {
            return false;
        }

        parity = 1u;

        if (tr >= t)
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
    float term0 = f * dx_sq;
    float term1 = oy * dxdy;
    float term2 = ox * dy_sq;
    float base = term0 - term1 + term2;
    float base_scale = abs(term0) + abs(term1) + abs(term2);
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

    bool hit = false;

    if (t0 > 0.0)
    {
        float y = t0 * dy + oy;
        float x = (y * y) / (4.0 * f);
        if (dot(parabola.clip_normal, vec2(x, y)) <= parabola.clip_offset)
        {
            parity ^= 1u;
            if (t0 < t)
            {
                t = t0;
                local = vec2(x, y);
                hit = true;
            }
        }
    }

    if (t1 > 0.0)
    {
        float y = t1 * dy + oy;
        float x = (y * y) / (4.0 * f);
        if (dot(parabola.clip_normal, vec2(x, y)) <= parabola.clip_offset)
        {
            parity ^= 1u;
            if (t1 < t)
            {
                t = t1;
                local = vec2(x, y);
                hit = true;
            }
        }
    }

    return hit;
}

#if 1

bool intersect(vec2 origin, vec2 direction, out float t, out vec2 local, out uint geometry_type, out uint geometry_index, out uint volume_mask)
{
    t = FLOAT_MAX;
    local = vec2(0.0);
    geometry_type = GEOMETRY_NONE;
    geometry_index = INVALID_ID;
    volume_mask = 0u;

    uint parity;

    for (uint i = 0u; i < num_lines; ++i)
    {
        Line line = lines[i];
        if (intersect_line(origin, direction, line, t, local, parity))
        {
            geometry_type = GEOMETRY_LINE;
            geometry_index = i;
        }
        if (line.volume_in_id != INVALID_ID) volume_mask ^= parity << line.volume_in_id;
        if (line.volume_out_id != INVALID_ID) volume_mask ^= parity << line.volume_out_id;
    }
    for (uint i = 0u; i < num_arcs; ++i)
    {
        Arc arc = arcs[i];
        if (intersect_arc(origin, direction, arc, t, local, parity))
        {
            geometry_type = GEOMETRY_ARC;
            geometry_index = i;
        }
        if (arc.volume_in_id != INVALID_ID) volume_mask ^= parity << arc.volume_in_id;
        if (arc.volume_out_id != INVALID_ID) volume_mask ^= parity << arc.volume_out_id;
    }
    for (uint i = 0u; i < num_parabolas; ++i)
    {
        Parabola parabola = parabolas[i];
        if (intersect_parabola(origin, direction, parabola, t, local, parity))
        {
            geometry_type = GEOMETRY_PARABOLA;
            geometry_index = i;
        }
        if (parabola.volume_in_id != INVALID_ID) volume_mask ^= parity << parabola.volume_in_id;
        if (parabola.volume_out_id != INVALID_ID) volume_mask ^= parity << parabola.volume_out_id;
    }

    return geometry_type != GEOMETRY_NONE;
}

#else


bool is_leaf(uint child)
{
    return (child & 0x80000000u) != 0u;
}

uint leaf_type(uint child)
{
    return (child >> 8u) & 0xffu;
}

uint leaf_index(uint child)
{
    return child & 0xffu;
}

bool intersect_aabb(vec2 origin, vec2 direction, vec2 inv_direction, AABB box, inout float t)
{
    if (direction.x == 0.0 && (origin.x < box.min.x || origin.x > box.max.x))
    {
        return false;
    }

    if (direction.y == 0.0 && (origin.y < box.min.y || origin.y > box.max.y))
    {
        return false;
    }

    vec2 t0 = (box.min - origin) * inv_direction;
    vec2 t1 = (box.max - origin) * inv_direction;

    vec2 t_min = min(t0, t1);
    vec2 t_max = max(t0, t1);

    float lo = max(max(t_min.x, t_min.y), 0.0);
    float hi = min(t_max.x, t_max.y);

    t = lo;
    return lo <= hi;
}

void intersect_primitive(vec2 origin, vec2 direction, uint primitive_ref, inout float t, inout vec2 local, inout uint geometry_type, inout uint geometry_index, inout uint volume_mask)
{
    uint type = leaf_type(primitive_ref);
    uint index = leaf_index(primitive_ref);

    uint parity;

    switch (type)
    {
        case GEOMETRY_LINE:
        {
            Line line = lines[index];
            if (intersect_line(origin, direction, line, t, local, parity))
            {
                geometry_type = GEOMETRY_LINE;
                geometry_index = index;
            }
            if (line.volume_in_id != INVALID_ID) volume_mask ^= parity << line.volume_in_id;
            if (line.volume_out_id != INVALID_ID) volume_mask ^= parity << line.volume_out_id;
            break;
        }
        case GEOMETRY_ARC:
        {
            Arc arc = arcs[index];
            if (intersect_arc(origin, direction, arc, t, local, parity))
            {
                geometry_type = GEOMETRY_ARC;
                geometry_index = index;
            }
            if (arc.volume_in_id != INVALID_ID) volume_mask ^= parity << arc.volume_in_id;
            if (arc.volume_out_id != INVALID_ID) volume_mask ^= parity << arc.volume_out_id;
            break;
        }
        case GEOMETRY_PARABOLA:
        {
            Parabola parabola = parabolas[index];
            if (intersect_parabola(origin, direction, parabola, t, local, parity))
            {
                geometry_type = GEOMETRY_PARABOLA;
                geometry_index = index;
            }
            if (parabola.volume_in_id != INVALID_ID) volume_mask ^= parity << parabola.volume_in_id;
            if (parabola.volume_out_id != INVALID_ID) volume_mask ^= parity << parabola.volume_out_id;
            break;
        }
    }
}

bool intersect(vec2 origin, vec2 direction, out float t, out vec2 local, out uint geometry_type, out uint geometry_index, out uint volume_mask)
{
    t = FLOAT_MAX;
    local = vec2(0.0);
    geometry_type = GEOMETRY_NONE;
    geometry_index = INVALID_ID;
    volume_mask = 0u;

    if (num_bvh_nodes == 0)
    {
        return false;
    }

    vec2 inv_direction = 1.0 / direction;

#define MAX_BVH_STACK_SIZE 16  // TODO: might have to be tweaked
    uint stack[MAX_BVH_STACK_SIZE];
    uint stack_size = 0u;

    stack[stack_size++] = 0u;

    while (stack_size != 0u)
    {
        uint ref = stack[--stack_size];

        if (is_leaf(ref))
        {
            intersect_primitive(origin, direction, ref, t, local, geometry_type, geometry_index, volume_mask);
            continue;
        }

        BVH_node node = bvh_nodes[ref];

        float t0;
        float t1;
        bool hit0 = node.child0 != INVALID_CHILD && intersect_aabb(origin, direction, inv_direction, node.aabbs[0], t0);
        bool hit1 = node.child1 != INVALID_CHILD && intersect_aabb(origin, direction, inv_direction, node.aabbs[1], t1);

        if (hit0 && hit1)
        {
            // Push furthest child first
            if (t0 < t1)
            {
                stack[stack_size++] = node.child1;
                stack[stack_size++] = node.child0;
            }
            else
            {
                stack[stack_size++] = node.child0;
                stack[stack_size++] = node.child1;
            }
        }
        else if (hit0)
        {
            stack[stack_size++] = node.child0;
        }
        else if (hit1)
        {
            stack[stack_size++] = node.child1;
        }
    }

    return geometry_type != GEOMETRY_NONE;
}

#endif

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
    out float pdf, // Actual PDF if non-delta, else branch probability
    out bool is_delta,
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
        pdf = max(dot(normal, ray_direction), 0.0) * 0.5;
        is_delta = false;
    } 
    else if (surface.type == SURFACE_SPECULAR) 
    {
        ray_origin = offset_position_along_normal(hit.position, normal);
        ray_direction = reflect(ray_direction, normal);
        
        vec3 f0 = surface.base_color;
        float c = max(1.0 - cos_theta_i, 0.0);
        vec3 fresnel_reflectance = f0 + (1.0 - f0) * (c * c * c * c * c);
        throughput *= fresnel_reflectance;
        pdf = 1.0;
        is_delta = true;
    } 
    else if (surface.type == SURFACE_DIELECTRIC) 
    {
        float eta = is_entering ? 1.0 / surface.ior_ratio : surface.ior_ratio;
        float sin2_theta_t = (eta * eta) * (1.0 - cos_theta_i * cos_theta_i);

        if (sin2_theta_t > 1.0) // Total internal reflection
        {
            ray_origin = offset_position_along_normal(hit.position, normal);
            ray_direction = reflect(ray_direction, normal);
            pdf = 1.0;
            is_delta = true;
            return;
        }

        float cos_theta_t = sqrt(1.0 - sin2_theta_t);
        float r0 = (eta - 1.0) / (eta + 1.0);
        float f0 = r0 * r0;
        float cos_fresnel = is_entering ? cos_theta_i : cos_theta_t;
        float c = max(1.0 - cos_fresnel, 0.0);
        float fresnel_reflectance = f0 + (1.0 - f0) * c * c * c * c * c;

        if (random(rng_state) < fresnel_reflectance) // Reflect
        {
            ray_origin = offset_position_along_normal(hit.position, normal);
            ray_direction = reflect(ray_direction, normal);
            pdf = fresnel_reflectance;
            is_delta = true;
        }
        else // Refract
        {
            ray_origin = offset_position_along_normal(hit.position, -normal);
            ray_direction = eta * ray_direction + (eta * cos_theta_i - cos_theta_t) * normal;
            throughput *= surface.base_color * eta;
            pdf = 1.0 - fresnel_reflectance;
            is_delta = true;
        }
    }
}

bool evaluate_volume(Volume volume, float t, inout vec2 ray_origin, inout vec2 ray_direction, inout vec3 throughput, inout uint rng_state)
{
    vec3 sigma_a = volume.absorption;
    vec3 sigma_s = volume.scattering;
    vec3 sigma_t = sigma_a + sigma_s;

    if (max(sigma_t.x, max(sigma_t.y, sigma_t.z)) <= 0.0)
    {
        return false;
    }

    if (max(sigma_s.x, max(sigma_s.y, sigma_s.z)) <= 0.0)
    {
        throughput *= exp(-sigma_t * t);
        return false;
    }

    float u_channel = random(rng_state);
    float sigma_t_sample;
    if (u_channel < 1.0 / 3.0)
    {
        sigma_t_sample = sigma_t.x;
    }
    else if (u_channel < 2.0 / 3.0)
    {
        sigma_t_sample = sigma_t.y;
    }
    else
    {
        sigma_t_sample = sigma_t.z;
    }

    float distance;
    if (sigma_t_sample > 0.0)
    {
        distance = -log(1.0 - random(rng_state)) / sigma_t_sample;
    }
    else
    {
        distance = FLOAT_MAX;
    }

    vec3 Tr = exp(-sigma_t * min(distance, t));

    if (distance < t)
    {
        ray_origin += ray_direction * distance;

        float g = volume.phase_anisotropy;
        if (g <= -1.0)
        {
            ray_direction = -ray_direction;
        }
        else if (g < 1.0)
        {
            float half_phi = PI * random(rng_state) - 0.5 * PI;
            float sh = sin(half_phi);
            float ch = cos(half_phi);
            float a = (1.0 - g) / (1.0 + g);
            float a_sq = a * a;
            float ch_sq = ch * ch;
            float sh_sq = sh * sh;
            float denom = ch_sq + a_sq * sh_sq;
            float cos_theta = (ch_sq - a_sq * sh_sq) / denom;
            float sin_theta = (2.0 * a * sh * ch) / denom;
            vec2 forward = ray_direction;
            vec2 right = vec2(-forward.y, forward.x);
            ray_direction = cos_theta * forward + sin_theta * right;
        }

        float q = (1.0 / 3.0) * dot(sigma_t, Tr);
        throughput *= sigma_s * Tr / q;

        return true;
    }

    float Q = (1.0 / 3.0) * (Tr.x + Tr.y + Tr.z);
    throughput *= Tr / Q;

    return false;
}

uint lsb_index(uint u)
{
    uint lsb = u & -u; // keep only LSB = 1 << i = 2^i
    float f = float(lsb);
    uint b = floatBitsToUint(f);
    uint e = (b >> 23u) - 127u; // exponent = log2(f) = i
    return e;
}

// Fit c0, c1, c2 from target RGB
float spectrum(float lambda, float c0, float c1, float c2)
{
    float x = (c0 * lambda + c1) * lambda + c2;
    return 0.5 * x * inversesqrt(x * x + 1.0) + 0.5;
}

void main()
{
    uvec2 pixel = uvec2(gl_FragCoord.xy);
    uint pixel_index = pixel.y * image_size.x + pixel.x;
    uint rng_state = hash(pixel_index) ^ hash(uint(sample_index));

    bool alive = false;
    vec2 ray_origin;
    vec2 ray_direction;
    vec3 radiance;
    vec3 throughput;
    int depth;

    vec4 accumulated_color = vec4(0.0);

    // FIXME
    int segments_per_frame = samples_per_frame * 8;

    for (int i = 0; i < segments_per_frame; ++i)
    {
        if (!alive)
        {
            vec2 uv = (vec2(pixel) + vec2(random(rng_state), random(rng_state))) / vec2(image_size);
            ray_origin = view_position + (uv - 0.5) * view_size;
            float angle = 2.0 * PI * random(rng_state);
            ray_direction = vec2(cos(angle), sin(angle));
            radiance = vec3(0.0);
            throughput = vec3(1.0);
            depth = 0;
            alive = true;
        }

        if (depth >= 32)
        {
            accumulated_color += vec4(radiance, 1.0);
            alive = false;
            continue;
        }

        // Russian Roulette termination
        // NOTE: this is equivalent to doing it on the previous iteration
        // just before any "continue" and at the end of the loop body.
        if (depth >= 4)
        {
            float survival_prob = clamp(max(throughput.r, max(throughput.g, throughput.b)), 0.05, 1.0);
            if (random(rng_state) >= survival_prob)
            {
                accumulated_color += vec4(radiance, 1.0);
                alive = false;
                continue;
            }
            throughput /= survival_prob;
        }
        ++depth;

        float t;
        vec2 local;
        uint geometry_type;
        uint geometry_index;
        uint volume_mask;
        bool is_hit = intersect(ray_origin, ray_direction, t, local, geometry_type, geometry_index, volume_mask);
        if (!is_hit)
        {
            accumulated_color += vec4(radiance, 1.0);
            alive = false;
            continue;
        }

        Hit hit = get_hit(ray_origin, ray_direction, t, local, geometry_type, geometry_index);

        if (volume_mask != 0u)
        {
            uint volume_id = lsb_index(volume_mask);
            Volume volume = volumes[volume_id];
            bool scatter = evaluate_volume(volume, t, ray_origin, ray_direction, throughput, rng_state);
            if (scatter)
            {
                continue;
            }
        }
        
        if (hit.surface_id == INVALID_ID)
        {
            // No surface interaction, continue tracing in the same direction
            vec2 inside_normal = dot(ray_direction, hit.normal) >= 0.0 ? hit.normal : -hit.normal;
            ray_origin = offset_position_along_normal(hit.position, inside_normal);
            continue;
        }

        Surface surface = surfaces[hit.surface_id];

        radiance += throughput * surface.emission_color * surface.emission_strength;

        float pdf;
        bool is_delta;
        evaluate_surface(hit, surface, ray_origin, ray_direction, throughput, pdf, is_delta, rng_state);
    }
    
    out_color = accumulated_color;
}

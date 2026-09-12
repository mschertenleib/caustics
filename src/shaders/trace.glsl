
precision highp float;



struct Material
{
    vec3 base_color;
    vec3 emissive_color;
    float emissive_strength;
    int type;
    float ior;
};

struct Circle
{
    vec2 center;
    float radius;
    uint material_id;
};

struct Line
{
    vec2 a;
    vec2 b;
    uint material_id;
};

struct Arc
{
    vec2 center;
    float radius;
    vec2 a;
    float b;
    uint material_id;
};

struct Hit
{
    vec2 position;
    vec2 normal;
    uint material_id;
};



// FIXME
layout(std140) uniform Materials { Material materials[MATERIAL_COUNT > 0 ? MATERIAL_COUNT : 1]; };
layout(std140) uniform Circles { Circle circles[CIRCLE_COUNT > 0 ? CIRCLE_COUNT : 1]; };
layout(std140) uniform Lines { Line lines[LINE_COUNT > 0 ? LINE_COUNT : 1]; };
layout(std140) uniform Arcs { Arc arcs[ARC_COUNT > 0 ? ARC_COUNT : 1]; };

uniform int sample_index;
uniform int samples_per_frame;
uniform vec2 view_position;
uniform vec2 view_size;
uniform uvec2 image_size;

out vec4 out_color;



const float pi = 3.1415926535897931;

const int geometry_none = 0;
const int geometry_circle = 1;
const int geometry_line = 2;
const int geometry_arc = 3;

const int material_diffuse = 0;
const int material_specular = 1;
const int material_dielectric = 2;



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

bool intersect_circle(vec2 origin, vec2 direction, vec2 center, float radius, inout float t)
{
    vec2 oc = center - origin;
    float oc_dot_dir = dot(oc, direction);
    float discriminant = oc_dot_dir * oc_dot_dir - dot(oc, oc) + radius * radius;
    if (discriminant < 0.0)
    {
        return false;
    }

    float sqrt_discriminant = sqrt(discriminant);
    float t1 = oc_dot_dir - sqrt_discriminant;
    if (t1 > 0.0 && t1 < t)
    {
        t = t1;
        return true;
    }

    float t2 = oc_dot_dir + sqrt_discriminant;
    if (t2 > 0.0 && t2 < t)
    {
        t = t2;
        return true;
    }

    return false;
}

bool intersect_line(vec2 origin, vec2 direction, vec2 a, vec2 b, inout float t, inout float u)
{
    vec2 ab = b - a;
    float determinant = direction.x * ab.y - direction.y * ab.x;
    if (abs(determinant) < 1e-6) // Parallel
    {
        return false;
    }

    mat2 mat_inv = 1.0 / determinant * mat2(-direction.y, -ab.y, direction.x, ab.x);
    vec2 result = mat_inv * (origin - a);
    float intersection_u = result.x;
    float intersection_t = result.y;
    if (intersection_t > 0.0 && intersection_t < t && intersection_u >= 0.0 && intersection_u <= 1.0)
    {
        t = intersection_t;
        u = intersection_u;
        return true;
    }

    return false;
}

bool intersect_arc(vec2 origin, vec2 direction, vec2 center, float radius, vec2 a, float b, inout float t)
{
    vec2 oc = center - origin;
    float oc_dot_dir = dot(oc, direction);
    float discriminant = oc_dot_dir * oc_dot_dir - dot(oc, oc) + radius * radius;
    if (discriminant < 0.0)
    {
        return false;
    }

    float sqrt_discriminant = sqrt(discriminant);
    float t1 = oc_dot_dir - sqrt_discriminant;
    if (t1 > 0.0 && t1 < t)
    {
        vec2 rel_hit_pos = origin + t1 * direction - center;
        if (dot(a, rel_hit_pos) >= b)
        {
            t = t1;
            return true;
        }
    }
    
    float t2 = oc_dot_dir + sqrt_discriminant;
    if (t2 > 0.0 && t2 < t)
    {
        vec2 rel_hit_pos = origin + t2 * direction - center;
        if (dot(a, rel_hit_pos) >= b)
        {
            t = t2;
            return true;
        }
    }

    return false;
}

bool intersect(vec2 origin, vec2 direction, out float t, out float u, out int geometry_type, out int geometry_index)
{
    t = 1e6;
    u = 0.0;
    geometry_type = geometry_none;
    geometry_index = -1;

    for (int i = 0; i < CIRCLE_COUNT; ++i)
    {
        if (intersect_circle(origin, direction, circles[i].center, circles[i].radius, t))
        {
            geometry_type = geometry_circle;
            geometry_index = i;
        }
    }
    for (int i = 0; i < LINE_COUNT; ++i)
    {
        if (intersect_line(origin, direction, lines[i].a, lines[i].b, t, u))
        {
            geometry_type = geometry_line;
            geometry_index = i;
        }
    }
    for (int i = 0; i < ARC_COUNT; ++i)
    {
        if (intersect_arc(origin, direction, arcs[i].center, arcs[i].radius, arcs[i].a, arcs[i].b, t))
        {
            geometry_type = geometry_arc;
            geometry_index = i;
        }
    }

    return geometry_type != geometry_none;
}

Hit get_hit(vec2 origin, vec2 direction, float t, float u, int geometry_type, int geometry_index)
{
    Hit hit;

    switch(geometry_type)
    {
    case geometry_circle:
    {
        Circle circle = circles[geometry_index];
        hit.position = origin + t * direction;
        // A negative radius means the object normal (defining the
        // "outside" of solid objects) points towards the center
        hit.normal = sign(circle.radius) * normalize(hit.position - circle.center);
        // Re-project the hit position onto the circle
        hit.position = circle.center + hit.normal * circle.radius;
        hit.material_id = circle.material_id;
        break;
    }
    case geometry_line:
    {
        Line line = lines[geometry_index];
        // FIXME: can use mix() ?
        hit.position = line.a + u * (line.b - line.a);
        vec2 line_dir = normalize(line.b - line.a);
        hit.normal = vec2(line_dir.y, -line_dir.x);
        hit.material_id = line.material_id;
        break;
    }
    case geometry_arc:
    {
        Arc arc = arcs[geometry_index];
        hit.position = origin + t * direction;
        // A negative radius means the object normal (defining the
        // "outside" of solid objects) points towards the center
        hit.normal = sign(arc.radius) * normalize(hit.position - arc.center);
        // Re-project the hit position onto the arc
        hit.position = arc.center + hit.normal * arc.radius;
        hit.material_id = arc.material_id;
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
        intBitsToFloat(floatBitsToInt(position.y) + ((position.y < 0.0) ? -of_i.y : of_i.y)));
    // Use a floating-point offset instead for points near (0,0), the origin.
    const float origin = 1.0 / 32.0;
    const float float_scale = 1.0 / 65536.0;
    return vec2(
        abs(position.x) < origin ? position.x + float_scale * normal.x : p_i.x,
        abs(position.y) < origin ? position.y + float_scale * normal.y : p_i.y);
}

vec2 sample_diffuse(vec2 normal, inout uint rng_state)
{
    float sin_theta = 2.0 * random(rng_state) - 1.0;
    float cos_theta = sqrt(1.0 - sin_theta * sin_theta);
    vec2 tangent = vec2(-normal.y, normal.x);
    return cos_theta * normal + sin_theta * tangent;
}

void evaluate_material(
    Hit hit,
    Material material,
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

    if (material.type == material_diffuse) 
    {
        ray_origin = offset_position_along_normal(hit.position, normal);
        ray_direction = sample_diffuse(normal, rng_state);
        throughput *= material.base_color;
    } 
    else if (material.type == material_specular) 
    {
        ray_origin = offset_position_along_normal(hit.position, normal);
        ray_direction = reflect(ray_direction, normal);
        
        vec3 f0 = material.base_color;
        float c = max(1.0 - cos_theta_i, 0.0);
        vec3 fresnel_reflectance = f0 + (1.0 - f0) * (c * c * c * c * c);
        throughput *= fresnel_reflectance; 
    } 
    else if (material.type == material_dielectric) 
    {
        const float n_vacuum = 1.0;
        float n_A = is_entering ? n_vacuum : material.ior;
        float n_B = is_entering ? material.ior : n_vacuum;
        float relative_ior = n_A / n_B;
        float sin2_theta_t = (relative_ior * relative_ior) * (1.0 - cos_theta_i * cos_theta_i);

        if (sin2_theta_t > 1.0) // Total internal reflection
        {
            ray_origin = offset_position_along_normal(hit.position, normal);
            ray_direction = reflect(ray_direction, normal);
            return;
        }

        float cos_theta_t = sqrt(1.0 - sin2_theta_t);
        float r0 = (n_A - n_B) / (n_A + n_B);
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
            ray_direction = relative_ior * ray_direction + (relative_ior * cos_theta_i - cos_theta_t) * normal;
            throughput *= material.base_color * relative_ior;
        }
    }
}

vec3 compute_radiance(vec2 origin, vec2 direction, inout uint rng_state)
{
    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);

    for (int depth = 0; depth <= 32; ++depth)
    {
        float t;
        float u;
        int geometry_type;
        int geometry_index;
        bool is_hit = intersect(origin, direction, t, u, geometry_type, geometry_index);

        if (!is_hit)
        {
            //const vec3 environment_emission = vec3(0.2, 0.2, 0.2);
            //radiance += throughput * environment_emission;
            break;
        }

        Hit hit = get_hit(origin, direction, t, u, geometry_type, geometry_index);
        Material material = materials[hit.material_id];

        // FIXME: we need to keep track of the IOR, not use vacuum.

// FIXME
// ALso should go before !is_hit check? Maybe unnecessary since we probably don't want
// to allow scattering everywhere, even if this could technically be implied by some
/// ill-formed, non-closed dielectric geometries).
#if 0
        if (material.type == material_dielectric && dot(direction, hit.normal) > 0.0)
        {
            const float sigma_a = 0.0;
            const float sigma_s = 5.0;
            const float sigma_t = sigma_a + sigma_s;
            float scatter_distance = -log(1.0 - random(rng_state)) / sigma_t;
            if (scatter_distance < t)
            {
                origin += direction * scatter_distance;
                throughput *= sigma_s / sigma_t;
                float angle = 2.0 * pi * random(rng_state);
                direction = vec2(cos(angle), sin(angle));
                continue;
            }
            throughput *= exp(-sigma_t * t);
        }
#endif

        radiance += throughput * material.emissive_color * material.emissive_strength;

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

        evaluate_material(hit, material, origin, direction, throughput, rng_state);
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
        float angle = 2.0 * pi * random(rng_state);
        vec2 ray_direction = vec2(cos(angle), sin(angle));
        vec3 radiance = compute_radiance(ray_origin, ray_direction, rng_state);
        accumulated_color += vec4(radiance, 1.0);
    }

    out_color = accumulated_color / float(samples_per_frame);
}

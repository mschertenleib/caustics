
precision highp float;

#ifdef COMPUTE_SHADER
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
#endif


struct Material
{
    vec3 color;
    vec3 emissivity;
    int type;
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


#ifdef COMPUTE_SHADER
layout(rgba32f, binding = 0) uniform restrict image2D accumulation_image;
#endif

layout(std140) uniform Materials { Material materials[MATERIAL_COUNT]; };
layout(std140) uniform Circles { Circle circles[CIRCLE_COUNT]; };
layout(std140) uniform Lines { Line lines[LINE_COUNT]; };
layout(std140) uniform Arcs { Arc arcs[ARC_COUNT]; };


uniform int sample_index;
uniform int samples_per_frame;
uniform vec2 view_position;
uniform vec2 view_size;

#ifndef COMPUTE_SHADER
uniform uvec2 image_size;
out vec4 out_color;
#endif


#define PI 3.1415926535897931

#define DIFFUSE 0
#define SPECULAR 1
#define DIELECTRIC 2

#define GEOMETRY_NONE 0
#define GEOMETRY_CIRCLE 1
#define GEOMETRY_LINE 2
#define GEOMETRY_ARC 3


uint hash(uint x)
{
    x += x << 10;
    x ^= x >> 6;
    x += x << 3;
    x ^= x >> 11;
    x += x << 15;
    return x;
}

float random(inout uint rng_state)
{
    // Returns a value uniformly sampled from [0.0, 1.0]
    // NOTE: can return 1.0 because of floating-point rounding !
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return float(rng_state) / 4294967296.0;
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

bool intersect_line(vec2 origin, vec2 direction, vec2 a, vec2 b, inout float t, out float u)
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
    geometry_type = GEOMETRY_NONE;
    geometry_index = -1;

    for (int i = 0; i < CIRCLE_COUNT; ++i)
    {
        if (intersect_circle(origin, direction, circles[i].center, circles[i].radius, t))
        {
            geometry_type = GEOMETRY_CIRCLE;
            geometry_index = i;
        }
    }
    for (int i = 0; i < LINE_COUNT; ++i)
    {
        if (intersect_line(origin, direction, lines[i].a, lines[i].b, t, u))
        {
            geometry_type = GEOMETRY_LINE;
            geometry_index = i;
        }
    }
    for (int i = 0; i < ARC_COUNT; ++i)
    {
        if (intersect_arc(origin, direction, arcs[i].center, arcs[i].radius, arcs[i].a, arcs[i].b, t))
        {
            geometry_type = GEOMETRY_ARC;
            geometry_index = i;
        }
    }

    return geometry_type != GEOMETRY_NONE;
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

Hit get_hit(vec2 origin, vec2 direction, float t, float u, int geometry_type, int geometry_index)
{
    Hit hit;

    switch(geometry_type)
    {
    case GEOMETRY_CIRCLE:
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
    case GEOMETRY_LINE:
    {
        Line line = lines[geometry_index];
        hit.position = line.a + u * (line.b - line.a);
        vec2 line_dir = normalize(line.b - line.a);
        hit.normal = vec2(line_dir.y, -line_dir.x);
        hit.material_id = line.material_id;
        break;
    }
    case GEOMETRY_ARC:
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

vec2 reflect_diffuse(vec2 normal, inout uint rng_state)
{
#if 0 // Uniform
    float angle = 2.0 * PI * random(rng_state);
    return normalize(normal + vec2(cos(angle), sin(angle)));
#else // Cosine weighted
    vec2 tangent = vec2(-normal.y, normal.x);
    float u = random(rng_state);
    float sin_theta = 2.0 * u - 1.0;
    float cos_theta = sqrt(1.0 - sin_theta * sin_theta);
    return cos_theta * normal + sin_theta * tangent;
#endif
}

vec3 radiance(vec2 origin, vec2 direction, inout uint rng_state)
{
    vec3 accumulated_color = vec3(0.0);
    vec3 accumulated_reflectance = vec3(1.0);

    const int max_depth = 32;
    for (int depth = 0; depth <= max_depth; ++depth)
    {
        float t;
        float u;
        int geometry_type;
        int geometry_index;
        bool is_hit = intersect(origin, direction, t, u, geometry_type, geometry_index);

        if (!is_hit)
        {
            return accumulated_color;
        }

        Hit hit = get_hit(origin, direction, t, u, geometry_type, geometry_index);
        Material material = materials[hit.material_id];
        
        accumulated_color += accumulated_reflectance * material.emissivity;

        vec3 color = material.color;
        float max_color = max(color.r, max(color.g, color.b));
        // Russian Roulette ray termination
        if (random(rng_state) < max_color && depth < max_depth)
        {
            color /= max_color;
        }
        else
        {
            return accumulated_color;
        }

        accumulated_reflectance *= color;

        // hit.normal: object normal, defining "inside" and "outside" for relevant
        //             primitives (circles and dielectric primitives)
        // normal:     front-facing normal as seen by the incoming ray
        bool into = dot(direction, hit.normal) < 0.0;
        vec2 normal = into ? hit.normal : -hit.normal;
        
        switch (material.type)
        {
        case DIFFUSE:
        {
            origin = offset_position_along_normal(hit.position, normal);
            direction = reflect_diffuse(normal, rng_state);
            break;
        }
        case SPECULAR:
        {
            origin = offset_position_along_normal(hit.position, normal);
            direction = reflect(direction, normal);
            break;
        }
        case DIELECTRIC:
        {
            vec2 reflected_dir = reflect(direction, hit.normal);

            const float n_air = 1.0;
            const float n_glass = 1.5;
            float n_ratio = into ? n_air / n_glass : n_glass / n_air;
            float dir_dot_normal = dot(direction, normal);
            float cos2t = 1.0 - n_ratio * n_ratio * (1.0 - dir_dot_normal * dir_dot_normal);
            // Total internal reflection
            if (cos2t < 0.0)
            {
                origin = offset_position_along_normal(hit.position, normal);
                direction = reflected_dir;
                continue;
            }

            vec2 transmitted_dir = normalize(direction * n_ratio - hit.normal *
                ((into ? 1.0 : -1.0) * (dir_dot_normal * n_ratio + sqrt(cos2t))));

            float a = n_glass - n_air;
            float b = n_glass + n_air;
            float R0 = a * a / (b * b);
            float c = 1.0 - (into ? -dir_dot_normal : dot(transmitted_dir, hit.normal));
            float Re = R0 + (1.0 - R0) * c * c * c * c * c;
            float Tr = 1.0 - Re;
            float P = 0.25 + 0.5 * Re;
            float RP = Re / P;
            float TP = Tr / (1.0 - P);
            if (random(rng_state) < P)
            {
                accumulated_reflectance *= RP;
                // FIXME: I feel like we should be using hit.normal here.
                // We should really double check the entire refraction code.
                origin = offset_position_along_normal(hit.position, normal);
                direction = reflected_dir;
            }
            else
            {
                accumulated_reflectance *= TP;
                origin = offset_position_along_normal(hit.position, -normal);
                direction = transmitted_dir;
            }
            break;
        }
        }
    }

    // NOTE: this is unreachable. If we reach the last iteration,
    // we will return before computing a new ray.
    return accumulated_color;
}

void main()
{
#ifdef COMPUTE_SHADER
    uvec2 image_size = imageSize(accumulation_image);
    if (gl_GlobalInvocationID.x >= image_size.x || gl_GlobalInvocationID.y >= image_size.y)
    {
        return;
    }
    uvec2 pixel = gl_GlobalInvocationID.xy;
#else
    uvec2 pixel = uvec2(gl_FragCoord.xy);
#endif

    uint pixel_index = pixel.y * image_size.x + pixel.x;
    uint rng_state = hash(pixel_index) + hash(uint(sample_index));

    vec4 accumulated_color = vec4(0.0);
    for (int i = 0; i < samples_per_frame; ++i)
    {
        vec2 uv = (vec2(pixel) + vec2(random(rng_state), random(rng_state))) / vec2(image_size);
        vec2 ray_origin = view_position + (uv - 0.5) * view_size;
        float angle = 2.0 * PI * random(rng_state);
        vec2 ray_direction = vec2(cos(angle), sin(angle));
        accumulated_color += vec4(radiance(ray_origin, ray_direction, rng_state), 1.0);
    }
    
#ifdef COMPUTE_SHADER
    vec4 average_color = imageLoad(accumulation_image, ivec2(gl_GlobalInvocationID.xy));
    average_color = (average_color * sample_index + accumulated_color) / (sample_index + samples_per_frame);
    imageStore(accumulation_image, ivec2(gl_GlobalInvocationID.xy), average_color);
#else
    out_color = accumulated_color / float(samples_per_frame);
#endif
}

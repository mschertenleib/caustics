
precision highp float;



struct Material
{
    vec3 base_color;
    vec3 emissive_color;
    float emissive_strength;
    uint type;
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

float random(inout uint rng_state)
{
    // Returns a value uniformly sampled in [0.0, 1.0)

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

// FIXME: this might be completely irrelevant in our 2D case

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

#if 0
vec2 sample_diffuse(vec2 normal, inout uint rng_state)
{
    vec2 tangent = vec2(-normal.y, normal.x);
    float u = random(rng_state);
    float sin_theta = 2.0 * u - 1.0;
    float cos_theta = sqrt(1.0 - sin_theta * sin_theta);
    return cos_theta * normal + sin_theta * tangent;
}

vec2 sample_ggx(vec2 normal, float roughness, inout uint rng_state)
{
    float alpha = roughness * roughness;
    float u = random(rng_state);
    
    // Inverse CDF of the 1D GGX slope distribution
    float slope = alpha * tan(pi * (u - 0.5));
    
    vec2 tangent = vec2(-normal.y, normal.x);

    // Half-vector (microfacet normal)
    vec2 h = normalize(normal - slope * tangent);
    return h;
}

void evaluate_material(
    Hit hit,
    Material mat,
    inout vec2 ray_origin,
    inout vec2 ray_dir,
    inout vec3 throughput,
    inout uint rng_state)
{
    // hit.normal: object normal, defining "inside" and "outside" where relevant
    // normal:     front-facing normal as seen by the incoming ray
    bool into = dot(ray_dir, hit.normal) < 0.0;
    vec2 normal = into ? hit.normal : -hit.normal;

    const float n_air = 1.0;
    float n_mat = mat.ior;
    float eta = into ? n_air / n_mat : n_mat / n_air;

    // Microfacet normal
    vec2 h = sample_ggx(normal, mat.roughness, rng_state);
    
    // Fresnel
    float n_diff = n_mat - n_air;
    float n_sum = n_mat + n_air;
    float F0_dielectric = n_diff * n_diff / (n_sum * n_sum);
    vec3 F0 = mix(vec3(F0_dielectric), mat.base_color, mat.metallic);
    float abs_cos_I = min(abs(dot(ray_dir, h)), 1.0);
    float cos_I_1m = 1.0 - abs_cos_I;
    vec3 F = F0 + (1.0 - F0) * cos_I_1m * cos_I_1m * cos_I_1m * cos_I_1m * cos_I_1m;

    // Masking ratio for importance sampled GGX
    float k = mat.roughness * 0.5;
    float n_dot_dir = max(dot(normal, -ray_dir), 1e-4);
    float G_ratio = n_dot_dir / (n_dot_dir * (1.0 - k) + k);

    float p_metal = mat.metallic;
    float p_trans = (1.0 - mat.metallic) * mat.transmission;
    float p_diffuse = (1.0 - mat.metallic) * (1.0 - mat.transmission);
    float selector = random(rng_state);

    if (selector < p_metal) // Metallic lobe
    {
        ray_dir = reflect(ray_dir, h);
        throughput *= F * G_ratio;
        ray_origin = offset_position_along_normal(hit.position, normal);
    } 
    else if (selector < p_metal + p_trans) // Transmissive lobe
    {
        float cos_I = -dot(ray_dir, h);
        float sin_2T = eta * eta * (1.0 - cos_I * cos_I);
        
        vec2 refracted_dir = vec2(0.0);
        bool can_refract = false;
        if (sin_2T <= 1.0) // No total internal reflection
        {
            float cos_T = sqrt(1.0 - sin_2T);
            refracted_dir = eta * ray_dir + (eta * cos_I - cos_T) * h;
            can_refract = true;
        }

        float F_avg = (F.r + F.g + F.b) / 3.0;

        if (!can_refract || random(rng_state) < F_avg)
        {
            // Reflection
            ray_dir = reflect(ray_dir, h);
            throughput *= G_ratio; 
            ray_origin = offset_position_along_normal(hit.position, normal);
        } 
        else
        {
            // Refraction
            ray_dir = refracted_dir;
            throughput *= mat.base_color * G_ratio * eta;
            ray_origin = offset_position_along_normal(hit.position, -normal);
        }
    }
    else // Diffuse lobe
    {
        ray_dir = sample_diffuse(normal, rng_state);
        throughput *= mat.base_color;
        ray_origin = offset_position_along_normal(hit.position, normal);
    }
}


// -----------------------------------------------------------------------------
// 2D cosine-weighted diffuse sampling
// -----------------------------------------------------------------------------

vec2 sample_diffuse(vec2 normal, inout uint rng_state)
{
    vec2 tangent = vec2(-normal.y, normal.x);

    float u = random(rng_state);

    // PDF(theta) = cos(theta) / 2
    float sin_theta = 2.0 * u - 1.0;
    float cos_theta = sqrt(max(0.0, 1.0 - sin_theta * sin_theta));

    return cos_theta * normal + sin_theta * tangent;
}


// -----------------------------------------------------------------------------
// Logistic 2D microfacet distribution
//
// D(theta) = [1 / (4s)] coth(pi / (4s)) sech^2(theta / (2s))
//
// theta is the signed angle between the microfacet normal and the macroscopic
// surface normal.
//
// Unlike 3D GGX, this is specifically chosen because it has a tractable,
// normalized 2D representation.
// -----------------------------------------------------------------------------

float logistic_cdf(float theta, float s)
{
    float t = tanh(pi / (4.0 * s));
    return 0.5 * (1.0 + tanh(theta / (2.0 * s)) / t);
}


float sample_logistic_visible_normal(
    float theta_i,
    float roughness,
    inout uint rng_state)
{
    // All microfacet normals satisfying dot(wi, h) > 0.
    //
    // If theta_i is the angle of wi relative to the macroscopic normal,
    // the visible interval is:
    //
    // theta_min = max(theta_i, 0) - pi/2
    // theta_max = min(theta_i, 0) + pi/2

    float theta_min = max(theta_i, 0.0) - PI_HALF;
    float theta_max = min(theta_i, 0.0) + PI_HALF;

    // Avoid an exactly zero roughness value.
    float s = max(roughness, 1e-4);

    float a = tanh(theta_min / (2.0 * s));
    float b = tanh(theta_max / (2.0 * s));

    float u = random(rng_state);

    float x = mix(a, b, u);

    return 2.0 * s * atanh(x);
}


vec2 sample_visible_microfacet(
    vec2 wi,
    vec2 normal,
    float roughness,
    inout uint rng_state)
{
    vec2 tangent = vec2(-normal.y, normal.x);

    // wi = sin(theta_i) * tangent + cos(theta_i) * normal
    float sin_theta_i = dot(wi, tangent);
    float theta_i = asin(clamp(sin_theta_i, -1.0, 1.0));

    float theta_h =
        sample_logistic_visible_normal(
            theta_i,
            roughness,
            rng_state);

    return sin(theta_h) * tangent + cos(theta_h) * normal;
}


// -----------------------------------------------------------------------------
// Exact dielectric Fresnel
//
// cos_i is measured against the microfacet normal and is positive.
// eta = n_i / n_t.
// -----------------------------------------------------------------------------

float dielectric_fresnel(
    float cos_i,
    float eta,
    out bool total_internal_reflection)
{
    cos_i = clamp(cos_i, 0.0, 1.0);

    float sin2_i = max(0.0, 1.0 - cos_i * cos_i);
    float sin2_t = eta * eta * sin2_i;

    if (sin2_t >= 1.0)
    {
        total_internal_reflection = true;
        return 1.0;
    }

    total_internal_reflection = false;

    float cos_t = sqrt(max(0.0, 1.0 - sin2_t));

    float rs =
        (cos_i - eta * cos_t) /
        (cos_i + eta * cos_t);

    float rp =
        (eta * cos_i - cos_t) /
        (eta * cos_i + cos_t);

    return 0.5 * (rs * rs + rp * rp);
}


// -----------------------------------------------------------------------------
// Refraction through a microfacet.
//
// wi points away from the surface, i.e. toward the previous path vertex.
// h points into the incident medium.
// eta = n_i / n_t.
//
// Returns false on TIR.
// -----------------------------------------------------------------------------

bool refract_microfacet(
    vec2 wi,
    vec2 h,
    float eta,
    out vec2 wo)
{
    float cos_i = dot(wi, h);

    if (cos_i <= 0.0)
        return false;

    float sin2_t = eta * eta * max(0.0, 1.0 - cos_i * cos_i);

    if (sin2_t >= 1.0)
        return false;

    float cos_t = sqrt(max(0.0, 1.0 - sin2_t));

    // wi and h are on the incident side.
    // The resulting wo is on the transmitted side.
    wo = -eta * wi + (eta * cos_i - cos_t) * h;

    return true;
}


// -----------------------------------------------------------------------------
// Reflection from a microfacet.
// -----------------------------------------------------------------------------

vec2 reflect_microfacet(vec2 wi, vec2 h)
{
    // wi points away from the surface.
    // The reflected direction is also away from the surface.
    return 2.0 * dot(wi, h) * h - wi;
}


// -----------------------------------------------------------------------------
// Main material evaluation
//
// Direction convention:
//     ray_dir : direction in which the path ray travels
//     wi      : direction away from the surface toward the previous vertex
//
// Thus:
//     wi = -ray_dir
//
// The implementation uses three material classes:
//
//     metallic      -> colored rough conductor
//     transmission  -> rough dielectric
//     diffuse       -> 2D Lambertian
//
// Metallic is treated as a mixture weight, rather than simultaneously being
// used as both a mixture probability and an F0 interpolation parameter.
// -----------------------------------------------------------------------------

void evaluate_material(
    Hit hit,
    Material mat,
    inout vec2 ray_origin,
    inout vec2 ray_dir,
    inout vec3 throughput,
    inout uint rng_state)
{
    // -------------------------------------------------------------------------
    // Orient the normal toward the incident side.
    // -------------------------------------------------------------------------

    bool entering = dot(ray_dir, hit.normal) < 0.0;

    vec2 n = entering ? hit.normal : -hit.normal;

    // wi points away from the surface.
    vec2 wi = -ray_dir;

    // -------------------------------------------------------------------------
    // Material mixture.
    //
    // Metallic is a conductor-vs-dielectric mixture.
    // Transmission determines whether the dielectric component is diffuse
    // or transmissive.
    // -------------------------------------------------------------------------

    float p_metal = clamp(mat.metallic, 0.0, 1.0);

    float p_transmission =
        (1.0 - p_metal) *
        clamp(mat.transmission, 0.0, 1.0);

    float p_diffuse =
        (1.0 - p_metal) *
        (1.0 - clamp(mat.transmission, 0.0, 1.0));

    float selector = random(rng_state);


    // =========================================================================
    // METAL
    // =========================================================================

    if (selector < p_metal)
    {
        // For a conductor, use the base color as the normal-incidence
        // reflectance. This is the usual RGB metallic workflow approximation.
        //
        // Unlike a dielectric, there is no transmission.

        vec2 h =
            sample_visible_microfacet(
                wi,
                n,
                mat.roughness,
                rng_state);

        float cos_i = max(dot(wi, h), 0.0);

        if (cos_i <= EPS)
        {
            throughput = vec3(0.0);
            return;
        }

        // Schlick conductor approximation.
        vec3 F =
            mat.base_color +
            (vec3(1.0) - mat.base_color) *
            pow(1.0 - cos_i, 5.0);

        vec2 wo = reflect_microfacet(wi, h);

        // The visible-normal sampling procedure only samples microfacets
        // visible from wi. Nevertheless, reject numerical invalid samples.
        float cos_o = dot(wo, n);

        if (cos_o <= EPS)
        {
            throughput = vec3(0.0);
            return;
        }

        // This is analog sampling of the rough-mirror model:
        // the sampled branch carries its Fresnel/conductor reflectance.
        //
        // Since p_metal is the actual mixture weight, it does not need to
        // appear here again.
        throughput *= F;

        ray_dir = wo;
        ray_origin =
            offset_position_along_normal(
                hit.position,
                n);

        return;
    }


    // =========================================================================
    // DIELECTRIC TRANSMISSION
    // =========================================================================

    if (selector < p_metal + p_transmission)
    {
        // The oriented normal points into the incident medium.
        //
        // For a ray entering the material:
        //     n_i = air
        //     n_t = material
        //
        // For a ray leaving it:
        //     n_i = material
        //     n_t = air

        float n_i = entering ? 1.0 : mat.ior;
        float n_t = entering ? mat.ior : 1.0;

        float eta = n_i / n_t;

        vec2 h =
            sample_visible_microfacet(
                wi,
                n,
                mat.roughness,
                rng_state);

        float cos_i = dot(wi, h);

        if (cos_i <= EPS)
        {
            throughput = vec3(0.0);
            return;
        }

        bool tir;
        float F =
            dielectric_fresnel(
                cos_i,
                eta,
                tir);

        // ---------------------------------------------------------------------
        // Reflection
        // ---------------------------------------------------------------------

        if (tir || random(rng_state) < F)
        {
            vec2 wo = reflect_microfacet(wi, h);

            if (dot(wo, n) <= EPS)
            {
                throughput = vec3(0.0);
                return;
            }

            // IMPORTANT:
            //
            // F is a scalar here, and it is exactly the probability with
            // which reflection was selected. Therefore the Fresnel factor
            // cancels against the roulette probability.
            //
            // The base color is used as the transmitted tint below, not here.

            ray_dir = wo;
            ray_origin =
                offset_position_along_normal(
                    hit.position,
                    n);

            return;
        }


        // ---------------------------------------------------------------------
        // Refraction
        // ---------------------------------------------------------------------

        vec2 wo;

        if (!refract_microfacet(wi, h, eta, wo))
        {
            // Numerically possible only near the TIR boundary.
            throughput = vec3(0.0);
            return;
        }

        if (dot(wo, n) >= -EPS)
        {
            throughput = vec3(0.0);
            return;
        }

        // Fresnel roulette already accounts for the (1-F) probability.
        //
        // base_color is used here as a simple transmission tint. This is a
        // surface/thin-transmission approximation, not Beer-Lambert volume
        // absorption.
        throughput *= mat.base_color;

        ray_dir = wo;
        ray_origin =
            offset_position_along_normal(
                hit.position,
                -n);

        return;
    }


    // =========================================================================
    // DIFFUSE
    // =========================================================================

    {
        vec2 wo =
            sample_diffuse(
                n,
                rng_state);

        // 2D Lambertian:
        //
        //     f_d = base_color / 2
        //
        // cosine-weighted sampling:
        //
        //     p(wo) = cos(theta_o) / 2
        //
        // Therefore:
        //
        //     f_d cos(theta_o) / p(wo) = base_color

        throughput *= mat.base_color;

        ray_dir = wo;
        ray_origin =
            offset_position_along_normal(
                hit.position,
                n);
    }
}

#endif



vec2 sample_diffuse(vec2 normal, inout uint rng_state)
{
    vec2 tangent = vec2(-normal.y, normal.x);
    float u = random(rng_state);
    float sin_theta = 2.0 * u - 1.0;
    float cos_theta = sqrt(1.0 - sin_theta * sin_theta);
    return cos_theta * normal + sin_theta * tangent;
}

void evaluate_material(
    Hit hit,
    Material material,
    inout vec2 ray_origin,
    inout vec2 ray_dir,
    inout vec3 throughput,
    inout uint rng_state)
{
    // hit.normal: object normal, defining "inside" and "outside" for relevant primitives
    // normal:     front-facing normal as seen by the incoming ray
    bool into = dot(ray_dir, hit.normal) < 0.0;
    vec2 normal = into ? hit.normal : -hit.normal;
    
    switch (material.type)
    {
        case material_diffuse:
        {
            ray_origin = offset_position_along_normal(hit.position, normal);
            ray_dir = sample_diffuse(normal, rng_state);
            break;
        }
        case material_specular:
        {
            ray_origin = offset_position_along_normal(hit.position, normal);
            ray_dir = reflect(ray_dir, normal);
            break;
        }
        case material_dielectric:
        {
            vec2 reflected_dir = reflect(ray_dir, hit.normal);

            const float n_air = 1.0;
            float n_ratio = into ? n_air / material.ior : material.ior / n_air;
            float dir_dot_normal = dot(ray_dir, normal);
            float cos2t = 1.0 - n_ratio * n_ratio * (1.0 - dir_dot_normal * dir_dot_normal);
            // Total internal reflection
            if (cos2t < 0.0)
            {
                ray_origin = offset_position_along_normal(hit.position, normal);
                ray_dir = reflected_dir;
                break;
            }

            vec2 transmitted_dir = normalize(ray_dir * n_ratio - hit.normal *
                ((into ? 1.0 : -1.0) * (dir_dot_normal * n_ratio + sqrt(cos2t))));

            float a = material.ior - n_air;
            float b = material.ior + n_air;
            float R0 = a * a / (b * b);
            float c = 1.0 - (into ? -dir_dot_normal : dot(transmitted_dir, hit.normal));
            float Re = R0 + (1.0 - R0) * c * c * c * c * c;
            float Tr = 1.0 - Re;
            float P = 0.25 + 0.5 * Re;
            float RP = Re / P;
            float TP = Tr / (1.0 - P);
            if (random(rng_state) < P)
            {
                throughput *= RP;
                ray_origin = offset_position_along_normal(hit.position, normal);
                ray_dir = reflected_dir;
            }
            else
            {
                throughput *= TP;
                ray_origin = offset_position_along_normal(hit.position, -normal);
                ray_dir = transmitted_dir;
            }
            break;
        }
    }
}

vec3 compute_radiance(vec2 origin, vec2 direction, inout uint rng_state)
{
    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);

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
            return radiance;
        }

        Hit hit = get_hit(origin, direction, t, u, geometry_type, geometry_index);
        Material material = materials[hit.material_id];
        
        radiance += throughput * material.emissive_color * material.emissive_strength;

        vec3 color = material.base_color;
        float max_color = max(color.r, max(color.g, color.b));
        // Russian Roulette ray termination
        if (random(rng_state) < max_color && depth < max_depth)
        {
            color /= max_color;
        }
        else
        {
            return radiance;
        }
        throughput *= color;

        evaluate_material(hit, material, origin, direction, throughput, rng_state);
    }

    // NOTE: this is unreachable. If we reach the last iteration,
    // we will return before computing a new ray.
    return radiance;
}

void main()
{
    uvec2 pixel = uvec2(gl_FragCoord.xy);
    uint pixel_index = pixel.y * image_size.x + pixel.x;
    uint rng_state = hash(pixel_index) ^ hash(uint(sample_index));

    vec3 accumulated_color = vec3(0.0);
    for (int i = 0; i < samples_per_frame; ++i)
    {
        vec2 uv = (vec2(pixel) + vec2(random(rng_state), random(rng_state))) / vec2(image_size);
        vec2 ray_origin = view_position + (uv - 0.5) * view_size;
        float angle = 2.0 * pi * random(rng_state);
        vec2 ray_direction = vec2(cos(angle), sin(angle));
        vec3 radiance = compute_radiance(ray_origin, ray_direction, rng_state);
        accumulated_color += radiance;
    }
    
    out_color = vec4(accumulated_color, 1.0) / float(samples_per_frame);
}

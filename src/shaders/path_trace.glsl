
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
    float clip_x_min;
    float clip_x_max;
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
    vec2 right = vec2(axis.y, -axis.x);

    vec2 ro = origin - parabola.vertex;
    float ox = dot(ro, right);
    float oy = dot(ro, axis);
    float dx = dot(direction, right);
    float dy = dot(direction, axis);
    float f = parabola.focal;
    float a = dx * dx;
    float b = ox * dx - 2.0 * f * dy;
    float c = ox * ox - 4.0 * f * oy;

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

        float x = ox;
        if (x < parabola.clip_x_min || x > parabola.clip_x_max)
        {
            return false;
        }

        parity = 1u;

        if (tr >= t)
        {
            return false;
        }

        t = tr;
        float y = x * x / (4.0 * f);
        local = vec2(x, y);
        return true;
    }

    float dx_sq = dx * dx;
    float dy_sq = dy * dy;
    float dx_dy = dx * dy;
    float term0 = f * dy_sq;
    float term1 = ox * dx_dy;
    float term2 = oy * dx_sq;
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
        float x = t0 * dx + ox;
        if (x >= parabola.clip_x_min && x <= parabola.clip_x_max)
        {
            parity ^= 1u;
            if (t0 < t)
            {
                t = t0;
                float y = x * x / (4.0 * f);
                local = vec2(x, y);
                hit = true;
            }
        }
    }

    if (t1 > 0.0)
    {
        float x = t1 * dx + ox;
        if (x >= parabola.clip_x_min && x <= parabola.clip_x_max)
        {
            parity ^= 1u;
            if (t1 < t)
            {
                t = t1;
                float y = x * x / (4.0 * f);
                local = vec2(x, y);
                hit = true;
            }
        }
    }

    return hit;
}

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
            vec2 right = vec2(parabola.axis.y, -parabola.axis.x);
            hit.position = local.x * right + local.y * parabola.axis + parabola.vertex;
            // Points towards the lower side (away from the axis)
            hit.normal = normalize(local.x * right - 2.0 * parabola.focal * parabola.axis);
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

uint lsb_index(uint u)
{
    uint lsb = u & -u; // keep only LSB = 1 << i = 2^i
    float f = float(lsb);
    uint b = floatBitsToUint(f);
    uint e = (b >> 23u) - 127u; // exponent = log2(f) = i
    return e;
}

void evaluate_surface(
    Hit hit,
    Surface surface,
    inout vec2 ray_origin,
    inout vec2 ray_direction,
    float wavelength,
    inout float throughput,
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
        //throughput *= surface.base_color;
        throughput *= 1.0;
        pdf = max(dot(normal, ray_direction), 0.0) * 0.5;
        is_delta = false;
    } 
    else if (surface.type == SURFACE_SPECULAR) 
    {
        ray_origin = offset_position_along_normal(hit.position, normal);
        ray_direction = reflect(ray_direction, normal);
        
        //vec3 f0 = surface.base_color;
        //float c = max(1.0 - cos_theta_i, 0.0);
        //vec3 fresnel_reflectance = f0 + (1.0 - f0) * (c * c * c * c * c);
        //throughput *= fresnel_reflectance;
        throughput *= 1.0;
        pdf = 1.0;
        is_delta = true;
    } 
    else if (surface.type == SURFACE_DIELECTRIC) 
    {
        //float eta = is_entering ? 1.0 / surface.ior_ratio : surface.ior_ratio;
        float ior_ratio = surface.ior_ratio + 0.4 * (wavelength - 380.0) / 400.0;
        float eta = is_entering ? 1.0 / ior_ratio : ior_ratio;
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
            //throughput *= surface.base_color * eta;
            throughput *= surface.base_color.r * eta;
            pdf = 1.0 - fresnel_reflectance;
            is_delta = true;
        }
    }
}

bool evaluate_volume(Volume volume, float t, inout vec2 ray_origin, inout vec2 ray_direction, float wavelength, inout float throughput, inout uint rng_state)
{
    //float sigma_a = volume_absorption(volume, wavelength);
    //float sigma_s = volume_scattering(volume, wavelength);
    float sigma_a = volume.absorption.r * (0.8 + 1.2 * (wavelength - 380.0) / 400.0);
    float sigma_s = volume.scattering.r * (0.8 + 1.2 * (wavelength - 380.0) / 400.0);
    float sigma_t = sigma_a + sigma_s;

    if (sigma_t <= 0.0)
    {
        return false;
    }

    if (sigma_s <= 0.0)
    {
        throughput *= exp(-sigma_t * t);
        return false;
    }

    float distance = -log(1.0 - random(rng_state)) / sigma_t;

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

        throughput *= sigma_s / sigma_t;

        return true;
    }

    return false;
}


// Fit c0, c1, c2 from target RGB
//
// Or more intuitively define x as:
//
// x = 2 * m / w * ((lambda - p)² - w² / 4)
// m: slope of the transition (0.5 crossing, where x = 0)
// w: width of the peak/trough (distance between the two 0.5 crossings)
// p: center of the peak/trough
//
// This can be converted to the efficient representation:
//
// x = (c0 * lambda + c1) * lambda + c2
// c0 = 2 * m / w
// c1 = 2 * m / w * (-2 * p)
// c2 = 2 * m / w * (p² - w² / 4)
//
float spectrum(float lambda, float c0, float c1, float c2)
{
    float x = (c0 * lambda + c1) * lambda + c2;
    return 0.5 * x * inversesqrt(x * x + 1.0) + 0.5;
}

float gauss_dist_log(float x, float a, float b, float c)
{
    x = (1.0 / c) * (x - b);
    return a - 0.5 * x * x;
}

float gtanh(float x, float a, float b, float c)
{
    return a / (1.0 + exp(c * (b - x)));
}

// CIE color matching functions from Thomas Puls in "Chromaticity Preserving Analytic Approximations
// to the CIE Color Matching Functions", Journal of Computer Graphics Techniques Vol. 14, No. 1, 2025

float cie_1931_cmf_sum(float wavelength)
{
    float x0 = gauss_dist_log(wavelength, 0.728039841, 452.53733, 20.6399874);
    float x1 = gauss_dist_log(wavelength, -0.903768755, 436.50728, 6.80213866);
    float x2 = gauss_dist_log(wavelength, -1.14220357, 426.696451, 5.82897816);
    float x3 = gauss_dist_log(wavelength, 0.418410092, 567.51012, 43.4403059);
    float x4 = gauss_dist_log(wavelength, -0.567908923, 606.986608, 25.572052);

    float x_max = max(max(max(max(x0, x1), x2), x3), x4);
    float cutoff = x_max - 36.737;

    float result = 0.0;
    if (x0 > cutoff) result += exp(x0);
    if (x1 > cutoff) result += exp(x1);
    if (x2 > cutoff) result += exp(x2);
    if (x3 > cutoff) result += exp(x3);
    if (x4 > cutoff) result += exp(x4);

    return result;
}

vec3 cie_1931_chromaticity_xyz(float wavelength)
{
    float y = gtanh(wavelength, 1.60615643, 502.007554, 0.0967029313) +
              gtanh(wavelength, -0.800580541, 513.882498, 0.0881332766) +
              gtanh(wavelength, -0.550389265, 572.073576, 0.0499662324) +
              0.00795843634;
    float z = gtanh(wavelength, 0.878531722, 500.858724, -0.107613031) +
              gtanh(wavelength, -0.0569332683, 470.256269, -0.0756868209);
    return vec3(1.0 - (y + z), y, z);
}

vec3 cie_1931_cmf_xyz(float wavelength)
{
    vec3 xyz = cie_1931_chromaticity_xyz(wavelength);
    float s = cie_1931_cmf_sum(wavelength);
    return s * xyz;
}

vec3 xyz_to_rgb(vec3 xyz)
{
    const mat3 m = mat3(
        3.24096994, -0.96924364, 0.05563008,
        -1.53738318, 1.87596750, -0.20397696,
        -0.49861076, 0.04155506, 1.05697151
    );
    return m * xyz;
}

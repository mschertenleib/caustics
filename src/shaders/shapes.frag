precision highp float;

in vec2 local;
flat in vec4 params;
flat in vec3 color;

out vec4 frag_color;


#define PI 3.14159274
#define FLOAT_MAX 3.40282346e38 // Should be 3.40282347e38, but WebGL flags it as overflowing


// From https://iquilezles.org/articles/distfunctions/
float distance_line(vec2 p, float height)
{
    p.y -= clamp(p.y, 0.0, height);
    return length(p);
}

// From https://iquilezles.org/articles/distfunctions/
float distance_arc(vec2 p, vec2 dir_end, float radius)
{
    p.x = abs(p.x);
    float k = (dir_end.y * p.x > dir_end.x * p.y) ? dot(p, dir_end) : length(p);
    return sqrt(dot(p, p) + radius * radius - 2.0 * radius * k);
}

float cbrt(float x)
{
    return sign(x) * pow(abs(x), 1.0 / 3.0);
}

void test_parabola_point(vec2 p, float t, float f, float lo, float hi, inout float distance_sq)
{
    if (t >= lo && t <= hi)
    {
        vec2 closest = vec2(t, t * t / (4.0 * f));
        vec2 delta = p - closest;
        distance_sq = min(distance_sq, dot(delta, delta));
    }
}

float distance_parabola(vec2 p, float f, float lo, float hi)
{
    // Closest point on the infinite parabola is (t, t²/(4f)),
    // where t solves:
    //
    //   min ||(t, t²/(4f)) - (x, y)||²
    //
    //   => t³ + (8f² - 4fy)t - 8f²x = 0

    float distance_sq = FLOAT_MAX;

    float x = abs(p.x);
    float a = 8.0 * f * f - 4.0 * f * p.y;
    float q = 4.0 * f * f * x;
    float h = q * q + (a * a * a) / 27.0;

    float side = (p.x < 0.0) ? -1.0 : 1.0;

    if (h >= 0.0)  // One real root
    {
        float r = sqrt(h);
        float u = cbrt(q + r) + cbrt(q - r);

        test_parabola_point(p, side * u, f, lo, hi, distance_sq);
    }
    else  // Three real roots
    {        
        float r = 2.0 * sqrt(-a / 3.0);
        float c = q / sqrt(-(a * a * a) / 27.0);
        float theta = acos(clamp(c, -1.0, 1.0)) / 3.0;
        float u0 = r * cos(theta);
        float u1 = r * cos(theta + 2.0 / 3.0 * PI);
        float u2 = r * cos(theta + 4.0 / 3.0 * PI);

        test_parabola_point(p, side * u0, f, lo, hi, distance_sq);
        test_parabola_point(p, side * u1, f, lo, hi, distance_sq);
        test_parabola_point(p, side * u2, f, lo, hi, distance_sq);
    }

    test_parabola_point(p, lo, f, lo, hi, distance_sq);
    test_parabola_point(p, hi, f, lo, hi, distance_sq);

    return sqrt(distance_sq);
}

void main()
{
#if defined(LINE)
    float dist = distance_line(local, params.x);
#elif defined(ARC)
    float dist = distance_arc(local, params.xy, params.z);
#elif defined(PARABOLA)
    float dist = distance_parabola(local, params.x, params.y, params.z);
#endif

    float thickness_radius = params.w;
    // FIXME
    //float pixel_size = fwidth(local.x);
    float pixel_size = length(vec2(dFdx(dist), dFdy(dist)));
    float alpha = 1.0 - smoothstep(-pixel_size * 0.5, pixel_size * 0.5, dist - thickness_radius);

    if (alpha <= 0.0)
    {
        discard;
    }

    frag_color = vec4(color, alpha);
}

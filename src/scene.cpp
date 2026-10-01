#include "scene.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <numbers>
#include <random>
#include <sstream>
#include <string>

namespace
{

constexpr std::uint32_t invalid_child {
    std::numeric_limits<std::uint32_t>::max()};

constexpr AABB empty_aabb {
    .min = {std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity()},
    .max = {-std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()},
};

enum struct Primitive_type : std::uint32_t
{
    line = 1,
    arc = 2,
    parabola = 3
};

struct Primitive
{
    AABB aabb;
    vec2 centroid;
    Primitive_type type;
    std::uint32_t index;
};

[[nodiscard]] constexpr AABB compute_aabb(const Line &line) noexcept
{
    return {.min = {std::min(line.vertex_a.x, line.vertex_b.x),
                    std::min(line.vertex_a.y, line.vertex_b.y)},
            .max = {std::max(line.vertex_a.x, line.vertex_b.x),
                    std::max(line.vertex_a.y, line.vertex_b.y)}};
}

[[nodiscard]] constexpr AABB compute_aabb(const Arc &arc) noexcept
{
    assert(false);
    return {
        .min = {std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity()},
        .max = {-std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity()},
    };
}

[[nodiscard]] constexpr AABB compute_aabb(const Parabola &parabola) noexcept
{
    assert(false);
    return {
        .min = {std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity()},
        .max = {-std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity()},
    };
}

[[nodiscard]] constexpr vec2 centroid(const AABB &aabb) noexcept
{
    return 0.5f * (aabb.max + aabb.min);
}

[[nodiscard]] constexpr AABB merge_aabb(const AABB &a, const AABB &b) noexcept
{
    return {.min = {std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y)},
            .max = {std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y)}};
}

[[nodiscard]] constexpr AABB expand_aabb(const AABB &aabb,
                                         const vec2 &point) noexcept
{
    return {
        .min = {std::min(aabb.min.x, point.x), std::min(aabb.min.y, point.y)},
        .max = {std::max(aabb.max.x, point.x), std::max(aabb.max.y, point.y)},
    };
}

[[nodiscard]] constexpr float measure_aabb(const AABB &aabb) noexcept
{
    const auto dx = std::max(0.0f, aabb.max.x - aabb.min.x);
    const auto dy = std::max(0.0f, aabb.max.y - aabb.min.y);
    return dx + dy;
}

[[nodiscard]] constexpr bool is_leaf(std::uint32_t child) noexcept
{
    return (child & 0x80000000u) != 0u;
}

[[nodiscard]] constexpr Primitive_type leaf_type(std::uint32_t child) noexcept
{
    return static_cast<Primitive_type>((child >> 8u) & 0xffu);
}

[[nodiscard]] constexpr std::uint32_t leaf_index(std::uint32_t child) noexcept
{
    return child & 0xffu;
}

[[nodiscard]] constexpr std::uint32_t make_leaf(Primitive_type type,
                                                std::uint32_t index) noexcept
{
    return 0x80000000u | (static_cast<std::uint32_t>(type) << 8u) | index;
}

[[nodiscard]] std::size_t partition_primitives(
    std::vector<Primitive> &primitives, std::size_t begin, std::size_t end)
{
    auto centroid_bounds = empty_aabb;
    for (auto i = begin; i < end; ++i)
    {
        centroid_bounds = expand_aabb(centroid_bounds, primitives[i].centroid);
    }

    const auto extent = centroid_bounds.max - centroid_bounds.min;
    const auto axis = (extent.x >= extent.y) ? 0 : 1;
    const auto mid = begin + (end - begin) / 2;

    std::nth_element(primitives.begin() + static_cast<std::ptrdiff_t>(begin),
                     primitives.begin() + static_cast<std::ptrdiff_t>(mid),
                     primitives.begin() + static_cast<std::ptrdiff_t>(end),
                     [axis](const Primitive &a, const Primitive &b)
                     {
                         return axis == 0 ? a.centroid.x < b.centroid.x
                                          : a.centroid.y < b.centroid.y;
                     });

    return mid;
}

void build_bvh(Scene &scene)
{
    std::vector<Primitive> primitives;

    const auto num_primitives =
        scene.lines.size() + scene.arcs.size() + scene.parabolas.size();
    if (num_primitives == 0)
    {
        return;
    }

    primitives.reserve(num_primitives);

    const auto add_primitives = [&](Primitive_type type, const auto &prims)
    {
        for (std::uint32_t i {0}; i < static_cast<std::uint32_t>(prims.size());
             ++i)
        {
            const auto aabb = compute_aabb(prims[i]);
            primitives.push_back({.aabb = aabb,
                                  .centroid = centroid(aabb),
                                  .type = type,
                                  .index = i});
        }
    };
    add_primitives(Primitive_type::line, scene.lines);
    add_primitives(Primitive_type::arc, scene.arcs);
    add_primitives(Primitive_type::parabola, scene.parabolas);

    scene.bvh_nodes.clear();
    scene.bvh_nodes.reserve(num_primitives > 1 ? num_primitives - 1 : 1);

    const auto push_node = [&]
    {
        const auto index = static_cast<std::uint32_t>(scene.bvh_nodes.size());
        scene.bvh_nodes.push_back({.aabbs = {empty_aabb, empty_aabb},
                                   .children = {invalid_child, invalid_child}});
        return index;
    };

    const auto merge_range_aabb = [&](std::size_t begin, std::size_t end)
    {
        auto aabb = empty_aabb;
        for (auto i = begin; i < end; ++i)
        {
            aabb = merge_aabb(aabb, primitives[i].aabb);
        }
        return aabb;
    };

    const auto root = push_node();

    struct Primitive_range
    {
        std::size_t begin;
        std::size_t end;
        std::uint32_t node;
    };

    std::vector<Primitive_range> stack;
    stack.reserve(primitives.size());
    stack.push_back({.begin = 0, .end = primitives.size(), .node = root});

    while (!stack.empty())
    {
        const auto primitive_range = stack.back();
        stack.pop_back();

        const auto count = primitive_range.end - primitive_range.begin;

        if (count == 1)
        {
            const auto &p = primitives[primitive_range.begin];

            auto &node = scene.bvh_nodes[primitive_range.node];
            node.aabbs[0] = p.aabb;
            node.children[0] = make_leaf(p.type, p.index);

            continue;
        }

        if (count == 2)
        {
            const auto &p0 = primitives[primitive_range.begin + 0];
            const auto &p1 = primitives[primitive_range.begin + 1];

            auto &node = scene.bvh_nodes[primitive_range.node];
            node.aabbs[0] = p0.aabb;
            node.children[0] = make_leaf(p0.type, p0.index);
            node.aabbs[1] = p1.aabb;
            node.children[1] = make_leaf(p1.type, p1.index);

            continue;
        }

        const auto mid = partition_primitives(
            primitives, primitive_range.begin, primitive_range.end);

        const auto aabb_0 = merge_range_aabb(primitive_range.begin, mid);
        const auto aabb_1 = merge_range_aabb(mid, primitive_range.end);

        const auto node_0 = push_node();
        const auto node_1 = push_node();

        auto &node = scene.bvh_nodes[primitive_range.node];
        node.aabbs[0] = aabb_0;
        node.aabbs[1] = aabb_1;
        node.children[0] = node_0;
        node.children[1] = node_1;

        stack.push_back(
            {.begin = mid, .end = primitive_range.end, .node = node_1});
        stack.push_back(
            {.begin = primitive_range.begin, .end = mid, .node = node_0});
    }
}

} // namespace

void to_json(nlohmann::json &j, const vec2 &v)
{
    j = nlohmann::json::array({v.x, v.y});
}

void to_json(nlohmann::json &j, const vec3 &v)
{
    j = nlohmann::json::array({v.x, v.y, v.z});
}

void from_json(const nlohmann::json &j, vec2 &v)
{
    j.at(0).get_to(v.x);
    j.at(1).get_to(v.y);
}

void from_json(const nlohmann::json &j, vec3 &v)
{
    j.at(0).get_to(v.x);
    j.at(1).get_to(v.y);
    j.at(2).get_to(v.z);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    Surface, base_color, type, emissive_color, emissive_strength, ior_ratio)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    Line, vertex_a, vertex_b, surface_id, volume_in_id, volume_out_id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Arc,
                                   center,
                                   radius,
                                   clip_offset,
                                   clip_normal,
                                   surface_id,
                                   volume_in_id,
                                   volume_out_id)

Scene create_scene(int texture_width, int texture_height)
{
    const auto view_x = 0.5f;
    const auto view_y = 0.5f * static_cast<float>(texture_height) /
                        static_cast<float>(texture_width);
    const auto view_width = 1.0f;
    const auto view_height = 1.0f * static_cast<float>(texture_height) /
                             static_cast<float>(texture_width);

    Scene scene {};
    scene.view_x = view_x;
    scene.view_y = view_y;
    scene.view_width = view_width;
    scene.view_height = view_height;

#if 0
    scene.surfaces = {Surface {.base_color = {0.75f, 0.75f, 0.75f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {1.0f, 1.0f, 1.0f},
                               .emissive_strength = 6.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.75f, 0.55f, 0.25f},
                               .type = Surface_type::dielectric,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.5f},
                      Surface {.base_color = {0.25f, 0.75f, 0.75f},
                               .type = Surface_type::dielectric,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.5f},
                      Surface {.base_color = {0.75f, 0.25f, 0.75f},
                               .type = Surface_type::specular,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.75f, 0.75f, 0.75f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {1.0f, 1.0f, 1.0f},
                               .type = Surface_type::dielectric,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.5f}};
    scene.volumes = {Volume {.absorption = {3.0f, 0.0f, 0.0f},
                             .phase_anisotropy = 0.7f,
                             .scattering = {20.0f, 20.0f, 20.0f}},
                     Volume {.absorption = {0.0f, 0.0f, 0.0f},
                             .phase_anisotropy = 0.0f,
                             .scattering = {20.0f, 20.0f, 20.0f}}};
    scene.lines = {Line {.vertex_a = {0.35f, 0.05f},
                         .vertex_b = {0.1f, 0.2f},
                         .surface_id = 3,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {0.1f, 0.4f},
                         .vertex_b = {0.4f, 0.6f},
                         .surface_id = 4,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id}};
    scene.arcs = {
        Arc {.center = {0.8f, 0.5f},
             .radius = 0.03f,
             .clip_offset = 0.0f,
             .clip_normal = {},
             .surface_id = 0,
             .volume_in_id = invalid_id,
             .volume_out_id = invalid_id},
        Arc {.center = {0.5f, 0.3f},
             .radius = 0.15f,
             .clip_offset = 0.0f,
             .clip_normal = {},
             .surface_id = 1,
             .volume_in_id = 0,
             .volume_out_id = invalid_id},
        Arc {.center = {0.8f, 0.2f},
             .radius = 0.05f,
             .clip_offset = 0.0f,
             .clip_normal = {},
             .surface_id = 2,
             .volume_in_id = invalid_id,
             .volume_out_id = invalid_id},
        Arc {.center = {0.6f, 0.6f},
             .radius = 0.1f,
             .clip_offset = -0.04f,
             .clip_normal = {-0.5f, std::numbers::sqrt3_v<float> * 0.5f},
             .surface_id = 3,
             .volume_in_id = invalid_id,
             .volume_out_id = invalid_id},
        Arc {.center = {0.25f, 0.32f - 0.075f},
             .radius = 0.1f,
             .clip_offset = 0.075f,
             .clip_normal = {0.0f, 1.0f},
             .surface_id = 5,
             .volume_in_id = 1,
             .volume_out_id = invalid_id},
        Arc {.center = {0.25f, 0.32f + 0.075f},
             .radius = 0.1f,
             .clip_offset = 0.075f,
             .clip_normal = {0.0f, -1.0f},
             .surface_id = 5,
             .volume_in_id = 1,
             .volume_out_id = invalid_id}};
    scene.parabolas = {Parabola {.vertex = {0.86f, 0.66f},
                                 .axis = normalize(vec2 {-1.0f, -2.0f}),
                                 .focal = 0.01f,
                                 .clip_offset = 0.1f,
                                 .clip_normal = {1.0f, 0.0f},
                                 .surface_id = 3,
                                 .volume_in_id = invalid_id,
                                 .volume_out_id = invalid_id}};

#elif 1

    scene.surfaces = {Surface {.base_color = {0.75f, 0.75f, 0.75f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {1.0f, 1.0f, 1.0f},
                               .emissive_strength = 4.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.75f, 0.5f, 0.0f},
                               .type = Surface_type::specular,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.5f, 0.75f, 0.75f},
                               .type = Surface_type::specular,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.25f, 0.50f, 0.75f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f}};

    std::minstd_rand rng(42);
    std::uniform_int_distribution<int> shape_dist(0, 2);
    std::uniform_real_distribution<float> coord_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> radius_dist(0.0f, 0.3f);
    std::uniform_real_distribution<float> angle_dist(
        0.0f, 2.0f * std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> angle_x_dist(
        -0.25f * std::numbers::pi_v<float>, 0.25f * std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> surface_dist(0.0f, 1.0f);
    const auto surface_id = [&] -> std::uint32_t
    {
        const auto p = surface_dist(rng);
        if (p < 0.2f)
            return 0;
        if (p < 0.5f)
            return 1;
        if (p < 0.7f)
            return 2;
        return 3;
    };
    const auto dir = [&] -> vec2
    {
        const auto angle = angle_dist(rng);
        return {std::cos(angle), std::sin(angle)};
    };
    const auto x_dir = [&] -> vec2
    {
        const auto angle = angle_x_dist(rng);
        return {std::cos(angle), std::sin(angle)};
    };

    for (int i {0}; i < 128; ++i)
    {
        // const auto s = shape_dist(rng);
        const auto s = 0;
        if (s == 0)
        {
            const vec2 vertex_a {coord_dist(rng), coord_dist(rng)};
            const vec2 delta {0.05f * (2.0f * coord_dist(rng) - 1.0f),
                              0.05f * (2.0f * coord_dist(rng) - 1.0f)};
            scene.lines.push_back({.vertex_a = vertex_a,
                                   .vertex_b = vertex_a + delta,
                                   .surface_id = surface_id(),
                                   .volume_in_id = invalid_id,
                                   .volume_out_id = invalid_id});
        }
        else if (s == 1)
        {
            const auto radius = radius_dist(rng);
            std::uniform_real_distribution<float> clip_dist(-radius, radius);
            scene.arcs.push_back({.center = {coord_dist(rng), coord_dist(rng)},
                                  .radius = radius,
                                  .clip_offset = clip_dist(rng),
                                  .clip_normal = dir(),
                                  .surface_id = surface_id(),
                                  .volume_in_id = invalid_id,
                                  .volume_out_id = invalid_id});
        }
        else
        {
            scene.parabolas.push_back(
                {.vertex = {coord_dist(rng), coord_dist(rng)},
                 .axis = dir(),
                 .focal = radius_dist(rng),
                 .clip_offset = radius_dist(rng),
                 .clip_normal = x_dir(),
                 .surface_id = surface_id(),
                 .volume_in_id = invalid_id,
                 .volume_out_id = invalid_id});
        }
    }

#elif 1
    constexpr vec2 light_center {0.8f, 0.5f};
    constexpr float light_radius {0.003f};
    constexpr float angle {std::numbers::pi_v<float> * 1.25f};
    constexpr float lens_radius {0.06f};
    constexpr float lens_half_thickness {0.002f};

    const vec2 lens_dir {std::cos(angle), std::sin(angle)};
    const auto lens_dist = lens_radius * 1.1f;
    const auto lens_center = light_center + lens_dir * lens_dist;
    const auto lens_offset = lens_radius - lens_half_thickness;
    const auto lens_half_width =
        std::sqrt(lens_radius * lens_radius - lens_offset * lens_offset);
    const auto cover_radius =
        std::sqrt(lens_dist * lens_dist + lens_half_width * lens_half_width);

    scene.surfaces = {Surface {.base_color = {1.0f, 1.0f, 1.0f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {1.0f, 1.0f, 1.0f},
                               .emissive_strength = 20.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.0f, 0.0f, 0.0f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {1.0f, 1.0f, 1.0f},
                               .type = Surface_type::dielectric,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.5f},
                      Surface {.base_color = {1.0f, 0.75f, 0.5f},
                               .type = Surface_type::dielectric,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.7f},
                      Surface {.base_color = {0.75f, 0.5f, 1.0f},
                               .type = Surface_type::specular,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f}};
    scene.volumes = {Volume {.absorption = {3.0f, 0.0f, 0.0f},
                             .phase_anisotropy = 0.7f,
                             .scattering = {20.0f, 20.0f, 20.0f}}};
    scene.lines = {Line {.vertex_a = {5.0f, 0.3f},
                         .vertex_b = {-4.0f, 0.3f},
                         .surface_id = 3,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {5.0f, 0.25f},
                         .vertex_b = {-4.0f, 0.25f},
                         .surface_id = 3,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {-4.0f, 0.2f},
                         .vertex_b = {5.0f, 0.2f},
                         .surface_id = 3,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {-4.0f, 0.15f},
                         .vertex_b = {5.0f, 0.15f},
                         .surface_id = 3,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id}};
    scene.arcs = {Arc {.center = light_center,
                       .radius = light_radius,
                       .clip_offset = 0.0f,
                       .clip_normal = {},
                       .surface_id = 0,
                       .volume_in_id = invalid_id,
                       .volume_out_id = invalid_id},
                  Arc {.center = light_center,
                       .radius = cover_radius,
                       .clip_offset = -lens_dist,
                       .clip_normal = -lens_dir,
                       .surface_id = 1,
                       .volume_in_id = invalid_id,
                       .volume_out_id = invalid_id},
                  Arc {.center = lens_center - lens_offset * lens_dir,
                       .radius = lens_radius,
                       .clip_offset = lens_radius - lens_half_thickness,
                       .clip_normal = lens_dir,
                       .surface_id = 2,
                       .volume_in_id = invalid_id,
                       .volume_out_id = invalid_id},
                  Arc {.center = lens_center + lens_offset * lens_dir,
                       .radius = lens_radius,
                       .clip_offset = lens_radius - lens_half_thickness,
                       .clip_normal = -lens_dir,
                       .surface_id = 2,
                       .volume_in_id = invalid_id,
                       .volume_out_id = invalid_id}};
#elif 0
    scene.surfaces = {Surface {.base_color = {0.75f, 0.75f, 0.75f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {1.0f, 1.0f, 1.0f},
                               .emissive_strength = 6.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.75f, 0.75f, 0.75f},
                               .type = Surface_type::diffuse,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f}};
    scene.lines = {Line {.vertex_a = {0.3f, 0.3f * view_height / view_width},
                         .vertex_b = {0.25f, 0.5f * view_height / view_width},
                         .surface_id = 1,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {0.25f, 0.5f * view_height / view_width},
                         .vertex_b = {0.3f, 0.7f * view_height / view_width},
                         .surface_id = 1,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {0.75f, 0.7f * view_height / view_width},
                         .vertex_b = {0.7f, 0.5f * view_height / view_width},
                         .surface_id = 1,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {0.7f, 0.5f * view_height / view_width},
                         .vertex_b = {0.75f, 0.3f * view_height / view_width},
                         .surface_id = 1,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id}};
    scene.arcs = {Arc {.center = {0.5f, 0.5f * view_height / view_width},
                       .radius = 0.05f,
                       .clip_offset = 0.0f,
                       .clip_normal = {},
                       .surface_id = 0,
                       .volume_in_id = invalid_id,
                       .volume_out_id = invalid_id}};
#else
    scene.surfaces = {Surface {.base_color = {0.75f, 0.75f, 0.75f},
                               .type = Surface_type::dielectric,
                               .emissive_color = {1.0f, 1.0f, 1.0f},
                               .emissive_strength = 6.0f,
                               .ior_ratio = 1.0f},
                      Surface {.base_color = {0.25f, 0.75f, 0.75f},
                               .type = Surface_type::specular,
                               .emissive_color = {},
                               .emissive_strength = 0.0f,
                               .ior_ratio = 1.0f}};
    scene.lines = {Line {.vertex_a = {0.2f, 0.3f},
                         .vertex_b = {0.23f, 0.4f},
                         .surface_id = 0,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id},
                   Line {.vertex_a = {0.4f, 0.2f},
                         .vertex_b = {0.7f, 0.4f},
                         .surface_id = 1,
                         .volume_in_id = invalid_id,
                         .volume_out_id = invalid_id}};
#endif

    build_bvh(scene);

    return scene;
}

std::expected<Scene, std::string> load_scene(const std::filesystem::path &path)
{
    return std::unexpected("Scene loading not fully implemented");

    std::error_code ec;

    if (!std::filesystem::exists(path, ec))
    {
        if (ec)
        {
            return std::unexpected(ec.message());
        }
        return std::unexpected("File not found");
    }

    if (!std::filesystem::is_regular_file(path, ec))
    {
        if (ec)
        {
            return std::unexpected(ec.message());
        }
        return std::unexpected("Not a file");
    }

    std::ifstream file(path);
    if (!file)
    {
        return std::unexpected("Failed to open file for reading");
    }

    // FIXME: error handling for JSON parsing
    const auto data = nlohmann::json::parse(file);

    Scene scene {};

    data.at("view_x").get_to(scene.view_x);
    data.at("view_y").get_to(scene.view_y);
    data.at("view_width").get_to(scene.view_width);
    data.at("view_height").get_to(scene.view_height);
    data.at("surfaces").get_to(scene.surfaces);
    data.at("lines").get_to(scene.lines);
    data.at("arcs").get_to(scene.arcs);

    return scene;
}

std::expected<void, std::string> save_scene(const Scene &scene,
                                            const std::filesystem::path &path)
{
    return std::unexpected("Scene saving not fully implemented");

    std::ofstream file(path);
    if (!file)
    {
        return std::unexpected("Failed to open file for writing");
    }

    nlohmann::json data;

    data["view_x"] = scene.view_x;
    data["view_y"] = scene.view_y;
    data["view_width"] = scene.view_width;
    data["view_height"] = scene.view_height;
    data["surfaces"] = scene.surfaces;
    data["lines"] = scene.lines;
    data["arcs"] = scene.arcs;

    file << std::setw(4) << data << '\n';

    return {};
}

#include "scene.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <numbers>
#include <random>
#include <sstream>
#include <string>

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
    std::uniform_real_distribution<float> dist_01(0.0f, 1.0f);
    std::uniform_real_distribution<float> coord_dist_x(0.1f, 0.9f);
    std::uniform_real_distribution<float> coord_dist_y(0.05f, 0.6f);
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

    for (int i {0}; i < 256; ++i)
    {
        const auto s = 0; // shape_dist(rng);
        if (s == 0)
        {
            const vec2 vertex_a {coord_dist_x(rng), coord_dist_y(rng)};
            const vec2 delta {0.1f * (2.0f * dist_01(rng) - 1.0f),
                              0.1f * (2.0f * dist_01(rng) - 1.0f)};
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
            scene.arcs.push_back(
                {.center = {coord_dist_x(rng), coord_dist_y(rng)},
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
                {.vertex = {coord_dist_x(rng), coord_dist_y(rng)},
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

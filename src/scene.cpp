#include "scene.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>

using json = nlohmann::json;

void to_json(json &j, const vec2 &v)
{
    j = json::array({v.x, v.y});
}

void to_json(json &j, const vec3 &v)
{
    j = json::array({v.x, v.y, v.z});
}

void from_json(const json &j, vec2 &v)
{
    j.at(0).get_to(v.x);
    j.at(1).get_to(v.y);
}

void from_json(const json &j, vec3 &v)
{
    j.at(0).get_to(v.x);
    j.at(1).get_to(v.y);
    j.at(2).get_to(v.z);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    Material, base_color, emissive_color, emissive_strength, type, ior)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Circle, center, radius, material_id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Line, a, b, material_id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Arc, center, radius, a, b, material_id)

Scene create_scene(int texture_width, int texture_height)
{
    const auto view_x = 0.5f;
    const auto view_y = 0.5f * static_cast<float>(texture_height) /
                        static_cast<float>(texture_width);
    const auto view_width = 1.0f;
    const auto view_height = 1.0f * static_cast<float>(texture_height) /
                             static_cast<float>(texture_width);

#if 0
    Scene scene {
        .view_x = view_x,
        .view_y = view_y,
        .view_width = view_width,
        .view_height = view_height,
        .materials = {Material {.base_color = {0.75f, 0.75f, 0.75f},
                                .emissive_color = {1.0f, 1.0f, 1.0f},
                                .emissive_strength = 6.0f,
                                .type = Material_type::diffuse,
                                .ior = 1.0f},
                      Material {.base_color = {0.75f, 0.55f, 0.25f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::dielectric,
                                .ior = 1.5f},
                      Material {.base_color = {0.25f, 0.75f, 0.75f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::dielectric,
                                .ior = 1.5f},
                      Material {.base_color = {0.75f, 0.25f, 0.75f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::specular,
                                .ior = 1.0f},
                      Material {.base_color = {0.75f, 0.75f, 0.75f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::diffuse,
                                .ior = 1.0f},
                      Material {.base_color = {1.0f, 1.0f, 1.0f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::dielectric,
                                .ior = 1.5f}},
        .circles = {Circle {{0.8f, 0.5f}, 0.03f, 0},
                    Circle {{0.5f, 0.3f}, 0.15f, 1},
                    Circle {{0.8f, 0.2f}, 0.05f, 2}},
        .lines = {Line {{0.35f, 0.05f}, {0.1f, 0.2f}, 3},
                  Line {{0.1f, 0.4f}, {0.4f, 0.6f}, 4}},
        .arcs = {
            Arc {{0.6f, 0.6f},
                 0.1f,
                 {-0.5f, std::numbers::sqrt3_v<float> * 0.5f},
                 -0.04f,
                 3},
            Arc {{0.25f, 0.32f - 0.075f}, 0.1f, {0.0f, 1.0f}, 0.075f, 5},
            Arc {{0.25f, 0.32f + 0.075f}, 0.1f, {0.0f, -1.0f}, 0.075f, 5}}};
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

    Scene scene {
        .view_x = view_x,
        .view_y = view_y,
        .view_width = view_width,
        .view_height = view_height,
        .materials = {Material {.base_color = {1.0f, 1.0f, 1.0f},
                                .emissive_color = {1.0f, 1.0f, 1.0f},
                                .emissive_strength = 20.0f,
                                .type = Material_type::diffuse,
                                .ior = 1.0f},
                      Material {.base_color = {0.0f, 0.0f, 0.0f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::diffuse,
                                .ior = 1.0f},
                      Material {.base_color = {1.0f, 1.0f, 1.0f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::dielectric,
                                .ior = 1.5f},
                      Material {.base_color = {1.0f, 0.75f, 0.5f},
                                .emissive_color = {},
                                .emissive_strength = 0.0f,
                                .type = Material_type::dielectric,
                                .ior = 1.7f}},
        .circles = {Circle {light_center, light_radius, 0}},
        .lines = {Line {{5.0f, 0.3f}, {-4.0f, 0.3f}, 3},
                  Line {{5.0f, 0.25f}, {-4.0f, 0.25f}, 3},
                  Line {{-4.0f, 0.2f}, {5.0f, 0.2f}, 3},
                  Line {{-4.0f, 0.15f}, {5.0f, 0.15f}, 3}},
        .arcs = {Arc {light_center, cover_radius, -lens_dir, -lens_dist, 1},
                 Arc {lens_center - lens_offset * lens_dir,
                      lens_radius,
                      lens_dir,
                      lens_radius - lens_half_thickness,
                      2},
                 Arc {lens_center + lens_offset * lens_dir,
                      lens_radius,
                      -lens_dir,
                      lens_radius - lens_half_thickness,
                      2}}};
#elif 1
    Scene scene {
        .view_x = view_x,
        .view_y = view_y,
        .view_width = view_width,
        .view_height = view_height,
        .materials =
            {
                Material {.base_color = {0.75f, 0.75f, 0.75f},
                          .emissive_color = {1.0f, 1.0f, 1.0f},
                          .emissive_strength = 6.0f,
                          .type = Material_type::diffuse,
                          .ior = 1.0f},
                Material {.base_color = {0.75f, 0.75f, 0.75f},
                          .emissive_color = {},
                          .emissive_strength = 0.0f,
                          .type = Material_type::diffuse,
                          .ior = 1.0f},
            },
        .circles = {Circle {{0.5f, 0.5f * view_height / view_width}, 0.05f, 0}},
        .lines = {Line {{0.3f, 0.7f * view_height / view_width},
                        {0.25f, 0.5f * view_height / view_width},
                        1},
                  Line {{0.3f, 0.3f * view_height / view_width},
                        {0.25f, 0.5f * view_height / view_width},
                        1},
                  Line {{0.75f, 0.7f * view_height / view_width},
                        {0.7f, 0.5f * view_height / view_width},
                        1},
                  Line {{0.75f, 0.3f * view_height / view_width},
                        {0.7f, 0.5f * view_height / view_width},
                        1}},
        .arcs = {}};
#else
    Scene scene {.view_x = view_x,
                 .view_y = view_y,
                 .view_width = view_width,
                 .view_height = view_height,
                 .materials = {Material {.base_color = {0.75f, 0.75f, 0.75f},
                                         .emissive_color = {1.0f, 1.0f, 1.0f},
                                         .emissive_strength = 6.0f,
                                         .type = Material_type::dielectric,
                                         .ior = 1.0f},
                               Material {.base_color = {0.25f, 0.75f, 0.75f},
                                         .emissive_color = {},
                                         .emissive_strength = 0.0f,
                                         .type = Material_type::specular,
                                         .ior = 1.0f}},
                 .circles = {},
                 .lines = {Line {{0.2f, 0.3f}, {0.23f, 0.4f}, 0},
                           Line {{0.4f, 0.2f}, {0.7f, 0.4f}, 1}},
                 .arcs = {}};
#endif

    return scene;
}

std::expected<Scene, std::string> load_scene(const std::filesystem::path &path)
{
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
    const auto data = json::parse(file);

    Scene scene {};

    data.at("view_x").get_to(scene.view_x);
    data.at("view_y").get_to(scene.view_y);
    data.at("view_width").get_to(scene.view_width);
    data.at("view_height").get_to(scene.view_height);
    data.at("materials").get_to(scene.materials);
    data.at("circles").get_to(scene.circles);
    data.at("lines").get_to(scene.lines);
    data.at("arcs").get_to(scene.arcs);

    return scene;
}

std::expected<void, std::string> save_scene(const Scene &scene,
                                            const std::filesystem::path &path)
{
    std::ofstream file(path);
    if (!file)
    {
        return std::unexpected("Failed to open file for writing");
    }

    json data;

    data["view_x"] = scene.view_x;
    data["view_y"] = scene.view_y;
    data["view_width"] = scene.view_width;
    data["view_height"] = scene.view_height;
    data["materials"] = scene.materials;
    data["circles"] = scene.circles;
    data["lines"] = scene.lines;
    data["arcs"] = scene.arcs;

    file << std::setw(4) << data << '\n';

    return {};
}

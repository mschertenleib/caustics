#include "scene.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Material, color, emissivity, type)
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

#if 1
    Scene scene {
        .view_x = view_x,
        .view_y = view_y,
        .view_width = view_width,
        .view_height = view_height,
        .materials =
            {Material {{0.75f, 0.75f, 0.75f},
                       {6.0f, 6.0f, 6.0f},
                       Material_type::diffuse},
             Material {{0.75f, 0.55f, 0.25f}, {}, Material_type::dielectric},
             Material {{0.25f, 0.75f, 0.75f}, {}, Material_type::dielectric},
             Material {{1.0f, 0.0f, 1.0f}, {}, Material_type::specular},
             Material {{0.75f, 0.75f, 0.75f}, {}, Material_type::diffuse},
             Material {{1.0f, 1.0f, 1.0f}, {}, Material_type::dielectric}},
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
#elif 0
    Scene scene {
        .view_x = view_x,
        .view_y = view_y,
        .view_width = view_width,
        .view_height = view_height,
        .materials = {Material {
            {0.75f, 0.75f, 0.75f}, {6.0f, 6.0f, 6.0f}, Material_type::diffuse}},
        .circles = {Circle {{0.5f, 0.5f * view_height / view_width}, 0.05f, 0}},
        .lines = {},
        .arcs = {}};
#else
    Scene scene {.view_x = view_x,
                 .view_y = view_y,
                 .view_width = view_width,
                 .view_height = view_height,
                 .materials = {Material {{0.75f, 0.75f, 0.75f},
                                         {6.0f, 6.0f, 6.0f},
                                         Material_type::diffuse}},
                 .circles = {},
                 .lines = {Line {{0.2f, 0.3f}, {0.25f, 0.4f}, 0}},
                 .arcs = {}};
#endif

    return scene;
}

std::optional<Scene> load_scene(const char *file_name)
{
    std::cout << "Loading scene from \"" << file_name << "\"\n";

    std::ifstream file(file_name);
    if (!file.is_open())
    {
        std::cerr << "Failed to read scene file \"" << file_name << '\"';
        return std::nullopt;
    }

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

void save_scene(const Scene &scene, const char *file_name)
{
    std::cout << "Saving scene to \"" << file_name << "\"\n";

    std::ofstream file(file_name);
    if (!file.is_open())
    {
        std::ostringstream message;
        message << "Failed to write to scene file \"" << file_name << '\"';
        throw std::runtime_error(message.str());
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
}

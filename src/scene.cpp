#include "scene.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

    Scene scene {};

    // FIXME: this is just a test, we need an actual parser that is more
    // intelligent (like handling newlines between key and value, as well as
    // nested keys)

    std::string line;
    while (std::getline(std::ws(file), line))
    {
        constexpr auto sep = ": \t";
        const auto key_end = line.find_first_of(sep);
        const auto key = line.substr(0, key_end);
        if (key_end == std::string::npos)
        {
            continue;
        }
        const auto value_start = line.find_first_not_of(sep, key_end);
        if (value_start == std::string::npos)
        {
            continue;
        }
        const auto value = line.substr(value_start);
        if (key == "view_x")
        {
            scene.view_x = std::stof(value);
        }
        else if (key == "view_y")
        {
            scene.view_y = std::stof(value);
        }
        else if (key == "view_width")
        {
            scene.view_width = std::stof(value);
        }
        else if (key == "view_height")
        {
            scene.view_height = std::stof(value);
        }
    }

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

    file.precision(std::numeric_limits<float>::max_digits10);

    file << "view_x: " << scene.view_x << '\n';
    file << "view_y: " << scene.view_y << '\n';
    file << "view_width: " << scene.view_width << '\n';
    file << "view_height: " << scene.view_height << '\n';

    const auto write_vec2 = [&file](const auto &prefix, const vec2 &v)
    {
        file << prefix << "x: " << v.x << '\n';
        file << prefix << "y: " << v.y << '\n';
    };
    const auto write_color = [&file](const auto &prefix, const vec3 &v)
    {
        file << prefix << "r: " << v.x << '\n';
        file << prefix << "g: " << v.y << '\n';
        file << prefix << "b: " << v.z << '\n';
    };

    file << "materials:" << (scene.materials.empty() ? " []\n" : "\n");
    for (const auto &material : scene.materials)
    {
        file << "  - color:\n";
        write_color("        ", material.color);
        file << "    emissivity:\n";
        write_color("        ", material.emissivity);
        file << "    type: " << static_cast<int>(material.type) << '\n';
    }

    file << "circles:" << (scene.circles.empty() ? " []\n" : "\n");
    for (const auto &circle : scene.circles)
    {
        file << "  - center:\n";
        write_vec2("        ", circle.center);
        file << "    radius: " << circle.radius << '\n';
        file << "    material_id: " << circle.material_id << '\n';
    }

    file << "lines:" << (scene.lines.empty() ? " []\n" : "\n");
    for (const auto &line : scene.lines)
    {
        file << "  - a:\n";
        write_vec2("        ", line.a);
        file << "    b:\n";
        write_vec2("        ", line.b);
        file << "    material_id: " << line.material_id << '\n';
    }

    file << "arcs:" << (scene.arcs.empty() ? " []\n" : "\n");
    for (const auto &arc : scene.arcs)
    {
        file << "  - center:\n";
        write_vec2("        ", arc.center);
        file << "    radius: " << arc.radius << '\n';
        file << "    a:\n";
        write_vec2("        ", arc.a);
        file << "    b: " << arc.b << '\n';
        file << "    material_id: " << arc.material_id << '\n';
    }
}
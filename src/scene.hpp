#ifndef SCENE_HPP
#define SCENE_HPP

#include "vec.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

enum struct Material_type : std::uint32_t
{
    diffuse,
    specular,
    dielectric
};

struct alignas(16) Material
{
    alignas(16) vec3 color;
    alignas(16) vec3 emissivity;
    Material_type type;
};

struct alignas(16) Circle
{
    alignas(8) vec2 center;
    float radius;
    std::uint32_t material_id;
};

struct alignas(16) Line
{
    alignas(8) vec2 a;
    alignas(8) vec2 b;
    std::uint32_t material_id;
};

struct alignas(16) Arc
{
    alignas(8) vec2 center;
    float radius;
    alignas(8) vec2 a;
    float b;
    std::uint32_t material_id;
};

struct Scene
{
    float view_x;
    float view_y;
    float view_width;
    float view_height;
    std::vector<Material> materials;
    std::vector<Circle> circles;
    std::vector<Line> lines;
    std::vector<Arc> arcs;
};

[[nodiscard]] Scene create_scene(int texture_width, int texture_height);

[[nodiscard]] std::expected<Scene, std::string>
load_scene(const std::filesystem::path &path);

[[nodiscard]] std::expected<void, std::string>
save_scene(const Scene &scene, const std::filesystem::path &path);

#endif
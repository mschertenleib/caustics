#ifndef SCENE_HPP
#define SCENE_HPP

#include "vec.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

inline constexpr std::uint32_t invalid_id {
    std::numeric_limits<std::uint32_t>::max()};

enum struct Surface_type : std::uint32_t
{
    diffuse = 0,
    specular = 1,
    dielectric = 2
};

// NOTE: these structs have to mirror their definition in GLSL code, and have
// the same std140 layout

struct alignas(16) Surface
{
    vec3 base_color;
    Surface_type type;
    vec3 emissive_color;
    float emissive_strength;
    float ior_ratio;
};

struct alignas(16) Volume
{
    vec3 absorption;
    float phase_anisotropy;
    vec3 scattering;
};

struct alignas(16) Line
{
    vec2 vertex_a;
    vec2 vertex_b;
    std::uint32_t surface_id;
    std::uint32_t volume_in_id;
    std::uint32_t volume_out_id;
};

struct alignas(16) Arc
{
    vec2 center;
    float radius;
    float clip_offset;
    vec2 clip_normal;
    std::uint32_t surface_id;
    std::uint32_t volume_in_id;
    std::uint32_t volume_out_id;
};

struct alignas(16) Parabola
{
    vec2 vertex;
    vec2 axis;
    float focal;
    float clip_offset;
    vec2 clip_normal;
    std::uint32_t surface_id;
    std::uint32_t volume_in_id;
    std::uint32_t volume_out_id;
};

struct Scene
{
    float view_x;
    float view_y;
    float view_width;
    float view_height;
    std::vector<Surface> surfaces;
    std::vector<Volume> volumes;
    std::vector<Line> lines;
    std::vector<Arc> arcs;
    std::vector<Parabola> parabolas;
};

[[nodiscard]] Scene create_scene(int texture_width, int texture_height);

[[nodiscard]] std::expected<Scene, std::string>
load_scene(const std::filesystem::path &path);

[[nodiscard]] std::expected<void, std::string>
save_scene(const Scene &scene, const std::filesystem::path &path);

#endif
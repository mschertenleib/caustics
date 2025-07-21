#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "scene.hpp"
#include "unique_resource.hpp"
#include "vec.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define GLFW_INCLUDE_ES3
#define GL_GLES_PROTOTYPES 0
#else
#define GLFW_INCLUDE_GLCOREARB
#endif
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#include <cstdint>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#define NO_COMPUTE_SHADER
#endif

struct GLFW_deleter
{
    void operator()(bool);
};

struct Window_deleter
{
    void operator()(struct GLFWwindow *window);
};

struct ImGui_deleter
{
    void operator()(bool);
};

struct ImGui_glfw_deleter
{
    void operator()(bool);
};

struct ImGui_opengl_deleter
{
    void operator()(bool);
};

struct GL_deleter
{
    void (*destroy)(GLuint);
    void operator()(GLuint handle);
};

struct GL_array_deleter
{
    void (*destroy)(GLsizei, const GLuint *);
    void operator()(GLuint handle);
};

struct Window_state
{
    float scale_x;
    float scale_y;
    int framebuffer_width;
    int framebuffer_height;
    float scroll_offset;
};

struct Vertex
{
    vec2 position;
    vec4 local;
    vec3 color;
};

struct Raster_geometry
{
    std::size_t circle_indices_offset;
    std::size_t circle_indices_size;
    std::size_t line_indices_offset;
    std::size_t line_indices_size;
    std::size_t arc_indices_offset;
    std::size_t arc_indices_size;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct Application
{
    void run();
    void main_loop_update();

    Unique_resource<bool, GLFW_deleter> glfw_context {};
    Unique_resource<struct GLFWwindow *, Window_deleter> window {};
    Unique_resource<bool, ImGui_deleter> imgui_context {};
    Unique_resource<bool, ImGui_glfw_deleter> imgui_glfw_context {};
    Unique_resource<bool, ImGui_opengl_deleter> imgui_opengl_context {};
    Window_state window_state {};
    bool auto_workload {};
    int texture_width {};
    int texture_height {};
    Scene scene {};
    Unique_resource<GLuint, GL_array_deleter> accumulation_texture {};
    Unique_resource<GLuint, GL_array_deleter> target_texture {};
    Unique_resource<GLuint, GL_deleter> trace_program {};
#ifdef NO_COMPUTE_SHADER
    Unique_resource<GLuint, GL_array_deleter> empty_vao {};
    GLint loc_image_size {};
#endif
    GLint loc_sample_index {};
    GLint loc_samples_per_frame {};
    GLint loc_view_position {};
    GLint loc_view_size {};
    Unique_resource<GLuint, GL_deleter> post_program {};
#ifdef NO_COMPUTE_SHADER
    Unique_resource<GLuint, GL_array_deleter> float_fbo {};
#endif
    Unique_resource<GLuint, GL_array_deleter> fbo {};
#ifndef __EMSCRIPTEN__
    Unique_resource<GLuint, GL_array_deleter> query_start {};
    Unique_resource<GLuint, GL_array_deleter> query_end {};
#endif
    Unique_resource<GLuint, GL_array_deleter> materials_ubo {};
    Unique_resource<GLuint, GL_array_deleter> circles_ubo {};
    Unique_resource<GLuint, GL_array_deleter> lines_ubo {};
    Unique_resource<GLuint, GL_array_deleter> arcs_ubo {};
    float thickness {}; // In fraction of the view height
    Raster_geometry raster_geometry {};
    Unique_resource<GLuint, GL_array_deleter> vao {};
    Unique_resource<GLuint, GL_array_deleter> vbo {};
    Unique_resource<GLuint, GL_array_deleter> ibo {};
    Unique_resource<GLuint, GL_deleter> circle_program {};
    Unique_resource<GLuint, GL_deleter> line_program {};
    Unique_resource<GLuint, GL_deleter> arc_program {};
    GLint loc_view_position_draw_circle {};
    GLint loc_view_size_draw_circle {};
    GLint loc_view_position_draw_line {};
    GLint loc_view_size_draw_line {};
    GLint loc_view_position_draw_arc {};
    GLint loc_view_size_draw_arc {};
    unsigned int sample_index {};
    unsigned int samples_per_frame {};
    double last_time {};
    unsigned int sum_samples {};
    int num_frames {};
    bool p_pressed {};
    bool s_pressed {};
    bool l_pressed {};
    bool dragging {};
    bool draw_geometry {};
    float drag_source_mouse_x {};
    float drag_source_mouse_y {};
};

#endif

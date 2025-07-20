#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "scene.hpp"

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
#include <memory>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#define NO_COMPUTE_SHADER
#endif

struct Deleter
{
    void operator()(struct GLFWwindow *window);
    void operator()(struct ImGuiContext *ctx);
};

struct GL_array_deleter
{
    void (*destroy)(GLsizei, const GLuint *);
    void operator()(GLuint handle);
};

template <typename Destroy>
class GL_object
{
public:
    constexpr GL_object() : m_object {}, m_destroy {}
    {
    }

    constexpr GL_object(GLuint object, Destroy &&destroy)
        : m_object {object}, m_destroy {std::move(destroy)}
    {
    }

    constexpr GL_object(GLuint object, const Destroy &destroy)
        : m_object {object}, m_destroy {destroy}
    {
    }

    constexpr GL_object(GL_object &&rhs) noexcept
        : m_object {std::move(rhs.m_object)},
          m_destroy {std::move(rhs.m_destroy)}
    {
        rhs.m_object = 0;
    }

    constexpr GL_object &operator=(GL_object &&rhs) noexcept
    {
        GL_object tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    GL_object(const GL_object &) = delete;
    GL_object &operator=(const GL_object &) = delete;

    constexpr ~GL_object() noexcept
    {
        if (m_object != 0)
        {
            m_destroy(m_object);
        }
    }

    [[nodiscard]] constexpr GLuint get() const noexcept
    {
        return m_object;
    }

    constexpr void swap(GL_object &rhs) noexcept
    {
        using std::swap;
        swap(m_object, rhs.m_object);
        swap(m_destroy, rhs.m_destroy);
    }

private:
    GLuint m_object;
    Destroy m_destroy;
};

struct Window_state
{
    float scale_x;
    float scale_y;
    int framebuffer_width;
    int framebuffer_height;
    float scroll_offset;
};

struct vec4
{
    float x;
    float y;
    float z;
    float w;
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

    std::unique_ptr<struct GLFWwindow, Deleter> window {};
    std::unique_ptr<struct ImGuiContext, Deleter> imgui_context {};
    Window_state window_state {};
    bool auto_workload {};
    int texture_width {};
    int texture_height {};
    Scene scene {};
    GL_object<GL_array_deleter> accumulation_texture {};
    GL_object<GL_array_deleter> target_texture {};
    GL_object<PFNGLDELETEPROGRAMPROC> trace_program {};
#ifdef NO_COMPUTE_SHADER
    GL_object<GL_array_deleter> empty_vao {};
    GLint loc_image_size {};
#endif
    GLint loc_sample_index {};
    GLint loc_samples_per_frame {};
    GLint loc_view_position {};
    GLint loc_view_size {};
    GL_object<PFNGLDELETEPROGRAMPROC> post_program {};
#ifdef NO_COMPUTE_SHADER
    GL_object<GL_array_deleter> float_fbo {};
#endif
    GL_object<GL_array_deleter> fbo {};
#ifndef __EMSCRIPTEN__
    GL_object<GL_array_deleter> query_start {};
    GL_object<GL_array_deleter> query_end {};
#endif
    GL_object<GL_array_deleter> materials_ubo {};
    GL_object<GL_array_deleter> circles_ubo {};
    GL_object<GL_array_deleter> lines_ubo {};
    GL_object<GL_array_deleter> arcs_ubo {};
    float thickness {}; // In fraction of the view height
    Raster_geometry raster_geometry {};
    GL_object<GL_array_deleter> vao {};
    GL_object<GL_array_deleter> vbo {};
    GL_object<GL_array_deleter> ibo {};
    GL_object<PFNGLDELETEPROGRAMPROC> circle_program {};
    GL_object<PFNGLDELETEPROGRAMPROC> line_program {};
    GL_object<PFNGLDELETEPROGRAMPROC> arc_program {};
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

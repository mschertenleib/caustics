#include "application.hpp"
#include "scene.hpp"
#include "unique_handle.hpp"
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

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

constexpr std::size_t max_surfaces {256};
constexpr std::size_t max_volumes {32};
constexpr std::size_t max_lines {256};
constexpr std::size_t max_arcs {256};
constexpr std::size_t max_parabolas {256};
constexpr std::size_t max_ubo_size {16'384};

static_assert(max_surfaces * sizeof(Surface) <= max_ubo_size);
static_assert(max_volumes * sizeof(Volume) <= max_ubo_size);
static_assert(max_lines * sizeof(Line) <= max_ubo_size);
static_assert(max_arcs * sizeof(Arc) <= max_ubo_size);
static_assert(max_parabolas * sizeof(Parabola) <= max_ubo_size);

#define ENUMERATE_GL_FUNCTIONS_COMMON(f)                                       \
    f(PFNGLENABLEPROC, glEnable);                                              \
    f(PFNGLCREATESHADERPROC, glCreateShader);                                  \
    f(PFNGLDELETESHADERPROC, glDeleteShader);                                  \
    f(PFNGLSHADERSOURCEPROC, glShaderSource);                                  \
    f(PFNGLCOMPILESHADERPROC, glCompileShader);                                \
    f(PFNGLGETSHADERIVPROC, glGetShaderiv);                                    \
    f(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);                          \
    f(PFNGLATTACHSHADERPROC, glAttachShader);                                  \
    f(PFNGLCREATEPROGRAMPROC, glCreateProgram);                                \
    f(PFNGLDELETEPROGRAMPROC, glDeleteProgram);                                \
    f(PFNGLLINKPROGRAMPROC, glLinkProgram);                                    \
    f(PFNGLGETPROGRAMIVPROC, glGetProgramiv);                                  \
    f(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);                        \
    f(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);                      \
    f(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex);                  \
    f(PFNGLUNIFORM1IPROC, glUniform1i);                                        \
    f(PFNGLUNIFORM1UIPROC, glUniform1ui);                                      \
    f(PFNGLUNIFORM2UIPROC, glUniform2ui);                                      \
    f(PFNGLUNIFORM1FPROC, glUniform1f);                                        \
    f(PFNGLUNIFORM2FPROC, glUniform2f);                                        \
    f(PFNGLGENTEXTURESPROC, glGenTextures);                                    \
    f(PFNGLDELETETEXTURESPROC, glDeleteTextures);                              \
    f(PFNGLBINDTEXTUREPROC, glBindTexture);                                    \
    f(PFNGLTEXPARAMETERIPROC, glTexParameteri);                                \
    f(PFNGLTEXIMAGE2DPROC, glTexImage2D);                                      \
    f(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);                            \
    f(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers);                      \
    f(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);                            \
    f(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D);                  \
    f(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus);              \
    f(PFNGLGENBUFFERSPROC, glGenBuffers);                                      \
    f(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);                                \
    f(PFNGLBINDBUFFERPROC, glBindBuffer);                                      \
    f(PFNGLBUFFERDATAPROC, glBufferData);                                      \
    f(PFNGLBINDBUFFERBASEPROC, glBindBufferBase);                              \
    f(PFNGLBUFFERSUBDATAPROC, glBufferSubData);                                \
    f(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);                            \
    f(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);                      \
    f(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);                            \
    f(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding);                    \
    f(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);                    \
    f(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);            \
    f(PFNGLDRAWELEMENTSPROC, glDrawElements);                                  \
    f(PFNGLDRAWARRAYSPROC, glDrawArrays);                                      \
    f(PFNGLUSEPROGRAMPROC, glUseProgram);                                      \
    f(PFNGLVIEWPORTPROC, glViewport);                                          \
    f(PFNGLCLEARCOLORPROC, glClearColor);                                      \
    f(PFNGLCLEARPROC, glClear);                                                \
    f(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer);                            \
    f(PFNGLGETINTEGERVPROC, glGetIntegerv);                                    \
    f(PFNGLGETSTRINGIPROC, glGetStringi);                                      \
    f(PFNGLFINISHPROC, glFinish);                                              \
    f(PFNGLBLENDEQUATIONPROC, glBlendEquation);                                \
    f(PFNGLBLENDFUNCPROC, glBlendFunc);                                        \
    f(PFNGLBLENDCOLORPROC, glBlendColor);                                      \
    f(PFNGLACTIVETEXTUREPROC, glActiveTexture);                                \
    f(PFNGLGETSTRINGPROC, glGetString);                                        \
    f(PFNGLREADPIXELSPROC, glReadPixels);

#define ENUMERATE_GL_FUNCTIONS_430(f)                                          \
    f(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback);                  \
    f(PFNGLGENQUERIESPROC, glGenQueries);                                      \
    f(PFNGLDELETEQUERIESPROC, glDeleteQueries);                                \
    f(PFNGLQUERYCOUNTERPROC, glQueryCounter);                                  \
    f(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v);

#ifndef __EMSCRIPTEN__
#define ENUMERATE_GL_FUNCTIONS(f)                                              \
    ENUMERATE_GL_FUNCTIONS_COMMON(f) ENUMERATE_GL_FUNCTIONS_430(f)
#else
#define ENUMERATE_GL_FUNCTIONS(f) ENUMERATE_GL_FUNCTIONS_COMMON(f)
#endif

// clang-format off
#define DECLARE_GL_FUNCTION(type, name) type name {nullptr}
// clang-format on

ENUMERATE_GL_FUNCTIONS(DECLARE_GL_FUNCTION)

struct GLFW_deleter
{
    void operator()()
    {
        glfwTerminate();
    }
};

struct Window_deleter
{
    void operator()(struct GLFWwindow *window)
    {
        glfwDestroyWindow(window);
    }
};

struct ImGui_deleter
{
    void operator()()
    {
        ImGui::DestroyContext();
    }
};

struct ImGui_glfw_deleter
{
    void operator()()
    {
        ImGui_ImplGlfw_Shutdown();
    }
};

struct ImGui_opengl_deleter
{
    void operator()()
    {
        ImGui_ImplOpenGL3_Shutdown();
    }
};

struct GL_deleter
{
    void (*destroy)(GLuint);
    void operator()(GLuint handle)
    {
        destroy(handle);
    }
};

struct GL_array_deleter
{
    void (*destroy)(GLsizei, const GLuint *);
    void operator()(GLuint handle)
    {
        destroy(1, &handle);
    }
};

struct Window_state
{
    float scale_x;
    float scale_y;
    int framebuffer_width;
    int framebuffer_height;
    bool framebuffer_size_changed;
    float scroll_offset;
};

struct Raster_line
{
    vec2 vertex_a;
    vec2 vertex_b;
    vec3 color;
    float thickness;
};

struct Raster_arc
{
    vec2 center;
    float radius;
    float clip_offset;
    vec2 clip_normal;
    vec3 color;
    float thickness;
};

struct Raster_parabola
{
    vec2 vertex;
    vec2 axis;
    float focal;
    float clip_x_min;
    float clip_x_max;
    vec3 color;
    float thickness;
};

struct Vertex
{
    vec2 position;
    vec2 local;
    vec4 params;
    vec3 color;
};

struct Raster_geometry
{
    std::vector<Raster_line> lines;
    std::vector<Raster_arc> arcs;
    std::vector<Raster_parabola> parabolas;
    std::size_t line_indices_offset;
    std::size_t line_indices_size;
    std::size_t arc_indices_offset;
    std::size_t arc_indices_size;
    std::size_t parabola_indices_offset;
    std::size_t parabola_indices_size;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct Application_state
{
    Unique_handle<bool, GLFW_deleter> glfw_context {};
    Unique_handle<struct GLFWwindow *, Window_deleter> window {};
    Unique_handle<bool, ImGui_deleter> imgui_context {};
    Unique_handle<bool, ImGui_glfw_deleter> imgui_glfw_context {};
    Unique_handle<bool, ImGui_opengl_deleter> imgui_opengl_context {};
    Window_state window_state {};
    bool auto_workload {};
    int texture_width {};
    int texture_height {};
    Scene scene {};
    Unique_handle<GLuint, GL_array_deleter> accumulation_texture {};
    Unique_handle<GLuint, GL_array_deleter> target_texture {};
    Unique_handle<GLuint, GL_deleter> trace_program {};
    Unique_handle<GLuint, GL_array_deleter> empty_vao {};
    GLint loc_num_lines {};
    GLint loc_num_arcs {};
    GLint loc_num_parabolas {};
    GLint loc_image_size {};
    GLint loc_sample_index {};
    GLint loc_samples_per_frame {};
    GLint loc_view_position {};
    GLint loc_view_size {};
    Unique_handle<GLuint, GL_deleter> post_program {};
    GLint loc_exposure_value {};
    float exposure_value {};
    Unique_handle<GLuint, GL_array_deleter> float_fbo {};
    Unique_handle<GLuint, GL_array_deleter> fbo {};
#ifndef __EMSCRIPTEN__
    Unique_handle<GLuint, GL_array_deleter> query_start {};
    Unique_handle<GLuint, GL_array_deleter> query_end {};
#endif
    Unique_handle<GLuint, GL_array_deleter> surfaces_ubo {};
    Unique_handle<GLuint, GL_array_deleter> volumes_ubo {};
    Unique_handle<GLuint, GL_array_deleter> lines_ubo {};
    Unique_handle<GLuint, GL_array_deleter> arcs_ubo {};
    Unique_handle<GLuint, GL_array_deleter> parabolas_ubo {};
    float thickness {}; // In fraction of the view height // FIXME
    Raster_geometry raster_geometry {};
    Unique_handle<GLuint, GL_array_deleter> vao {};
    Unique_handle<GLuint, GL_array_deleter> vbo {};
    Unique_handle<GLuint, GL_array_deleter> ibo {};
    Unique_handle<GLuint, GL_deleter> parabola_program {};
    Unique_handle<GLuint, GL_deleter> line_program {};
    Unique_handle<GLuint, GL_deleter> arc_program {};
    GLint loc_view_position_draw_parabola {};
    GLint loc_view_size_draw_parabola {};
    GLint loc_view_position_draw_line {};
    GLint loc_view_size_draw_line {};
    GLint loc_view_position_draw_arc {};
    GLint loc_view_size_draw_arc {};
    unsigned int max_samples {500'000};
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

template <std::invocable C, std::invocable<GLuint> D>
[[nodiscard]] auto create_gl_object(C &&create, D &&destroy)
{
    return Unique_handle(create(), GL_deleter {destroy});
}

template <std::invocable<GLenum> C, std::invocable<GLuint> D>
[[nodiscard]] auto create_gl_object(C &&create, GLenum arg, D &&destroy)
{
    return Unique_handle(create(arg), GL_deleter {destroy});
}

template <std::invocable<GLsizei, GLuint *> C,
          std::invocable<GLsizei, const GLuint *> D>
[[nodiscard]] auto create_gl_object(C &&create, D &&destroy)
{
    GLuint object {};
    create(1, &object);
    return Unique_handle(object, GL_array_deleter {destroy});
}

struct Viewport
{
    int x;
    int y;
    int width;
    int height;
};

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void glfw_window_content_scale_callback(GLFWwindow *window,
                                        float xscale,
                                        float yscale)
{
    auto *const window_state =
        static_cast<Window_state *>(glfwGetWindowUserPointer(window));
    assert(window_state);

    window_state->scale_x = xscale;
    window_state->scale_y = yscale;
}

void glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    auto *const window_state =
        static_cast<Window_state *>(glfwGetWindowUserPointer(window));
    assert(window_state);

    window_state->framebuffer_width = width;
    window_state->framebuffer_height = height;
    window_state->framebuffer_size_changed = true;
}

void glfw_scroll_callback(GLFWwindow *window,
                          [[maybe_unused]] double xoffset,
                          double yoffset)
{
    auto *const window_state =
        static_cast<Window_state *>(glfwGetWindowUserPointer(window));
    assert(window_state);

    window_state->scroll_offset = static_cast<float>(yoffset);
}

void load_gl_functions()
{
#define LOAD_GL_FUNCTION(type, name)                                           \
    name = reinterpret_cast<type>(glfwGetProcAddress(#name));                  \
    assert(name != nullptr)

    ENUMERATE_GL_FUNCTIONS(LOAD_GL_FUNCTION)

#undef LOAD_GL_FUNCTION
}

#ifndef __EMSCRIPTEN__
void APIENTRY gl_debug_callback([[maybe_unused]] GLenum source,
                                GLenum type,
                                [[maybe_unused]] GLuint id,
                                GLenum severity,
                                [[maybe_unused]] GLsizei length,
                                const GLchar *message,
                                [[maybe_unused]] const void *user_param)
{
    if (type == GL_DEBUG_TYPE_OTHER ||
        severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        return;
    }
    std::cerr << message << '\n';
}
#endif

[[nodiscard]] std::vector<const char *> get_all_extensions()
{
    int num_extensions {};
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

    std::vector<const char *> extensions;
    extensions.reserve(static_cast<std::size_t>(num_extensions));

    for (int i {0}; i < num_extensions; ++i)
    {
        const auto *const str =
            glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
        extensions.push_back(reinterpret_cast<const char *>(str));
    }

    return extensions;
}

[[nodiscard]] std::string read_file(const char *file_name)
{
    std::ifstream file(file_name);
    if (!file.is_open())
    {
        std::ostringstream oss;
        oss << "Failed to open file \"" << file_name << '\"';
        throw std::runtime_error(oss.str());
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

[[nodiscard]] constexpr Viewport centered_viewport(int src_width,
                                                   int src_height,
                                                   int dst_width,
                                                   int dst_height) noexcept
{
    const auto src_aspect_ratio =
        static_cast<float>(src_width) / static_cast<float>(src_height);
    const auto dst_aspect_ratio =
        static_cast<float>(dst_width) / static_cast<float>(dst_height);
    if (src_aspect_ratio > dst_aspect_ratio)
    {
        const auto height =
            static_cast<int>(static_cast<float>(dst_width) / src_aspect_ratio);
        return {0, (dst_height - height) / 2, dst_width, height};
    }
    else
    {
        const auto width =
            static_cast<int>(static_cast<float>(dst_height) * src_aspect_ratio);
        return {(dst_width - width) / 2, 0, width, dst_height};
    }
}

[[nodiscard]] auto create_shader(GLenum type,
                                 std::size_t sources_size,
                                 const char *const sources[])
{
    auto shader = create_gl_object(glCreateShader, type, glDeleteShader);

    glShaderSource(
        shader.get(), static_cast<GLsizei>(sources_size), sources, nullptr);
    glCompileShader(shader.get());

    int success {};
    glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetShaderiv(shader.get(), GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetShaderInfoLog(shader.get(), buf_length, nullptr, message.data());
        throw std::runtime_error(
            std::format("Shader compilation failed:\n{}\n", message));
    }

    return shader;
}

[[nodiscard]] auto create_program_object(GLuint vertex_shader,
                                         GLuint fragment_shader)
{
    auto program = create_gl_object(glCreateProgram, glDeleteProgram);

    glAttachShader(program.get(), vertex_shader);
    glAttachShader(program.get(), fragment_shader);
    glLinkProgram(program.get());

    int success {};
    glGetProgramiv(program.get(), GL_LINK_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetProgramiv(program.get(), GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetProgramInfoLog(program.get(), buf_length, nullptr, message.data());
        throw std::runtime_error(
            std::format("Program linking failed:\n{}\n", message));
    }

    return program;
}

[[nodiscard]] auto create_trace_program(const char *glsl_version)
{
    const auto vertex_shader_code = read_file("shaders/fullscreen.vert");
    const char *const vertex_shader_sources[] {
        glsl_version, "\n", vertex_shader_code.c_str()};
    const auto vertex_shader = create_shader(GL_VERTEX_SHADER,
                                             std::size(vertex_shader_sources),
                                             vertex_shader_sources);

    const auto fragment_shader_code = read_file("shaders/trace.frag");
    const auto header = std::format("{}\n"
                                    "#define MAX_SURFACES {}u\n"
                                    "#define MAX_VOLUMES {}u\n"
                                    "#define MAX_LINES {}u\n"
                                    "#define MAX_ARCS {}u\n"
                                    "#define MAX_PARABOLAS {}u\n",
                                    glsl_version,
                                    max_surfaces,
                                    max_volumes,
                                    max_lines,
                                    max_arcs,
                                    max_parabolas);
    const char *const fragment_shader_sources[] {header.c_str(),
                                                 fragment_shader_code.c_str()};
    const auto fragment_shader =
        create_shader(GL_FRAGMENT_SHADER,
                      std::size(fragment_shader_sources),
                      fragment_shader_sources);

    return create_program_object(vertex_shader.get(), fragment_shader.get());
}

[[nodiscard]] auto create_shapes_programs(const char *glsl_version)
{
    const auto vertex_shader_code = read_file("shaders/shapes.vert");
    const char *const vertex_shader_sources[] {
        glsl_version, "\n", vertex_shader_code.c_str()};
    const auto vertex_shader = create_shader(GL_VERTEX_SHADER,
                                             std::size(vertex_shader_sources),
                                             vertex_shader_sources);

    const auto fragment_shader_code = read_file("shaders/shapes.frag");

    const auto make_program = [&](const char *define)
    {
        const char *const fragment_shader_sources[] {
            glsl_version,
            "\n#define ",
            define,
            "\n",
            fragment_shader_code.c_str()};
        const auto fragment_shader =
            create_shader(GL_FRAGMENT_SHADER,
                          std::size(fragment_shader_sources),
                          fragment_shader_sources);
        return create_program_object(vertex_shader.get(),
                                     fragment_shader.get());
    };

    return std::tuple {
        make_program("LINE"), make_program("ARC"), make_program("PARABOLA")};
}

[[nodiscard]] auto create_program(const char *glsl_version,
                                  const char *vertex_shader_file_name,
                                  const char *fragment_shader_file_name)
{
    const auto vertex_shader_code = read_file(vertex_shader_file_name);
    const char *const vertex_shader_sources[] {
        glsl_version, "\n", vertex_shader_code.c_str()};
    const auto vertex_shader = create_shader(GL_VERTEX_SHADER,
                                             std::size(vertex_shader_sources),
                                             vertex_shader_sources);

    const auto fragment_shader_code = read_file(fragment_shader_file_name);
    const char *const fragment_shader_sources[] {
        glsl_version, "\n", fragment_shader_code.c_str()};
    const auto fragment_shader =
        create_shader(GL_FRAGMENT_SHADER,
                      std::size(fragment_shader_sources),
                      fragment_shader_sources);

    return create_program_object(vertex_shader.get(), fragment_shader.get());
}

[[nodiscard]] auto create_accumulation_texture(GLsizei width, GLsizei height)
{
    auto texture = create_gl_object(glGenTextures, glDeleteTextures);

    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA32F,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

[[nodiscard]] auto create_target_texture(GLsizei width, GLsizei height)
{
    auto texture = create_gl_object(glGenTextures, glDeleteTextures);

    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8, // FIXME: should this be sRGB ?
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

[[nodiscard]] auto create_framebuffer(GLuint texture)
{
    auto fbo = create_gl_object(glGenFramebuffers, glDeleteFramebuffers);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::ostringstream oss;
        oss << "Framebuffer error: ";
        switch (status)
        {
        case GL_FRAMEBUFFER_UNDEFINED: oss << "GL_FRAMEBUFFER_UNDEFINED"; break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            oss << "GL_FRAMEBUFFER_UNSUPPORTED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
            break;
#ifndef __EMSCRIPTEN__
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
            break;
#endif
        default:
            oss << "0x" << std::setfill('0') << std::setw(4) << std::hex
                << status;
        }
        oss << '\n';
        throw std::runtime_error(oss.str());
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return fbo;
}

[[nodiscard]] auto
create_vertex_and_index_buffers(const Raster_geometry &geometry)
{
    auto vao = create_gl_object(glGenVertexArrays, glDeleteVertexArrays);
    glBindVertexArray(vao.get());

    auto vbo = create_gl_object(glGenBuffers, glDeleteBuffers);
    glBindBuffer(GL_ARRAY_BUFFER, vbo.get());
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizei>(geometry.vertices.size() * sizeof(Vertex)),
        geometry.vertices.data(),
        GL_DYNAMIC_DRAW);

    auto ibo = create_gl_object(glGenBuffers, glDeleteBuffers);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo.get());
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizei>(geometry.indices.size() * sizeof(std::uint32_t)),
        geometry.indices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(0,
                          sizeof(Vertex::position) / sizeof(float),
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, position)));
    glVertexAttribPointer(1,
                          sizeof(Vertex::local) / sizeof(float),
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, local)));
    glVertexAttribPointer(2,
                          sizeof(Vertex::params) / sizeof(float),
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, params)));
    glVertexAttribPointer(3,
                          sizeof(Vertex::color) / sizeof(float),
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    return std::tuple {std::move(vao), std::move(vbo), std::move(ibo)};
}

void update_vertex_buffer(GLuint vao,
                          GLuint vbo,
                          const Raster_geometry &geometry)
{
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizei>(geometry.vertices.size() * sizeof(Vertex)),
        geometry.vertices.data());

    glBindVertexArray(0);
}

[[nodiscard]] auto create_uniform_buffer(std::size_t size)
{
    if (size > max_ubo_size)
    {
        throw std::runtime_error(std::format(
            "Uniform buffer too big ({}B > {}B)", size, max_ubo_size));
    }

    auto ubo = create_gl_object(glGenBuffers, glDeleteBuffers);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo.get());
    glBufferData(GL_UNIFORM_BUFFER,
                 static_cast<GLsizeiptr>(size),
                 nullptr,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return ubo;
}

template <typename T>
void upload_uniform_buffer(GLuint ubo, const std::vector<T> &data)
{
    // FIXME: we should compare against the UBO's allocated capacity, not the
    // max. Or actually compare the number of elements, but we can't do it from
    // inside here. Should we just avoid T and make this function accept a
    // std::span<const std::byte> ?
    const auto data_size = data.size() * sizeof(T);
    if (data_size > max_ubo_size)
    {
        throw std::runtime_error(std::format(
            "Uniform buffer too big ({}B > {}B)", data_size, max_ubo_size));
    }

    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(data.size() * sizeof(T)),
                    data.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

#ifdef __EMSCRIPTEN__
// clang-format off
EM_JS(void, download_as_png_js, (const char* file_name, const std::uint8_t* data_ptr, std::size_t data_size), {
    const filename = UTF8ToString(file_name);
    const view = HEAPU8.subarray(data_ptr, data_ptr + data_size);

    const blob = new Blob([view], { type: 'image/png' });
    const url = URL.createObjectURL(blob);

    const a = document.createElement('a');
    a.style.display = 'none';
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
});
// clang-format on
#endif

void save_as_png(const char *file_name, int width, int height, GLuint fbo)
{
    std::cout << "Saving " << width << " x " << height << " image to \""
              << file_name << "\"\n";

    constexpr int channels {4};
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width * height * channels));

    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // The OpenGL image origin is in the bottom-left corner
    stbi_flip_vertically_on_write(1);

    // TODO: cleanup

#ifdef __EMSCRIPTEN__

    int png_size {};
    const std::unique_ptr<std::uint8_t, decltype(&std::free)> png(
        stbi_write_png_to_mem(pixels.data(),
                              width * channels,
                              width,
                              height,
                              channels,
                              &png_size),
        &std::free);
    if (!png)
    {
        throw std::runtime_error("Failed to write PNG image");
    }

    download_as_png_js(
        file_name, png.get(), static_cast<std::size_t>(png_size));

#else

    const auto write_result = stbi_write_png(
        file_name, width, height, channels, pixels.data(), width * channels);
    if (write_result == 0)
    {
        throw std::runtime_error(
            std::format("Failed to write PNG image to \"{}\"", file_name));
    }

#endif
}

[[nodiscard]] constexpr float screen_to_world(float x,
                                              int screen_min,
                                              int screen_size,
                                              float world_center,
                                              float world_size) noexcept
{
    const auto u =
        (x - static_cast<float>(screen_min)) / static_cast<float>(screen_size);
    return world_center + (u - 0.5f) * world_size;
}

void clear_geometry(Raster_geometry &geometry)
{
    geometry.lines.clear();
    geometry.arcs.clear();
    geometry.parabolas.clear();
}

void push_line(const Raster_line &line, Raster_geometry &geometry)
{
    geometry.lines.push_back(line);
}

void push_arc(const Raster_arc &arc, Raster_geometry &geometry)
{
    geometry.arcs.push_back(arc);
}

void push_parabola(const Raster_parabola &parabola, Raster_geometry &geometry)
{
    geometry.parabolas.push_back(parabola);
}

void push_scene_geometry(const Scene &scene,
                         float thickness,
                         Raster_geometry &geometry)
{
    thickness *= scene.view_height;

    const auto get_color = [&scene](std::uint32_t surface_id)
    {
        if (surface_id != invalid_id)
        {
            return scene.surfaces[surface_id].base_color;
        }
        else
        {
            return vec3 {1.0f, 1.0f, 1.0f};
        }
    };

    for (const auto &line : scene.lines)
    {
        push_line({.vertex_a = line.vertex_a,
                   .vertex_b = line.vertex_b,
                   .color = get_color(line.surface_id),
                   .thickness = thickness},
                  geometry);
    }

    for (const auto &arc : scene.arcs)
    {
        push_arc({.center = arc.center,
                  .radius = arc.radius,
                  .clip_offset = arc.clip_normal != vec2 {} ? arc.clip_offset
                                                            : -arc.radius,
                  .clip_normal = arc.clip_normal != vec2 {} ? arc.clip_normal
                                                            : vec2 {0.0f, 1.0f},
                  .color = get_color(arc.surface_id),
                  .thickness = thickness},
                 geometry);
    }

    for (const auto &parabola : scene.parabolas)
    {
        push_parabola({.vertex = parabola.vertex,
                       .axis = parabola.axis,
                       .focal = parabola.focal,
                       .clip_x_min = parabola.clip_x_min,
                       .clip_x_max = parabola.clip_x_max,
                       .color = get_color(parabola.surface_id),
                       .thickness = thickness},
                      geometry);
    }
}

void create_vertex_and_index_data(Raster_geometry &geometry,
                                  float view_height,
                                  int viewport_height)
{
    geometry.vertices.clear();
    geometry.indices.clear();

    const auto pixel_size = view_height / static_cast<float>(viewport_height);
    const auto half_pixel_size = pixel_size * 0.5f;

    geometry.line_indices_offset = geometry.indices.size();
    for (const auto &line : geometry.lines)
    {
        const auto line_vec = line.vertex_b - line.vertex_a;
        const auto line_length = norm(line_vec);
        const auto local_x_max = line.thickness + half_pixel_size;
        const auto local_x_min = -local_x_max;
        const auto local_y_min = -local_x_max;
        const auto local_y_max = line_length + line.thickness + half_pixel_size;

        const auto up = line_vec * (1.0f / line_length);
        const vec2 right {up.y, -up.x};
        const auto bottom_left =
            line.vertex_a + local_x_min * right + local_y_min * up;
        const auto bottom_right =
            line.vertex_a + local_x_max * right + local_y_min * up;
        const auto top_right =
            line.vertex_a + local_x_max * right + local_y_max * up;
        const auto top_left =
            line.vertex_a + local_x_min * right + local_y_max * up;

        const auto first_index =
            static_cast<std::uint32_t>(geometry.vertices.size());
        geometry.vertices.push_back(
            {.position = bottom_left,
             .local = {local_x_min, local_y_min},
             .params = {line_length, 0.0f, 0.0f, line.thickness},
             .color = line.color});
        geometry.vertices.push_back(
            {.position = bottom_right,
             .local = {local_x_max, local_y_min},
             .params = {line_length, 0.0f, 0.0f, line.thickness},
             .color = line.color});
        geometry.vertices.push_back(
            {.position = top_right,
             .local = {local_x_max, local_y_max},
             .params = {line_length, 0.0f, 0.0f, line.thickness},
             .color = line.color});
        geometry.vertices.push_back(
            {.position = top_left,
             .local = {local_x_min, local_y_max},
             .params = {line_length, 0.0f, 0.0f, line.thickness},
             .color = line.color});
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 1);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 3);
    }
    geometry.line_indices_size =
        geometry.indices.size() - geometry.line_indices_offset;

    geometry.arc_indices_offset = geometry.indices.size();
    for (const auto &arc : geometry.arcs)
    {
        const auto dir_end_y = arc.clip_offset / arc.radius;
        const auto dir_end_x = std::sqrt(1.0f - dir_end_y * dir_end_y);
        const auto local_x_max =
            arc.clip_offset > 0.0f
                ? dir_end_x * arc.radius + arc.thickness + half_pixel_size
                : arc.radius + arc.thickness + half_pixel_size;
        const auto local_x_min = -local_x_max;
        const auto local_y_min =
            arc.clip_offset - arc.thickness - half_pixel_size;
        const auto local_y_max = arc.radius + arc.thickness + half_pixel_size;

        const auto up = arc.clip_normal;
        const vec2 right {up.y, -up.x};
        const auto bottom_left =
            arc.center + local_x_min * right + local_y_min * up;
        const auto bottom_right =
            arc.center + local_x_max * right + local_y_min * up;
        const auto top_right =
            arc.center + local_x_max * right + local_y_max * up;
        const auto top_left =
            arc.center + local_x_min * right + local_y_max * up;

        const auto first_index =
            static_cast<std::uint32_t>(geometry.vertices.size());
        geometry.vertices.push_back(
            {.position = bottom_left,
             .local = {local_x_min, local_y_min},
             .params = {dir_end_x, dir_end_y, arc.radius, arc.thickness},
             .color = arc.color});
        geometry.vertices.push_back(
            {.position = bottom_right,
             .local = {local_x_max, local_y_min},
             .params = {dir_end_x, dir_end_y, arc.radius, arc.thickness},
             .color = arc.color});
        geometry.vertices.push_back(
            {.position = top_right,
             .local = {local_x_max, local_y_max},
             .params = {dir_end_x, dir_end_y, arc.radius, arc.thickness},
             .color = arc.color});
        geometry.vertices.push_back(
            {.position = top_left,
             .local = {local_x_min, local_y_max},
             .params = {dir_end_x, dir_end_y, arc.radius, arc.thickness},
             .color = arc.color});
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 1);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 3);
    }
    geometry.arc_indices_size =
        geometry.indices.size() - geometry.arc_indices_offset;

    geometry.parabola_indices_offset = geometry.indices.size();
    for (const auto &parabola : geometry.parabolas)
    {
        const auto clip_x_min = parabola.clip_x_min;
        const auto clip_x_max = parabola.clip_x_max;

        // Closest x to the vertex at x = 0
        const auto x0 = std::clamp(0.0f, clip_x_min, clip_x_max);
        const auto curve_y_min = (x0 * x0) / (4.0f * parabola.focal);
        const auto curve_y_max =
            std::max(clip_x_min * clip_x_min, clip_x_max * clip_x_max) /
            (4.0f * parabola.focal);
        const auto local_x_min =
            clip_x_min - parabola.thickness - half_pixel_size;
        const auto local_x_max =
            clip_x_max + parabola.thickness + half_pixel_size;
        const auto local_y_min =
            curve_y_min - parabola.thickness - half_pixel_size;
        const auto local_y_max =
            curve_y_max + parabola.thickness + half_pixel_size;

        const auto up = parabola.axis;
        const vec2 right {up.y, -up.x};
        const auto bottom_left =
            parabola.vertex + local_x_min * right + local_y_min * up;
        const auto bottom_right =
            parabola.vertex + local_x_max * right + local_y_min * up;
        const auto top_right =
            parabola.vertex + local_x_max * right + local_y_max * up;
        const auto top_left =
            parabola.vertex + local_x_min * right + local_y_max * up;

        const auto first_index =
            static_cast<std::uint32_t>(geometry.vertices.size());
        geometry.vertices.push_back({.position = bottom_left,
                                     .local = {local_x_min, local_y_min},
                                     .params = {parabola.focal,
                                                clip_x_min,
                                                clip_x_max,
                                                parabola.thickness},
                                     .color = parabola.color});
        geometry.vertices.push_back({.position = bottom_right,
                                     .local = {local_x_max, local_y_min},
                                     .params = {parabola.focal,
                                                clip_x_min,
                                                clip_x_max,
                                                parabola.thickness},
                                     .color = parabola.color});
        geometry.vertices.push_back({.position = top_right,
                                     .local = {local_x_max, local_y_max},
                                     .params = {parabola.focal,
                                                clip_x_min,
                                                clip_x_max,
                                                parabola.thickness},
                                     .color = parabola.color});
        geometry.vertices.push_back({.position = top_left,
                                     .local = {local_x_min, local_y_max},
                                     .params = {parabola.focal,
                                                clip_x_min,
                                                clip_x_max,
                                                parabola.thickness},
                                     .color = parabola.color});
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 1);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 3);
    }
    geometry.parabola_indices_size =
        geometry.indices.size() - geometry.parabola_indices_offset;
}

void init_application(Application_state &app)
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    app.glfw_context.reset(true);

#ifdef __EMSCRIPTEN__
    // WebGL 2.0
    constexpr auto glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#else
    constexpr auto glsl_version = "#version 430 core";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 0);

    auto *const window_ptr =
        glfwCreateWindow(1280, 720, "Caustics", nullptr, nullptr);
    if (window_ptr == nullptr)
    {
        // FIXME ???
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    app.window.reset(window_ptr);

    glfwMakeContextCurrent(app.window.get());

    glfwSetWindowUserPointer(app.window.get(), &app.window_state);

    glfwSetWindowContentScaleCallback(app.window.get(),
                                      &glfw_window_content_scale_callback);
    glfwSetFramebufferSizeCallback(app.window.get(),
                                   &glfw_framebuffer_size_callback);
    glfwSetScrollCallback(app.window.get(), &glfw_scroll_callback);

    glfwGetWindowContentScale(
        app.window.get(), &app.window_state.scale_x, &app.window_state.scale_y);
    glfwGetFramebufferSize(app.window.get(),
                           &app.window_state.framebuffer_width,
                           &app.window_state.framebuffer_height);

    glfwSwapInterval(1);

    load_gl_functions();

#ifdef __EMSCRIPTEN__
    constexpr const char *required_extensions[] {"GL_EXT_color_buffer_float"};
    const auto supported_extensions = get_all_extensions();
    for (const char *const required_extension : required_extensions)
    {
        if (std::none_of(
                supported_extensions.cbegin(),
                supported_extensions.cend(),
                [required_extension](const char *extension)
                { return std::strcmp(extension, required_extension) == 0; }))
        {
            std::ostringstream message;
            message << "Extension " << required_extension
                    << " is not supported ";
            throw std::runtime_error(message.str());
        }
    }
#else
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&gl_debug_callback, nullptr);
#endif

    const std::string_view renderer(
        reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    std::cout << "Renderer: \"" << renderer << "\"\n";

#ifndef __EMSCRIPTEN__
    app.auto_workload = false;
    if (renderer.find("Mesa") != std::string_view::npos &&
        renderer.find("Intel") != std::string_view::npos)
    {
        std::cout << "Disabling adaptive workload\n";
        // FIXME: this is just to get some decent performance, but we should
        // really find a way to activate V-Sync even when timer queries do not
        // work.
        glfwSwapInterval(0);
    }
    else
    {
        std::cout << "Enabling adaptive workload\n";
        app.auto_workload = true;
    }
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    app.imgui_context.reset(true);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(app.window.get(), true))
    {
        throw std::runtime_error("ImGui: failed to initialize GLFW backend");
    }
    app.imgui_glfw_context.reset(true);

#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(app.window.get(), "#canvas");
#endif

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        throw std::runtime_error("ImGui: failed to initialize OpenGL backend");
    }
    app.imgui_opengl_context.reset(true);

    app.texture_width = 320;
    app.texture_height = 240;
    app.scene = create_scene(app.texture_width, app.texture_height);

    app.accumulation_texture =
        create_accumulation_texture(app.texture_width, app.texture_height);

    app.target_texture =
        create_target_texture(app.texture_width, app.texture_height);

    app.trace_program = create_trace_program(glsl_version);
    app.empty_vao = create_gl_object(glGenVertexArrays, glDeleteVertexArrays);
    app.loc_num_lines =
        glGetUniformLocation(app.trace_program.get(), "num_lines");
    app.loc_num_arcs =
        glGetUniformLocation(app.trace_program.get(), "num_arcs");
    app.loc_num_parabolas =
        glGetUniformLocation(app.trace_program.get(), "num_parabolas");
    app.loc_image_size =
        glGetUniformLocation(app.trace_program.get(), "image_size");
    app.loc_sample_index =
        glGetUniformLocation(app.trace_program.get(), "sample_index");
    app.loc_samples_per_frame =
        glGetUniformLocation(app.trace_program.get(), "samples_per_frame");
    app.loc_view_position =
        glGetUniformLocation(app.trace_program.get(), "view_position");
    app.loc_view_size =
        glGetUniformLocation(app.trace_program.get(), "view_size");

    app.post_program = create_program(
        glsl_version, "shaders/fullscreen.vert", "shaders/post.frag");
    app.loc_exposure_value =
        glGetUniformLocation(app.post_program.get(), "exposure_value");

    glUseProgram(app.post_program.get());
    glUniform1i(
        glGetUniformLocation(app.post_program.get(), "accumulation_texture"),
        0);

    app.float_fbo = create_framebuffer(app.accumulation_texture.get());

    app.fbo = create_framebuffer(app.target_texture.get());

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifndef __EMSCRIPTEN__
    app.query_start = create_gl_object(glGenQueries, glDeleteQueries);
    app.query_end = create_gl_object(glGenQueries, glDeleteQueries);
#endif

    app.surfaces_ubo = create_uniform_buffer(max_surfaces * sizeof(Surface));
    app.volumes_ubo = create_uniform_buffer(max_volumes * sizeof(Volume));
    app.lines_ubo = create_uniform_buffer(max_lines * sizeof(Line));
    app.arcs_ubo = create_uniform_buffer(max_arcs * sizeof(Arc));
    app.parabolas_ubo = create_uniform_buffer(max_parabolas * sizeof(Parabola));

    upload_uniform_buffer(app.surfaces_ubo.get(), app.scene.surfaces);
    upload_uniform_buffer(app.volumes_ubo.get(), app.scene.volumes);
    upload_uniform_buffer(app.lines_ubo.get(), app.scene.lines);
    upload_uniform_buffer(app.arcs_ubo.get(), app.scene.arcs);
    upload_uniform_buffer(app.parabolas_ubo.get(), app.scene.parabolas);

    const auto bind_ubo = [program = app.trace_program.get()](
                              GLuint ubo, const char *name, GLuint binding)
    {
        const auto block_index = glGetUniformBlockIndex(program, name);
        glUniformBlockBinding(program, block_index, binding);
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubo);
    };
    bind_ubo(app.surfaces_ubo.get(), "Surfaces", 1);
    bind_ubo(app.volumes_ubo.get(), "Volumes", 2);
    bind_ubo(app.lines_ubo.get(), "Lines", 3);
    bind_ubo(app.arcs_ubo.get(), "Arcs", 4);
    bind_ubo(app.parabolas_ubo.get(), "Parabolas", 5);

    app.thickness = 0.005f;
    const auto viewport =
        centered_viewport(app.texture_width,
                          app.texture_height,
                          app.window_state.framebuffer_width,
                          app.window_state.framebuffer_height);
    push_scene_geometry(app.scene, app.thickness, app.raster_geometry);
    create_vertex_and_index_data(
        app.raster_geometry, app.scene.view_height, viewport.height);

    // TODO: we should probably organize the raster geometry better. We need to
    // clarify the distinction between updating the vertex buffer (when
    // zooming or moving objects) and re-creating the vertex and index buffers
    // with a new size (when adding or removing objects).
    std::tie(app.vao, app.vbo, app.ibo) =
        create_vertex_and_index_buffers(app.raster_geometry);

    std::tie(app.line_program, app.arc_program, app.parabola_program) =
        create_shapes_programs(glsl_version);
    app.loc_view_position_draw_line =
        glGetUniformLocation(app.line_program.get(), "view_position");
    app.loc_view_size_draw_line =
        glGetUniformLocation(app.line_program.get(), "view_size");
    app.loc_view_position_draw_arc =
        glGetUniformLocation(app.arc_program.get(), "view_position");
    app.loc_view_size_draw_arc =
        glGetUniformLocation(app.arc_program.get(), "view_size");
    app.loc_view_position_draw_parabola =
        glGetUniformLocation(app.parabola_program.get(), "view_position");
    app.loc_view_size_draw_parabola =
        glGetUniformLocation(app.parabola_program.get(), "view_size");

    app.samples_per_frame = 1;
    app.last_time = glfwGetTime();
    app.draw_geometry = true;
}

void make_scene_ui(Scene &scene,
                   bool &surfaces_changed,
                   bool &volumes_changed,
                   bool &lines_changed,
                   bool &arcs_changed,
                   bool &parabolas_changed)
{
    // FIXME

    constexpr auto edit_vec2 = [](const char *label, vec2 &v)
    { return ImGui::DragFloat2(label, &v.x, 0.01f); };

    constexpr auto edit_unit_vec2 = [](const char *label, vec2 &v)
    {
        auto old_v = v;
        if (ImGui::DragFloat2(label, &v.x, 0.01f, -1.0f, 1.0f))
        {
            if (v.x != old_v.x)
                v.y = std::copysign(std::sqrt(1.0f - v.x * v.x), v.y);
            else
                v.x = std::copysign(std::sqrt(1.0f - v.y * v.y), v.x);
            return true;
        }
        return false;
    };

    constexpr auto edit_vec3 = [](const char *label, vec3 &v)
    { return ImGui::DragFloat3(label, &v.x, 0.01f, 0.0f, 1000.0f); };

    constexpr auto edit_color = [](const char *label, vec3 &v)
    { return ImGui::ColorEdit3(label, &v.x); };

    constexpr auto edit_material_id = [](const char *label, std::uint32_t &id)
    { return ImGui::InputScalar(label, ImGuiDataType_U32, &id); };

    if (ImGui::TreeNode("Surfaces"))
    {
        for (std::size_t i = 0; i < scene.surfaces.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            Surface &m = scene.surfaces[i];

            if (ImGui::TreeNode("Surface"))
            {
                constexpr const char *items[] {
                    "Diffuse", "Specular", "Dielectric"};
                auto current_item = static_cast<int>(m.type);
                if (ImGui::Combo(
                        "Type", &current_item, items, std::size(items)))
                {
                    m.type = static_cast<Surface_type>(current_item);
                    surfaces_changed = true;
                }

                if (edit_color("Base color", m.base_color))
                    surfaces_changed = true;
                if (edit_color("Emissive color", m.emissive_color))
                    surfaces_changed = true;
                if (ImGui::DragFloat("Emissive strength",
                                     &m.emissive_strength,
                                     0.01f,
                                     0.0f,
                                     100.0f,
                                     "%.2f"))
                    surfaces_changed = true;
                if (ImGui::DragFloat(
                        "IOR", &m.ior_ratio, 0.01f, 1.0f, 3.0f, "%.2f"))
                    surfaces_changed = true;

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Volumes"))
    {
        for (std::size_t i = 0; i < scene.volumes.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            auto &v = scene.volumes[i];

            if (ImGui::TreeNode("Volume"))
            {
                if (edit_vec3("absorption", v.absorption))
                    volumes_changed = true;
                if (edit_vec3("scattering", v.scattering))
                    volumes_changed = true;
                if (ImGui::DragFloat("phase_anisotropy",
                                     &v.phase_anisotropy,
                                     0.01f,
                                     -1.0f,
                                     1.0f))
                    volumes_changed = true;

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Lines"))
    {
        for (std::size_t i = 0; i < scene.lines.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            Line &l = scene.lines[i];

            if (ImGui::TreeNode("Line"))
            {
                if (edit_vec2("a", l.vertex_a))
                    lines_changed = true;
                if (edit_vec2("b", l.vertex_b))
                    lines_changed = true;
                if (edit_material_id("surface_id", l.surface_id))
                    lines_changed = true;
                if (edit_material_id("volume_in_id", l.volume_in_id))
                    lines_changed = true;
                if (edit_material_id("volume_out_id", l.volume_out_id))
                    lines_changed = true;

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Arcs"))
    {
        for (std::size_t i = 0; i < scene.arcs.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            auto &a = scene.arcs[i];

            if (ImGui::TreeNode("Arc"))
            {
                if (edit_vec2("center", a.center))
                    arcs_changed = true;
                if (ImGui::DragFloat("radius", &a.radius, 0.01f, 0.0f, 1000.0f))
                    arcs_changed = true;
                if (edit_unit_vec2("clip normal", a.clip_normal))
                    arcs_changed = true;
                if (ImGui::DragFloat("clip offset", &a.clip_offset, 0.01f))
                    arcs_changed = true;
                if (edit_material_id("surface_id", a.surface_id))
                    arcs_changed = true;
                if (edit_material_id("volume_in_id", a.volume_in_id))
                    arcs_changed = true;
                if (edit_material_id("volume_out_id", a.volume_out_id))
                    arcs_changed = true;

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Parabolas"))
    {
        for (std::size_t i = 0; i < scene.parabolas.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            auto &p = scene.parabolas[i];

            if (ImGui::TreeNode("Parabola"))
            {
                if (edit_vec2("vertex", p.vertex))
                    parabolas_changed = true;
                if (edit_unit_vec2("axis", p.axis))
                    parabolas_changed = true;
                if (ImGui::DragFloat("focal", &p.focal, 0.01f, 0.0f, 1000.0f))
                    parabolas_changed = true;
                if (ImGui::DragFloat("clip x min", &p.clip_x_min, 0.01f))
                    parabolas_changed = true;
                if (ImGui::DragFloat("clip x max", &p.clip_x_max, 0.01f))
                    parabolas_changed = true;
                if (edit_material_id("surface_id", p.surface_id))
                    parabolas_changed = true;
                if (edit_material_id("volume_in_id", p.volume_in_id))
                    parabolas_changed = true;
                if (edit_material_id("volume_out_id", p.volume_out_id))
                    parabolas_changed = true;

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }
}

void main_loop_update(Application_state &app)
{
    // FIXME: this doesn't work with Emscripten (scroll_offset is always zero
    // after glfwPollEvents), because the callback is actually called
    // asynchronously and not necessarily when we call glfwPollEvents.
    // We should call glfwPollEvents as the very first thing in this function,
    // such that behaviour will appear identical whether event callbacks are
    // called synchronously or not. And note from GLFW's documentation:
    // "Do not assume that callbacks you set will _only_ be called in response
    // to event processing functions like this one [glfwPollEvents]"
    app.window_state.scroll_offset = 0.0f;
    glfwPollEvents();

    const auto viewport =
        centered_viewport(app.texture_width,
                          app.texture_height,
                          app.window_state.framebuffer_width,
                          app.window_state.framebuffer_height);

    double xpos {};
    double ypos {};
    glfwGetCursorPos(app.window.get(), &xpos, &ypos);
    auto mouse_world_x =
        screen_to_world(static_cast<float>(xpos) * app.window_state.scale_x,
                        viewport.x,
                        viewport.width,
                        app.scene.view_x,
                        app.scene.view_width);
    auto mouse_world_y =
        screen_to_world(static_cast<float>(ypos) * app.window_state.scale_y,
                        viewport.y + viewport.height,
                        -viewport.height,
                        app.scene.view_y,
                        app.scene.view_height);

    if (!ImGui::GetIO().WantCaptureMouse)
    {
        if (glfwGetMouseButton(app.window.get(), GLFW_MOUSE_BUTTON_LEFT) ==
            GLFW_PRESS)
        {
            if (!app.dragging)
            {
                app.dragging = true;
                app.drag_source_mouse_x = mouse_world_x;
                app.drag_source_mouse_y = mouse_world_y;
            }
            else
            {
                const auto drag_delta_x =
                    mouse_world_x - app.drag_source_mouse_x;
                const auto drag_delta_y =
                    mouse_world_y - app.drag_source_mouse_y;
                if (drag_delta_x != 0.0f || drag_delta_y != 0.0f)
                {
                    app.scene.view_x -= drag_delta_x;
                    app.scene.view_y -= drag_delta_y;
                    mouse_world_x = app.drag_source_mouse_x;
                    mouse_world_y = app.drag_source_mouse_y;
                    app.sample_index = 0;
                }
            }
        }
        else
        {
            app.dragging = false;
        }

        if (app.window_state.scroll_offset != 0.0f)
        {
            constexpr float zoom_factor {1.2f};
            const auto zoom = app.window_state.scroll_offset > 0.0f
                                  ? 1.0f / zoom_factor
                                  : zoom_factor;
            app.scene.view_x =
                mouse_world_x - (mouse_world_x - app.scene.view_x) * zoom;
            app.scene.view_y =
                mouse_world_y - (mouse_world_y - app.scene.view_y) * zoom;
            app.scene.view_width *= zoom;
            app.scene.view_height *= zoom;
            app.sample_index = 0;

            // NOTE: we update the geometry when scrolling because the
            // thickness is constant in view space and therefore changes in
            // world space
            clear_geometry(app.raster_geometry);
            push_scene_geometry(app.scene, app.thickness, app.raster_geometry);
            create_vertex_and_index_data(
                app.raster_geometry, app.scene.view_height, viewport.height);
            update_vertex_buffer(
                app.vao.get(), app.vbo.get(), app.raster_geometry);
        }
    }

    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        if (glfwGetKey(app.window.get(), GLFW_KEY_R) == GLFW_PRESS)
        {
            app.sample_index = 0;
        }

        if (const auto p_state = glfwGetKey(app.window.get(), GLFW_KEY_P);
            p_state == GLFW_PRESS && !app.p_pressed)
        {
            app.p_pressed = true;
            save_as_png("image.png",
                        app.texture_width,
                        app.texture_height,
                        app.fbo.get());
        }
        else if (p_state == GLFW_RELEASE)
        {
            app.p_pressed = false;
        }

        if (const auto s_state = glfwGetKey(app.window.get(), GLFW_KEY_S);
            s_state == GLFW_PRESS && !app.s_pressed)
        {
            app.s_pressed = true;
            std::cout << "Saving scene to \"scene.json\"\n";
            if (const auto result = save_scene(app.scene, "scene.json");
                !result.has_value())
            {
                std::cerr << result.error() << '\n';
            }
        }
        else if (s_state == GLFW_RELEASE)
        {
            app.s_pressed = false;
        }

        if (const auto l_state = glfwGetKey(app.window.get(), GLFW_KEY_L);
            l_state == GLFW_PRESS && !app.l_pressed)
        {
            app.l_pressed = true;
            std::cout << "Loading scene from \"scene.json\"\n";
            if (const auto new_scene = load_scene("scene.json");
                new_scene.has_value())
            {
                std::cerr << "Scene loading successful but not yet supported "
                             "as it requires recreating some resources\n";
            }
            else
            {
                std::cerr << new_scene.error() << '\n';
            }
        }
        else if (l_state == GLFW_RELEASE)
        {
            app.l_pressed = false;
        }
    }

    // FIXME
    if (app.window_state.framebuffer_size_changed)
    {
        app.window_state.framebuffer_size_changed = false;
        clear_geometry(app.raster_geometry);
        push_scene_geometry(app.scene, app.thickness, app.raster_geometry);
        create_vertex_and_index_data(
            app.raster_geometry, app.scene.view_height, viewport.height);
        update_vertex_buffer(app.vao.get(), app.vbo.get(), app.raster_geometry);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    bool do_post_process {false};
    bool surfaces_changed {};
    bool volumes_changed {};
    bool lines_changed {};
    bool arcs_changed {};
    bool parabolas_changed {};

    auto *const vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize({300.0f, vp->Size.y}, ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints({100.0f, vp->Size.y},
                                        {600.0f, vp->Size.y});
    constexpr ImGuiWindowFlags window_flags {ImGuiWindowFlags_NoMove};

    ImVec2 window_pos {};
    ImVec2 window_size {};
    if (ImGui::Begin("UI", nullptr, window_flags))
    {
        ImGui::Text("%.3f ms/frame (%.2f fps)",
                    static_cast<double>(1000.0f / ImGui::GetIO().Framerate),
                    static_cast<double>(ImGui::GetIO().Framerate));
        ImGui::Text("%u samples/frame", app.samples_per_frame);
        ImGui::Text("%u samples", app.sample_index);

        ImGui::SeparatorText("Render");

        // FIXME
        static int width {320};
        static int height {240};
        ImGui::SetNextItemWidth(60.0f);
        if (ImGui::InputInt("px##image_width", &width, 0, 0))
            width = std::clamp(width, 1, 4096);
        ImGui::SameLine();
        ImGui::TextUnformatted("x");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        if (ImGui::InputInt("px##image_height", &height, 0, 0))
            height = std::clamp(height, 1, 4096);
        ImGui::SameLine();
        ImGui::Button("Apply");

        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::InputScalar("Samples",
                               ImGuiDataType_U32,
                               &app.max_samples,
                               nullptr,
                               nullptr,
                               "%u"))
            app.max_samples = std::clamp(app.max_samples, 1u, 500'000u);

        ImGui::SeparatorText("View");

        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputFloat("X", &app.scene.view_x, 0.0f, 0.0f, "%.5g");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputFloat("Y", &app.scene.view_y, 0.0f, 0.0f, "%.5g");

        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputFloat("Width", &app.scene.view_width, 0.0f, 0.0f, "%.5g");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputFloat("Height", &app.scene.view_height, 0.0f, 0.0f, "%.5g");

        ImGui::SeparatorText("Post");

        ImGui::SetNextItemWidth(200.0f);
        do_post_process = ImGui::SliderFloat(
            "Exposure", &app.exposure_value, -10.0f, 10.0f, "%.1f");
        ImGui::Checkbox("Draw geometry", &app.draw_geometry);

        ImGui::SeparatorText("Scene");

        make_scene_ui(app.scene,
                      surfaces_changed,
                      volumes_changed,
                      lines_changed,
                      arcs_changed,
                      parabolas_changed);

        window_pos = ImGui::GetWindowPos();
        window_size = ImGui::GetWindowSize();
    }
    else
    {
        window_pos = ImVec2(0.0f, 0.0f);
        window_size = ImVec2(0.0f, 0.0f);
    }
    ImGui::End();

    // FIXME: we need to use this for our viewport bounds, but we need the
    // viewport in the input processing before the UI is drawn. So we need to
    // save the viewport for next frame.
    /*const auto viewport_x = window_pos.x + window_size.x;
    const auto viewport_y = vp->Pos.y;
    const auto viewport_width = vp->Size.x - viewport_x;
    const auto viewport_height = vp->Size.y;
    std::cout << std::format("Viewport ({}, {}) ({}, {})\n",
                             viewport_x,
                             viewport_y,
                             viewport_width,
                             viewport_height);*/

    ImGui::Render();

    if (surfaces_changed || volumes_changed || lines_changed || arcs_changed ||
        parabolas_changed)
    {
        app.sample_index = 0;
    }

#ifndef __EMSCRIPTEN__
    if (app.auto_workload)
    {
        glQueryCounter(app.query_start.get(), GL_TIMESTAMP);
    }
#endif

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const auto do_render = app.sample_index < app.max_samples;
    do_post_process = do_post_process || do_render;

    if (do_render)
    {
        if (surfaces_changed)
            upload_uniform_buffer(app.surfaces_ubo.get(), app.scene.surfaces);
        if (volumes_changed)
            upload_uniform_buffer(app.volumes_ubo.get(), app.scene.volumes);
        if (lines_changed)
            upload_uniform_buffer(app.lines_ubo.get(), app.scene.lines);
        if (arcs_changed)
            upload_uniform_buffer(app.arcs_ubo.get(), app.scene.arcs);
        if (parabolas_changed)
            upload_uniform_buffer(app.parabolas_ubo.get(), app.scene.parabolas);

        const auto samples_this_frame =
            std::min(app.samples_per_frame, app.max_samples - app.sample_index);
        glUseProgram(app.trace_program.get());

        glUniform1ui(app.loc_num_lines,
                     static_cast<GLuint>(app.scene.lines.size()));
        glUniform1ui(app.loc_num_arcs,
                     static_cast<GLuint>(app.scene.arcs.size()));
        glUniform1ui(app.loc_num_parabolas,
                     static_cast<GLuint>(app.scene.parabolas.size()));
        glUniform1i(app.loc_sample_index, static_cast<GLint>(app.sample_index));
        glUniform1i(app.loc_samples_per_frame,
                    static_cast<GLint>(samples_this_frame));
        glUniform2f(app.loc_view_position, app.scene.view_x, app.scene.view_y);
        glUniform2f(
            app.loc_view_size, app.scene.view_width, app.scene.view_height);

        glBindFramebuffer(GL_FRAMEBUFFER, app.float_fbo.get());
        glUniform2ui(app.loc_image_size,
                     static_cast<GLuint>(app.texture_width),
                     static_cast<GLuint>(app.texture_height));
        glBindVertexArray(app.empty_vao.get());

        // FIXME
        if (app.sample_index == 0)
        {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // new_average = alpha * sample_average + (1 - alpha) * old_average
        /*const auto alpha =
            static_cast<float>(samples_this_frame) /
            static_cast<float>(sample_index + samples_this_frame);
        glBlendColor(alpha, alpha, alpha, alpha);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);*/
        glBlendFunc(GL_ONE, GL_ONE);

        glViewport(0, 0, app.texture_width, app.texture_height);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        app.sample_index += samples_this_frame;
        app.sum_samples += samples_this_frame;
    }

    if (do_post_process)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, app.fbo.get());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app.accumulation_texture.get());
        glUseProgram(app.post_program.get());
        glUniform1f(app.loc_exposure_value, app.exposure_value);
        glBindVertexArray(app.empty_vao.get());
        glViewport(0, 0, app.texture_width, app.texture_height);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, app.fbo.get());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0,
                      0,
                      app.texture_width,
                      app.texture_height,
                      viewport.x,
                      viewport.y,
                      viewport.x + viewport.width,
                      viewport.y + viewport.height,
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (app.draw_geometry)
    {
        glBindVertexArray(app.vao.get());

        glUseProgram(app.line_program.get());
        glUniform2f(app.loc_view_position_draw_line,
                    app.scene.view_x,
                    app.scene.view_y);
        glUniform2f(app.loc_view_size_draw_line,
                    app.scene.view_width,
                    app.scene.view_height);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(app.raster_geometry.line_indices_size),
            GL_UNSIGNED_INT,
            reinterpret_cast<void *>(app.raster_geometry.line_indices_offset *
                                     sizeof(std::uint32_t)));

        glUseProgram(app.arc_program.get());
        glUniform2f(
            app.loc_view_position_draw_arc, app.scene.view_x, app.scene.view_y);
        glUniform2f(app.loc_view_size_draw_arc,
                    app.scene.view_width,
                    app.scene.view_height);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(app.raster_geometry.arc_indices_size),
            GL_UNSIGNED_INT,
            reinterpret_cast<void *>(app.raster_geometry.arc_indices_offset *
                                     sizeof(std::uint32_t)));

        glUseProgram(app.parabola_program.get());
        glUniform2f(app.loc_view_position_draw_parabola,
                    app.scene.view_x,
                    app.scene.view_y);
        glUniform2f(app.loc_view_size_draw_parabola,
                    app.scene.view_width,
                    app.scene.view_height);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(app.raster_geometry.parabola_indices_size),
            GL_UNSIGNED_INT,
            reinterpret_cast<void *>(
                app.raster_geometry.parabola_indices_offset *
                sizeof(std::uint32_t)));
    }

#ifndef __EMSCRIPTEN__
    if (app.auto_workload)
    {
        glQueryCounter(app.query_end.get(), GL_TIMESTAMP);
    }
#endif

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // FIXME: This changes CPU frame time measurement behaviour, since with
    // Emscripten vsync is done outside the main loop function. We should align
    // our native version to this behaviour.
#ifndef __EMSCRIPTEN__
    glfwSwapBuffers(app.window.get());
#endif

    ++app.num_frames;
    const double current_time {glfwGetTime()};
    if (const auto elapsed = current_time - app.last_time; elapsed >= 1.0)
    {
        std::cout << static_cast<double>(app.num_frames) / elapsed << " fps, "
                  << app.samples_per_frame << " samples/frame, "
                  << app.sum_samples << " samples/s, " << app.sample_index
                  << " samples\n";
        app.num_frames = 0;
        app.last_time = current_time;
        app.sum_samples = 0;
    }

#ifndef __EMSCRIPTEN__
    // NOTE: for some reason, GL_TIMESTAMP or GL_TIME_ELAPSED queries on
    // Intel with Mesa drivers return non-sense numbers, rendering them
    // useless. For this reason, we unfortunately cannot rely on GPU timing
    // at all. We could technically have a workaround for this specific
    // platform, but that also assumes that GL_RENDERER will always return a
    // string correctly identifying the driver (what happens on WebGL?).
    if (app.auto_workload && app.sample_index < app.max_samples)
    {
        GLuint64 start_time {};
        GLuint64 end_time {};
        glGetQueryObjectui64v(
            app.query_start.get(), GL_QUERY_RESULT, &start_time);
        glGetQueryObjectui64v(app.query_end.get(), GL_QUERY_RESULT, &end_time);
        const auto elapsed = static_cast<double>(end_time - start_time) / 1e9;
        constexpr double target_compute_per_frame {0.014};
        const auto samples_per_frame_f =
            static_cast<double>(app.samples_per_frame) *
            target_compute_per_frame / elapsed;
        app.samples_per_frame =
            static_cast<unsigned int>(std::max(samples_per_frame_f, 1.0));
    }
#endif
}

} // namespace

void run()
{
#ifdef __EMSCRIPTEN__

    // NOTE: The emscripten_set_main_loop(..., simulate_infinite_loop=true) call
    // unwinds the stack (but global or static storage duration objects are not
    // destroyed), before transferring control to the browser. For this reason,
    // any object passed to the main loop callback must have static storage
    // duration.

    static Application_state app {};
    init_application(app);

    ImGui::GetIO().IniFilename = nullptr;

    emscripten_set_main_loop_arg(
        [](void *arg)
        { main_loop_update(*static_cast<Application_state *>(arg)); },
        &app,
        0,
        true);

#else

    Application_state app {};
    init_application(app);

    while (!glfwWindowShouldClose(app.window.get()))
    {
        main_loop_update(app);
    }

#endif
}

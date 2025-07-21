#include "application.hpp"
#include "scene.hpp"
#include "vec.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
#include <vector>

namespace
{

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
    f(PFNGLBLENDFUNCPROC, glBlendFunc);                                        \
    f(PFNGLBLENDCOLORPROC, glBlendColor);                                      \
    f(PFNGLACTIVETEXTUREPROC, glActiveTexture);                                \
    f(PFNGLGETSTRINGPROC, glGetString);

#define ENUMERATE_GL_FUNCTIONS_430(f)                                          \
    f(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback);                  \
    f(PFNGLGETTEXIMAGEPROC, glGetTexImage);                                    \
    f(PFNGLDISPATCHCOMPUTEPROC, glDispatchCompute);                            \
    f(PFNGLMEMORYBARRIERPROC, glMemoryBarrier);                                \
    f(PFNGLBINDIMAGETEXTUREPROC, glBindImageTexture);                          \
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

PFNGLGETERRORPROC glGetError {nullptr};

#ifndef __EMSCRIPTEN__

// clang-format off
#define DECLARE_GL_FUNCTION(type, name) type name {nullptr}
// clang-format on

#else

void check_gl_error(
    const std::source_location &loc = std::source_location::current())
{
    auto error = glGetError();
    if (error == GL_NO_ERROR)
    {
        return;
    }

    std::cerr << loc.file_name() << ':' << loc.line() << ": ";
    const char *sep {""};

    do
    {
        std::cerr << sep;
        switch (error)
        {
        case GL_INVALID_ENUM: std::cerr << "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: std::cerr << "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: std::cerr << "GL_INVALID_OPERATION"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            std::cerr << "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY: std::cerr << "GL_OUT_OF_MEMORY"; break;
        default: std::cerr << "GL error " << error; break;
        }
        sep = ", ";
    } while ((error = glGetError()) != GL_NO_ERROR);

    std::cerr << "\n";
}

template <typename T>
struct GL_function;

template <typename R, typename... Args>
struct GL_function<R (*)(Args...)>
{
    R operator()(
        Args... args,
        const std::source_location &loc = std::source_location::current()) const
    {
        if constexpr (std::is_void_v<R>)
        {
            function(args...);
            check_gl_error(loc);
        }
        else
        {
            R result {function(args...)};
            check_gl_error(loc);
            return result;
        }
    }

    R (*function)(Args...);
};

// clang-format off
#define DECLARE_GL_FUNCTION(type, name) GL_function<type> name {nullptr}
// clang-format on

#endif

ENUMERATE_GL_FUNCTIONS(DECLARE_GL_FUNCTION)

template <std::invocable C, std::invocable<GLuint> D>
[[nodiscard]] auto create_object(C &&create, D &&destroy)
{
#ifdef __EMSCRIPTEN__
    return Unique_resource(create(), GL_deleter {destroy.function});
#else
    return Unique_resource(create(), GL_deleter {destroy});
#endif
}

template <std::invocable<GLenum> C, std::invocable<GLuint> D>
[[nodiscard]] auto create_object(C &&create, GLenum arg, D &&destroy)
{
#ifdef __EMSCRIPTEN__
    return Unique_resource(create(arg), GL_deleter {destroy.function});
#else
    return Unique_resource(create(arg), GL_deleter {destroy});
#endif
}

template <std::invocable<GLsizei, GLuint *> C,
          std::invocable<GLsizei, const GLuint *> D>
[[nodiscard]] auto create_object(C &&create, D &&destroy)
{
    GLuint object {};
    create(1, &object);
#ifdef __EMSCRIPTEN__
    return Unique_resource(object, GL_array_deleter {destroy.function});
#else
    return Unique_resource(object, GL_array_deleter {destroy});
#endif
}

struct Viewport
{
    int x;
    int y;
    int width;
    int height;
};

constexpr unsigned int max_ubo_size {16'384};

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
#ifdef __EMSCRIPTEN__
#define LOAD_GL_FUNCTION(type, name)                                           \
    name.function = reinterpret_cast<type>(glfwGetProcAddress(#name));         \
    assert(name.function != nullptr)
#else
#define LOAD_GL_FUNCTION(type, name)                                           \
    name = reinterpret_cast<type>(glfwGetProcAddress(#name));                  \
    assert(name != nullptr)
#endif

    ENUMERATE_GL_FUNCTIONS(LOAD_GL_FUNCTION)

    glGetError =
        reinterpret_cast<PFNGLGETERRORPROC>(glfwGetProcAddress("glGetError"));

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

[[nodiscard]] auto
create_shader(GLenum type, std::size_t size, const char *const code[])
{
    auto shader = create_object(glCreateShader, type, glDeleteShader);

    glShaderSource(shader.get(), static_cast<GLsizei>(size), code, nullptr);
    glCompileShader(shader.get());

    int success {};
    glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetShaderiv(shader.get(), GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetShaderInfoLog(shader.get(), buf_length, nullptr, message.data());
        std::ostringstream oss;
        oss << "Shader compilation failed:\n" << message << '\n';
        throw std::runtime_error(oss.str());
    }

    return shader;
}

[[nodiscard]] auto create_program(auto &&...shaders)
{
    auto program = create_object(glCreateProgram, glDeleteProgram);

    (glAttachShader(program.get(), shaders), ...);
    glLinkProgram(program.get());

    int success {};
    glGetProgramiv(program.get(), GL_LINK_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetProgramiv(program.get(), GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetProgramInfoLog(program.get(), buf_length, nullptr, message.data());
        std::ostringstream oss;
        oss << "Program linking failed:\n" << message << '\n';
        throw std::runtime_error(oss.str());
    }

    return program;
}

#ifndef __EMSCRIPTEN__
[[nodiscard]] auto create_trace_compute_program(const char *glsl_version,
                                                const Scene &scene)
{
    const auto shader_code = read_file("shaders/trace.glsl");
    std::ostringstream header;
    header << glsl_version << '\n'
           << "#define COMPUTE_SHADER\n"
           << "#define MATERIAL_COUNT " << scene.materials.size() << '\n'
           << "#define CIRCLE_COUNT " << scene.circles.size() << '\n'
           << "#define LINE_COUNT " << scene.lines.size() << '\n'
           << "#define ARC_COUNT " << scene.arcs.size() << '\n';
    const auto header_str = header.str();
    const char *const sources[] {header_str.c_str(), shader_code.c_str()};
    const auto shader =
        create_shader(GL_COMPUTE_SHADER, std::size(sources), sources);

    return create_program(shader.get());
}
#endif

[[nodiscard]] auto create_trace_graphics_program(const char *glsl_version,
                                                 const Scene &scene)
{
    const auto vertex_shader_code = read_file("shaders/fullscreen.vert");
    const char *const vertex_shader_sources[] {
        glsl_version, "\n", vertex_shader_code.c_str()};
    const auto vertex_shader = create_shader(GL_VERTEX_SHADER,
                                             std::size(vertex_shader_sources),
                                             vertex_shader_sources);

    const auto fragment_shader_code = read_file("shaders/trace.glsl");
    std::ostringstream header;
    header << glsl_version << '\n'
           << "#define MATERIAL_COUNT " << scene.materials.size() << '\n'
           << "#define CIRCLE_COUNT " << scene.circles.size() << '\n'
           << "#define LINE_COUNT " << scene.lines.size() << '\n'
           << "#define ARC_COUNT " << scene.arcs.size() << '\n';
    const auto header_str = header.str();
    const char *const fragment_shader_sources[] {header_str.c_str(),
                                                 fragment_shader_code.c_str()};
    const auto fragment_shader =
        create_shader(GL_FRAGMENT_SHADER,
                      std::size(fragment_shader_sources),
                      fragment_shader_sources);

    return create_program(vertex_shader.get(), fragment_shader.get());
}

#ifndef __EMSCRIPTEN__
[[nodiscard]] auto create_post_compute_program(const char *glsl_version)
{
    const auto shader_code = read_file("shaders/post.glsl");
    const char *const sources[] {
        glsl_version, "\n#define COMPUTE_SHADER\n", shader_code.c_str()};
    const auto shader =
        create_shader(GL_COMPUTE_SHADER, std::size(sources), sources);

    return create_program(shader.get());
}
#endif

[[nodiscard]] auto
create_graphics_program(const char *glsl_version,
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

    return create_program(vertex_shader.get(), fragment_shader.get());
}

[[nodiscard]] auto create_post_graphics_program(const char *glsl_version)
{
    return create_graphics_program(
        glsl_version, "shaders/fullscreen.vert", "shaders/post.glsl");
}

[[nodiscard]] auto create_accumulation_texture(GLsizei width, GLsizei height)
{
    auto texture = create_object(glGenTextures, glDeleteTextures);

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
    auto texture = create_object(glGenTextures, glDeleteTextures);

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
    auto fbo = create_object(glGenFramebuffers, glDeleteFramebuffers);

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

[[nodiscard]] auto create_vertex_index_buffers(const Raster_geometry &geometry)
{
    auto vao = create_object(glGenVertexArrays, glDeleteVertexArrays);
    glBindVertexArray(vao.get());

    auto vbo = create_object(glGenBuffers, glDeleteBuffers);
    glBindBuffer(GL_ARRAY_BUFFER, vbo.get());
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizei>(geometry.vertices.size() * sizeof(Vertex)),
        geometry.vertices.data(),
        GL_DYNAMIC_DRAW);

    auto ibo = create_object(glGenBuffers, glDeleteBuffers);
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
                          sizeof(Vertex::color) / sizeof(float),
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

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

template <typename T>
[[nodiscard]] auto create_uniform_buffer(const std::vector<T> &data)
{
    auto ubo = create_object(glGenBuffers, glDeleteBuffers);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo.get());

    const auto data_size = data.size() * sizeof(T);
    if (data_size > max_ubo_size)
    {
        std::ostringstream oss;
        oss << "Uniform buffer too big (" << data_size << "B > " << max_ubo_size
            << "B)";
        throw std::runtime_error(oss.str());
    }

    glBufferData(GL_UNIFORM_BUFFER,
                 static_cast<GLsizeiptr>(data_size),
                 data.data(),
                 GL_STATIC_DRAW);

    return ubo;
}

[[nodiscard]] constexpr unsigned int align_up(unsigned int value,
                                              unsigned int alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0);
    return (value + (alignment - 1)) & ~(alignment - 1);
}

#ifndef __EMSCRIPTEN__
void save_as_png(const char *file_name, int width, int height, GLuint texture)
{
    std::cout << "Saving " << width << " x " << height << " image to \""
              << file_name << "\"\n";

    constexpr int channels {4};
    const auto horizontal_size =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
    const auto vertical_size = static_cast<std::size_t>(height);

    std::vector<std::uint8_t> pixels(horizontal_size * vertical_size);

    glFinish();
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Flip the image vertically since the OpenGL image origin is in the
    // bottom-left corner.
    std::vector<std::uint8_t> tmp_row(horizontal_size);
    for (std::size_t i {0}; i < vertical_size / 2; ++i)
    {
        auto *const top = &pixels[i * horizontal_size];
        auto *const bottom = &pixels[(vertical_size - 1 - i) * horizontal_size];
        std::memcpy(tmp_row.data(), top, horizontal_size);
        std::memcpy(top, bottom, horizontal_size);
        std::memcpy(bottom, tmp_row.data(), horizontal_size);
    }

    const auto write_result = stbi_write_png(
        file_name, width, height, channels, pixels.data(), width * channels);
    if (write_result == 0)
    {
        std::ostringstream message;
        message << "Failed to write PNG image to \"" << file_name << '\"';
        throw std::runtime_error(message.str());
    }
}
#endif

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

void create_raster_geometry(const Scene &scene,
                            float thickness,
                            Raster_geometry &geometry)
{
    geometry.vertices.clear();
    geometry.indices.clear();

    thickness *= scene.view_height;

    geometry.circle_indices_offset = geometry.indices.size();
    for (const auto &circle : scene.circles)
    {
        const auto half_side = circle.radius + 0.5f * thickness;
        const auto bottom_left = circle.center + vec2 {-half_side, -half_side};
        const auto bottom_right = circle.center + vec2 {half_side, -half_side};
        const auto top_right = circle.center + vec2 {half_side, half_side};
        const auto top_left = circle.center + vec2 {-half_side, half_side};
        const auto rel_thickness = thickness / half_side;

        const auto color = scene.materials[circle.material_id].color;
        const auto first_index =
            static_cast<std::uint32_t>(geometry.vertices.size());
        geometry.vertices.push_back(
            {bottom_left, {-1.0, -1.0f, rel_thickness, 0.0f}, color});
        geometry.vertices.push_back(
            {bottom_right, {1.0f, -1.0f, rel_thickness, 0.0f}, color});
        geometry.vertices.push_back(
            {top_right, {1.0f, 1.0f, rel_thickness, 0.0f}, color});
        geometry.vertices.push_back(
            {top_left, {-1.0f, 1.0f, rel_thickness, 0.0f}, color});
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 1);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 3);
    }
    geometry.circle_indices_size =
        geometry.indices.size() - geometry.circle_indices_offset;

    geometry.line_indices_offset = geometry.indices.size();
    for (const auto &line : scene.lines)
    {
        const auto line_vec = line.b - line.a;
        const auto line_length = norm(line_vec);
        const auto line_dir = line_vec * (1.0f / line_length);
        const auto delta_left =
            vec2 {-line_dir.y, line_dir.x} * (thickness * 0.5f);
        const auto delta_up = line_dir * (thickness * 0.5f);
        const auto start_left = line.a + delta_left - delta_up;
        const auto start_right = line.a - delta_left - delta_up;
        const auto end_left = line.b + delta_left + delta_up;
        const auto end_right = line.b - delta_left + delta_up;
        const auto aspect_ratio = line_length / thickness;

        const auto color = scene.materials[line.material_id].color;
        const auto first_index =
            static_cast<std::uint32_t>(geometry.vertices.size());
        geometry.vertices.push_back(
            {start_left, {-0.5f, 0.5f, -aspect_ratio - 0.5f, 0.0f}, color});
        geometry.vertices.push_back(
            {start_right, {0.5f, 0.5f, -aspect_ratio - 0.5f, 0.0f}, color});
        geometry.vertices.push_back(
            {end_right, {0.5f, -aspect_ratio - 0.5f, 0.5f, 0.0f}, color});
        geometry.vertices.push_back(
            {end_left, {-0.5f, -aspect_ratio - 0.5f, 0.5f, 0.0f}, color});
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
    for (const auto &arc : scene.arcs)
    {
        const auto half_side = arc.radius + 0.5f * thickness;
        const auto bottom_y = arc.b - 0.5f * thickness;
        const auto dir = arc.a;
        const auto left = vec2 {-dir.y, dir.x};
        const auto bottom_left = arc.center + dir * bottom_y + left * half_side;
        const auto bottom_right =
            arc.center + dir * bottom_y - left * half_side;
        const auto top_right = arc.center + (dir - left) * half_side;
        const auto top_left = arc.center + (dir + left) * half_side;
        const auto rel_thickness = thickness / half_side;
        const auto cutoff = arc.b / half_side;
        const auto bottom_coord = bottom_y / half_side;

        const auto color = scene.materials[arc.material_id].color;
        const auto first_index =
            static_cast<std::uint32_t>(geometry.vertices.size());
        geometry.vertices.push_back(
            {bottom_left, {-1.0, bottom_coord, rel_thickness, cutoff}, color});
        geometry.vertices.push_back(
            {bottom_right, {1.0f, bottom_coord, rel_thickness, cutoff}, color});
        geometry.vertices.push_back(
            {top_right, {1.0f, 1.0f, rel_thickness, cutoff}, color});
        geometry.vertices.push_back(
            {top_left, {-1.0f, 1.0f, rel_thickness, cutoff}, color});
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 1);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 0);
        geometry.indices.push_back(first_index + 2);
        geometry.indices.push_back(first_index + 3);
    }
    geometry.arc_indices_size =
        geometry.indices.size() - geometry.arc_indices_offset;
}

} // namespace

void GLFW_deleter::operator()(bool)
{
    glfwTerminate();
}

void Window_deleter::operator()(struct GLFWwindow *window)
{
    glfwDestroyWindow(window);
}

void ImGui_deleter::operator()(bool)
{
    ImGui::DestroyContext();
}

void ImGui_glfw_deleter::operator()(bool)
{
    ImGui_ImplGlfw_Shutdown();
}

void ImGui_opengl_deleter::operator()(bool)
{
    ImGui_ImplOpenGL3_Shutdown();
}

void GL_deleter::operator()(GLuint handle)
{
    destroy(handle);
}

void GL_array_deleter::operator()(GLuint handle)
{
    destroy(1, &handle);
}

void Application::run()
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfw_context = decltype(glfw_context)(true);

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
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    window = decltype(window)(window_ptr);

    glfwMakeContextCurrent(window.get());

    glfwSetWindowUserPointer(window.get(), &window_state);

    glfwSetWindowContentScaleCallback(window.get(),
                                      &glfw_window_content_scale_callback);
    glfwSetFramebufferSizeCallback(window.get(),
                                   &glfw_framebuffer_size_callback);
    glfwSetScrollCallback(window.get(), &glfw_scroll_callback);

    glfwGetWindowContentScale(
        window.get(), &window_state.scale_x, &window_state.scale_y);
    glfwGetFramebufferSize(window.get(),
                           &window_state.framebuffer_width,
                           &window_state.framebuffer_height);

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
    auto_workload = false;
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
        auto_workload = true;
    }
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imgui_context = decltype(imgui_context)(true);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window.get(), true))
    {
        throw std::runtime_error("ImGui: failed to initialize GLFW backend");
    }
    imgui_glfw_context = decltype(imgui_glfw_context)(true);

#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window.get(), "#canvas");
#endif

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        throw std::runtime_error("ImGui: failed to initialize OpenGL backend");
    }
    imgui_opengl_context = decltype(imgui_opengl_context)(true);

    texture_width = 320;
    texture_height = 240;
    scene = create_scene(texture_width, texture_height);

    accumulation_texture =
        create_accumulation_texture(texture_width, texture_height);
#ifndef NO_COMPUTE_SHADER
    glBindImageTexture(0,
                       accumulation_texture.get(),
                       0,
                       GL_FALSE,
                       0,
                       GL_READ_WRITE,
                       GL_RGBA32F);
#endif

    target_texture = create_target_texture(texture_width, texture_height);
#ifndef NO_COMPUTE_SHADER
    glBindImageTexture(
        5, target_texture.get(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
#endif

#ifndef NO_COMPUTE_SHADER
    trace_program = create_trace_compute_program(glsl_version, scene);
#else
    trace_program = create_trace_graphics_program(glsl_version, scene);
    empty_vao = create_object(glGenVertexArrays, glDeleteVertexArrays);
    loc_image_size = glGetUniformLocation(trace_program.get(), "image_size");
#endif

    loc_sample_index =
        glGetUniformLocation(trace_program.get(), "sample_index");
    loc_samples_per_frame =
        glGetUniformLocation(trace_program.get(), "samples_per_frame");
    loc_view_position =
        glGetUniformLocation(trace_program.get(), "view_position");
    loc_view_size = glGetUniformLocation(trace_program.get(), "view_size");

#ifndef NO_COMPUTE_SHADER
    post_program = create_post_compute_program(glsl_version);
#else
    post_program = create_post_graphics_program(glsl_version);

    glUseProgram(post_program.get());
    glUniform1i(
        glGetUniformLocation(post_program.get(), "accumulation_texture"), 0);

    float_fbo = create_framebuffer(accumulation_texture.get());
#endif

    fbo = create_framebuffer(target_texture.get());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifndef __EMSCRIPTEN__
    query_start = create_object(glGenQueries, glDeleteQueries);
    query_end = create_object(glGenQueries, glDeleteQueries);
#endif

    materials_ubo = create_uniform_buffer(scene.materials);
    circles_ubo = create_uniform_buffer(scene.circles);
    lines_ubo = create_uniform_buffer(scene.lines);
    arcs_ubo = create_uniform_buffer(scene.arcs);

    const auto bind_ubo = [program = trace_program.get()](
                              GLuint ubo, const char *name, GLuint binding)
    {
        const auto block_index = glGetUniformBlockIndex(program, name);
        glUniformBlockBinding(program, block_index, binding);
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubo);
    };
    bind_ubo(materials_ubo.get(), "Materials", 1);
    bind_ubo(circles_ubo.get(), "Circles", 2);
    bind_ubo(lines_ubo.get(), "Lines", 3);
    bind_ubo(arcs_ubo.get(), "Arcs", 4);

    thickness = 0.0075f;
    create_raster_geometry(scene, thickness, raster_geometry);

    // TODO: we should probably organize the raster geometry better. We need to
    // clarify the distinction between updating the vertex buffer (when
    // zooming or moving objects) and re-creating the vertex and index buffers
    // with a new size (when adding or removing objects).
    std::tie(vao, vbo, ibo) = create_vertex_index_buffers(raster_geometry);

    circle_program = create_graphics_program(
        glsl_version, "shaders/shader.vert", "shaders/circle.frag");
    line_program = create_graphics_program(
        glsl_version, "shaders/shader.vert", "shaders/line.frag");
    arc_program = create_graphics_program(
        glsl_version, "shaders/shader.vert", "shaders/arc.frag");
    loc_view_position_draw_circle =
        glGetUniformLocation(circle_program.get(), "view_position");
    loc_view_size_draw_circle =
        glGetUniformLocation(circle_program.get(), "view_size");
    loc_view_position_draw_line =
        glGetUniformLocation(line_program.get(), "view_position");
    loc_view_size_draw_line =
        glGetUniformLocation(line_program.get(), "view_size");
    loc_view_position_draw_arc =
        glGetUniformLocation(arc_program.get(), "view_position");
    loc_view_size_draw_arc =
        glGetUniformLocation(arc_program.get(), "view_size");

    samples_per_frame = 1;
    last_time = glfwGetTime();
    draw_geometry = true;

#ifdef __EMSCRIPTEN__

    ImGui::GetIO().IniFilename = nullptr;

    emscripten_set_main_loop_arg(
        [](void *arg) { static_cast<Application *>(arg)->main_loop_update(); },
        this,
        0,
        true);

#else

    while (!glfwWindowShouldClose(window.get()))
    {
        main_loop_update();
    }

#endif
}

void Application::main_loop_update()
{
    constexpr unsigned int max_samples {200'000};

    window_state.scroll_offset = 0.0f;
    glfwPollEvents();

    const auto viewport = centered_viewport(texture_width,
                                            texture_height,
                                            window_state.framebuffer_width,
                                            window_state.framebuffer_height);

    double xpos {};
    double ypos {};
    glfwGetCursorPos(window.get(), &xpos, &ypos);
    auto mouse_world_x =
        screen_to_world(static_cast<float>(xpos) * window_state.scale_x,
                        viewport.x,
                        viewport.width,
                        scene.view_x,
                        scene.view_width);
    auto mouse_world_y =
        screen_to_world(static_cast<float>(ypos) * window_state.scale_y,
                        viewport.y + viewport.height,
                        -viewport.height,
                        scene.view_y,
                        scene.view_height);

    if (!ImGui::GetIO().WantCaptureMouse)
    {
        if (glfwGetMouseButton(window.get(), GLFW_MOUSE_BUTTON_LEFT) ==
            GLFW_PRESS)
        {
            if (!dragging)
            {
                dragging = true;
                drag_source_mouse_x = mouse_world_x;
                drag_source_mouse_y = mouse_world_y;
            }
            else
            {
                const auto drag_delta_x = mouse_world_x - drag_source_mouse_x;
                const auto drag_delta_y = mouse_world_y - drag_source_mouse_y;
                if (drag_delta_x != 0.0f || drag_delta_y != 0.0f)
                {
                    scene.view_x -= drag_delta_x;
                    scene.view_y -= drag_delta_y;
                    mouse_world_x = drag_source_mouse_x;
                    mouse_world_y = drag_source_mouse_y;
                    sample_index = 0;
                }
            }
        }
        else
        {
            dragging = false;
        }

        if (window_state.scroll_offset != 0.0f)
        {
            constexpr float zoom_factor {1.2f};
            const auto zoom = window_state.scroll_offset > 0.0f
                                  ? 1.0f / zoom_factor
                                  : zoom_factor;
            scene.view_x =
                mouse_world_x - (mouse_world_x - scene.view_x) * zoom;
            scene.view_y =
                mouse_world_y - (mouse_world_y - scene.view_y) * zoom;
            scene.view_width *= zoom;
            scene.view_height *= zoom;
            sample_index = 0;

            // NOTE: we update the geometry when scrolling because the
            // thickness is constant in view space and therefore changes in
            // world space
            create_raster_geometry(scene, thickness, raster_geometry);
            update_vertex_buffer(vao.get(), vbo.get(), raster_geometry);
        }
    }

    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        if (glfwGetKey(window.get(), GLFW_KEY_R) == GLFW_PRESS)
        {
            sample_index = 0;
        }

        if (const auto p_state = glfwGetKey(window.get(), GLFW_KEY_P);
            p_state == GLFW_PRESS && !p_pressed)
        {
            p_pressed = true;
// FIXME
#ifndef __EMSCRIPTEN__
            save_as_png("image.png",
                        texture_width,
                        texture_height,
                        target_texture.get());
#endif
        }
        else if (p_state == GLFW_RELEASE)
        {
            p_pressed = false;
        }

        if (const auto s_state = glfwGetKey(window.get(), GLFW_KEY_S);
            s_state == GLFW_PRESS && !s_pressed)
        {
            s_pressed = true;
            save_scene(scene, "scene.json");
        }
        else if (s_state == GLFW_RELEASE)
        {
            s_pressed = false;
        }

        if (const auto l_state = glfwGetKey(window.get(), GLFW_KEY_L);
            l_state == GLFW_PRESS && !l_pressed)
        {
            l_pressed = true;
            const auto new_scene = load_scene("scene.json");
            std::cerr << "Scene loading is not yet supported as it requires "
                         "recreating some resources\n";
        }
        else if (l_state == GLFW_RELEASE)
        {
            l_pressed = false;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow();

    if (ImGui::Begin("UI"))
    {
        ImGui::Text("%.3f ms/frame (%.2f fps)",
                    static_cast<double>(1000.0f / ImGui::GetIO().Framerate),
                    static_cast<double>(ImGui::GetIO().Framerate));
        ImGui::Text("%u samples", sample_index);

        ImGui::Checkbox("Draw geometry", &draw_geometry);
        ImGui::End();
    }

    ImGui::Render();

#ifndef __EMSCRIPTEN__
    if (auto_workload)
    {
        glQueryCounter(query_start.get(), GL_TIMESTAMP);
    }
#endif

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (sample_index < max_samples)
    {
        const auto samples_this_frame =
            std::min(samples_per_frame, max_samples - sample_index);
        glUseProgram(trace_program.get());
        glUniform1i(loc_sample_index, static_cast<int>(sample_index));
        glUniform1i(loc_samples_per_frame,
                    static_cast<int>(samples_this_frame));
        glUniform2f(loc_view_position, scene.view_x, scene.view_y);
        glUniform2f(loc_view_size, scene.view_width, scene.view_height);

#ifndef NO_COMPUTE_SHADER
        unsigned int num_groups_x {
            align_up(static_cast<unsigned int>(texture_width), 16) / 16,
        };
        unsigned int num_groups_y {
            align_up(static_cast<unsigned int>(texture_height), 16) / 16,
        };

        glDispatchCompute(num_groups_x, num_groups_y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(post_program.get());
        glDispatchCompute(num_groups_x, num_groups_y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
#else
        glBindFramebuffer(GL_FRAMEBUFFER, float_fbo.get());
        glUniform2ui(loc_image_size,
                     static_cast<unsigned int>(texture_width),
                     static_cast<unsigned int>(texture_height));
        glBindVertexArray(empty_vao.get());

        // new_average = alpha * sample_average + (1 - alpha) * old_average
        const auto alpha = static_cast<float>(samples_this_frame) /
                           static_cast<float>(sample_index + samples_per_frame);
        glBlendColor(alpha, alpha, alpha, alpha);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);

        glViewport(0, 0, texture_width, texture_height);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, accumulation_texture.get());
        glUseProgram(post_program.get());

        glDrawArrays(GL_TRIANGLES, 0, 3);
#endif

        sample_index += samples_this_frame;
        sum_samples += samples_this_frame;
    }

    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.get());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0,
                      0,
                      texture_width,
                      texture_height,
                      viewport.x,
                      viewport.y,
                      viewport.x + viewport.width,
                      viewport.y + viewport.height,
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (draw_geometry)
    {
        glBindVertexArray(vao.get());

        glUseProgram(circle_program.get());
        glUniform2f(loc_view_position_draw_circle, scene.view_x, scene.view_y);
        glUniform2f(
            loc_view_size_draw_circle, scene.view_width, scene.view_height);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(raster_geometry.circle_indices_size),
            GL_UNSIGNED_INT,
            reinterpret_cast<void *>(raster_geometry.circle_indices_offset *
                                     sizeof(std::uint32_t)));

        glUseProgram(line_program.get());
        glUniform2f(loc_view_position_draw_line, scene.view_x, scene.view_y);
        glUniform2f(
            loc_view_size_draw_line, scene.view_width, scene.view_height);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(raster_geometry.line_indices_size),
            GL_UNSIGNED_INT,
            reinterpret_cast<void *>(raster_geometry.line_indices_offset *
                                     sizeof(std::uint32_t)));

        glUseProgram(arc_program.get());
        glUniform2f(loc_view_position_draw_arc, scene.view_x, scene.view_y);
        glUniform2f(
            loc_view_size_draw_arc, scene.view_width, scene.view_height);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(raster_geometry.arc_indices_size),
            GL_UNSIGNED_INT,
            reinterpret_cast<void *>(raster_geometry.arc_indices_offset *
                                     sizeof(std::uint32_t)));
    }

#ifndef __EMSCRIPTEN__
    if (auto_workload)
    {
        glQueryCounter(query_end.get(), GL_TIMESTAMP);
    }
#endif

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window.get());

    ++num_frames;
    const double current_time {glfwGetTime()};
    if (const auto elapsed = current_time - last_time; elapsed >= 1.0)
    {
        std::cout << static_cast<double>(num_frames) / elapsed << " fps, "
                  << samples_per_frame << " samples/frame, " << sum_samples
                  << " samples/s, " << sample_index << " samples\n";
        num_frames = 0;
        last_time = current_time;
        sum_samples = 0;
    }

#ifndef __EMSCRIPTEN__
    // NOTE: for some reason, GL_TIMESTAMP or GL_TIME_ELAPSED queries on
    // Intel with Mesa drivers return non-sense numbers, rendering them
    // useless. For this reason, we unfortunately cannot rely on GPU timing
    // at all. We could technically have a workaround for this specific
    // platform, but that also assumes that GL_RENDERER will always return a
    // string correctly identifying the driver (what happens on WebGL?).
    if (auto_workload && sample_index < max_samples)
    {
        GLuint64 start_time {};
        GLuint64 end_time {};
        glGetQueryObjectui64v(query_start.get(), GL_QUERY_RESULT, &start_time);
        glGetQueryObjectui64v(query_end.get(), GL_QUERY_RESULT, &end_time);
        const auto elapsed = static_cast<double>(end_time - start_time) / 1e9;
        constexpr double target_compute_per_frame {0.014};
        const auto samples_per_frame_f =
            static_cast<double>(samples_per_frame) * target_compute_per_frame /
            elapsed;
        samples_per_frame =
            static_cast<unsigned int>(std::max(samples_per_frame_f, 1.0));
    }
#endif
}
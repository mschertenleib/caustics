#include "vec2.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define GLFW_INCLUDE_GLCOREARB
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace
{

#define ENUMERATE_GL_FUNCTIONS(f)                                              \
    f(PFNGLENABLEPROC, glEnable);                                              \
    f(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback);                  \
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
    f(PFNGLUNIFORM1UIPROC, glUniform1ui);                                      \
    f(PFNGLUNIFORM2UIPROC, glUniform2ui);                                      \
    f(PFNGLUNIFORM2FPROC, glUniform2f);                                        \
    f(PFNGLGENTEXTURESPROC, glGenTextures);                                    \
    f(PFNGLDELETETEXTURESPROC, glDeleteTextures);                              \
    f(PFNGLBINDTEXTUREPROC, glBindTexture);                                    \
    f(PFNGLTEXPARAMETERIPROC, glTexParameteri);                                \
    f(PFNGLTEXIMAGE2DPROC, glTexImage2D);                                      \
    f(PFNGLBINDIMAGETEXTUREPROC, glBindImageTexture);                          \
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
    f(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData);                          \
    f(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);                            \
    f(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);                      \
    f(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);                            \
    f(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);                    \
    f(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);            \
    f(PFNGLDRAWELEMENTSPROC, glDrawElements);                                  \
    f(PFNGLDRAWARRAYSPROC, glDrawArrays);                                      \
    f(PFNGLUSEPROGRAMPROC, glUseProgram);                                      \
    f(PFNGLDISPATCHCOMPUTEPROC, glDispatchCompute);                            \
    f(PFNGLVIEWPORTPROC, glViewport);                                          \
    f(PFNGLCLEARCOLORPROC, glClearColor);                                      \
    f(PFNGLCLEARPROC, glClear);                                                \
    f(PFNGLMEMORYBARRIERPROC, glMemoryBarrier);                                \
    f(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer);                            \
    f(PFNGLGENQUERIESPROC, glGenQueries);                                      \
    f(PFNGLDELETEQUERIESPROC, glDeleteQueries);                                \
    f(PFNGLQUERYCOUNTERPROC, glQueryCounter);                                  \
    f(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v);                    \
    f(PFNGLGETTEXIMAGEPROC, glGetTexImage);                                    \
    f(PFNGLGETINTEGERVPROC, glGetIntegerv);                                    \
    f(PFNGLFINISHPROC, glFinish);                                              \
    f(PFNGLFLUSHPROC, glFlush);                                                \
    f(PFNGLBLENDFUNCPROC, glBlendFunc);                                        \
    f(PFNGLBLENDCOLORPROC, glBlendColor);                                      \
    f(PFNGLGETSTRINGPROC, glGetString);

// clang-format off
#define DECLARE_GL_FUNCTION(type, name) type name {nullptr}
// clang-format on

ENUMERATE_GL_FUNCTIONS(DECLARE_GL_FUNCTION)

template <typename Destroy>
class GL_object
{
public:
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

[[nodiscard]] auto create_object(GLuint (*create)(), void (*destroy)(GLuint))
{
    assert(create);
    assert(destroy);

    return GL_object(create(), destroy);
}

[[nodiscard]] auto
create_object(GLuint (*create)(GLenum), GLenum arg, void (*destroy)(GLuint))
{
    assert(create);
    assert(destroy);

    return GL_object(create(arg), destroy);
}

[[nodiscard]] auto create_object(void (*create)(GLsizei, GLuint *),
                                 void (*destroy)(GLsizei, const GLuint *))
{
    assert(create);
    assert(destroy);

    GLuint object {};
    create(1, &object);
    return GL_object(object, [destroy](GLuint id) { destroy(1, &id); });
}

struct vec3
{
    float x;
    float y;
    float z;
};

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

struct Viewport
{
    int x;
    int y;
    int width;
    int height;
};

struct Vertex
{
    vec2 position;
    vec3 uvw;
    vec3 color;
};

struct Window_state
{
    float scale_x;
    float scale_y;
    int framebuffer_width;
    int framebuffer_height;
    float scroll_offset;
};

constexpr unsigned int max_ubo_size {16'384};
constexpr unsigned int max_material_count {max_ubo_size / sizeof(Material)};
constexpr unsigned int max_circle_count {max_ubo_size / sizeof(Circle)};
constexpr unsigned int max_line_count {max_ubo_size / sizeof(Line)};
constexpr unsigned int max_arc_count {max_ubo_size / sizeof(Arc)};

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
#define LOAD_GL_FUNCTION(type, name)                                           \
    name = reinterpret_cast<type>(glfwGetProcAddress(#name));                  \
    assert(name != nullptr)

    ENUMERATE_GL_FUNCTIONS(LOAD_GL_FUNCTION)

#undef LOAD_GL_FUNCTION
}

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

[[nodiscard]] auto create_shader(GLenum type, const char *code)
{
    return create_shader(type, 1, &code);
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

[[nodiscard]] auto create_trace_compute_program(const Scene &scene)
{
    const auto shader_code = read_file("trace.glsl");
    std::ostringstream header;
    header << "#version 430\n"
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

[[nodiscard]] auto create_post_compute_program()
{
    const auto shader_code = read_file("post.comp");
    const auto shader = create_shader(GL_COMPUTE_SHADER, shader_code.c_str());

    return create_program(shader.get());
}

[[nodiscard]] auto create_trace_graphics_program(const Scene &scene)
{
    const auto vertex_shader_code = read_file("fullscreen.vert");
    const auto vertex_shader =
        create_shader(GL_VERTEX_SHADER, vertex_shader_code.c_str());

    const auto fragment_shader_code = read_file("trace.glsl");
    std::ostringstream header;
    header << "#version 430\n"
           << "#define MATERIAL_COUNT " << scene.materials.size() << '\n'
           << "#define CIRCLE_COUNT " << scene.circles.size() << '\n'
           << "#define LINE_COUNT " << scene.lines.size() << '\n'
           << "#define ARC_COUNT " << scene.arcs.size() << '\n';
    const auto header_str = header.str();
    const char *const sources[] {header_str.c_str(),
                                 fragment_shader_code.c_str()};
    const auto fragment_shader =
        create_shader(GL_FRAGMENT_SHADER, std::size(sources), sources);

    return create_program(vertex_shader.get(), fragment_shader.get());
}

[[nodiscard]] auto create_graphics_program()
{
    const auto vertex_shader_code = read_file("shader.vert");
    const auto vertex_shader =
        create_shader(GL_VERTEX_SHADER, vertex_shader_code.c_str());

    const auto fragment_shader_code = read_file("line.frag");
    const auto fragment_shader =
        create_shader(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());

    return create_program(vertex_shader.get(), fragment_shader.get());
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
    glBindImageTexture(
        0, texture.get(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

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
    glBindImageTexture(
        5, texture.get(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

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
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            oss << "GL_FRAMEBUFFER_UNSUPPORTED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            oss << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
            break;
        default:
            oss << "0x" << std::setfill('0') << std::setw(4) << std::hex
                << status;
        }
        oss << '\n';
        throw std::runtime_error(oss.str());
    }

    return fbo;
}

[[nodiscard]] auto
create_vertex_index_buffers(const std::vector<Vertex> &vertices,
                            const std::vector<std::uint32_t> &indices)
{
    auto vao = create_object(glGenVertexArrays, glDeleteVertexArrays);
    glBindVertexArray(vao.get());

    auto vbo = create_object(glGenBuffers, glDeleteBuffers);
    glBindBuffer(GL_ARRAY_BUFFER, vbo.get());
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizei>(vertices.size() * sizeof(Vertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    auto ibo = create_object(glGenBuffers, glDeleteBuffers);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizei>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, position)));
    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, uvw)));
    glVertexAttribPointer(2,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    return std::tuple {std::move(vao), std::move(vbo), std::move(ibo)};
}

template <typename T>
[[nodiscard]] auto create_uniform_buffer(GLuint binding,
                                         const std::vector<T> &data)
{
    auto ubo = create_object(glGenBuffers, glDeleteBuffers);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo.get());

    const auto data_size = data.size() * sizeof(T);
    if (data_size > max_ubo_size)
    {
        std::ostringstream oss;
        oss << "Uniform buffer too big (" << data_size << " > " << max_ubo_size
            << ')';
        throw std::runtime_error(oss.str());
    }

    glBufferData(GL_UNIFORM_BUFFER,
                 static_cast<GLsizeiptr>(data_size),
                 data.data(),
                 GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubo.get());

    return ubo;
}

[[nodiscard]] constexpr unsigned int align_up(unsigned int value,
                                              unsigned int alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0);
    return (value + (alignment - 1)) & ~(alignment - 1);
}

void save_as_png(const char *file_name, int width, int height)
{
    std::cout << "Saving " << width << " x " << height << " image to \""
              << file_name << "\"... " << std::flush;

    glFinish();

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) *
                                     static_cast<std::size_t>(height) * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    const auto write_result =
        stbi_write_png(file_name, width, height, 4, pixels.data(), width * 4);
    if (write_result == 0)
    {
        std::ostringstream message;
        message << "Failed to write PNG image to \"" << file_name << '\"';
        throw std::runtime_error(message.str());
    }

    std::cout << "Saved\n" << std::flush;
}

[[nodiscard]] constexpr float screen_to_world(double x,
                                              int screen_min,
                                              int screen_size,
                                              float world_center,
                                              float world_size) noexcept
{
    const auto u = (static_cast<float>(x) - static_cast<float>(screen_min)) /
                   static_cast<float>(screen_size);
    return world_center + (u - 0.5f) * world_size;
}

[[nodiscard]] auto create_vertices_indices(const Scene &scene)
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    const float thickness {0.005f};

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
        const auto first_index = static_cast<std::uint32_t>(vertices.size());
        vertices.push_back(
            {start_left, {-0.5f, 0.5f, -aspect_ratio - 0.5f}, color});
        vertices.push_back(
            {start_right, {0.5f, 0.5f, -aspect_ratio - 0.5f}, color});
        vertices.push_back(
            {end_right, {0.5f, -aspect_ratio - 0.5f, 0.5f}, color});
        vertices.push_back(
            {end_left, {-0.5f, -aspect_ratio - 0.5f, 0.5f}, color});
        indices.push_back(first_index + 0);
        indices.push_back(first_index + 1);
        indices.push_back(first_index + 2);
        indices.push_back(first_index + 0);
        indices.push_back(first_index + 2);
        indices.push_back(first_index + 3);
    }

    return std::pair {vertices, indices};
}

[[nodiscard]] Scene create_scene(int texture_width, int texture_height)
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

void run()
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, 1);

    auto window_ptr = glfwCreateWindow(1280, 720, "Caustics", nullptr, nullptr);
    if (window_ptr == nullptr)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    const auto destroy_window = [](GLFWwindow *window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    };
    const std::unique_ptr<GLFWwindow, decltype(destroy_window)> window(
        window_ptr, destroy_window);

    glfwMakeContextCurrent(window.get());

    Window_state window_state {};
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

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&gl_debug_callback, nullptr);

    const std::string_view renderer(
        reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    std::cout << "Renderer: \"" << renderer << "\", ";
    bool auto_workload {};
    if (renderer.find("Mesa") != std::string_view::npos &&
        renderer.find("Intel") != std::string_view::npos)
    {
        std::cout << "disabling adaptive workload\n";
        auto_workload = false;
        // FIXME: this is just to get some decent performance, but we should
        // really find a way to activate V-Sync even when timer queries do not
        // work.
        glfwSwapInterval(0);
    }
    else
    {
        std::cout << "enabling adaptive workload\n";
        auto_workload = true;
    }

    int texture_width {320};
    int texture_height {240};
    auto scene = create_scene(texture_width, texture_height);

#define COMPUTE_SHADER

#ifdef COMPUTE_SHADER
    const auto trace_program = create_trace_compute_program(scene);
#else
    const auto trace_program = create_trace_graphics_program(scene);
    const auto empty_vao =
        create_object(glGenVertexArrays, glDeleteVertexArrays);
#endif

    const auto loc_sample_index =
        glGetUniformLocation(trace_program.get(), "sample_index");
    const auto loc_samples_per_frame =
        glGetUniformLocation(trace_program.get(), "samples_per_frame");
    const auto loc_view_position =
        glGetUniformLocation(trace_program.get(), "view_position");
    const auto loc_view_size =
        glGetUniformLocation(trace_program.get(), "view_size");

#ifndef COMPUTE_SHADER
    const auto loc_image_size =
        glGetUniformLocation(trace_program.get(), "image_size");
#endif

    const auto post_program = create_post_compute_program();

    const auto accumulation_texture =
        create_accumulation_texture(texture_width, texture_height);

    const auto target_texture =
        create_target_texture(texture_width, texture_height);

#ifndef COMPUTE_SHADER
    const auto float_fbo = create_framebuffer(accumulation_texture.get());
#endif
    const auto fbo = create_framebuffer(target_texture.get());

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto query_start = create_object(glGenQueries, glDeleteQueries);
    const auto query_end = create_object(glGenQueries, glDeleteQueries);

    const auto materials_ubo = create_uniform_buffer(1, scene.materials);
    const auto circles_ubo = create_uniform_buffer(2, scene.circles);
    const auto lines_ubo = create_uniform_buffer(3, scene.lines);
    const auto arcs_ubo = create_uniform_buffer(4, scene.arcs);

    const auto [vertices, indices] = create_vertices_indices(scene);
    const auto [vao, vbo, ibo] = create_vertex_index_buffers(vertices, indices);
    const auto draw_program = create_graphics_program();
    const auto loc_view_position_draw =
        glGetUniformLocation(draw_program.get(), "view_position");
    const auto loc_view_size_draw =
        glGetUniformLocation(draw_program.get(), "view_size");

    constexpr unsigned int max_samples {200'000};
    unsigned int sample_index {0};
    unsigned int samples_per_frame {1};

    double last_time {glfwGetTime()};
    unsigned int sum_samples {0};
    int num_frames {0};

    bool s_pressed {false};
    bool dragging {false};
    float drag_source_mouse_x {};
    float drag_source_mouse_y {};

    while (!glfwWindowShouldClose(window.get()))
    {
        window_state.scroll_offset = 0.0f;
        glfwPollEvents();

        const auto viewport =
            centered_viewport(texture_width,
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
        }

        if (glfwGetKey(window.get(), GLFW_KEY_R) == GLFW_PRESS)
        {
            sample_index = 0;
        }

        if (const auto s_state = glfwGetKey(window.get(), GLFW_KEY_S);
            s_state == GLFW_PRESS && !s_pressed)
        {
            s_pressed = true;
            save_as_png("out.png", texture_width, texture_height);
        }
        else if (s_state == GLFW_RELEASE)
        {
            s_pressed = false;
        }

        if (auto_workload)
        {
            glQueryCounter(query_start.get(), GL_TIMESTAMP);
        }

        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (sample_index < max_samples)
        {
            const auto samples_this_frame =
                std::min(samples_per_frame, max_samples - sample_index);
            glUseProgram(trace_program.get());
            glUniform1ui(loc_sample_index, sample_index);
            glUniform1ui(loc_samples_per_frame, samples_this_frame);
            glUniform2f(loc_view_position, scene.view_x, scene.view_y);
            glUniform2f(loc_view_size, scene.view_width, scene.view_height);
            unsigned int num_groups_x {
                align_up(static_cast<unsigned int>(texture_width), 16) / 16,
            };
            unsigned int num_groups_y {
                align_up(static_cast<unsigned int>(texture_height), 16) / 16,
            };

#ifdef COMPUTE_SHADER
            glDispatchCompute(num_groups_x, num_groups_y, 1);
#else
            glBindFramebuffer(GL_FRAMEBUFFER, float_fbo.get());
            glUniform2ui(loc_image_size,
                         static_cast<unsigned int>(texture_width),
                         static_cast<unsigned int>(texture_height));
            glBindVertexArray(empty_vao.get());

            // new_average = alpha * sample_average + (1 - alpha) * old_average
            const auto alpha =
                static_cast<float>(samples_this_frame) /
                static_cast<float>(sample_index + samples_per_frame);
            glBlendColor(alpha, alpha, alpha, alpha);
            glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);

            glViewport(0, 0, texture_width, texture_height);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.get());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

            sample_index += samples_this_frame;
            sum_samples += samples_this_frame;

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glUseProgram(post_program.get());
            glDispatchCompute(num_groups_x, num_groups_y, 1);

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

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

        glUseProgram(draw_program.get());
        glUniform2f(loc_view_position_draw, scene.view_x, scene.view_y);
        glUniform2f(loc_view_size_draw, scene.view_width, scene.view_height);
#ifndef COMPUTE_SHADER
        glBindVertexArray(vao.get());
#endif
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT,
                       nullptr);

        if (auto_workload)
        {
            glQueryCounter(query_end.get(), GL_TIMESTAMP);
        }

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
            glGetQueryObjectui64v(
                query_start.get(), GL_QUERY_RESULT, &start_time);
            glGetQueryObjectui64v(query_end.get(), GL_QUERY_RESULT, &end_time);
            const auto elapsed =
                static_cast<double>(end_time - start_time) / 1e9;
            constexpr double target_compute_per_frame {0.014};
            const auto samples_per_frame_f =
                static_cast<double>(samples_per_frame) *
                target_compute_per_frame / elapsed;
            samples_per_frame =
                static_cast<unsigned int>(std::max(samples_per_frame_f, 1.0));
        }
    }
}

} // namespace

int main()
{
    try
    {
        run();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cout.flush();
        std::cerr << "Exception thrown: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cout.flush();
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }
}

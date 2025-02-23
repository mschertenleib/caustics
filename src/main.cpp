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
    f(PFNGLGETSTRINGPROC, glGetString);

// clang-format off
#define DECLARE_GL_FUNCTION(type, name) type name {nullptr}
// clang-format on

ENUMERATE_GL_FUNCTIONS(DECLARE_GL_FUNCTION)

template <typename F>
class Scope_exit
{
public:
    [[nodiscard]] explicit Scope_exit(F &&f) noexcept : m_f(std::move(f))
    {
    }

    [[nodiscard]] explicit Scope_exit(F &f) noexcept : m_f(f)
    {
    }

    ~Scope_exit() noexcept
    {
        m_f();
    }

    Scope_exit(const Scope_exit &) = delete;
    Scope_exit(Scope_exit &&) = delete;
    Scope_exit &operator=(const Scope_exit &) = delete;
    Scope_exit &operator=(Scope_exit &&) = delete;

private:
    F m_f;
};

template <typename F>
class Scope_fail
{
public:
    [[nodiscard]] explicit Scope_fail(F &&f) noexcept
        : m_exception_count(std::uncaught_exceptions()), m_f(std::move(f))
    {
    }

    [[nodiscard]] explicit Scope_fail(F &f) noexcept
        : m_exception_count(std::uncaught_exceptions()), m_f(f)
    {
    }

    ~Scope_fail() noexcept
    {
        if (std::uncaught_exceptions() > m_exception_count)
        {
            m_f();
        }
    }

    Scope_fail(const Scope_fail &) = delete;
    Scope_fail(Scope_fail &&) = delete;
    Scope_fail &operator=(const Scope_fail &) = delete;
    Scope_fail &operator=(Scope_fail &&) = delete;

private:
    int m_exception_count;
    F m_f;
};

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2)      CONCATENATE_IMPL(s1, s2)
#define SCOPE_EXIT(f)            const Scope_exit CONCATENATE(scope_exit_, __LINE__)(f)
#define SCOPE_FAIL(f)            const Scope_fail CONCATENATE(scope_fail_, __LINE__)(f)

struct Rectangle
{
    int x0;
    int y0;
    int x1;
    int y1;
};

struct vec2
{
    float x;
    float y;
};

struct vec3
{
    float x;
    float y;
    float z;
};

enum struct Material_type
{
    diffuse,
    specular,
    dielectric
};

struct Material
{
    alignas(16) vec3 color;
    alignas(16) vec3 emissivity;
    alignas(4) Material_type type;
};

struct Circle
{
    alignas(16) vec2 center;
    float radius;
    unsigned int material_id;
};

struct Line
{
    alignas(16) vec2 a;
    alignas(8) vec2 b;
    unsigned int material_id;
};

struct Arc
{
    alignas(16) vec2 center;
    float radius;
    alignas(8) vec2 a;
    float b;
    unsigned int material_id;
};

struct Scene
{
    std::vector<Material> materials;
    std::vector<Circle> circles;
    std::vector<Line> lines;
    std::vector<Arc> arcs;
};

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

double scroll_offset {}; // FIXME: this shouldn't be global, but this will be
                         // fixed when we move everything to callbacks
void glfw_scroll_callback([[maybe_unused]] GLFWwindow *window,
                          [[maybe_unused]] double xoffset,
                          double yoffset)
{
    scroll_offset = yoffset;
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

[[nodiscard]] constexpr Rectangle centered_rectangle(int src_width,
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
        const int x0 {0};
        const int y0 {(dst_height - height) / 2};
        const int x1 {dst_width};
        const int y1 {y0 + height};
        return {x0, y0, x1, y1};
    }
    else
    {
        const auto width =
            static_cast<int>(static_cast<float>(dst_height) * src_aspect_ratio);
        const int x0 {(dst_width - width) / 2};
        const int y0 {0};
        const int x1 {x0 + width};
        const int y1 {dst_height};
        return {x0, y0, x1, y1};
    }
}

[[nodiscard]] GLuint create_shader(GLenum type, const char *code)
{
    const auto shader = glCreateShader(type);
    SCOPE_FAIL([shader] { glDeleteShader(shader); });

    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    int success {};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetShaderInfoLog(shader, buf_length, nullptr, message.data());
        std::ostringstream oss;
        oss << "Shader compilation failed:\n" << message << '\n';
        throw std::runtime_error(oss.str());
    }

    return shader;
}

[[nodiscard]] GLuint create_program(auto &&...shaders)
{
    const auto program = glCreateProgram();
    SCOPE_FAIL([program] { glDeleteProgram(program); });

    (glAttachShader(program, shaders), ...);
    glLinkProgram(program);

    int success {};
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetProgramInfoLog(program, buf_length, nullptr, message.data());
        std::ostringstream oss;
        oss << "Program linking failed:\n" << message << '\n';
        throw std::runtime_error(oss.str());
    }

    return program;
}

[[nodiscard]] GLuint create_compute_program(const char *file_name)
{
    const auto shader_code = read_file(file_name);
    const auto c_shader_code = shader_code.c_str();
    const auto shader = create_shader(GL_COMPUTE_SHADER, c_shader_code);
    SCOPE_EXIT([shader] { glDeleteShader(shader); });

    const auto program = create_program(shader);

    return program;
}

[[maybe_unused]] [[nodiscard]] GLuint create_vert_frag_program()
{
    constexpr auto vertex_shader_code = R"(#version 430
layout (location = 0) in vec2 vertex_position;
void main()
{
    gl_Position = vec4(vertex_position, 0.0, 1.0);
})";

    constexpr auto fragment_shader_code = R"(#version 430
out vec4 frag_color;
void main()
{
    frag_color = vec4(1.0, 0.0, 1.0, 1.0);
})";

    const auto vertex_shader =
        create_shader(GL_VERTEX_SHADER, vertex_shader_code);
    SCOPE_EXIT([vertex_shader] { glDeleteShader(vertex_shader); });

    const auto fragment_shader =
        create_shader(GL_FRAGMENT_SHADER, fragment_shader_code);
    SCOPE_EXIT([fragment_shader] { glDeleteShader(fragment_shader); });

    const auto program = create_program(vertex_shader, fragment_shader);

    return program;
}

[[nodiscard]] GLuint create_accumulation_texture(GLsizei width, GLsizei height)
{
    GLuint texture {};
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
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
    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    return texture;
}

[[nodiscard]] GLuint create_target_texture(GLsizei width, GLsizei height)
{
    GLuint texture {};
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
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
    glBindImageTexture(5, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    return texture;
}

[[nodiscard]] GLuint create_framebuffer(GLuint texture)
{
    GLuint fbo {};
    glGenFramebuffers(1, &fbo);
    SCOPE_FAIL([&fbo] { glDeleteFramebuffers(1, &fbo); });

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
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

[[maybe_unused]] [[nodiscard]] std::pair<GLuint, GLuint>
create_vao_vbo(unsigned int num_vertices)
{
    GLuint vao {};
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo {};
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 num_vertices * 2 * sizeof(float),
                 nullptr,
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    return {vao, vbo};
}

template <typename T>
[[nodiscard]] GLuint create_storage_buffer(GLuint binding,
                                           const std::vector<T> &data)
{
    GLuint ssbo {};
    glGenBuffers(1, &ssbo);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(data.size() * sizeof(T)),
                 data.data(),
                 GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbo);

    return ssbo;
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

    std::cout << "Saved" << std::endl;
}

void run()
{
    glfwSetErrorCallback(&glfw_error_callback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    SCOPE_EXIT([] { glfwTerminate(); });

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, 1);
    const auto window =
        glfwCreateWindow(1280, 720, "Caustics", nullptr, nullptr);
    if (window == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }
    SCOPE_EXIT([window] { glfwDestroyWindow(window); });

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    float x_scale {};
    float y_scale {};
    glfwGetWindowContentScale(window, &x_scale, &y_scale);

    load_gl_functions();

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&gl_debug_callback, nullptr);

    const std::string renderer(
        reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    std::cout << "Renderer: \"" << renderer << "\", ";
    bool auto_workload {};
    if (renderer.find("Mesa") != std::string::npos &&
        renderer.find("Intel") != std::string::npos)
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
    float view_x {0.5f};
    float view_y {view_x * static_cast<float>(texture_height) /
                  static_cast<float>(texture_width)};
    float view_width {1.0f};
    float view_height {view_width * static_cast<float>(texture_height) /
                       static_cast<float>(texture_width)};

#if 1
    Scene scene {
        .materials =
            {Material {{0.75f, 0.75f, 0.75f},
                       {6.0f, 6.0f, 6.0f},
                       Material_type::diffuse},
             Material {{0.75f, 0.25f, 0.25f}, {}, Material_type::dielectric},
             Material {{0.25f, 0.25f, 0.75f}, {}, Material_type::dielectric},
             Material {{1.0f, 1.0f, 1.0f}, {}, Material_type::specular},
             Material {{0.75f, 0.75f, 0.75f}, {}, Material_type::diffuse},
             Material {{0.75f, 0.75f, 0.75f}, {}, Material_type::dielectric}},
        .circles = {Circle {{0.8f, 0.5f}, 0.03f, 0},
                    Circle {{0.5f, 0.3f}, 0.15f, 1},
                    Circle {{0.8f, 0.2f}, 0.05f, 2}},
        .lines = {Line {{0.1f, 0.2f}, {0.35f, 0.05f}, 3},
                  Line {{0.1f, 0.4f}, {0.4f, 0.6f}, 4}},
        .arcs = {Arc {{0.6f, 0.6f},
                      0.1f,
                      {-0.5f, std::numbers::sqrt3_v<float> * 0.5f},
                      -0.04f,
                      3}}};
#else
    Scene scene {
        .materials = {Material {
            {0.75f, 0.75f, 0.75f}, {6.0f, 6.0f, 6.0f}, Material_type::diffuse}},
        .circles = {Circle {{0.5f, 0.5f * view_height / view_width}, 0.05f, 0}},
        .lines = {},
        .arcs = {}};
#endif

    const auto compute_program = create_compute_program("trace.comp");
    SCOPE_EXIT([compute_program] { glDeleteProgram(compute_program); });

    const auto loc_sample_index =
        glGetUniformLocation(compute_program, "sample_index");
    const auto loc_samples_per_frame =
        glGetUniformLocation(compute_program, "samples_per_frame");
    const auto loc_view_position =
        glGetUniformLocation(compute_program, "view_position");
    const auto loc_view_size =
        glGetUniformLocation(compute_program, "view_size");

    const auto post_program = create_compute_program("post.comp");
    SCOPE_EXIT([post_program] { glDeleteProgram(post_program); });

    const auto accumulation_texture =
        create_accumulation_texture(texture_width, texture_height);
    SCOPE_EXIT([&accumulation_texture]
               { glDeleteTextures(1, &accumulation_texture); });

    const auto target_texture =
        create_target_texture(texture_width, texture_height);
    SCOPE_EXIT([&target_texture] { glDeleteTextures(1, &target_texture); });

    const auto fbo = create_framebuffer(target_texture);
    SCOPE_EXIT([&fbo] { glDeleteFramebuffers(1, &fbo); });

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    GLuint query_start {};
    GLuint query_end {};
    glGenQueries(1, &query_start);
    SCOPE_EXIT([&query_start] { glDeleteQueries(1, &query_start); });
    glGenQueries(1, &query_end);
    SCOPE_EXIT([&query_end] { glDeleteQueries(1, &query_end); });

    const auto materials_ssbo = create_storage_buffer(1, scene.materials);
    SCOPE_EXIT([&materials_ssbo] { glDeleteBuffers(1, &materials_ssbo); });
    const auto circles_ssbo = create_storage_buffer(2, scene.circles);
    SCOPE_EXIT([&circles_ssbo] { glDeleteBuffers(1, &circles_ssbo); });
    const auto lines_ssbo = create_storage_buffer(3, scene.lines);
    SCOPE_EXIT([&lines_ssbo] { glDeleteBuffers(1, &lines_ssbo); });
    const auto arcs_ssbo = create_storage_buffer(4, scene.arcs);
    SCOPE_EXIT([&arcs_ssbo] { glDeleteBuffers(1, &arcs_ssbo); });

    constexpr unsigned int max_samples {200'000};
    unsigned int sample_index {0};
    unsigned int samples_per_frame {1};

    double last_time {glfwGetTime()};
    unsigned int sum_samples {0};
    int num_frames {0};

    bool s_pressed {false};
    bool dragging {false};
    double drag_source_mouse_x {};
    double drag_source_mouse_y {};
    float drag_source_view_x {};
    float drag_source_view_y {};

    glfwSetScrollCallback(window, &glfw_scroll_callback);

    while (!glfwWindowShouldClose(window))
    {
        scroll_offset = 0.0;
        glfwPollEvents();

        int framebuffer_width {};
        int framebuffer_height {};
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

        const auto [x0, y0, x1, y1] = centered_rectangle(texture_width,
                                                         texture_height,
                                                         framebuffer_width,
                                                         framebuffer_height);

        // FIXME: all of this should be done in callbacks
        if (const auto mouse_state =
                glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
            mouse_state == GLFW_PRESS)
        {
            double xpos {};
            double ypos {};
            glfwGetCursorPos(window, &xpos, &ypos);

            if (!dragging)
            {
                dragging = true;
                drag_source_mouse_x = xpos;
                drag_source_mouse_y = ypos;
                drag_source_view_x = view_x;
                drag_source_view_y = view_y;
            }
            else
            {
                // Drag vector
                const auto drag_world_x =
                    static_cast<float>(xpos - drag_source_mouse_x) * x_scale /
                    static_cast<float>(x1 - x0) * view_width;
                const auto drag_world_y =
                    static_cast<float>(ypos - drag_source_mouse_y) * y_scale /
                    static_cast<float>(y1 - y0) * view_height;

                view_x = drag_source_view_x - drag_world_x;
                view_y = drag_source_view_y - drag_world_y;
                sample_index = 0;
            }
        }
        else if (mouse_state == GLFW_RELEASE)
        {
            dragging = false;
        }

        if (scroll_offset != 0.0)
        {
            constexpr float zoom_factor {1.5f};

            double xpos {};
            double ypos {};
            glfwGetCursorPos(window, &xpos, &ypos);
            const auto mouse_world_x =
                view_x +
                ((static_cast<float>(xpos) * x_scale - static_cast<float>(x0)) /
                     static_cast<float>(x1 - x0) -
                 0.5f) *
                    view_width;
            const auto mouse_world_y =
                view_y +
                ((static_cast<float>(ypos) * y_scale - static_cast<float>(y0)) /
                     static_cast<float>(y1 - y0) -
                 0.5f) *
                    view_height;

            if (scroll_offset > 0.0)
            {
                view_x += (mouse_world_x - view_x) * (zoom_factor - 1.0f);
                view_y += (mouse_world_y - view_y) * (zoom_factor - 1.0f);
                view_width /= zoom_factor;
                view_height /= zoom_factor;
            }
            else
            {
                view_x -= (mouse_world_x - view_x) * (zoom_factor - 1.0f);
                view_y -= (mouse_world_y - view_y) * (zoom_factor - 1.0f);
                view_width *= zoom_factor;
                view_height *= zoom_factor;
            }
            sample_index = 0;
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        {
            sample_index = 0;
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && !s_pressed)
        {
            s_pressed = true;
            save_as_png("out.png", texture_width, texture_height);
        }
        else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_RELEASE)
        {
            s_pressed = false;
        }

        if (auto_workload)
        {
            glQueryCounter(query_start, GL_TIMESTAMP);
        }

        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (sample_index < max_samples)
        {
            const auto samples_this_frame =
                std::min(samples_per_frame, max_samples - sample_index);
            glUseProgram(compute_program);
            glUniform1ui(loc_sample_index, sample_index);
            glUniform1ui(loc_samples_per_frame, samples_this_frame);
            glUniform2f(loc_view_position, view_x, view_y);
            glUniform2f(loc_view_size, view_width, view_height);
            unsigned int num_groups_x {
                align_up(static_cast<unsigned int>(texture_width), 16) / 16,
            };
            unsigned int num_groups_y {
                align_up(static_cast<unsigned int>(texture_height), 16) / 16,
            };
            glDispatchCompute(num_groups_x, num_groups_y, 1);
            sample_index += samples_this_frame;
            sum_samples += samples_this_frame;

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glUseProgram(post_program);
            glDispatchCompute(num_groups_x, num_groups_y, 1);

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        // NOTE: we switch y0 and y1 to have (0, 0) in the
        // top left corner
        glBlitFramebuffer(0,
                          0,
                          texture_width,
                          texture_height,
                          x0,
                          y1,
                          x1,
                          y0,
                          GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);

        if (auto_workload)
        {
            glQueryCounter(query_end, GL_TIMESTAMP);
        }

        glfwSwapBuffers(window);

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
            glGetQueryObjectui64v(query_start, GL_QUERY_RESULT, &start_time);
            glGetQueryObjectui64v(query_end, GL_QUERY_RESULT, &end_time);
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

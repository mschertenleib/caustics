#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
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
    f(PFNGLUSEPROGRAMPROC, glUseProgram);                                      \
    f(PFNGLDISPATCHCOMPUTEPROC, glDispatchCompute);                            \
    f(PFNGLVIEWPORTPROC, glViewport);                                          \
    f(PFNGLCLEARCOLORPROC, glClearColor);                                      \
    f(PFNGLCLEARPROC, glClear);                                                \
    f(PFNGLMEMORYBARRIERPROC, glMemoryBarrier);                                \
    f(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer);

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

struct Circle
{
    float cx;
    float cy;
    float r;
};

struct Scene
{
    float width;
    float height;
    std::vector<Circle> circles;
};

void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void load_gl_functions()
{
#define LOAD_GL_FUNCTION(type, name)                                           \
    name = reinterpret_cast<type>(glfwGetProcAddress(#name))

    ENUMERATE_GL_FUNCTIONS(LOAD_GL_FUNCTION)

#undef LOAD_GL_FUNCTION
}

void APIENTRY gl_debug_callback([[maybe_unused]] GLenum source,
                                [[maybe_unused]] GLenum type,
                                [[maybe_unused]] GLuint id,
                                [[maybe_unused]] GLenum severity,
                                [[maybe_unused]] GLsizei length,
                                const GLchar *message,
                                [[maybe_unused]] const void *user_param)
{
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

[[nodiscard]] GLuint create_compute_program(const char *file_name)
{
    const auto shader_code = read_file(file_name);
    const auto c_shader_code = shader_code.c_str();

    const auto shader_id = glCreateShader(GL_COMPUTE_SHADER);
    SCOPE_EXIT([shader_id] { glDeleteShader(shader_id); });

    glShaderSource(shader_id, 1, &c_shader_code, nullptr);
    glCompileShader(shader_id);
    int success {};
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetShaderInfoLog(shader_id, buf_length, nullptr, message.data());
        std::ostringstream oss;
        oss << "Shader compilation failed:\n" << message << '\n';
        throw std::runtime_error(oss.str());
    }

    const auto program_id = glCreateProgram();
    SCOPE_FAIL([program_id] { glDeleteProgram(program_id); });

    glAttachShader(program_id, shader_id);
    glLinkProgram(program_id);
    glGetProgramiv(program_id, GL_LINK_STATUS, &success);
    if (!success)
    {
        int buf_length {};
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &buf_length);
        std::string message(static_cast<std::size_t>(buf_length), '\0');
        glGetProgramInfoLog(program_id, buf_length, nullptr, message.data());
        std::ostringstream oss;
        oss << "Program linking failed:\n" << message << '\n';
        throw std::runtime_error(oss.str());
    }

    return program_id;
}

[[nodiscard]] GLuint create_texture(GLsizei width, GLsizei height)
{
    GLuint texture_id {};
    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);
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
        0, texture_id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    return texture_id;
}

[[nodiscard]] GLuint create_framebuffer(GLuint texture_id)
{
    GLuint fbo_id {};
    glGenFramebuffers(1, &fbo_id);
    SCOPE_FAIL([&fbo_id] { glDeleteFramebuffers(1, &fbo_id); });

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

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

    return fbo_id;
}

} // namespace

int main()
{
    try
    {
        glfwSetErrorCallback(&glfw_error_callback);

        if (!glfwInit())
        {
            return EXIT_FAILURE;
        }
        SCOPE_EXIT([] { glfwTerminate(); });

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_CONTEXT_DEBUG, 1);
        const auto window =
            glfwCreateWindow(1280, 720, "Caustics", nullptr, nullptr);
        if (window == nullptr)
        {
            return EXIT_FAILURE;
        }
        SCOPE_EXIT([window] { glfwDestroyWindow(window); });

        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);

        float x_scale {};
        float y_scale {};
        glfwGetWindowContentScale(window, &x_scale, &y_scale);

        load_gl_functions();

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(&gl_debug_callback, nullptr);

        const auto program_id = create_compute_program("shader.comp");
        SCOPE_EXIT([program_id] { glDeleteProgram(program_id); });

        const auto loc_sample_index =
            glGetUniformLocation(program_id, "sample_index");
        const auto loc_world_size =
            glGetUniformLocation(program_id, "world_size");
        const auto loc_mouse = glGetUniformLocation(program_id, "mouse");

        constexpr int texture_width {320};
        constexpr int texture_height {240};
        const auto texture_id = create_texture(texture_width, texture_height);
        SCOPE_EXIT([&texture_id] { glDeleteTextures(1, &texture_id); });

        const auto fbo_id = create_framebuffer(texture_id);
        SCOPE_EXIT([&fbo_id] { glDeleteFramebuffers(1, &fbo_id); });

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        constexpr float world_width {1.0f};
        constexpr float world_height {world_width *
                                      static_cast<float>(texture_height) /
                                      static_cast<float>(texture_width)};
        unsigned int sample_index {0};
        double last_xpos {};
        double last_ypos {};

        double last_time {glfwGetTime()};
        int num_frames {0};

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            int framebuffer_width {};
            int framebuffer_height {};
            glfwGetFramebufferSize(
                window, &framebuffer_width, &framebuffer_height);

            const auto [x0, y0, x1, y1] =
                centered_rectangle(texture_width,
                                   texture_height,
                                   framebuffer_width,
                                   framebuffer_height);

            double xpos {};
            double ypos {};
            glfwGetCursorPos(window, &xpos, &ypos);
            // Normalized mouse coordinates
            const auto mouse_x = static_cast<float>(
                (xpos * static_cast<double>(x_scale) - x0) / (x1 - x0));
            const auto mouse_y = static_cast<float>(
                (ypos * static_cast<double>(y_scale) - y0) / (y1 - y0));
            if (xpos != last_xpos || ypos != last_ypos)
            {
                // If the mouse moved, reset render
                sample_index = 0;
            }
            last_xpos = xpos;
            last_ypos = ypos;

            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
            {
                sample_index = 0;
            }

            glViewport(0, 0, framebuffer_width, framebuffer_height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(program_id);
            glUniform1ui(loc_sample_index, sample_index);
            glUniform2f(loc_world_size, world_width, world_height);
            glUniform2f(loc_mouse, mouse_x, mouse_y);
            glDispatchCompute(static_cast<unsigned int>(texture_width),
                              static_cast<unsigned int>(texture_height),
                              1);
            ++sample_index;

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            // NOTE: we switch y0 and y1 to have (0, 0) in the top left corner
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

            ++num_frames;
            const double current_time {glfwGetTime()};
            if (const auto elapsed = current_time - last_time; elapsed >= 1.0)
            {
                std::cout << static_cast<double>(num_frames) / elapsed
                          << " fps, "
                          << elapsed * 1000.0 / static_cast<double>(num_frames)
                          << " ms/frame, " << sample_index
                          << (sample_index > 1 ? " samples\n" : " sample\n");
                num_frames = 0;
                last_time = current_time;
            }

            glfwSwapBuffers(window);
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception thrown: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }
}

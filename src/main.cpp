#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{

#define ENUMERATE_GL_FUNCTIONS(f)                                              \
    f(PFNGLCREATESHADERPROC, glCreateShader);                                  \
    f(PFNGLSHADERSOURCEPROC, glShaderSource);                                  \
    f(PFNGLCOMPILESHADERPROC, glCompileShader);                                \
    f(PFNGLGETSHADERIVPROC, glGetShaderiv);                                    \
    f(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);                          \
    f(PFNGLATTACHSHADERPROC, glAttachShader);                                  \
    f(PFNGLCREATEPROGRAMPROC, glCreateProgram);                                \
    f(PFNGLLINKPROGRAMPROC, glLinkProgram);                                    \
    f(PFNGLGETPROGRAMIVPROC, glGetProgramiv);                                  \
    f(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);                        \
    f(PFNGLDELETESHADERPROC, glDeleteShader);                                  \
    f(PFNGLGENTEXTURESPROC, glGenTextures);                                    \
    f(PFNGLBINDTEXTUREPROC, glBindTexture);                                    \
    f(PFNGLTEXPARAMETERIPROC, glTexParameteri);                                \
    f(PFNGLTEXIMAGE2DPROC, glTexImage2D);                                      \
    f(PFNGLBINDIMAGETEXTUREPROC, glBindImageTexture);                          \
    f(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);                            \
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

void load_gl_functions()
{
#define LOAD_GL_FUNCTION(type, name)                                           \
    name = reinterpret_cast<type>(glfwGetProcAddress(#name))

    ENUMERATE_GL_FUNCTIONS(LOAD_GL_FUNCTION)

#undef LOAD_GL_FUNCTION
}

template <typename F>
class Scope_guard
{
public:
    explicit Scope_guard(F &&f) : m_f(std::move(f))
    {
    }

    explicit Scope_guard(F &f) : m_f(f)
    {
    }

    ~Scope_guard() noexcept
    {
        m_f();
    }

    Scope_guard(const Scope_guard &) = delete;
    Scope_guard(Scope_guard &&) = delete;
    Scope_guard &operator=(const Scope_guard &) = delete;
    Scope_guard &operator=(Scope_guard &&) = delete;

private:
    F m_f;
};

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2)      CONCATENATE_IMPL(s1, s2)
#define SCOPE_EXIT(f)            const Scope_guard CONCATENATE(scope_guard_, __LINE__)(f)

[[nodiscard]] std::string read_file(const char *file_name)
{
    std::ifstream file(file_name);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

} // namespace

int main()
{
    try
    {
        glfwSetErrorCallback(
            [](int error, const char *description) {
                std::cerr << "GLFW error " << error << ": " << description
                          << '\n';
            });

        if (!glfwInit())
        {
            return EXIT_FAILURE;
        }
        SCOPE_EXIT([] { glfwTerminate(); });

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        const auto window =
            glfwCreateWindow(1280, 720, "Caustics", nullptr, nullptr);
        if (window == nullptr)
        {
            return EXIT_FAILURE;
        }
        SCOPE_EXIT([window] { glfwDestroyWindow(window); });

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        load_gl_functions();

        const auto shader_code = read_file("shader.comp");
        const auto c_shader_code = shader_code.c_str();

        const auto shader_id = glCreateShader(GL_COMPUTE_SHADER);
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
            std::cerr << "Shader compilation failed:\n" << message << '\n';
        }
        const auto program_id = glCreateProgram();
        glAttachShader(program_id, shader_id);
        glLinkProgram(program_id);
        glGetProgramiv(program_id, GL_LINK_STATUS, &success);
        if (!success)
        {
            int buf_length {};
            glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &buf_length);
            std::string message(static_cast<std::size_t>(buf_length), '\0');
            glGetProgramInfoLog(
                program_id, buf_length, nullptr, message.data());
            std::cerr << "Program linking failed:\n" << message << '\n';
        }
        glDeleteShader(shader_id);

        // Create texture
        constexpr int texture_width {32};
        constexpr int texture_height {24};
        GLuint texture_id {};
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     texture_width,
                     texture_height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     nullptr);
        glBindImageTexture(
            0, texture_id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

        // Create framebuffer object
        GLuint fbo_id;
        glGenFramebuffers(1, &fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "Framebuffer status error\n";
            return EXIT_FAILURE;
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        double last_time {glfwGetTime()};
        int num_frames {0};

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            glUseProgram(program_id);
            glDispatchCompute(static_cast<unsigned int>(texture_width),
                              static_cast<unsigned int>(texture_height),
                              1);

            int framebuffer_width {};
            int framebuffer_height {};
            glfwGetFramebufferSize(
                window, &framebuffer_width, &framebuffer_height);

            const auto texture_aspect_ratio =
                static_cast<float>(texture_width) /
                static_cast<float>(texture_height);
            const auto framebuffer_aspect_ratio =
                static_cast<float>(framebuffer_width) /
                static_cast<float>(framebuffer_height);
            int dst_x0 {0};
            int dst_y0 {0};
            int dst_x1 {framebuffer_width};
            int dst_y1 {framebuffer_height};
            if (texture_aspect_ratio > framebuffer_aspect_ratio)
            {
                const auto dst_height =
                    static_cast<int>(static_cast<float>(framebuffer_width) /
                                     texture_aspect_ratio);
                dst_y0 = (framebuffer_height - dst_height) / 2;
                dst_y1 = dst_y0 + dst_height;
            }
            else
            {
                const auto dst_width =
                    static_cast<int>(static_cast<float>(framebuffer_height) *
                                     texture_aspect_ratio);
                dst_x0 = (framebuffer_width - dst_width) / 2;
                dst_x1 = dst_x0 + dst_width;
            }

            glViewport(0, 0, framebuffer_width, framebuffer_height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // TODO: check that this barrier is correct
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            glBlitFramebuffer(0,
                              0,
                              texture_width,
                              texture_height,
                              dst_x0,
                              dst_y0,
                              dst_x1,
                              dst_y1,
                              GL_COLOR_BUFFER_BIT,
                              GL_NEAREST);

            ++num_frames;
            const double current_time {glfwGetTime()};
            if (const auto elapsed = current_time - last_time; elapsed >= 1.0)
            {
                std::cout << "FPS: "
                          << static_cast<double>(num_frames) / elapsed << '\n';
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

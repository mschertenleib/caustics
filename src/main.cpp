#define GLFW_INCLUDE_GLCOREARB
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

struct Glfw_guard
{
    ~Glfw_guard()
    {
        glfwTerminate();
    }
};

struct Window_guard
{
    ~Window_guard()
    {
        glfwDestroyWindow(window);
    }

    GLFWwindow *window;
};

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
        Glfw_guard glfw_guard {};

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        const auto window =
            glfwCreateWindow(1280, 720, "Caustics", nullptr, nullptr);
        if (window == nullptr)
        {
            return EXIT_FAILURE;
        }
        Window_guard Window_guard {window};

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        // Create compute shader
        const auto shader_id = glCreateShader(GL_COMPUTE_SHADER);
        constexpr auto shader_code = R"(#version 430

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(rgba8, binding = 0) uniform image2D image;

void main() {
    ivec2 size = imageSize(image);
    vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(size);
    vec4 color = vec4(uv, 0.0, 1.0);
    imageStore(image, ivec2(gl_GlobalInvocationID.xy), color);
})";
        glShaderSource(shader_id, 1, &shader_code, nullptr);
        glCompileShader(shader_id);
        int success {};
        char buffer[1024] {};
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader_id, sizeof(buffer), nullptr, buffer);
            std::cerr << "Shader compilation failed:\n" << buffer << '\n';
        }
        const auto program_id = glCreateProgram();
        glAttachShader(program_id, shader_id);
        glLinkProgram(program_id);
        glGetProgramiv(program_id, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(program_id, sizeof(buffer), nullptr, buffer);
            std::cerr << "Program linking failed:\n" << buffer << '\n';
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

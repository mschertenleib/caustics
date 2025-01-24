#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        glfwSetErrorCallback(
            [](int error, const char *description) {
                std::cerr << "GLFW error " << error << ": " << description
                          << std::endl;
            });

        if (!glfwInit())
        {
            return EXIT_FAILURE;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

        const auto window =
            glfwCreateWindow(1280, 720, "Caustics", nullptr, nullptr);
        if (window == nullptr)
        {
            glfwTerminate();
            return EXIT_FAILURE;
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            int framebuffer_width {};
            int framebuffer_height {};
            glfwGetFramebufferSize(
                window, &framebuffer_width, &framebuffer_height);
            glViewport(0, 0, framebuffer_width, framebuffer_height);
            glClearColor(0.25f, 0.25f, 0.75f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glfwSwapBuffers(window);
        }

        glfwDestroyWindow(window);
        glfwTerminate();

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

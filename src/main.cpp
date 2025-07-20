#include "application.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        // NOTE: when compiling with Emscripten, the
        // emscripten_set_main_loop(..., simulate_infinite_loop=true) call
        // unwinds the stack (but global or static storage duration objects are
        // not destroyed), before transferring control to the browser. For this
        // reason, any object passed to the main loop callback must have static
        // storage duration.

        static Application app;
        app.run();

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

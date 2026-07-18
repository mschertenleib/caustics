cmake_minimum_required(VERSION 3.19)

project(caustics LANGUAGES CXX)


set(TARGET_WEB OFF)
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(TARGET_WEB ON)
endif()


set(CPM_DOWNLOAD_VERSION 0.38.2)

if (CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif (DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else ()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif ()

if (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif ()

include(${CPM_DOWNLOAD_LOCATION})


find_package(OpenGL REQUIRED)


if (NOT TARGET_WEB)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    CPMAddPackage(
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.4
    )
endif ()


CPMAddPackage(
        GITHUB_REPOSITORY ocornut/imgui
        GIT_TAG v1.91.9
        DOWNLOAD_ONLY YES
)
add_library(imgui STATIC)
target_sources(imgui PRIVATE
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui.h
        ${imgui_SOURCE_DIR}/imgui_internal.h
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PRIVATE glfw)


CPMAddPackage(
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG f4a71b1
    DOWNLOAD_ONLY YES
)


CPMAddPackage(
    GITHUB_REPOSITORY nlohmann/json
    GIT_TAG v3.12.0
)


add_executable(caustics)
target_sources(caustics PRIVATE
    src/main.cpp
    src/application.hpp src/application.cpp
    src/scene.hpp src/scene.cpp
    src/unique_resource.hpp
    src/vec.hpp
)
target_compile_features(caustics PRIVATE cxx_std_23)
target_include_directories(caustics SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(caustics PRIVATE imgui nlohmann_json::nlohmann_json)


if (TARGET_WEB)
    set_target_properties(caustics PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/web)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_include_directories(caustics PRIVATE ${EMSCRIPTEN_ROOT_PATH}/sysroot/include)
    target_compile_options(caustics PRIVATE
        -Os
        -sDISABLE_EXCEPTION_CATCHING=0
        --use-port=contrib.glfw3
    )
    target_link_options(caustics PRIVATE
        -Os
        -sWASM=1
        -sUSE_WEBGL2=1
        --use-port=contrib.glfw3
        -sFULL_ES3=1
        -sASSERTIONS=1
        -sDISABLE_EXCEPTION_CATCHING=0
        -sFORCE_FILESYSTEM=1
        -sALLOW_MEMORY_GROWTH=1
        --emrun
        --shell-file ${CMAKE_SOURCE_DIR}/src/shell.html
        --preload-file ${CMAKE_SOURCE_DIR}/src/shaders/@shaders/
    )
    #FIXME: do we need other flags? Also we probably shouldn't link imgui with the static glfw if we build for web
    target_compile_options(imgui PRIVATE --use-port=contrib.glfw3)
    target_link_options(imgui PRIVATE --use-port=contrib.glfw3)
else ()
    target_link_libraries(caustics PRIVATE glfw)

    add_custom_target(shaders ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/shaders $<TARGET_FILE_DIR:caustics>/shaders
    )
    add_dependencies(caustics shaders)
endif ()


set(clang_warnings
    -Wall
    -Wextra
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
)
set(gcc_warnings
    ${clang_warnings}
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wswitch-enum
)
if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    target_compile_options(caustics PRIVATE ${clang_warnings})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(caustics PRIVATE ${gcc_warnings})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(caustics PRIVATE /W4)
else ()
    message(WARNING "No warnings set for compiler ${CMAKE_CXX_COMPILER_ID}")
endif ()



if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(san_compile -fsanitize=address,undefined,float-divide-by-zero,integer-divide-by-zero -fno-omit-frame-pointer)
    set(san_link -fsanitize=address,undefined)
    
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND san_compile
            -fsanitize-address-use-after-return=runtime
            -fsanitize-address-use-after-scope
            -fsanitize=implicit-conversion
        )
    endif()

    target_compile_options(caustics PRIVATE $<$<CONFIG:Debug>:${san_compile}>)
    target_link_options(caustics PRIVATE $<$<CONFIG:Debug>:${san_link}>)
    
    set(lsan_suppressions "${CMAKE_BINARY_DIR}/lsan.supp")
    file(WRITE "${lsan_suppressions}"
"leak:_glfwCreateContextGL
leak:_glfwInitEGL
leak:libwayland-client
leak:libgtk
leak:libfontconfig
")

    set(asan_defaults "symbolize=1:fast_unwind_on_malloc=0:detect_leaks=1:leak_check_at_exit=1")
    set(lsan_defaults "suppressions=$<SHELL_PATH:${lsan_suppressions}>")
    set(ubsan_defaults "print_stacktrace=1")
    find_program(LLVM_SYMBOLIZER NAMES llvm-symbolizer llvm-symbolizer-19 llvm-symbolizer-18)
    if (LLVM_SYMBOLIZER)
        string(APPEND asan_defaults ":external_symbolizer_path=$<SHELL_PATH:${LLVM_SYMBOLIZER}>")
    endif()

    set(san_defaults_src "${CMAKE_BINARY_DIR}/sanitizer_defaults.cpp")
    file(GENERATE OUTPUT "${san_defaults_src}" CONTENT
"extern \"C\" const char* __asan_default_options() {
    return \"${asan_defaults}\";
}
extern \"C\" const char* __lsan_default_options() {
    return \"${lsan_defaults}\";
}
extern \"C\" const char* __ubsan_default_options() {
    return \"${ubsan_defaults}\";
}")
    target_sources(caustics PRIVATE $<$<CONFIG:Debug>:${san_defaults_src}>)
endif ()


message(CHECK_START "Checking for IPO support")
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported_result OUTPUT ipo_supported_output)
if (ipo_supported_result)
    message(CHECK_PASS "supported")
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_MIN_SIZE_REL TRUE)
else ()
    message(CHECK_FAIL "not supported")
endif ()

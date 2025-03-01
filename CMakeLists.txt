cmake_minimum_required(VERSION 3.19)

project(caustics LANGUAGES CXX)


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


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
CPMAddPackage(
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.4
)


CPMAddPackage(
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG f4a71b1
    DOWNLOAD_ONLY YES
)


add_executable(caustics src/main.cpp)
target_compile_features(caustics PRIVATE cxx_std_20)
target_include_directories(caustics SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(caustics PRIVATE OpenGL::GL glfw)


# NOTE: this custom target is for always copying the shaders to the binary directory
set(SHADERS
    src/trace.glsl
    src/post.comp
    src/shader.vert
    src/shader.frag
    src/fullscreen.vert
)
add_custom_target(shaders ALL DEPENDS ${SHADERS})
foreach (SHADER IN LISTS SHADERS)
    cmake_path(APPEND CMAKE_SOURCE_DIR ${SHADER} OUTPUT_VARIABLE SHADER)
    add_custom_command(TARGET shaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER} $<TARGET_FILE_DIR:caustics>
    )
endforeach ()
add_dependencies(caustics shaders)


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


if ((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    target_compile_options(caustics PRIVATE $<$<CONFIG:Debug>:-fsanitize=undefined>)
    #target_compile_options(caustics PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
    target_link_options(caustics PRIVATE $<$<CONFIG:Debug>:-fsanitize=undefined>)
    #target_link_options(caustics PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
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

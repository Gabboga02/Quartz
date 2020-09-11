cmake_minimum_required(VERSION 3.17)
project(Quartz CXX)

find_package(Vulkan REQUIRED)

# Disable GLFW docs, tests and examples builds and forces to use statically linked vulkan loader.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# Disable SPIRV-Cross tests and exceptions, force build as a static library.
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(external/SPIRV-Cross)

# Disable FTL Tests, Benchmarks and enable C++17 Standard.
set(FTL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTL_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(FTL_CPP_17 ON CACHE BOOL "" FORCE)
add_subdirectory(external/FiberTaskingLib)

set(CMAKE_CXX_STANDARD 20)

add_executable(Quartz
    src/qz/gfx/assets.cpp
    src/qz/gfx/assets.hpp
    src/qz/gfx/buffer.cpp
    src/qz/gfx/buffer.hpp
    src/qz/gfx/clear.hpp
    src/qz/gfx/command_buffer.cpp
    src/qz/gfx/command_buffer.hpp
    src/qz/gfx/context.cpp
    src/qz/gfx/context.hpp
    src/qz/gfx/image.cpp
    src/qz/gfx/image.hpp
    src/qz/gfx/pipeline.cpp
    src/qz/gfx/pipeline.hpp
    src/qz/gfx/render_pass.cpp
    src/qz/gfx/render_pass.hpp
    src/qz/gfx/renderer.cpp
    src/qz/gfx/renderer.hpp
    src/qz/gfx/static_mesh.cpp
    src/qz/gfx/static_mesh.hpp
    src/qz/gfx/swapchain.cpp
    src/qz/gfx/swapchain.hpp
    src/qz/gfx/vma.cpp
    src/qz/gfx/window.cpp
    src/qz/gfx/window.hpp

    src/qz/meta/constants.hpp
    src/qz/meta/types.hpp

    src/qz/task/scheduler.cpp
    src/qz/task/scheduler.hpp

    src/qz/util/macros.hpp
    src/qz/util/fwd.hpp

    src/main.cpp)

target_compile_definitions(Quartz PUBLIC
    # Set DEBUG macro if Debug mode.
    $<$<CONFIG:Debug>:QUARTZ_DEBUG>

    # Removes annoying Window's shit.
    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>)

target_compile_options(Quartz PUBLIC $<$<BOOL:${MSVC}>:/EHsc>)

target_include_directories(Quartz PUBLIC
    src
    external/VulkanMemoryAllocator/src)

target_link_libraries(Quartz PUBLIC
    ftl
    glfw
    Vulkan::Vulkan
    spirv-cross-glsl)

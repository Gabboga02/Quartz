#pragma once

#include <cassert>
#include <cstdlib>
#include <cstdio>

#if _MSVC_LANG >= 201703L || __cplusplus >= 201703L
    #define qz_nodiscard [[nodiscard]]
    #define qz_noreturn [[noreturn]]
#else
    #define qz_nodiscard
    #define qz_noreturn
#endif

#if defined(QUARTZ_DEBUG)
    #define qz_assert(expr, msg) assert(expr && msg)
    #define qz_vulkan_check(expr) qz_assert(expr == VK_SUCCESS, "Result was not VK_SUCCESS")
#else
    #define qz_assert(expr, msg) (void)(expr), (void)(msg)
    #define qz_vulkan_check(expr) (void)(expr)
#endif

#define qz_force_assert(msg) (std::fprintf(stderr, "Assertion failed:%s\nFile: %s\nLine: %d\n", msg, __FILE__, __LINE__), std::abort())

#if defined(_WIN32)
    #define qz_unreachable() __assume(false)
#else
    #define qz_unreachable() __builtin_unreachable()
#endif

#define qz_load_instance_function(ctx, name) const auto name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(ctx.instance, #name))

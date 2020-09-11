#pragma once

// Heavily inspired by: https://github.com/Themaister/Granite/blob/master/vulkan/context.hpp

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <qz/util/macros.hpp>
#include <qz/util/fwd.hpp>

#include <cstdint>
#include <vector>

namespace qz::gfx {
    struct Settings {
        std::uint32_t version = VK_MAKE_VERSION(1, 2, 0);
        // TODO: Maybe more settings?
    };

    struct Context {
        VkInstance instance;
#if defined(QUARTZ_DEBUG)
        VkDebugUtilsMessengerEXT validation;
#endif
        VkPhysicalDevice gpu;
        VkDevice device;
        VmaAllocator allocator;
        VkQueue graphics;
        VkQueue transfer;
        std::uint32_t family = -1;
        VkCommandPool main_pool;

        qz_nodiscard static Context create(const Settings& = {}) noexcept;
        static void destroy(Context&) noexcept;
    };

} // namespace qz::gfx
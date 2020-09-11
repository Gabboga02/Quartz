#pragma once

#include <qz/util/macros.hpp>
#include <qz/util/fwd.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace qz::gfx {
    struct Buffer {
        struct CreateInfo {
            VkBufferUsageFlags flags;
            VmaMemoryUsage usage;
            std::size_t capacity;
        };

        VmaAllocation allocation;
        std::size_t capacity;
        VkBuffer handle;
        void* mapped;

        qz_nodiscard static Buffer create(const Context&, CreateInfo&&) noexcept;
        static void destroy(const Context&, Buffer&) noexcept;
    };
} // namespace qz::gfx
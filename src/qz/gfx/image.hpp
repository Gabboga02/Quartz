#pragma once

#include <qz/util/macros.hpp>
#include <qz/util/fwd.hpp>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace qz::gfx {
    struct Image {
        struct CreateInfo {
            std::uint32_t width;
            std::uint32_t height;
            std::uint32_t mips;
            VkFormat format;
            VkImageUsageFlags usage;
        };

        VkImage handle;
        VkImageView view;
        VmaAllocation allocation;
        VkImageAspectFlags aspect;
        VkFormat format;
        std::uint32_t mips;
        std::uint32_t width;
        std::uint32_t height;

        qz_nodiscard static Image create(const Context&, const Image::CreateInfo&) noexcept;
        static void destroy(const Context&, Image&) noexcept;
    };

    qz_nodiscard VkImageAspectFlags aspect_from_format(VkFormat) noexcept;
} // namespace qz::gfx
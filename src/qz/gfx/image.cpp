#include <qz/gfx/context.hpp>
#include <qz/gfx/image.hpp>

namespace qz::gfx {
    qz_nodiscard VkImageAspectFlags aspect_from_format(const VkFormat format) noexcept {
        switch (format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    qz_nodiscard Image Image::create(const Context& context, const Image::CreateInfo& info) noexcept {
        Image image{};

        VkImageCreateInfo image_create_info{};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.flags = {};
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = info.format;
        image_create_info.extent = { info.width, info.height, 1 };
        image_create_info.mipLevels = info.mips;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = info.usage;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.queueFamilyIndexCount = 0;
        image_create_info.pQueueFamilyIndices = nullptr;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocation_create_info{};
        allocation_create_info.flags = {};
        allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocation_create_info.requiredFlags = 0;
        allocation_create_info.preferredFlags = 0;
        allocation_create_info.memoryTypeBits = 0;
        allocation_create_info.pool = nullptr;
        allocation_create_info.pUserData = nullptr;

        qz_vulkan_check(vmaCreateImage(
            context.allocator,
            &image_create_info,
            &allocation_create_info,
            &image.handle,
            &image.allocation,
            nullptr));

        image.aspect = aspect_from_format(info.format);
        VkImageViewCreateInfo view_create_info{};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.flags = {};
        view_create_info.image = image.handle;
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create_info.format = info.format;
        view_create_info.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        view_create_info.subresourceRange.aspectMask = image.aspect;
        view_create_info.subresourceRange.baseMipLevel = 0;
        view_create_info.subresourceRange.levelCount = info.mips;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount = 1;
        qz_vulkan_check(vkCreateImageView(context.device, &view_create_info, nullptr, &image.view));

        image.format = info.format;
        image.height = info.height;
        image.width = info.width;
        image.mips = info.mips;

        return image;
    }

    void Image::destroy(const Context& context, Image& image) noexcept {
        vkDestroyImageView(context.device, image.view, nullptr);
        vmaDestroyImage(context.allocator, image.handle, image.allocation);
        image = {};
    }
} // namespace qz::gfx
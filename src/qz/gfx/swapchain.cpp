#include <qz/gfx/swapchain.hpp>
#include <qz/gfx/renderer.hpp>
#include <qz/gfx/image.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>

namespace qz::gfx {
    qz_nodiscard Swapchain Swapchain::create(const Context& context, const Window& window) noexcept {
        Swapchain swapchain;

        // Create window surface
        qz_vulkan_check(glfwCreateWindowSurface(context.instance, window.handle(), nullptr, &swapchain.surface));

        VkBool32 present_support;
        qz_vulkan_check(vkGetPhysicalDeviceSurfaceSupportKHR(context.gpu, context.family, swapchain.surface, &present_support));
        qz_assert(present_support, "Surface or family does not support presentation");

        // Query for surface capabilities (width, height, format, color space, etc..).
        VkSurfaceCapabilitiesKHR capabilities;
        qz_vulkan_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.gpu, swapchain.surface, &capabilities));

        // Adjust image count the swapchain will have.
        auto image_count = capabilities.minImageCount + 1;

        // Check we don't exceed maximum allowed swapchain images.
        if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
            image_count = capabilities.maxImageCount;
        }

        // Retrieve swapchain's images extent.
        if (capabilities.currentExtent.width != static_cast<std::uint32_t>(-1)) {
            swapchain.extent = capabilities.currentExtent;
        } else {
            swapchain.extent = VkExtent2D{
                std::clamp(window.width(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp(window.height(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
            };
        }

        // Query for supported surface formats.
        std::uint32_t format_count;
        qz_vulkan_check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.gpu, swapchain.surface, &format_count, nullptr));
        std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
        qz_vulkan_check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.gpu, swapchain.surface, &format_count, surface_formats.data()));

        auto format = surface_formats[0];
        for (const auto& each : surface_formats) {
            // Check if RGBA8_SRGB is supported, otherwise fallback to first format.
            if (each.format == VK_FORMAT_B8G8R8A8_SRGB && each.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                format = each;
                break;
            }
        }
        swapchain.format = format.format;

        // Fill out VkSwapchainCreateInfoKHR struct, provide all the informations we gathered so far to create the swapchain and its images.
        VkSwapchainCreateInfoKHR swapchain_create_info{};
        swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_create_info.surface = swapchain.surface;
        swapchain_create_info.minImageCount = image_count;
        swapchain_create_info.imageFormat = swapchain.format;
        swapchain_create_info.imageColorSpace = format.colorSpace;
        swapchain_create_info.imageExtent = swapchain.extent;
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 1;
        swapchain_create_info.pQueueFamilyIndices = &context.family;
        swapchain_create_info.preTransform = capabilities.currentTransform;
        swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        swapchain_create_info.clipped = true;
        swapchain_create_info.oldSwapchain = nullptr;

        // Create swapchain.
        qz_vulkan_check(vkCreateSwapchainKHR(context.device, &swapchain_create_info, nullptr, &swapchain.handle));

        // Get swapchain images.
        std::vector<VkImage> images;
        qz_vulkan_check(vkGetSwapchainImagesKHR(context.device, swapchain.handle, &image_count, nullptr));
        images.resize(image_count);
        qz_vulkan_check(vkGetSwapchainImagesKHR(context.device, swapchain.handle, &image_count, images.data()));

        // Fill out VkImageViewCreateInfo struct, we need to form "references" to VkImage handles to access a specific
        // subresource or to swizzle (map) components around.
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.format = swapchain.format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        // Get ImageView handles to Swapchain Images.
        std::vector<VkImageView> views;
        views.reserve(image_count);
        for (const auto& image : images) {
            image_view_create_info.image = image;
            qz_vulkan_check(vkCreateImageView(context.device, &image_view_create_info, nullptr, &views.emplace_back()));
        }

        swapchain.images.reserve(image_count);
        for (std::size_t i = 0; i < image_count; ++i) {
            swapchain.images.emplace_back(Image{
                .handle = images[i],
                .view = views[i],
                .allocation = nullptr,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                .format = swapchain.format,
                .mips = 1,
                .width = swapchain.extent.width,
                .height = swapchain.extent.height
            });
        }

        return swapchain;
    }

    const Image& Swapchain::operator [](const std::size_t idx) const noexcept {
        return images[idx];
    }

    void Swapchain::destroy(const Context& context, Swapchain& swapchain) noexcept {
        for (const auto& image : swapchain.images) {
            vkDestroyImageView(context.device, image.view, nullptr);
        }
        vkDestroySwapchainKHR(context.device, swapchain.handle, nullptr);
        vkDestroySurfaceKHR(context.instance, swapchain.surface, nullptr);
        swapchain = {};
    }
} // namespace qz::gfx
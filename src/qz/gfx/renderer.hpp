#pragma once

#include <qz/gfx/command_buffer.hpp>
#include <qz/gfx/swapchain.hpp>
#include <qz/util/macros.hpp>
#include <qz/meta/types.hpp>
#include <qz/util/fwd.hpp>

#include <vector>
#include <mutex>

#include <vulkan/vulkan.h>

namespace qz::gfx {
    struct FrameInfo {
        std::uint32_t index;
        std::uint32_t image_idx;
        VkSemaphore img_ready;
        VkSemaphore gfx_done;
        VkFence cmd_wait;
        const Image* image;
    };

    struct Renderer {
        Swapchain swapchain;

        std::uint32_t image_idx;
        std::uint32_t frame_idx;

        meta::in_flight_array<CommandBuffer> gfx_cmds;
        meta::in_flight_array<VkSemaphore> img_ready;
        meta::in_flight_array<VkSemaphore> gfx_done;
        meta::in_flight_array<VkFence> cmd_wait;

        qz_nodiscard static Renderer create(const Context&, const Window&) noexcept;
        static void destroy(const Context&, Renderer&) noexcept;
    };

    qz_nodiscard std::pair<CommandBuffer, FrameInfo> acquire_next_frame(Renderer&, const Context&) noexcept;

    void present_frame(Renderer&, const Context&, const CommandBuffer&, const FrameInfo&) noexcept;
    void wait_queue(VkQueue) noexcept;
} // namespace qz::gfx
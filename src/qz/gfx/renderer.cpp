#include <qz/gfx/static_mesh.hpp>
#include <qz/gfx/render_pass.hpp>
#include <qz/gfx/swapchain.hpp>
#include <qz/gfx/renderer.hpp>
#include <qz/gfx/image.hpp>

namespace qz::gfx {
    qz_nodiscard Renderer Renderer::create(const Context& context, const Window& window) noexcept {
        Renderer renderer{};

        // Create swapchain.
        renderer.swapchain = Swapchain::create(context, window);

        // Allocate rendering command buffers.
        VkCommandBufferAllocateInfo command_buffer_allocate_info{};
        command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.commandPool = context.main_pool;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = meta::in_flight;

        meta::in_flight_array<VkCommandBuffer> command_buffers{};
        qz_vulkan_check(vkAllocateCommandBuffers(context.device, &command_buffer_allocate_info, &command_buffers[0]));

        for (std::uint32_t index = 0; const auto& each : command_buffers) {
            renderer.gfx_cmds[index++] = CommandBuffer::from_raw(context.main_pool, each);
        }

        // Fence info.
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // Semaphore info.
        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // Create all synchronization objects.
        for (std::size_t i = 0; i < meta::in_flight; ++i) {
            qz_vulkan_check(vkCreateSemaphore(context.device, &semaphore_create_info, nullptr, &renderer.img_ready[i]));
            qz_vulkan_check(vkCreateSemaphore(context.device, &semaphore_create_info, nullptr, &renderer.gfx_done[i]));
            qz_vulkan_check(vkCreateFence(context.device, &fence_create_info, nullptr, &renderer.cmd_wait[i]));
        }

        return renderer;
    }

    void Renderer::destroy(const Context& context, Renderer& renderer) noexcept {
        Swapchain::destroy(context, renderer.swapchain);

        for (std::size_t i = 0; i < meta::in_flight; ++i) {
            vkDestroySemaphore(context.device, renderer.img_ready[i], nullptr);
            vkDestroySemaphore(context.device, renderer.gfx_done[i], nullptr);
            vkDestroyFence(context.device, renderer.cmd_wait[i], nullptr);
        }
    }

    qz_nodiscard std::pair<CommandBuffer, FrameInfo> acquire_next_frame(Renderer& renderer, const Context& context) noexcept {
        qz_vulkan_check(vkAcquireNextImageKHR(context.device, renderer.swapchain.handle, -1, renderer.img_ready[renderer.frame_idx], nullptr, &renderer.image_idx));
        qz_vulkan_check(vkWaitForFences(context.device, 1, &renderer.cmd_wait[renderer.frame_idx], true, -1));

        return { renderer.gfx_cmds[renderer.frame_idx], {
            renderer.frame_idx,
            renderer.image_idx,
            renderer.img_ready[renderer.frame_idx],
            renderer.gfx_done[renderer.frame_idx],
            renderer.cmd_wait[renderer.frame_idx],
            &renderer.swapchain[renderer.image_idx]
        } };
    }

    void present_frame(Renderer& renderer, const Context& context, const CommandBuffer& command_buffer, const FrameInfo& frame) noexcept {
        VkPipelineStageFlags wait_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pWaitDstStageMask = &wait_mask;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &frame.img_ready;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = command_buffer.ptr_handle();
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &frame.gfx_done;
        qz_vulkan_check(vkResetFences(context.device, 1, &frame.cmd_wait));
        qz_vulkan_check(vkQueueSubmit(context.graphics, 1, &submit_info, frame.cmd_wait));

        VkResult present_result{};
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &frame.gfx_done;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &renderer.swapchain.handle;
        present_info.pImageIndices = &frame.image_idx;
        present_info.pResults = &present_result;
        qz_vulkan_check(vkQueuePresentKHR(context.graphics, &present_info));
        qz_vulkan_check(present_result);

        renderer.frame_idx = (renderer.frame_idx + 1) % meta::in_flight;
    }

    void wait_queue(VkQueue queue) noexcept {
        qz_vulkan_check(vkQueueWaitIdle(queue));
    }
} // namespace qz::gfx

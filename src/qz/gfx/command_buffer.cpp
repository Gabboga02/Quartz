#include <qz/gfx/command_buffer.hpp>
#include <qz/gfx/static_mesh.hpp>
#include <qz/gfx/render_pass.hpp>
#include <qz/gfx/pipeline.hpp>
#include <qz/gfx/context.hpp>
#include <qz/gfx/buffer.hpp>

namespace qz::gfx {
    qz_nodiscard CommandBuffer CommandBuffer::allocate(const Context& context,  VkCommandPool command_pool) noexcept {
        VkCommandBuffer command_buffer{};
        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        qz_vulkan_check(vkAllocateCommandBuffers(context.device, &allocate_info, &command_buffer));

        return from_raw(command_pool, command_buffer);
    }

    CommandBuffer CommandBuffer::from_raw(VkCommandPool command_pool, VkCommandBuffer handle) noexcept {
        CommandBuffer command_buffer{};
        command_buffer._handle = handle;
        command_buffer._pool = command_pool;

        return command_buffer;
    }

    void CommandBuffer::destroy(const Context& context, CommandBuffer& command_buffer) noexcept {
        vkFreeCommandBuffers(context.device, command_buffer._pool, 1, &command_buffer._handle);
        command_buffer = {};
    }

    qz_nodiscard VkCommandBuffer CommandBuffer::handle() const noexcept {
        return _handle;
    }

    qz_nodiscard VkCommandBuffer* CommandBuffer::ptr_handle() noexcept {
        return &_handle;
    }

    qz_nodiscard const VkCommandBuffer* CommandBuffer::ptr_handle() const noexcept {
        return &_handle;
    }

    CommandBuffer& CommandBuffer::begin() noexcept {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        qz_vulkan_check(vkBeginCommandBuffer(_handle, &begin_info));
        return *this;
    }

    CommandBuffer& CommandBuffer::begin_render_pass(const RenderPass& render_pass, const std::size_t framebuffer) noexcept {
        _active_pass = &render_pass;
        const auto clear_values = render_pass.clears();

        VkRenderPassBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.renderPass = render_pass.handle();
        begin_info.framebuffer = render_pass.framebuffer(framebuffer);
        begin_info.renderArea.extent = render_pass.extent();
        begin_info.clearValueCount = clear_values.size();
        begin_info.pClearValues = clear_values.data();
        vkCmdBeginRenderPass(_handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        return *this;
    }

    CommandBuffer& CommandBuffer::set_viewport(meta::viewport_tag_t) noexcept {
        const auto extent = _active_pass->extent();
        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = extent.width;
        viewport.height = extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(_handle, 0, 1, &viewport);
        return *this;
    }

    CommandBuffer& CommandBuffer::set_viewport(VkViewport viewport) noexcept {
        vkCmdSetViewport(_handle, 0, 1, &viewport);
        return *this;
    }

    CommandBuffer& CommandBuffer::set_scissor(meta::scissor_tag_t) noexcept {
        const auto extent = _active_pass->extent();
        VkRect2D scissor{ {}, extent };
        vkCmdSetScissor(_handle, 0, 1, &scissor);
        return *this;
    }

    CommandBuffer& CommandBuffer::set_scissor(VkRect2D scissor) noexcept {
        vkCmdSetScissor(_handle, 0, 1, &scissor);
        return *this;
    }

    CommandBuffer& CommandBuffer::bind_pipeline(const Pipeline& pipeline) noexcept {
        vkCmdBindPipeline(_handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
        return *this;
    }

    CommandBuffer& CommandBuffer::bind_vertex_buffer(const Buffer& vertex) noexcept {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(_handle, 0, 1, &vertex.handle, &offset);
        return *this;
    }

    CommandBuffer& CommandBuffer::bind_index_buffer(const Buffer& index) noexcept {
        vkCmdBindIndexBuffer(_handle, index.handle, 0, VK_INDEX_TYPE_UINT32);
        return *this;
    }

    CommandBuffer& CommandBuffer::bind_static_mesh(const StaticMesh& mesh) noexcept {
        bind_vertex_buffer(mesh.geometry);
        bind_index_buffer(mesh.indices);
        return *this;
    }

    CommandBuffer& CommandBuffer::draw(const std::uint32_t vertices,
                                       const std::uint32_t instances,
                                       const std::uint32_t first_vertex,
                                       const std::uint32_t first_instance) noexcept {
        vkCmdDraw(_handle, vertices, instances, first_vertex, first_instance);
        return *this;
    }

    CommandBuffer& CommandBuffer::draw_indexed(const std::uint32_t indices,
                                const std::uint32_t instances,
                                const std::uint32_t first_index,
                                const std::uint32_t first_instance) noexcept {
        vkCmdDrawIndexed(_handle, indices, instances, first_index, 0, first_instance);
        return *this;
    }

    CommandBuffer& CommandBuffer::end_render_pass() noexcept {
        qz_assert(_active_pass, "No active renderpass at end_render_pass()");
        _active_pass = nullptr;
        vkCmdEndRenderPass(_handle);
        return *this;
    }

    CommandBuffer& CommandBuffer::copy_image(const Image& source, const Image& dest) noexcept {
        VkImageCopy image_region{};
        image_region.srcSubresource = {
            .aspectMask = source.aspect,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        image_region.srcOffset = {};
        image_region.dstSubresource = {
            .aspectMask = dest.aspect,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        image_region.dstOffset = {};
        image_region.extent = { source.width, source.height, 1 };

        vkCmdCopyImage(_handle,
            source.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dest.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &image_region);
        return *this;
    }

    CommandBuffer& CommandBuffer::copy_buffer(const Buffer& source, const Buffer& dest) noexcept {
        VkBufferCopy buffer_region{};
        buffer_region.size = source.capacity;
        buffer_region.srcOffset = 0;
        buffer_region.dstOffset = 0;
        vkCmdCopyBuffer(_handle, source.handle, dest.handle, 1, &buffer_region);
        return *this;
    }

    CommandBuffer& CommandBuffer::insert_layout_transition(const ImageMemoryBarrier& info) noexcept {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = info.source_access;
        barrier.dstAccessMask = info.dest_access;
        barrier.oldLayout = info.old_layout;
        barrier.newLayout = info.new_layout;
        barrier.srcQueueFamilyIndex = info.source_family;
        barrier.dstQueueFamilyIndex = info.dest_family;
        barrier.image = info.image->handle;
        barrier.subresourceRange = {
            .aspectMask = info.image->aspect,
            .baseMipLevel = 0,
            .levelCount = info.image->mips,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        vkCmdPipelineBarrier(
            _handle,
            info.source_stage,
            info.dest_stage,
            VkDependencyFlags{},
            0, nullptr,
            0, nullptr,
            1, &barrier);

        return *this;
    }

    void CommandBuffer::end() noexcept {
        qz_vulkan_check(vkEndCommandBuffer(_handle));
    }
} // namespace qz::gfx
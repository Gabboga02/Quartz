#include <qz/gfx/command_buffer.hpp>
#include <qz/gfx/static_mesh.hpp>
#include <qz/gfx/context.hpp>
#include <qz/gfx/assets.hpp>

#include <qz/task/scheduler.hpp>
#include <qz/meta/types.hpp>

#include <cstring>

namespace qz::gfx {
    struct TaskData {
        const Context* context;
        meta::Handle<StaticMesh> handle;
        std::vector<float> vertices;
        std::vector<std::uint32_t> indices;
    };

    qz_nodiscard meta::Handle<StaticMesh> request_static_mesh(const Context& context, StaticMesh::CreateInfo&& info) noexcept {
        const auto result = assets::emplace_empty<StaticMesh>();

        auto task_data = new TaskData{
            &context,
            result,
            std::move(info.geometry),
            std::move(info.indices),
        };

        task::get_scheduler().AddTask(ftl::Task{
            .Function = +[](ftl::TaskScheduler* scheduler, void* ptr) {
                const auto data = reinterpret_cast<TaskData*>(ptr);
                const auto thread_index = scheduler->GetCurrentThreadIndex();
                auto command_buffer = CommandBuffer::allocate(*data->context, task::get_command_pool(thread_index));

                auto vertex_staging = Buffer::create(*data->context, {
                    .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    .usage = VMA_MEMORY_USAGE_CPU_ONLY,
                    .capacity = data->vertices.size() * sizeof(float)
                });
                std::memcpy(vertex_staging.mapped, data->vertices.data(), vertex_staging.capacity);

                auto index_staging = Buffer::create(*data->context, {
                    .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    .usage = VMA_MEMORY_USAGE_CPU_ONLY,
                    .capacity = data->indices.size() * sizeof(std::uint32_t)
                });
                std::memcpy(index_staging.mapped, data->indices.data(), index_staging.capacity);

                auto geometry = Buffer::create(*data->context, {
                    .flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .capacity = vertex_staging.capacity
                });
                auto indices = Buffer::create(*data->context, {
                    .flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .capacity = index_staging.capacity
                });

                command_buffer
                    .begin()
                        .copy_buffer(vertex_staging, geometry)
                        .copy_buffer(index_staging, indices)
                    .end();

                VkFence done{};
                VkFenceCreateInfo fence_create_info{};
                fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                qz_vulkan_check(vkCreateFence(data->context->device, &fence_create_info, nullptr, &done));

                VkSubmitInfo submit_info{};
                submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.pWaitDstStageMask = nullptr;
                submit_info.waitSemaphoreCount = 0;
                submit_info.pWaitSemaphores = nullptr;
                submit_info.commandBufferCount = 1;
                submit_info.pCommandBuffers = command_buffer.ptr_handle();
                submit_info.signalSemaphoreCount = 0;
                submit_info.pSignalSemaphores = nullptr;

                {
                    std::lock_guard<std::mutex> lock(task::get_transfer_mutex());
                    qz_vulkan_check(vkQueueSubmit(data->context->transfer, 1, &submit_info, done));
                    while (vkGetFenceStatus(data->context->device, done) != VK_SUCCESS);
                    vkResetFences(data->context->device, 1, &done);
                    vkDestroyFence(data->context->device, done, nullptr);
                }
                assets::from_handle(data->handle) = {
                    geometry,
                    indices
                };
                assets::finalize(data->handle);
                Buffer::destroy(*data->context, vertex_staging);
                Buffer::destroy(*data->context, index_staging);
                CommandBuffer::destroy(*data->context, command_buffer);
                delete data;
            },
            .ArgData = task_data
        }, ftl::TaskPriority::High);
        return result;
    }
} // namespace qz::gfx
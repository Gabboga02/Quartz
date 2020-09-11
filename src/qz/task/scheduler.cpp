#include <qz/task/scheduler.hpp>
#include <qz/gfx/context.hpp>

namespace qz::task {
    static std::vector<VkCommandPool> command_pools;
    static ftl::TaskScheduler scheduler;
    static std::mutex transfer_mutex;

    void initialize_scheduler(const gfx::Context& context) noexcept {
        scheduler.Init({
            .Behavior = ftl::EmptyQueueBehavior::Sleep
        });

        VkCommandPoolCreateInfo command_pool_create_info{};
        command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        command_pool_create_info.queueFamilyIndex = context.family;
        command_pools.reserve(scheduler.GetThreadCount());
        for (std::size_t i = 0; i < scheduler.GetThreadCount(); ++i) {
            qz_vulkan_check(vkCreateCommandPool(context.device, &command_pool_create_info, nullptr, &command_pools.emplace_back()));
        }
    }

    qz_nodiscard ftl::TaskScheduler& get_scheduler() noexcept {
        return scheduler;
    }

    qz_nodiscard VkCommandPool get_command_pool(const std::size_t index) noexcept {
        return command_pools[index];
    }

    qz_nodiscard std::mutex& get_transfer_mutex() noexcept {
        return transfer_mutex;
    }

    void destroy_scheduler(const gfx::Context& context) noexcept {
        for (const auto& each : command_pools) {
            vkDestroyCommandPool(context.device, each, nullptr);
        }
    }
} // namespace qz::task
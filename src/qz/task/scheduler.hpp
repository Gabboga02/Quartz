#pragma once

#include <qz/util/macros.hpp>
#include <qz/util/fwd.hpp>

#include <ftl/task_scheduler.h>
#include <vulkan/vulkan.h>

namespace qz::task {
    void initialize_scheduler(const gfx::Context&) noexcept;
    qz_nodiscard ftl::TaskScheduler& get_scheduler() noexcept;
    qz_nodiscard VkCommandPool get_command_pool(std::size_t) noexcept;
    qz_nodiscard std::mutex& get_transfer_mutex() noexcept;
    void destroy_scheduler(const gfx::Context&) noexcept;
} // namespace qz::task
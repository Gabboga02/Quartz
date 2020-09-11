#pragma once

#include <qz/util/macros.hpp>
#include <qz/util/fwd.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace qz::gfx {
    enum class VertexAttribute : std::uint32_t {
        vec1 = sizeof(float[1]),
        vec2 = sizeof(float[2]),
        vec3 = sizeof(float[3]),
        vec4 = sizeof(float[4])
    };

    struct Pipeline {
        struct CreateInfo {
            const char* vertex;
            const char* fragment;
            std::vector<VertexAttribute> attributes;
            std::vector<VkDynamicState> states;
            VkRenderPass render_pass;
            std::uint32_t subpass;
        };
        VkPipeline handle;
        VkPipelineLayout layout;

        qz_nodiscard static Pipeline create(const Context&, CreateInfo&&) noexcept;
        static void destroy(const Context&, Pipeline&) noexcept;
    };
} // namespace qz::gfx
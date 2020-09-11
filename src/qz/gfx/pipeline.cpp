#include <qz/gfx/pipeline.hpp>
#include <qz/gfx/context.hpp>

#include <spirv.hpp>
#include <spirv_glsl.hpp>

#include <type_traits>
#include <numeric>
#include <fstream>
#include <cstring>
#include <string>

namespace qz::gfx {
    qz_nodiscard static std::vector<char> load_spirv_code(const char* path) noexcept {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        qz_assert(file.is_open(), "Failed to open file");

        std::vector<char> spirv(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(spirv.data(), spirv.size());
        return spirv;
    }

    qz_nodiscard Pipeline Pipeline::create(const Context& context, CreateInfo&& info) noexcept {
        VkPipelineShaderStageCreateInfo pipeline_stages[2] = {};
        pipeline_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_stages[0].pName = "main";

        pipeline_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_stages[1].pName = "main";

        { // Vertex shader.
            const auto binary = load_spirv_code(info.vertex);

            // Fill VkShaderModuleCreateInfo struct, give it a pointer and size of the spirv code.
            VkShaderModuleCreateInfo module_create_info{};
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.codeSize = binary.size();
            module_create_info.pCode = (const uint32_t*)binary.data();
            qz_vulkan_check(vkCreateShaderModule(context.device, &module_create_info, nullptr, &pipeline_stages[0].module));
        }

        std::vector<VkPipelineColorBlendAttachmentState> attachment_outputs;
        { // Fragment shader.
            const auto binary = load_spirv_code(info.fragment);
            const auto compiler = spirv_cross::CompilerGLSL((const std::uint32_t*)binary.data(), binary.size() / sizeof(std::uint32_t));
            const auto resources = compiler.get_shader_resources();

            VkShaderModuleCreateInfo module_create_info{};
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.codeSize = binary.size();
            module_create_info.pCode = (const uint32_t*)binary.data();
            qz_vulkan_check(vkCreateShaderModule(context.device, &module_create_info, nullptr, &pipeline_stages[1].module));

            attachment_outputs.reserve(resources.stage_outputs.size());
            for (const auto& output : resources.stage_outputs) {
                attachment_outputs.push_back({
                    .blendEnable = true,
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask =
                        VK_COLOR_COMPONENT_R_BIT |
                        VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT
                });
            }
        }

        // VkPipelineDynamicStateCreateInfo, Used to enable some dynamic pipeline states (such as Viewport or Scissor).
        VkPipelineDynamicStateCreateInfo pipeline_dynamic_states{};
        pipeline_dynamic_states.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pipeline_dynamic_states.dynamicStateCount = info.states.size();
        pipeline_dynamic_states.pDynamicStates = info.states.data();

        VkVertexInputBindingDescription vertex_binding_description{};
        vertex_binding_description.binding = 0;
        vertex_binding_description.stride =
            std::accumulate(info.attributes.begin(), info.attributes.end(), 0u, [](const auto value, const auto attribute) {
                return value + static_cast<std::underlying_type_t<decltype(attribute)>>(attribute);
            });
        vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions{};
        for (std::uint32_t location = 0, offset = 0; const auto each : info.attributes) {
            vertex_attribute_descriptions.push_back({
                .location = location++,
                .binding = 0,
                .format = [each]() noexcept {
                    switch (each) {
                        case VertexAttribute::vec1: return VK_FORMAT_R32_SFLOAT;
                        case VertexAttribute::vec2: return VK_FORMAT_R32G32_SFLOAT;
                        case VertexAttribute::vec3: return VK_FORMAT_R32G32B32_SFLOAT;
                        case VertexAttribute::vec4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                    }
                    return VK_FORMAT_UNDEFINED;
                }(),
                .offset = offset
            });
            offset += static_cast<std::underlying_type_t<decltype(each)>>(each);
        }

        // VkPipelineVertexInputStateCreateInfo, Used to specify input vertex format of a shader.
        VkPipelineVertexInputStateCreateInfo vertex_input_state{};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state.vertexBindingDescriptionCount = 1;
        vertex_input_state.pVertexBindingDescriptions = &vertex_binding_description;
        vertex_input_state.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
        vertex_input_state.pVertexAttributeDescriptions = vertex_attribute_descriptions.data();

        // VkPipelineInputAssemblyStateCreateInfo, Used to specify the topology of our vertices.
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
        input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state.primitiveRestartEnable = false;

        VkViewport viewport{};
        VkRect2D scissor{};

        // VkPipelineViewportStateCreateInfo, Specifies static pipeline viewport and scissor (we'll set those states dynamically).
        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        // VkPipelineRasterizationStateCreateInfo, Various informations about our geometry and the rasterizer's behaviour.
        VkPipelineRasterizationStateCreateInfo rasterizer_state{};
        rasterizer_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_state.depthClampEnable = false;
        rasterizer_state.rasterizerDiscardEnable = false;
        rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_state.lineWidth = 1.0f;
        rasterizer_state.cullMode = VK_CULL_MODE_NONE;
        rasterizer_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer_state.depthBiasEnable = false;
        rasterizer_state.depthBiasConstantFactor = 0.0f;
        rasterizer_state.depthBiasClamp = 0.0f;
        rasterizer_state.depthBiasSlopeFactor = 0.0f;

        // VkPipelineMultisampleStateCreateInfo, multisampling.
        VkPipelineMultisampleStateCreateInfo multisampling_state{};
        multisampling_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_state.sampleShadingEnable = false;
        multisampling_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_state.minSampleShading = 1.0f;
        multisampling_state.pSampleMask = nullptr;
        multisampling_state.alphaToCoverageEnable = false;
        multisampling_state.alphaToOneEnable = false;

        // VkPipelineDepthStencilStateCreateInfo, used to enable depth (and stencil) writing.
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
        depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state.depthTestEnable = false;
        depth_stencil_state.depthWriteEnable = false;
        depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_stencil_state.depthBoundsTestEnable = false;
        depth_stencil_state.stencilTestEnable = false;
        depth_stencil_state.front = {};
        depth_stencil_state.back = {};
        depth_stencil_state.minDepthBounds = 0.0f;
        depth_stencil_state.maxDepthBounds = 1.0f;

        // VkPipelineColorBlendStateCreateInfo, References all the attachments and allows to specify blend constants.
        VkPipelineColorBlendStateCreateInfo color_blend_state{};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.logicOpEnable = false;
        color_blend_state.logicOp = VK_LOGIC_OP_NO_OP;
        color_blend_state.attachmentCount = attachment_outputs.size();
        color_blend_state.pAttachments = attachment_outputs.data();
        color_blend_state.blendConstants[0] = 0.0f;
        color_blend_state.blendConstants[1] = 0.0f;
        color_blend_state.blendConstants[2] = 0.0f;
        color_blend_state.blendConstants[3] = 0.0f;

        // VkPipelineLayoutCreateInfo, Description of our shader's resources layout (uniform buffers and whatnot).
        VkPipelineLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 0;
        layout_create_info.pSetLayouts = nullptr;
        layout_create_info.pushConstantRangeCount = 0;
        layout_create_info.pPushConstantRanges = nullptr;

        // Create pipeline layout
        VkPipelineLayout layout;
        qz_vulkan_check(vkCreatePipelineLayout(context.device, &layout_create_info, nullptr, &layout));

        // Finally, VkGraphicsPipelineCreateInfo, Uses all the informations we gathered so far.
        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = 2;
        pipeline_create_info.pStages = pipeline_stages;
        pipeline_create_info.pVertexInputState = &vertex_input_state;
        pipeline_create_info.pInputAssemblyState = &input_assembly_state;
        pipeline_create_info.pViewportState = &viewport_state;
        pipeline_create_info.pRasterizationState = &rasterizer_state;
        pipeline_create_info.pMultisampleState = &multisampling_state;
        pipeline_create_info.pDepthStencilState = &depth_stencil_state;
        pipeline_create_info.pColorBlendState = &color_blend_state;
        pipeline_create_info.pDynamicState = &pipeline_dynamic_states;
        pipeline_create_info.layout = layout;
        pipeline_create_info.renderPass = info.render_pass;
        pipeline_create_info.subpass = info.subpass;
        pipeline_create_info.basePipelineHandle = nullptr;
        pipeline_create_info.basePipelineIndex = -1;

        // Create pipeline.
        VkPipeline pipeline;
        qz_vulkan_check(vkCreateGraphicsPipelines(context.device, nullptr, 1, &pipeline_create_info, nullptr, &pipeline));
        vkDestroyShaderModule(context.device, pipeline_stages[0].module, nullptr);
        vkDestroyShaderModule(context.device, pipeline_stages[1].module, nullptr);

        return { pipeline, layout };
    }

    void Pipeline::destroy(const Context& context, Pipeline& pipeline) noexcept {
        vkDestroyPipeline(context.device, pipeline.handle, nullptr);
        vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
        pipeline = {};
    }
} // namespace qz::gfx

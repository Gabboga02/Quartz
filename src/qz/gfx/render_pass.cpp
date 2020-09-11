#include <qz/gfx/render_pass.hpp>
#include <qz/gfx/context.hpp>

#include <optional>

namespace qz::gfx {
    qz_nodiscard static VkImageLayout deduce_reference_layout(const VkImageAspectFlags aspect) noexcept {
        switch (aspect) {
            case VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case VK_IMAGE_ASPECT_COLOR_BIT:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case VK_IMAGE_ASPECT_DEPTH_BIT:
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case VK_IMAGE_ASPECT_STENCIL_BIT:
                return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        }

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    qz_nodiscard RenderPass RenderPass::create(const Context& context, CreateInfo&& info) noexcept {
        RenderPass render_pass;

        VkExtent2D framebuffer_size;
        std::vector<VkAttachmentDescription> attachments;
        attachments.reserve(info.attachments.size());
        for (std::uint32_t index = 0; auto&& each : info.attachments) {
            Attachment attachment{};
            attachment.image = each.image;
            attachment.owning = each.owning;
            attachment.framebuffer = each.framebuffer;
            attachment.name = std::move(each.name);
            attachment.clear = each.clear;
            attachment.description = {
                .flags = {},
                .format = attachment.image.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = each.clear.type() != ClearValue::none ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = each.discard ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = (attachment.image.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) && each.clear.type() == ClearValue::depth ?
                                 VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = (attachment.image.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) && each.discard ?
                                  VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = each.layout,
            };
            attachment.reference = {
                .attachment = index++,
                .layout = deduce_reference_layout(attachment.image.aspect)
            };

            attachments.emplace_back(attachment.description);
            render_pass._attachments.emplace_back(attachment);
            framebuffer_size = { attachment.image.width, attachment.image.height };
        }

        struct SubpassStorage {
            std::optional<VkAttachmentReference> depth_attachment;
            std::vector<VkAttachmentReference> color_attachments;
            std::vector<VkAttachmentReference> input_attachments;
            std::vector<std::uint32_t> preserve_attachments;
        };

        std::vector<SubpassStorage> subpass_storage;
        std::vector<VkSubpassDescription> subpasses;

        subpass_storage.reserve(info.subpasses.size());
        subpasses.reserve(info.subpasses.size());
        for (auto&& each : info.subpasses) {
            auto& storage = subpass_storage.emplace_back();

            for (const auto& name : each.attachments) {
                auto attachment = render_pass.attachment(name);

                if (attachment.image.aspect & (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT)) {
                    storage.depth_attachment = attachment.reference;
                } else {
                    storage.color_attachments.emplace_back(attachment.reference);
                }
            }

            for (const auto& name : each.input) {
                storage.input_attachments.emplace_back(render_pass.attachment(name).reference);
            }

            for (const auto& name : each.preserve) {
                storage.preserve_attachments.emplace_back(render_pass.attachment(name).reference.attachment);
            }

            VkSubpassDescription subpass_description{};
            subpass_description.flags = {};
            subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass_description.inputAttachmentCount = storage.input_attachments.size();
            subpass_description.pInputAttachments = storage.input_attachments.data();
            subpass_description.colorAttachmentCount = storage.color_attachments.size();
            subpass_description.pColorAttachments = storage.color_attachments.data();
            subpass_description.pResolveAttachments = nullptr;
            subpass_description.pDepthStencilAttachment = storage.depth_attachment ? &*storage.depth_attachment : nullptr;
            subpass_description.preserveAttachmentCount = storage.preserve_attachments.size();
            subpass_description.pPreserveAttachments = storage.preserve_attachments.data();
            subpasses.emplace_back(subpass_description);
        }

        std::vector<VkSubpassDependency> dependencies;
        dependencies.reserve(info.dependencies.size());
        for (auto&& each : info.dependencies) {
            VkSubpassDependency dependency{};
            dependency.srcSubpass = each.source_subpass;
            dependency.dstSubpass = each.dest_subpass;
            dependency.srcStageMask = each.source_stage;
            dependency.dstStageMask = each.dest_stage;
            dependency.srcAccessMask = each.source_access;
            dependency.dstAccessMask = each.dest_access;
            dependency.dependencyFlags = {};

            dependencies.emplace_back(dependency);
        }

        VkRenderPassCreateInfo render_pass_create_info{};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.flags = {};
        render_pass_create_info.attachmentCount = attachments.size();
        render_pass_create_info.pAttachments = attachments.data();
        render_pass_create_info.subpassCount = subpasses.size();
        render_pass_create_info.pSubpasses = subpasses.data();
        render_pass_create_info.dependencyCount = dependencies.size();
        render_pass_create_info.pDependencies = dependencies.data();
        qz_vulkan_check(vkCreateRenderPass(context.device, &render_pass_create_info, nullptr, &render_pass._handle));

        std::vector<std::vector<VkImageView>> attachment_views(1);
        for (const auto& each : render_pass._attachments) {
            if (attachment_views.size() < each.framebuffer) {
                attachment_views.resize(each.framebuffer);
            }
            attachment_views[each.framebuffer].emplace_back(each.image.view);
        }

        render_pass._framebuffers.reserve(attachment_views.size());
        for (const auto& each : attachment_views) {
            VkFramebufferCreateInfo framebuffer_create_info{};
            framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.flags = {};
            framebuffer_create_info.renderPass = render_pass._handle;
            framebuffer_create_info.attachmentCount = each.size();
            framebuffer_create_info.pAttachments = each.data();
            framebuffer_create_info.width = framebuffer_size.width;
            framebuffer_create_info.height = framebuffer_size.height;
            framebuffer_create_info.layers = 1;
            qz_vulkan_check(vkCreateFramebuffer(context.device, &framebuffer_create_info, nullptr, &render_pass._framebuffers.emplace_back()));
        }

        return render_pass;
    }

    void RenderPass::destroy(const Context& context, RenderPass& render_pass) noexcept {
        for (auto& each : render_pass._attachments) {
            if (each.owning) {
                Image::destroy(context, each.image);
            }
        }
        render_pass._attachments = {};

        for (const auto& framebuffer : render_pass._framebuffers) {
            vkDestroyFramebuffer(context.device, framebuffer, nullptr);
        }
        render_pass._framebuffers = {};

        if (render_pass._handle) {
            vkDestroyRenderPass(context.device,render_pass. _handle, nullptr);
        }
        render_pass._handle = nullptr;
    }

    qz_nodiscard Attachment& RenderPass::attachment(const std::string_view name) noexcept {
        for (auto& attachment : _attachments) {
            if (attachment.name == name) {
                return attachment;
            }
        }
        qz_force_assert("Attachment not found");
    }

    qz_nodiscard const Attachment& RenderPass::attachment(const std::string_view name) const noexcept {
        for (const auto& attachment : _attachments) {
            if (attachment.name == name) {
                return attachment;
            }
        }
        qz_force_assert("Attachment not found");
    }

    qz_nodiscard VkFramebuffer& RenderPass::framebuffer(const std::size_t idx) noexcept {
        qz_assert(0 <= idx && idx < _framebuffers.size(), "Framebuffer index not in range");
        return _framebuffers[idx];
    }

    qz_nodiscard const VkFramebuffer& RenderPass::framebuffer(const std::size_t idx) const noexcept {
        qz_assert(0 <= idx && idx < _framebuffers.size(), "Framebuffer index not in range");
        return _framebuffers[idx];
    }

    qz_nodiscard VkExtent2D RenderPass::extent() const noexcept {
        const auto& attachment = _attachments[0];
        return {
            attachment.image.width,
            attachment.image.height
        };
    }

    qz_nodiscard std::vector<VkClearValue> RenderPass::clears() const noexcept {
        std::vector<VkClearValue> result;
        result.reserve(_attachments.size());
        for (const auto& each : _attachments) {
            result.emplace_back(each.clear.value());
        }

        return result;
    }

    VkRenderPass RenderPass::handle() const noexcept {
        return _handle;
    }
} // namespace qz::gfx
#include <qz/gfx/render_pass.hpp>
#include <qz/gfx/static_mesh.hpp>
#include <qz/gfx/pipeline.hpp>
#include <qz/gfx/renderer.hpp>
#include <qz/gfx/context.hpp>
#include <qz/gfx/window.hpp>
#include <qz/gfx/assets.hpp>

#include <qz/meta/constants.hpp>
#include <qz/task/scheduler.hpp>

#include <cmath>

int main() {
    using namespace qz;

    gfx::initialize_window_system();
    auto window = gfx::Window::create(1280, 720, "QuartzVk");
    auto context = gfx::Context::create();
    auto renderer = gfx::Renderer::create(context, window);
    task::initialize_scheduler(context);

    auto render_pass = gfx::RenderPass::create(context, {
        .attachments = { {
            .image = gfx::Image::create(context, {
                .width = 1280,
                .height = 720,
                .mips = 1,
                .format = renderer.swapchain.format,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            }),
            .name = "color",
            .framebuffer = 0,
            .owning = true,
            .discard = false,
            .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .clear = gfx::ClearColor{}
        } },
        .subpasses = { {
            .attachments = {
                "color"
            }
        } },
        .dependencies = { {
            .source_subpass = meta::external_subpass,
            .dest_subpass = 0,
            .source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dest_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .source_access = {},
            .dest_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        } }
    });

    auto pipeline = gfx::Pipeline::create(context, {
        .vertex = "../data/shaders/shader.vert.spv",
        .fragment = "../data/shaders/shader.frag.spv",
        .attributes = {
            gfx::VertexAttribute::vec3,
            gfx::VertexAttribute::vec3,
        },
        .states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        },
        .render_pass = render_pass.handle(),
        .subpass = 0
    });

    const auto triangle = gfx::request_static_mesh(context, {
        .geometry = {
            -1.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
             0.0f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f
        },
        .indices = {
            0, 1, 2
        }
    });

    const auto quad = gfx::request_static_mesh(context, {
        .geometry = {
            0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
            1.0f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
            1.0f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f
        },
        .indices = {
            0, 1, 2,
            1, 2, 3
        }
    });

    for (int i = 0; i < 1024; ++i) {
        (void)gfx::request_static_mesh(context, {
            .geometry = {
                0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
                1.0f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
                1.0f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f
            },
            .indices = {
                0, 1, 2,
                1, 2, 3
            }
        });
    }

    double delta_time = 0;
    double last_frame = 0;
    while (!window.should_close()) {
        auto [command_buffer, frame] = gfx::acquire_next_frame(renderer, context);

        const auto current_frame = gfx::get_time();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        gfx::ImageMemoryBarrier transfer_transition{};
        transfer_transition.image = frame.image;
        transfer_transition.source_family = meta::family_ignored;
        transfer_transition.dest_family = meta::family_ignored;
        transfer_transition.source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        transfer_transition.dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        transfer_transition.source_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        transfer_transition.dest_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        transfer_transition.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        transfer_transition.new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        gfx::ImageMemoryBarrier present_transition{};
        present_transition.image = frame.image;
        present_transition.source_family = meta::family_ignored;
        present_transition.dest_family = meta::family_ignored;
        present_transition.source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        present_transition.dest_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        present_transition.source_access = VK_ACCESS_TRANSFER_WRITE_BIT;
        present_transition.dest_access = {};
        present_transition.old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        present_transition.new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        command_buffer
            .begin()
            .begin_render_pass(render_pass, 0)
            .set_viewport(meta::full_viewport)
            .set_scissor(meta::full_scissor)
            .bind_pipeline(pipeline);

        if (assets::is_ready(triangle)) {
            command_buffer
                .bind_static_mesh(assets::from_handle(triangle))
                .draw_indexed(3, 1, 0, 0);
        }

        if (assets::is_ready(quad)) {
            command_buffer
                .bind_static_mesh(assets::from_handle(quad))
                .draw_indexed(6, 1, 0, 0);
        }

        command_buffer
            .end_render_pass()
            .insert_layout_transition(transfer_transition)
            .copy_image(render_pass.attachment("color").image, *frame.image)
            .insert_layout_transition(present_transition)
            .end();

        gfx::present_frame(renderer, context, command_buffer, frame);
        gfx::poll_events();
    }
    gfx::wait_queue(context.graphics);
    assets::free_all_resources(context);
    task::destroy_scheduler(context);

    gfx::Pipeline::destroy(context, pipeline);
    gfx::RenderPass::destroy(context, render_pass);

    gfx::Renderer::destroy(context, renderer);
    gfx::Context::destroy(context);
    gfx::Window::destroy(window);

    gfx::terminate_window_system();
    return 0;
}

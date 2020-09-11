#pragma once

#include <qz/util/macros.hpp>
#include <qz/util/fwd.hpp>

#include <cstdint>

namespace qz::gfx {
    struct Window {
    private:
        GLFWwindow* _handle;
        std::uint32_t _width;
        std::uint32_t _height;
        const char* _title;

        Window(GLFWwindow*, std::uint32_t, std::uint32_t, const char*) noexcept;
    public:
        qz_nodiscard static Window create(std::uint32_t, std::uint32_t, const char*) noexcept;
        static void destroy(Window&) noexcept;

        Window() noexcept = default;

        qz_nodiscard bool should_close() const noexcept;
        qz_nodiscard GLFWwindow* handle() const noexcept;
        qz_nodiscard std::uint32_t width() const noexcept;
        qz_nodiscard std::uint32_t height() const noexcept;
    };

    void initialize_window_system() noexcept;
    qz_nodiscard double get_time() noexcept;
    void poll_events() noexcept;
    void terminate_window_system() noexcept;
} // namespace qz::gfx
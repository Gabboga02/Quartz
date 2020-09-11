#pragma once

#include <qz/util/macros.hpp>
#include <qz/meta/types.hpp>

namespace qz::assets {
    template <typename T>
    qz_nodiscard meta::Handle<T> emplace_empty() noexcept;

    template <typename T>
    qz_nodiscard T& from_handle(meta::Handle<T>) noexcept;

    template <typename T>
    void finalize(meta::Handle<T>) noexcept;

    template <typename T>
    bool is_ready(meta::Handle<T>) noexcept;

    void free_all_resources(const gfx::Context&) noexcept;
} // namespace qz::assets
#pragma once

#include <qz/util/macros.hpp>
#include <qz/util/fwd.hpp>

#include <qz/gfx/buffer.hpp>

#include <cstdint>
#include <vector>

namespace qz::gfx {
    struct StaticMesh {
        struct CreateInfo {
            std::vector<float> geometry;
            std::vector<std::uint32_t> indices;
        };
        Buffer geometry;
        Buffer indices;
    };

    qz_nodiscard meta::Handle<StaticMesh> request_static_mesh(const Context&, StaticMesh::CreateInfo&&) noexcept;
} // namespace qz::gfx
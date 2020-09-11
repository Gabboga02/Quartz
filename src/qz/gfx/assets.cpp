#include <qz/gfx/static_mesh.hpp>
#include <qz/gfx/buffer.hpp>
#include <qz/gfx/assets.hpp>

#include <mutex>

namespace qz::assets {
    template <typename T>
    struct InternalStorage {
        T object;
        bool done;
    };

    template <typename T>
    static std::vector<InternalStorage<T>> assets;

    template <typename T>
    static std::mutex done_mutex;

    template <>
    qz_nodiscard meta::Handle<gfx::StaticMesh> emplace_empty() noexcept {
        std::lock_guard<std::mutex> lock(done_mutex<gfx::StaticMesh>);
        assets<gfx::StaticMesh>.emplace_back();
        return { assets<gfx::StaticMesh>.size() - 1 };
    }

    template <>
    qz_nodiscard gfx::StaticMesh& from_handle(const meta::Handle<gfx::StaticMesh> handle) noexcept {
        std::lock_guard<std::mutex> lock(done_mutex<gfx::StaticMesh>);
        qz_assert(0 <= handle.index && handle.index < assets<gfx::StaticMesh>.size(), "Invalid mesh handle");
        return assets<gfx::StaticMesh>[handle.index].object;
    }

    template <>
    void finalize(const meta::Handle<gfx::StaticMesh> handle) noexcept {
        std::lock_guard<std::mutex> lock(done_mutex<gfx::StaticMesh>);
        qz_assert(0 <= handle.index && handle.index < assets<gfx::StaticMesh>.size(), "Invalid mesh handle");
        assets<gfx::StaticMesh>[handle.index].done = true;
    }

    template <>
    bool is_ready(const meta::Handle<gfx::StaticMesh> handle) noexcept {
        std::lock_guard<std::mutex> lock(done_mutex<gfx::StaticMesh>);
        qz_assert(0 <= handle.index && handle.index < assets<gfx::StaticMesh>.size(), "Invalid mesh handle");
        return assets<gfx::StaticMesh>[handle.index].done;
    }

    void free_all_resources(const gfx::Context& context) noexcept {
        for (auto& each : assets<gfx::StaticMesh>) {
            gfx::Buffer::destroy(context, each.object.geometry);
            gfx::Buffer::destroy(context, each.object.indices);
        }
    }
} // namespace qz::assets
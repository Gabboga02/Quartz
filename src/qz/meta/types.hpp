#pragma once

#include <qz/meta/constants.hpp>

#include <cstdlib>
#include <array>

namespace qz::meta {
    template <typename T>
    using in_flight_array = std::array<T, in_flight>;

    template <typename T>
    struct Handle {
        std::size_t index;
    };
} // namespace qz::meta
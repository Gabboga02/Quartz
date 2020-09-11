#pragma once

#include <qz/util/macros.hpp>

#include <cstdint>
#include <string>

namespace qz::meta {
    constexpr struct viewport_tag_t {} full_viewport;
    constexpr struct scissor_tag_t {} full_scissor;

    constexpr auto in_flight = 2u;
    constexpr auto external_subpass = ~0u;
    constexpr auto family_ignored = ~0u;
} // namespace qz::meta
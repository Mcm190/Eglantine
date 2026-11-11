// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <optional>
#include "common/common_types.h"

namespace VideoCommon::DuskbloodsUpscale {

constexpr std::optional<std::array<u32, 2>> RenderRegion(
    u32 last_column, u32 last_row, std::array<u32, 2> color,
    std::array<u32, 2> output, std::array<u32, 2> motion) {
    // Compare the inclusive coordinates before adding one, including malformed UINT_MAX.
    if (last_column >= color[0] || last_row >= color[1] ||
        last_column >= output[0] || last_row >= output[1] ||
        last_column >= motion[0] || last_row >= motion[1]) {
        return std::nullopt;
    }
    return std::array{last_column + 1, last_row + 1};
}

} // namespace VideoCommon::DuskbloodsUpscale

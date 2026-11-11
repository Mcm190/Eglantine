// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "common/common_types.h"

namespace Tegra {

/// Layout-range suffixes keyed by address space and mapping generation; owned by one thread.
class MemoryLayoutCache {
public:
    [[nodiscard]] std::optional<size_t> Find(size_t owner, u64 generation,
                                            GPUVAddr address) const {
        for (const Entry& entry : entries) {
            if (!entry.valid || entry.owner != owner || entry.generation != generation ||
                address < entry.address) {
                continue;
            }
            const u64 offset = address - entry.address;
            if (offset == 0 || offset < entry.size) {
                return entry.size - offset;
            }
        }
        return std::nullopt;
    }

    void Insert(size_t owner, u64 generation, GPUVAddr address, size_t size) {
        entries[next] = {owner, generation, address, size, true};
        next = (next + 1) % entries.size();
    }

private:
    struct Entry {
        size_t owner{};
        u64 generation{};
        GPUVAddr address{};
        size_t size{};
        bool valid{};
    };
    std::array<Entry, 8> entries{};
    size_t next{};
};

} // namespace Tegra

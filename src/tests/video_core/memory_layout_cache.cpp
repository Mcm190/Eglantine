// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "common/range_map.h"
#include "video_core/memory_layout_cache.h"

TEST_CASE("GPU memory layout cache preserves range boundaries", "[video_core][layout_cache]") {
    Tegra::MemoryLayoutCache cache;
    CHECK_FALSE(cache.Find(0, 0, 0));
    cache.Insert(7, 3, 0x1000, 0x3000);
    REQUIRE(cache.Find(7, 3, 0x1000) == 0x3000);
    REQUIRE(cache.Find(7, 3, 0x1800) == 0x2800);
    REQUIRE(cache.Find(7, 3, 0x3fff) == 1);
    CHECK_FALSE(cache.Find(7, 3, 0xfff));
    CHECK_FALSE(cache.Find(7, 3, 0x4000));
    CHECK_FALSE(cache.Find(8, 3, 0x1000));
    CHECK_FALSE(cache.Find(7, 4, 0x1000));
    cache.Insert(7, 4, 0x1000, 0);
    REQUIRE(cache.Find(7, 4, 0x1000) == 0);
    CHECK_FALSE(cache.Find(7, 4, 0x1001));

    constexpr auto end = (std::numeric_limits<GPUVAddr>::max)();
    cache.Insert(7, 4, end - 16, 16);
    REQUIRE(cache.Find(7, 4, end - 1) == 1);
    CHECK_FALSE(cache.Find(7, 4, end));
    CHECK_FALSE(cache.Find(7, 4, 0));
}

TEST_CASE("GPU memory layout cache follows remaps and address spaces", "[video_core][layout_cache]") {
    constexpr size_t page_size = 4096;
    constexpr size_t page_count = 256;
    std::array<std::array<u8, page_count>, 2> pages{};
    std::array<Common::RangeMap<GPUVAddr, u8>, 2> maps{
        Common::RangeMap<GPUVAddr, u8>{0}, Common::RangeMap<GPUVAddr, u8>{0}};
    Tegra::MemoryLayoutCache cache;
    u64 generation{};
    u32 random = 431;
    size_t hits{};
    for (size_t iteration = 0; iteration < 10000; ++iteration) {
        random = random * 1664525U + 1013904223U;
        const size_t owner = (random >> 29) & 1;
        const size_t first = (random >> 8) % page_count;
        if (iteration % 13 == 0) {
            const size_t count = (std::min)(size_t{1 + (random >> 20) % 16}, page_count - first);
            const u8 kind = static_cast<u8>((random >> 17) % 4);
            maps[owner].Map(first * page_size, (first + count) * page_size, kind);
            std::fill_n(pages[owner].begin() + first, count, kind);
            ++generation;
        }
        const GPUVAddr address = first * page_size + (random & (page_size - 1));
        size_t expected{};
        if (const u8 kind = pages[owner][first]; kind != 0) {
            size_t last = first + 1;
            while (last < page_count && pages[owner][last] == kind) {
                ++last;
            }
            expected = last * page_size - address;
        }
        const auto cached = cache.Find(owner, generation, address);
        if (cached) {
            ++hits;
            REQUIRE(*cached == expected);
        } else {
            const size_t resolved = maps[owner].GetContinuousSizeFrom(address);
            REQUIRE(resolved == expected);
            cache.Insert(owner, generation, address, resolved);
        }
        REQUIRE(cache.Find(owner, generation, address) == expected);
    }
    CHECK(hits > 0);
}

TEST_CASE("GPU memory layout cache avoids repeated range lookups", "[video_core][layout_cache]") {
    Tegra::MemoryLayoutCache cache;
    size_t lookups{};
    for (GPUVAddr address = 0x10000; address < 0x20000; address += 16) {
        if (!cache.Find(1, 0, address)) {
            ++lookups;
            cache.Insert(1, 0, address, 0x20000 - address);
        }
        REQUIRE(cache.Find(1, 0, address) == 0x20000 - address);
    }
    CHECK(lookups == 1);
    for (u64 generation = 1; generation < 20; ++generation) {
        CHECK_FALSE(cache.Find(1, generation, 0x10000));
        cache.Insert(1, generation, 0x10000, 0x1000);
        CHECK(cache.Find(1, generation, 0x10800) == 0x800);
    }
}

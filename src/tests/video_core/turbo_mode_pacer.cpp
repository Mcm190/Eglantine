// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "video_core/renderer_vulkan/turbo_mode_pacer.h"

using namespace std::chrono_literals;

TEST_CASE("Clock boosting leaves continuously submitted game work alone", "[video_core][turbo_pacer]") {
    Vulkan::TurboModePacer pacer;
    const auto start = Vulkan::TurboModePacer::Clock::time_point{};
    for (int ms = 0; ms < 1000; ++ms) {
        const auto now = start + std::chrono::milliseconds{ms};
        CHECK_FALSE(pacer.ShouldBoost(now, now));
        CHECK_FALSE(pacer.ShouldBoost(now + 4ms, now));
    }
}

TEST_CASE("Clock boosting is bounded during submission gaps", "[video_core][turbo_pacer]") {
    Vulkan::TurboModePacer pacer;
    const auto start = Vulkan::TurboModePacer::Clock::time_point{};
    int boosts{};
    auto previous = start - 100ms;
    for (int ms = 0; ms <= 1000; ++ms) {
        const auto now = start + std::chrono::milliseconds{ms};
        if (pacer.ShouldBoost(now, start)) {
            CHECK(now - previous >= 8ms);
            CHECK(now - start >= 5ms);
            CHECK(now - start <= 100ms);
            previous = now;
            ++boosts;
        }
    }
    CHECK(boosts == 12);
}

TEST_CASE("Clock boosting resumes after new work and rejects immediate repeats",
          "[video_core][turbo_pacer]") {
    Vulkan::TurboModePacer pacer;
    const auto start = Vulkan::TurboModePacer::Clock::time_point{};
    CHECK(pacer.ShouldBoost(start + 8ms, start));
    CHECK_FALSE(pacer.ShouldBoost(start + 8ms, start));
    CHECK_FALSE(pacer.ShouldBoost(start + 15ms, start));
    CHECK(pacer.ShouldBoost(start + 16ms, start));
    CHECK_FALSE(pacer.ShouldBoost(start + 101ms, start));
    CHECK_FALSE(pacer.ShouldBoost(start + 500ms, start));
    CHECK_FALSE(pacer.ShouldBoost(start + 504ms, start + 500ms));
    CHECK(pacer.ShouldBoost(start + 505ms, start + 500ms));
}

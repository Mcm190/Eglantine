// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <optional>

namespace Vulkan {

class TurboModePacer {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto PollInterval = std::chrono::milliseconds{8};

    bool ShouldBoost(Clock::time_point now, Clock::time_point last_submission) {
        const auto idle = now - last_submission;
        if (idle < std::chrono::milliseconds{5} || idle > std::chrono::milliseconds{100}) {
            return false;
        }
        if (last_boost && now - *last_boost < PollInterval) {
            return false;
        }
        last_boost = now;
        return true;
    }

private:
    std::optional<Clock::time_point> last_boost;
};

} // namespace Vulkan

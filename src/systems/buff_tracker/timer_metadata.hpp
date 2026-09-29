#pragma once

#include <cmath>
#include <cstdint>

namespace BuffPanel::Systems::BuffTracker::Internal {

// Logical timer metadata consumed by Buff HUD. The qualified native StatList
// readable-range boundary remains 0x30 bytes, but skillLevel at +0x2C is not
// part of this model because Buff HUD does not use it for identity, expiry,
// presentation, or qualification.
struct TimerMetadata final {
    std::uint32_t flags{};
    std::uint32_t state{};
    float expireFrameFloat{};
    std::uint32_t skill{};
};

constexpr std::uint32_t MaximumTimerFrames = 25u * 60u * 60u * 24u * 7u;

[[nodiscard]] inline std::uint32_t TimerSkill(
    const TimerMetadata& metadata, std::uint32_t configuredSkill) noexcept {
    return metadata.skill != 0 ? metadata.skill : configuredSkill;
}

// A configured skill supplies identity/presentation only when native attribution
// is absent. The native state still owns the deadline; never synthesize one from
// skill level or a default duration.
[[nodiscard]] inline bool ResolveTimer(
    const TimerMetadata& metadata, std::uint32_t expectedState,
    std::uint32_t configuredSkill, std::uint32_t currentFrame,
    std::uint32_t& skill, std::uint32_t& expiry) noexcept {
    skill = TimerSkill(metadata, configuredSkill);
    expiry = 0;
    if (expectedState == 0 || metadata.state != expectedState || currentFrame == 0
        || skill == 0 || skill > 4095 || (metadata.flags & 0x20u) != 0
        || !std::isfinite(metadata.expireFrameFloat)) {
        return false;
    }
    const double frame = metadata.expireFrameFloat;
    const double rounded = std::floor(frame + 0.5);
    if (rounded <= 0.0 || rounded >= 4294967296.0 || std::fabs(frame - rounded) > 0.01) {
        return false;
    }
    const auto deadline = static_cast<std::uint32_t>(rounded);
    const auto remaining = deadline - currentFrame;
    if (static_cast<std::int32_t>(remaining) <= 0 || remaining > MaximumTimerFrames) {
        return false;
    }
    expiry = deadline;
    return true;
}

} // namespace BuffPanel::Systems::BuffTracker::Internal

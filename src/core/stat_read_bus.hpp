#pragma once

#include "rip_indirect_bridge.hpp"

#include <array>
#include <cstdint>

namespace BuffPanel::Core {

using RawGetUnitStatFn = std::int32_t(__fastcall*)(
    void* unit,
    std::int32_t statId,
    std::uint16_t layer) noexcept;

// Buff Panel only reads unit stats. Resolve D2RLoader's already-qualified
// GetUnitStat bridge and keep the callable target; never install a read hook or
// intercept another plugin's stat queries.
class StatReadBus final {
public:
    StatReadBus() = default;
    StatReadBus(const StatReadBus&) = delete;
    StatReadBus& operator=(const StatReadBus&) = delete;

    template <std::size_t N>
    [[nodiscard]] bool ResolveRawGetter(
        std::uintptr_t rva,
        const std::array<std::uint8_t, N>& expected,
        std::uintptr_t qualifiedSlotRva) noexcept {
        if (getter_ != nullptr) return true;
        RipIndirectBridgeResolution bridge{};
        if (!ResolveRipIndirectBridge(
                Services().context, rva, expected, qualifiedSlotRva, &bridge)) {
            return false;
        }
        getter_ = reinterpret_cast<RawGetUnitStatFn>(bridge.targetAddress);
        return getter_ != nullptr;
    }

    [[nodiscard]] RawGetUnitStatFn RawGetter() const noexcept { return getter_; }
    void Reset() noexcept { getter_ = nullptr; }

private:
    RawGetUnitStatFn getter_{};
};

[[nodiscard]] StatReadBus& StatReads() noexcept;

} // namespace BuffPanel::Core

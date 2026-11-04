#pragma once

#include "buff_display_bus.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace BuffPanel::Core {

// Resolved buff-hud-display.txt rows, keyed by native state id. BuffTracker
// replaces the set on the game thread when tables load; BuffHud reads it on
// the UI thread. Names are already localized UTF-8.
struct BuffPresentation final {
    std::uint32_t stateId{};
    BuffIconAtlas iconAtlas{BuffIconAtlas::Auto};
    std::uint16_t iconFrame{AutoIconFrame};
    std::string name{};
    std::string itemCode{};
};

class BuffPresentationRegistry final {
public:
    BuffPresentationRegistry() = default;
    BuffPresentationRegistry(const BuffPresentationRegistry&) = delete;
    BuffPresentationRegistry& operator=(const BuffPresentationRegistry&) = delete;

    // presentations must be sorted by stateId without duplicates.
    void Replace(std::vector<BuffPresentation> presentations) noexcept;
    void Clear() noexcept;
    [[nodiscard]] bool FindIcon(
        std::uint32_t stateId, BuffIconAtlas& atlas, std::uint16_t& frame) const noexcept;
    // Copies the name including its NUL; false when there is none or it does not fit.
    [[nodiscard]] bool FindName(std::uint32_t stateId, char* output, std::size_t outputSize) const noexcept;
    // Same contract as FindName, for the item code whose picture replaces the icon.
    [[nodiscard]] bool FindItemCode(std::uint32_t stateId, char* output, std::size_t outputSize) const noexcept;
    [[nodiscard]] std::size_t Count() const noexcept;

private:
    [[nodiscard]] const BuffPresentation* Find(std::uint32_t stateId) const noexcept;
    [[nodiscard]] bool CopyText(
        std::uint32_t stateId,
        std::string BuffPresentation::*field,
        char* output,
        std::size_t outputSize) const noexcept;

    mutable std::mutex mutex_;
    std::vector<BuffPresentation> presentations_;
};

[[nodiscard]] BuffPresentationRegistry& BuffPresentations() noexcept;

} // namespace BuffPanel::Core

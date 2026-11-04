#include "buff_presentation.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace BuffPanel::Core {
namespace {
BuffPresentationRegistry Registry{};
}

void BuffPresentationRegistry::Replace(std::vector<BuffPresentation> presentations) noexcept {
    std::lock_guard lock(mutex_);
    presentations_ = std::move(presentations);
}

void BuffPresentationRegistry::Clear() noexcept {
    std::lock_guard lock(mutex_);
    presentations_.clear();
}

const BuffPresentation* BuffPresentationRegistry::Find(std::uint32_t stateId) const noexcept {
    const auto it = std::lower_bound(
        presentations_.begin(),
        presentations_.end(),
        stateId,
        [](const BuffPresentation& presentation, std::uint32_t value) noexcept {
            return presentation.stateId < value;
        });
    return it != presentations_.end() && it->stateId == stateId ? &*it : nullptr;
}

bool BuffPresentationRegistry::FindIcon(
    std::uint32_t stateId, BuffIconAtlas& atlas, std::uint16_t& frame) const noexcept {
    if (stateId == 0) return false;
    std::lock_guard lock(mutex_);
    const auto* presentation = Find(stateId);
    if (presentation == nullptr || presentation->iconAtlas == BuffIconAtlas::Auto) return false;
    atlas = presentation->iconAtlas;
    frame = presentation->iconFrame;
    return true;
}

bool BuffPresentationRegistry::CopyText(
    std::uint32_t stateId,
    std::string BuffPresentation::*field,
    char* output,
    std::size_t outputSize) const noexcept {
    if (output == nullptr || outputSize == 0) return false;
    output[0] = '\0';
    if (stateId == 0) return false;
    std::lock_guard lock(mutex_);
    const auto* presentation = Find(stateId);
    if (presentation == nullptr) return false;
    const auto& text = presentation->*field;
    if (text.empty() || text.size() + 1 > outputSize) return false;
    std::memcpy(output, text.c_str(), text.size() + 1);
    return true;
}

bool BuffPresentationRegistry::FindName(
    std::uint32_t stateId, char* output, std::size_t outputSize) const noexcept {
    return CopyText(stateId, &BuffPresentation::name, output, outputSize);
}

bool BuffPresentationRegistry::FindItemCode(
    std::uint32_t stateId, char* output, std::size_t outputSize) const noexcept {
    return CopyText(stateId, &BuffPresentation::itemCode, output, outputSize);
}

std::size_t BuffPresentationRegistry::Count() const noexcept {
    std::lock_guard lock(mutex_);
    return presentations_.size();
}

BuffPresentationRegistry& BuffPresentations() noexcept {
    return Registry;
}

} // namespace BuffPanel::Core

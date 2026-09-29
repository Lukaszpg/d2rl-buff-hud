#include "hook_registry.hpp"

namespace BuffPanel::Core {
namespace {
HookRegistry Registry{};
}

void HookRegistry::Reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    records_ = {};
    count_ = 0;
}

HookRegistry& Hooks() noexcept {
    return Registry;
}

} // namespace BuffPanel::Core

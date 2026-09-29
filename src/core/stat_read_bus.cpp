#include "stat_read_bus.hpp"

namespace BuffPanel::Core {
namespace {
StatReadBus Reader{};
}

StatReadBus& StatReads() noexcept {
    return Reader;
}

} // namespace BuffPanel::Core

#pragma once

#include <D2RLPlugin/api.h>

#include "core/plugin_config.hpp"

namespace BuffPanel::Systems::BuffHud {

[[nodiscard]] bool Initialize(const D2RL::PluginContext* context, const Core::PluginConfig& config) noexcept;
void Shutdown() noexcept;

} // namespace BuffPanel::Systems::BuffHud

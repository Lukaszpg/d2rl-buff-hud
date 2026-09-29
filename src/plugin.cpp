#include <Windows.h>
#include <D2RLPlugin/api.h>
#include "core/services.hpp"
#include "core/hook_registry.hpp"
#include "core/stat_read_bus.hpp"
#include "core/stat_list_post_bus.hpp"
#include "core/buff_display_bus.hpp"
#include "native/native_contract.hpp"
#include "systems/buff_hud/buff_hud.hpp"
#include "systems/buff_tracker/buff_tracker.hpp"

namespace BuffPanel {
namespace {

static_assert(D2RL_PLUGIN_ABI_VERSION == 4,
    "Buff Panel 1.0.11 requires D2RLoader PluginSDK 0.2.x / plugin ABI 4.");

constexpr D2RL::PluginInfo Info{
    .infoSize = D2RL::PluginInfoSize,
    .abiVersion = D2RL_PLUGIN_ABI_VERSION,
    .id = "buff-panel",
    .name = "Buff Panel",
    .version = "1.0.11",
    .author = "MindH1ve",
    .description = "Standalone configurable D2RLoader buff panel and countdown timers.",
    .flags = D2RL::PluginFlags::Shared | D2RL::PluginFlags::NativeHooks,
};

void ShutdownRuntime() noexcept {
    Systems::BuffTracker::Shutdown();
    Systems::BuffHud::Shutdown();
    Core::StatListPosts().Reset();
    Core::StatReads().Reset();
    Core::Hooks().Reset();
    Core::BuffDisplays().EndSession();
    Core::ShutdownServices();
}

} // namespace
} // namespace BuffPanel

D2RL_PLUGIN_EXPORT const D2RL::PluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
    return &BuffPanel::Info;
}

D2RL_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadPlugin(
    const D2RL::PluginContext* context) noexcept {
    using namespace BuffPanel;
    if (context == nullptr || !Core::InitializeServices(context)) return false;
    if (!Systems::BuffHud::Initialize(context)) {
        ShutdownRuntime();
        return false;
    }
    // Buff Panel only needs to read live stat values. Resolve the loader-owned,
    // already-qualified bridge without installing a global stat-read hook.
    if (!Core::StatReads().ResolveRawGetter(
            Native::Contract::GetUnitStatRva,
            Native::Contract::GetUnitStatExpected,
            Native::Contract::GetUnitStatBridgeSlotRva)) {
        context->LogError("Buff Panel: cannot resolve D2RLoader's qualified GetUnitStat bridge; refusing to load.");
        ShutdownRuntime();
        return false;
    }
    if (!Systems::BuffTracker::Initialize(context)) {
        ShutdownRuntime();
        return false;
    }
    context->LogInfo("Buff Panel 1.0.11 loaded (D2R build 93847; PluginSDK 0.2.x/ABI 4).");
    return true;
}

D2RL_PLUGIN_EXPORT void __cdecl D2RLoaderUnloadPlugin() noexcept {
    BuffPanel::ShutdownRuntime();
}

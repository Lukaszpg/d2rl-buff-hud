// Exercise the production native binding (including qualification, pointer
// checks, encoding, and string length updates) without exporting test APIs.
#include "systems/buff_hud/buff_hud.cpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

namespace H = BuffPanel::Systems::BuffHud;
namespace {
alignas(8) std::array<std::byte, 0x1000> Widget{};
std::array<bool, 400> Visibility{};
std::size_t SetVisibilityCallCount{};
std::size_t FindChildCallCount{};
D2RL::Widgets::Result __cdecl SetVisibility(const D2RL::PluginContext*,
    D2RL::Widgets::WidgetHandle handle, bool visible) noexcept {
    ++SetVisibilityCallCount;
    if (handle < Visibility.size()) Visibility[handle] = visible;
    return D2RL::Widgets::Result::Success;
}
D2RL::Widgets::Result __cdecl SetEnabledState(const D2RL::PluginContext*,
    D2RL::Widgets::WidgetHandle, bool) noexcept {
    return D2RL::Widgets::Result::Success;
}
constexpr std::size_t TextField = 0x88; // Observed FocusableWidget field on 93847.
void* __fastcall FindPanel(const char*) noexcept { return Widget.data(); }
void* __fastcall FindChild(void*, const char*) noexcept {
    ++FindChildCallCount;
    return Widget.data();
}

void Field(std::size_t offset, std::uint64_t value) {
    std::memcpy(Widget.data() + offset, &value, sizeof(value));
}
std::uint64_t Length() {
    std::uint64_t value{};
    std::memcpy(&value, Widget.data() + TextField + 8, sizeof(value));
    return value;
}
void Bind(void* buffer, std::uint64_t length) {
    Widget = {};
    H::RenderStates = {};
    Field(TextField, reinterpret_cast<std::uintptr_t>(buffer));
    Field(TextField + 8, length);
    Field(TextField + 16, length);
}
}

int main() {
    H::FindTopLevelPanel = &FindPanel;
    H::FindChildWidgetByName = &FindChild;
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, info.dwPageSize * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(pages);
    DWORD old{};
    assert(VirtualProtect(pages + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old));
    auto* exact = pages + info.dwPageSize - H::TooltipReserveBytes;

    for (std::size_t slot = 0; slot < H::SlotCount; ++slot) {
        char reserve[H::TooltipReserveBytes]{};
        H::MakeTooltipReserve(slot, reserve);
        std::memcpy(exact, reserve, sizeof(reserve));
        Bind(exact, H::TooltipReserveLength);
        // Clear-on-open qualifies the buffer; a later assignment must work even
        // though the reserve marker and native length are no longer present.
        assert(H::WriteTooltipText(slot, ""));
        assert(Length() == 0);
        assert(H::WriteTooltipText(slot, "Bone Armor"));
        assert(Length() == 10 && std::strcmp(exact, "Bone Armor") == 0);
        assert(H::RenderStates[slot].qualifiedTooltipFieldOffset == TextField);
        assert(H::WriteTooltipText(slot, "Battle Command"));
        assert(Length() == 14 && std::strcmp(exact, "Battle Command") == 0);
        H::RestoreQualifiedTooltipBuffer(slot, H::RenderStates[slot]);
        assert(Length() == H::TooltipReserveLength);
        assert(std::memcmp(exact, reserve, sizeof(reserve)) == 0);
    }

    // UTF-16 has a distinct native character count; retain that fallback.
    std::array<std::uint16_t, H::TooltipReserveCodepoints + 1> wide{};
    H::MakeTooltipReserveUtf16(1, wide);
    Bind(wide.data(), H::TooltipReserveCodepoints);
    assert(H::WriteTooltipText(1, "Battle Orders"));
    assert(Length() == 13 && wide[0] == 'B' && wide[13] == 0);
    assert(H::RenderStates[1].qualifiedTooltipEncoding == H::TooltipStorageEncoding::BlizzardUtf16);

    // Incorrect slot tokens and ambiguous storage must remain untouched.
    char reserve[H::TooltipReserveBytes]{};
    H::MakeTooltipReserve(0, reserve);
    std::memcpy(exact, reserve, sizeof(reserve));
    Bind(exact, H::TooltipReserveLength);
    assert(!H::WriteTooltipText(1, "Wrong slot"));
    assert(std::memcmp(exact, reserve, sizeof(reserve)) == 0);
    std::memcpy(Widget.data() + 0x180, Widget.data() + TextField, 24);
    assert(!H::WriteTooltipText(0, "Ambiguous"));
    assert(std::memcmp(exact, reserve, sizeof(reserve)) == 0);
    // Both input directions are represented by replacing the current native
    // tree, then returning to a freshly reconstructed original allocation.
    H::InvalidateWidgetHandles();
    std::array<char, H::TimerReserveBytes> timer{};
    std::memcpy(timer.data(), H::TimerReserve, timer.size());
    Widget = {};
    Field(BuffPanel::Native::Contract::HudTextPointerOffset,
        reinterpret_cast<std::uintptr_t>(timer.data()));
    H::ReconcileNativeWidgets();
    assert(H::NativeCountdownWidgets[0] == Widget.data());
    H::RenderStates[0].visible = true;
    H::RenderStates[0].timerVisible = true;
    H::RenderStates[0].key = 42;
    // Marker restored in the same allocation: must bypass same-second caches.
    H::ReconcileNativeWidgets();
    assert(!H::RenderStates[0].visible && H::RenderStates[0].key == 0);
    std::memcpy(timer.data(), "2:23", 5);
    H::RenderStates[0].visible = true;
    H::RenderStates[0].timerVisible = true;
    H::RenderStates[0].key = 42;
    H::ReconcileNativeWidgets();
    assert(H::RenderStates[0].key == 42); // ordinary polling preserves state
    std::array<char, H::TimerReserveBytes> other{};
    for (int transition = 0; transition < 6; ++transition) {
        auto* next = transition % 2 == 0 ? other.data() : timer.data();
        std::memcpy(next, H::TimerReserve, H::TimerReserveBytes);
        Field(BuffPanel::Native::Contract::HudTextPointerOffset,
            reinterpret_cast<std::uintptr_t>(next));
        H::ReconcileNativeWidgets();
        assert(!H::RenderStates[0].visible);
        assert(H::NativeCountdownBuffers[0] == reinterpret_cast<std::uintptr_t>(next));
        H::RenderStates[0].visible = true;
        H::RenderStates[0].qualifiedTimerBuffer = reinterpret_cast<std::uintptr_t>(next);
    }
    // Unavailable tree must drop native qualifications without touching old data.
    H::FindTopLevelPanel = nullptr;
    H::ReconcileNativeWidgets();
    assert(H::RenderStates[0].qualifiedTimerBuffer == 0);
    assert(std::memcmp(other.data(), H::TimerReserve, other.size()) == 0);
    D2RL::PluginContext context{};
    D2RL::WidgetService widgets{};
    widgets.setWidgetVisible = &SetVisibility;
    H::Context = &context;
    H::Widgets = &widgets;
    H::RenderStates = {};
    for (std::size_t i = 0; i < H::SlotCount; ++i) {
        H::Handles[i].slot = 100 + i;
        H::Handles[i].countdown = 200 + i;
        H::Handles[i].tooltip = 300 + i;
    }
    for (int transition = 0; transition < 6; ++transition) {
        Visibility.fill(true); // input switch resets native flags in place
        H::ReconcileNativeWidgets();
        for (std::size_t i = 0; i < H::SlotCount; ++i) {
            assert(!Visibility[100 + i]);
            assert(!Visibility[200 + i]);
            assert(!Visibility[300 + i]);
        }
    }

    // Dirty-state and reconciliation gating regression checks:
    // 1. Steady-state ReconcileNativeWidgets only probes the slot-0 canary and
    //    makes zero SetVisible calls when the native tree has not changed.
    widgets.setWidgetEnabled = &SetEnabledState;
    H::FindTopLevelPanel = &FindPanel;
    H::InvalidateWidgetHandles();
    H::HandlesResolved = true;
    for (std::size_t i = 0; i < H::SlotCount; ++i) {
        H::Handles[i].slot = 100 + i;
        H::Handles[i].countdown = 200 + i;
        H::Handles[i].tooltip = 300 + i;
        for (std::size_t a = 0; a < H::AtlasCount; ++a) {
            H::Handles[i].icons[a] = 10 + a;
        }
    }
    std::memcpy(timer.data(), H::TimerReserve, H::TimerReserveBytes);
    Widget = {};
    Field(BuffPanel::Native::Contract::HudTextPointerOffset,
        reinterpret_cast<std::uintptr_t>(timer.data()));
    assert(H::ReconcileNativeWidgets());
    assert(H::EmptySlotsHidden);
    SetVisibilityCallCount = 0;
    FindChildCallCount = 0;
    assert(!H::ReconcileNativeWidgets());
    assert(SetVisibilityCallCount == 0);
    assert(FindChildCallCount == 3); // BuffGrid -> BuffSlot00 -> Countdown only

    // 2. RenderSnapshot skips redundant work on same-revision/same-frame UI polls,
    //    and RenderSlot skips redundant WidgetService calls when seconds are unchanged.
    BuffPanel::Core::BuffDisplays().BeginSession(1);
    BuffPanel::Core::BuffDisplays().PublishGameFrame(1, 100);
    BuffPanel::Core::BuffDisplayEntry entry{};
    entry.key = 99;
    entry.iconAtlas = BuffPanel::Core::BuffIconAtlas::Global;
    entry.iconFrame = 4;
    entry.displayMode = BuffPanel::Core::BuffDisplayMode::Timer;
    entry.expireGameFrame = 200; // 4s remaining at 25 Hz
    assert(BuffPanel::Core::BuffDisplays().Upsert(entry));

    H::RenderSnapshot();
    assert(H::RenderStates[0].visible && H::RenderStates[0].timerVisible);
    assert(H::RenderStates[0].seconds == 4);
    assert(std::strcmp(timer.data(), "4") == 0);

    // Same revision + same game frame (e.g. 120 FPS UI poll between 25 Hz ticks):
    SetVisibilityCallCount = 0;
    H::RenderSnapshot();
    assert(SetVisibilityCallCount == 0);

    // Next 25 Hz game frame within the same displayed second (frame 101 -> still 4s):
    BuffPanel::Core::BuffDisplays().PublishGameFrame(1, 101);
    SetVisibilityCallCount = 0;
    H::RenderSnapshot();
    assert(H::LastRenderedGameFrame == 101);
    assert(SetVisibilityCallCount == 0);

    // Frame 126 -> 3s remaining: updates text in place without redundant SetVisible calls:
    BuffPanel::Core::BuffDisplays().PublishGameFrame(1, 126);
    SetVisibilityCallCount = 0;
    H::RenderSnapshot();
    assert(H::RenderStates[0].seconds == 3);
    assert(std::strcmp(timer.data(), "3") == 0);
    assert(SetVisibilityCallCount == 0);

    // In-place controller HUD reset (reserve marker restored on same frame) must
    // bypass the snapshot gate, reconcile all slots, and restore presentation:
    std::memcpy(timer.data(), H::TimerReserve, H::TimerReserveBytes);
    Visibility[100] = false;
    H::RenderSnapshot();
    assert(Visibility[100]);
    assert(H::RenderStates[0].visible && H::RenderStates[0].timerVisible);
    assert(std::strcmp(timer.data(), "3") == 0);

    BuffPanel::Core::BuffDisplays().EndSession(1);
    H::Context = nullptr;
    H::Widgets = nullptr;
    VirtualFree(pages, 0, MEM_RELEASE);
    std::cout << "PASS: production tooltip binding, all slots, exact allocation, native length, reuse, restore and rejection\n";
}

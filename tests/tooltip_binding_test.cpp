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
D2RL::Widgets::Result __cdecl SetVisibility(const D2RL::PluginContext*,
    D2RL::Widgets::WidgetHandle handle, bool visible) noexcept {
    if (handle < Visibility.size()) Visibility[handle] = visible;
    return D2RL::Widgets::Result::Success;
}
constexpr std::size_t TextField = 0x88; // Observed FocusableWidget field on 93847.
void* __fastcall FindPanel(const char*) noexcept { return Widget.data(); }
void* __fastcall FindChild(void*, const char*) noexcept { return Widget.data(); }

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
    H::Context = nullptr;
    H::Widgets = nullptr;
    VirtualFree(pages, 0, MEM_RELEASE);
    std::cout << "PASS: production tooltip binding, all slots, exact allocation, native length, reuse, restore and rejection\n";
}

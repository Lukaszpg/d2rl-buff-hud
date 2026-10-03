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
constexpr std::size_t TextField = 0x88; // Observed FocusableWidget field on 93847.
std::array<bool, 256> Enabled{};

D2RL::Widgets::Result __cdecl SetEnabled(
    const D2RL::PluginContext*,
    D2RL::Widgets::WidgetHandle handle,
    bool enabled) noexcept {
    assert(handle > 0 && handle < Enabled.size());
    Enabled[handle] = enabled;
    return D2RL::Widgets::Result::Success;
}

void TestHoverInteractionState() {
    D2RL::PluginContext context{};
    D2RL::WidgetService widgets{};
    widgets.setWidgetEnabled = &SetEnabled;
    H::Context = &context;
    H::Widgets = &widgets;
    H::HandlesResolved = true;
    H::HudPanel = 1;
    H::GridWidget = 2;

    std::uint64_t next = 3;
    for (auto& slot : H::Handles) {
        slot.slot = next++;
        slot.tooltip = next++;
        for (auto& icon : slot.icons) icon = next++;
    }

    H::RenderStates = {};
    H::RenderStates[0].visible = true;
    H::RenderStates[0].tooltipVisible = true;
    H::RenderStates[1].visible = true;
    H::RenderStates[1].tooltipVisible = false;

    assert(H::ApplyInputIsolation());
    assert(Enabled[H::HudPanel]);
    assert(Enabled[H::GridWidget]);
    for (std::size_t i = 0; i < H::Handles.size(); ++i) {
        const auto& slot = H::Handles[i];
        const bool occupied = H::RenderStates[i].visible;
        assert(Enabled[slot.slot] == occupied);
        assert(Enabled[slot.tooltip] == (occupied && H::RenderStates[i].tooltipVisible));
        for (const auto icon : slot.icons) assert(!Enabled[icon]);
    }

    H::Context = nullptr;
    H::Widgets = nullptr;
    H::HandlesResolved = false;
    H::HudPanel = D2RL::Widgets::InvalidHandle;
    H::GridWidget = D2RL::Widgets::InvalidHandle;
    H::Handles = {};
    H::RenderStates = {};
    Enabled.fill(false);
}
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
    TestHoverInteractionState();
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
    VirtualFree(pages, 0, MEM_RELEASE);
    std::cout << "PASS: production tooltip binding, all slots, exact allocation, native length, reuse, restore and rejection\n";
}

#include "buff_hud.hpp"
#include "tooltip_text.hpp"

#include "core/buff_display_bus.hpp"
#include "core/services.hpp"
#include "icon_frame_backend.hpp"
#include "native/native_contract.hpp"
#include "skill_icon_resolver.hpp"

#include <Windows.h>

#include <D2RLPlugin/localization.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>


namespace BuffPanel::Systems::BuffHud {
namespace {

using Internal::IconFrameBackendState;
using Internal::SkillIconDescriptor;

constexpr std::size_t SlotCount = Core::MaximumDisplayedBuffs;
constexpr std::size_t AtlasCount = 9;
constexpr std::uint32_t FramesPerSecond = 25;
constexpr char TimerReserve[] = "__BUFF_PANEL_TIMER_RESERVE_00__";
constexpr std::size_t TimerReserveBytes = sizeof(TimerReserve);
using Internal::TooltipReserveBytes;
using Internal::TooltipReserveLength;
constexpr std::size_t TooltipReserveCodepoints = TooltipReserveLength / 3;
static_assert(TooltipReserveCodepoints * 3 == TooltipReserveLength);
constexpr std::size_t TooltipNativeScanBytes = 0x1000;
constexpr std::uint32_t NoRenderedSeconds = std::numeric_limits<std::uint32_t>::max();
constexpr std::int32_t NoRenderedValue = std::numeric_limits<std::int32_t>::min();

enum class TooltipStorageEncoding : std::uint8_t {
    Unknown = 0,
    BlizzardUtf8 = 1,
    BlizzardUtf16 = 2,
};
static_assert(TimerReserveBytes == 32);

constexpr std::array<Core::BuffIconAtlas, AtlasCount> AtlasOrder{
    Core::BuffIconAtlas::Amazon,
    Core::BuffIconAtlas::Sorceress,
    Core::BuffIconAtlas::Necromancer,
    Core::BuffIconAtlas::Paladin,
    Core::BuffIconAtlas::Barbarian,
    Core::BuffIconAtlas::Druid,
    Core::BuffIconAtlas::Assassin,
    Core::BuffIconAtlas::Warlock,
    Core::BuffIconAtlas::Global,
};

constexpr std::array<const char*, AtlasCount> AtlasWidgetNames{
    "IconAmazon",
    "IconSorceress",
    "IconNecromancer",
    "IconPaladin",
    "IconBarbarian",
    "IconDruid",
    "IconAssassin",
    "IconWarlock",
    "IconGlobal",
};

using FindTopLevelPanelFn = void*(__fastcall*)(const char* name) noexcept;
using FindChildWidgetByNameFn = void*(__fastcall*)(void* parent, const char* name) noexcept;

const D2RL::PluginContext* Context{};
const D2RL::PanelService* Panels{};
const D2RL::WidgetService* Widgets{};
const D2RL::ThreadService* Threads{};
const D2RL::LifecycleService* Lifecycle{};
D2RL::Panels::RegistrationHandle RegisteredPanel{D2RL::Panels::InvalidHandle};
D2RL::Lifecycle::ListenerHandle DataTablesListener{D2RL::Lifecycle::InvalidHandle};
std::array<D2RL::Lifecycle::ListenerHandle, 3> GameplayListeners{};

FindTopLevelPanelFn FindTopLevelPanel{};
FindChildWidgetByNameFn FindChildWidgetByName{};

std::atomic<std::uint64_t> CurrentSessionGeneration{};
std::atomic<bool> PollScheduled{};
struct SlotHandles final {
    D2RL::Widgets::WidgetHandle slot{D2RL::Widgets::InvalidHandle};
    std::array<D2RL::Widgets::WidgetHandle, AtlasCount> icons{};
    D2RL::Widgets::WidgetHandle countdown{D2RL::Widgets::InvalidHandle};
    D2RL::Widgets::WidgetHandle tooltip{D2RL::Widgets::InvalidHandle};
};

struct SlotRenderState final {
    bool visible{};
    std::uint64_t key{};
    Core::BuffIconAtlas atlas{Core::BuffIconAtlas::Global};
    std::uint16_t requestedFrame{};
    std::uint16_t renderedFrame{};
    Core::BuffDisplayMode displayMode{Core::BuffDisplayMode::Timer};
    std::uint32_t seconds{NoRenderedSeconds};
    std::int32_t currentValue{NoRenderedValue};
    std::int32_t maximumValue{NoRenderedValue};
    std::int32_t sourceSkillId{Core::NoSourceSkillId};
    bool timerVisible{};
    bool tooltipVisible{};
    std::uintptr_t qualifiedTimerBuffer{};
    std::uintptr_t qualifiedTooltipWidget{};
    std::uintptr_t qualifiedTooltipBuffer{};
    std::size_t qualifiedTooltipFieldOffset{std::numeric_limits<std::size_t>::max()};
    TooltipStorageEncoding qualifiedTooltipEncoding{TooltipStorageEncoding::Unknown};
};

D2RL::Widgets::WidgetHandle HudPanel{D2RL::Widgets::InvalidHandle};
D2RL::Widgets::WidgetHandle GridWidget{D2RL::Widgets::InvalidHandle};
std::array<SlotHandles, SlotCount> Handles{};
std::array<SlotRenderState, SlotCount> RenderStates{};
bool HandlesResolved{};
std::uint64_t LastRenderedRevision{std::numeric_limits<std::uint64_t>::max()};

[[nodiscard]] std::size_t AtlasIndex(Core::BuffIconAtlas atlas) noexcept {
    for (std::size_t i = 0; i < AtlasOrder.size(); ++i) {
        if (AtlasOrder[i] == atlas) return i;
    }
    return AtlasOrder.size() - 1;
}

[[nodiscard]] bool IsReadableRange(const void* address, std::size_t size) noexcept {
    if (address == nullptr || size == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0
        || info.State != MEM_COMMIT
        || (info.Protect & PAGE_GUARD) != 0
        || (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto regionStart = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto regionEnd = regionStart + info.RegionSize;
    return start >= regionStart && start <= regionEnd && size <= regionEnd - start;
}

[[nodiscard]] bool IsWritableRange(void* address, std::size_t size) noexcept {
    if (!IsReadableRange(address, size)) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0) return false;
    const DWORD baseProtection = info.Protect & 0xFFU;
    return baseProtection == PAGE_READWRITE
        || baseProtection == PAGE_WRITECOPY
        || baseProtection == PAGE_EXECUTE_READWRITE
        || baseProtection == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
[[nodiscard]] bool ReadNativeField(const void* base, std::size_t offset, T& value) noexcept {
    if (base == nullptr) return false;
    const auto baseValue = reinterpret_cast<std::uintptr_t>(base);
    if (offset > static_cast<std::size_t>(UINTPTR_MAX - baseValue)) return false;
    const auto* address = reinterpret_cast<const void*>(baseValue + offset);
    if (!IsReadableRange(address, sizeof(T))) return false;
    std::memcpy(&value, address, sizeof(T));
    return true;
}

[[nodiscard]] bool IsTimerReserve(const char* buffer) noexcept {
    return buffer != nullptr
        && IsReadableRange(buffer, TimerReserveBytes)
        && std::memcmp(buffer, TimerReserve, TimerReserveBytes) == 0;
}

void MakeTooltipReserve(std::size_t slotIndex, char (&output)[TooltipReserveBytes]) noexcept {
    // Use a per-slot *invisible* UTF-8 qualification token instead of a long
    // run of ASCII spaces. The old reserve was visually blank but D2R still
    // measured all 127 spaces when the native tooltip field could not be
    // uniquely qualified, producing a huge empty tooltip. U+200B ZERO WIDTH
    // SPACE and U+200C ZERO WIDTH NON-JOINER give us 42 zero-width codepoints
    // (126 UTF-8 bytes) while the low five codepoints encode the slot id so the
    // backing std::string is unique for every one of the 21 reusable slots.
    constexpr std::array<unsigned char, 3> ZeroWidthSpace{0xE2, 0x80, 0x8B};
    constexpr std::array<unsigned char, 3> ZeroWidthNonJoiner{0xE2, 0x80, 0x8C};
    std::memset(output, 0, TooltipReserveBytes);
    for (std::size_t i = 0; i < TooltipReserveCodepoints; ++i) {
        const bool idBit = i < 5 && ((slotIndex >> i) & 1U) != 0;
        const auto& codepoint = idBit ? ZeroWidthNonJoiner : ZeroWidthSpace;
        std::memcpy(output + i * 3, codepoint.data(), codepoint.size());
    }
    output[TooltipReserveLength] = '\0';
}

void MakeTooltipReserveUtf16(std::size_t slotIndex, std::array<std::uint16_t, TooltipReserveCodepoints + 1>& output) noexcept {
    output.fill(0);
    for (std::size_t i = 0; i < TooltipReserveCodepoints; ++i) {
        const bool idBit = i < 5 && ((slotIndex >> i) & 1U) != 0;
        output[i] = static_cast<std::uint16_t>(idBit ? 0x200CU : 0x200BU);
    }
}

[[nodiscard]] bool Utf8ToUtf16(
    std::string_view input,
    std::array<std::uint16_t, TooltipReserveCodepoints + 1>& output,
    std::size_t& outputLength) noexcept {
    output.fill(0);
    outputLength = 0;
    std::size_t i{};
    while (i < input.size()) {
        const auto b0 = static_cast<unsigned char>(input[i]);
        std::uint32_t cp{};
        std::size_t count{};
        if (b0 < 0x80U) {
            cp = b0; count = 1;
        } else if ((b0 & 0xE0U) == 0xC0U && i + 1 < input.size()) {
            cp = static_cast<std::uint32_t>(b0 & 0x1FU) << 6;
            cp |= static_cast<unsigned char>(input[i + 1]) & 0x3FU;
            count = 2;
            if (cp < 0x80U) return false;
        } else if ((b0 & 0xF0U) == 0xE0U && i + 2 < input.size()) {
            cp = static_cast<std::uint32_t>(b0 & 0x0FU) << 12;
            cp |= static_cast<std::uint32_t>(static_cast<unsigned char>(input[i + 1]) & 0x3FU) << 6;
            cp |= static_cast<unsigned char>(input[i + 2]) & 0x3FU;
            count = 3;
            if (cp < 0x800U || (cp >= 0xD800U && cp <= 0xDFFFU)) return false;
        } else if ((b0 & 0xF8U) == 0xF0U && i + 3 < input.size()) {
            cp = static_cast<std::uint32_t>(b0 & 0x07U) << 18;
            cp |= static_cast<std::uint32_t>(static_cast<unsigned char>(input[i + 1]) & 0x3FU) << 12;
            cp |= static_cast<std::uint32_t>(static_cast<unsigned char>(input[i + 2]) & 0x3FU) << 6;
            cp |= static_cast<unsigned char>(input[i + 3]) & 0x3FU;
            count = 4;
            if (cp < 0x10000U || cp > 0x10FFFFU) return false;
        } else {
            return false;
        }
        for (std::size_t j = 1; j < count; ++j) {
            if ((static_cast<unsigned char>(input[i + j]) & 0xC0U) != 0x80U) return false;
        }
        if (cp <= 0xFFFFU) {
            if (outputLength >= TooltipReserveCodepoints) return false;
            output[outputLength++] = static_cast<std::uint16_t>(cp);
        } else {
            if (outputLength + 2 > TooltipReserveCodepoints) return false;
            cp -= 0x10000U;
            output[outputLength++] = static_cast<std::uint16_t>(0xD800U + (cp >> 10));
            output[outputLength++] = static_cast<std::uint16_t>(0xDC00U + (cp & 0x3FFU));
        }
        i += count;
    }
    return true;
}

[[nodiscard]] bool SetVisible(D2RL::Widgets::WidgetHandle handle, bool visible) noexcept;
[[nodiscard]] bool SetEnabled(D2RL::Widgets::WidgetHandle handle, bool enabled) noexcept;
[[nodiscard]] bool ApplyInputIsolation() noexcept;

[[nodiscard]] bool ResolveWidgetHandles() noexcept {
    if (HandlesResolved) return true;
    if (Context == nullptr || Widgets == nullptr) return false;

    HudPanel = D2RL::Widgets::InvalidHandle;
    GridWidget = D2RL::Widgets::InvalidHandle;
    Handles = {};

    if (Widgets->findPanel(Context, "buff-panel/BuffHud", &HudPanel) != D2RL::Widgets::Result::Success
        || Widgets->findWidget(Context, HudPanel, "BuffGrid", &GridWidget) != D2RL::Widgets::Result::Success) {
        return false;
    }

    for (std::size_t i = 0; i < Handles.size(); ++i) {
        char slotName[24]{};
        std::snprintf(slotName, sizeof(slotName), "BuffSlot%02zu", i);
        auto& slot = Handles[i];
        if (Widgets->findWidget(Context, GridWidget, slotName, &slot.slot)
            != D2RL::Widgets::Result::Success) {
            Handles = {};
            return false;
        }
        for (std::size_t atlas = 0; atlas < AtlasCount; ++atlas) {
            if (Widgets->findWidget(Context, slot.slot, AtlasWidgetNames[atlas], &slot.icons[atlas])
                != D2RL::Widgets::Result::Success) {
                Handles = {};
                return false;
            }
        }
        if (Widgets->findWidget(Context, slot.slot, "Countdown", &slot.countdown)
            != D2RL::Widgets::Result::Success
            || Widgets->findWidget(Context, slot.slot, "Tooltip", &slot.tooltip)
                != D2RL::Widgets::Result::Success) {
            Handles = {};
            return false;
        }

        // ButtonWidget visibility alone does not guarantee that D2R removes its
        // hit target. Empty BuffHud slots are display placeholders, so disable
        // every atlas button as soon as handles are resolved. Occupied slots
        // keep the selected atlas visible but disabled. A separate slot-local
        // FocusableWidget owns optional hover-only tooltip presentation.
        // In optional gameplay mode ApplyInputIsolation also disables the tooltip,
        // all slots, the grid and the panel to prioritize world input.
        for (const auto icon : slot.icons) {
            if (!SetEnabled(icon, false)) {
            }
        }
    }

    HandlesResolved = true;
    (void)ApplyInputIsolation();
    return true;
}

void RestoreQualifiedCountdownBuffers() noexcept {
    for (auto& state : RenderStates) {
        if (state.qualifiedTimerBuffer == 0) continue;
        auto* buffer = reinterpret_cast<char*>(state.qualifiedTimerBuffer);
        if (IsWritableRange(buffer, TimerReserveBytes)) {
            std::memcpy(buffer, TimerReserve, TimerReserveBytes);
        }
    }
}

void RestoreQualifiedTooltipBuffer(std::size_t slotIndex, SlotRenderState& state) noexcept {
    if (slotIndex >= SlotCount
        || state.qualifiedTooltipWidget == 0
        || state.qualifiedTooltipBuffer == 0
        || state.qualifiedTooltipFieldOffset == std::numeric_limits<std::size_t>::max()
        || state.qualifiedTooltipEncoding == TooltipStorageEncoding::Unknown) {
        return;
    }

    const auto stringObjectAddress = state.qualifiedTooltipWidget + state.qualifiedTooltipFieldOffset;
    if (!IsWritableRange(
            reinterpret_cast<void*>(stringObjectAddress + Native::Contract::BlizzardStringSizeOffset),
            sizeof(std::uint64_t))) {
        return;
    }

    std::uint64_t length{};
    if (state.qualifiedTooltipEncoding == TooltipStorageEncoding::BlizzardUtf8) {
        if (!IsWritableRange(reinterpret_cast<void*>(state.qualifiedTooltipBuffer), TooltipReserveBytes)) return;
        char reserve[TooltipReserveBytes]{};
        MakeTooltipReserve(slotIndex, reserve);
        std::memcpy(reinterpret_cast<void*>(state.qualifiedTooltipBuffer), reserve, TooltipReserveBytes);
        length = TooltipReserveLength;
    } else if (state.qualifiedTooltipEncoding == TooltipStorageEncoding::BlizzardUtf16) {
        constexpr std::size_t Utf16Bytes = (TooltipReserveCodepoints + 1) * sizeof(std::uint16_t);
        if (!IsWritableRange(reinterpret_cast<void*>(state.qualifiedTooltipBuffer), Utf16Bytes)) return;
        std::array<std::uint16_t, TooltipReserveCodepoints + 1> reserve{};
        MakeTooltipReserveUtf16(slotIndex, reserve);
        std::memcpy(reinterpret_cast<void*>(state.qualifiedTooltipBuffer), reserve.data(), Utf16Bytes);
        length = TooltipReserveCodepoints;
    } else {
        return;
    }

    std::memcpy(
        reinterpret_cast<void*>(stringObjectAddress + Native::Contract::BlizzardStringSizeOffset),
        &length,
        sizeof(length));
}

void RestoreQualifiedTooltipBuffers() noexcept {
    for (std::size_t slotIndex = 0; slotIndex < RenderStates.size(); ++slotIndex) {
        RestoreQualifiedTooltipBuffer(slotIndex, RenderStates[slotIndex]);
    }
}

void InvalidateWidgetHandles() noexcept {
    HudPanel = D2RL::Widgets::InvalidHandle;
    GridWidget = D2RL::Widgets::InvalidHandle;
    Handles = {};
    RenderStates = {};
    HandlesResolved = false;
    LastRenderedRevision = std::numeric_limits<std::uint64_t>::max();
}

[[nodiscard]] bool SetVisible(D2RL::Widgets::WidgetHandle handle, bool visible) noexcept {
    if (Context == nullptr || Widgets == nullptr || handle == D2RL::Widgets::InvalidHandle) return false;
    return Widgets->setWidgetVisible(Context, handle, visible) == D2RL::Widgets::Result::Success;
}

[[nodiscard]] bool SetEnabled(D2RL::Widgets::WidgetHandle handle, bool enabled) noexcept {
    if (Context == nullptr || Widgets == nullptr || handle == D2RL::Widgets::InvalidHandle) return false;
    return Widgets->setWidgetEnabled(Context, handle, enabled) == D2RL::Widgets::Result::Success;
}

// Call only on the UI thread, after ResolveWidgetHandles. A disabled widget
// can still be drawn in D2R (this is already how the atlas ButtonWidgets are
// rendered), but it cannot be relied on for native hover. Do not pretend that
// this proves end-to-end click-through: the test must also cover Panel hit-test
// behavior in the running game.
[[nodiscard]] bool ApplyInputIsolation() noexcept {
    if (!HandlesResolved || Context == nullptr || Widgets == nullptr) return false;
    bool allSucceeded = true;
    auto disable = [&](D2RL::Widgets::WidgetHandle handle) noexcept {
        if (!SetEnabled(handle, false)) allSucceeded = false;
    };
    for (auto& slot : Handles) {
        disable(slot.slot);
        disable(slot.tooltip);
        for (const auto icon : slot.icons) disable(icon);
    }
    disable(GridWidget);
    disable(HudPanel);
    if (!allSucceeded) Context->LogWarn(
        "Buff HUD: one or more widget enabled-state updates failed; gameplay input isolation may be incomplete.");
    return allSucceeded;
}

[[nodiscard]] void* ResolveNativeSlotChild(std::size_t slotIndex, const char* childName) noexcept {
    if (FindTopLevelPanel == nullptr || FindChildWidgetByName == nullptr
        || slotIndex >= SlotCount || childName == nullptr) {
        return nullptr;
    }
    void* panel = FindTopLevelPanel("buff-panel/BuffHud");
    void* grid = panel != nullptr ? FindChildWidgetByName(panel, "BuffGrid") : nullptr;
    char slotName[24]{};
    std::snprintf(slotName, sizeof(slotName), "BuffSlot%02zu", slotIndex);
    void* slot = grid != nullptr ? FindChildWidgetByName(grid, slotName) : nullptr;
    return slot != nullptr ? FindChildWidgetByName(slot, childName) : nullptr;
}

[[nodiscard]] bool WriteCountdownText(std::size_t slotIndex, const char* text) noexcept {
    if (slotIndex >= SlotCount || text == nullptr) return false;
    const auto length = std::strlen(text);
    if (length + 1 > TimerReserveBytes) return false;

    void* widget = ResolveNativeSlotChild(slotIndex, "Countdown");
    std::uintptr_t pointer{};
    if (widget == nullptr
        || !ReadNativeField(widget, Native::Contract::HudTextPointerOffset, pointer)
        || pointer == 0) {
        return false;
    }

    auto* buffer = reinterpret_cast<char*>(pointer);
    if (!IsWritableRange(buffer, TimerReserveBytes)) {
        return false;
    }

    auto& state = RenderStates[slotIndex];
    if (state.qualifiedTimerBuffer != pointer) {
        if (!IsTimerReserve(buffer)) {
            return false;
        }
        state.qualifiedTimerBuffer = pointer;
    }

    std::memcpy(buffer, TimerReserve, TimerReserveBytes);
    std::memcpy(buffer, text, length + 1);
    if (std::memcmp(buffer, text, length + 1) != 0) {
        return false;
    }
    return true;
}

[[nodiscard]] bool QualifyTooltipString(
    std::size_t slotIndex,
    void* widget,
    SlotRenderState& state) noexcept {
    if (widget == nullptr || slotIndex >= SlotCount) return false;

    char expectedUtf8[TooltipReserveBytes]{};
    MakeTooltipReserve(slotIndex, expectedUtf8);
    std::array<std::uint16_t, TooltipReserveCodepoints + 1> expectedUtf16{};
    MakeTooltipReserveUtf16(slotIndex, expectedUtf16);
    constexpr std::size_t Utf16Bytes = (TooltipReserveCodepoints + 1) * sizeof(std::uint16_t);

    std::size_t candidateCount{};
    std::size_t candidateOffset{};
    std::uintptr_t candidateBuffer{};
    TooltipStorageEncoding candidateEncoding{TooltipStorageEncoding::Unknown};

    // Build 93847 already has one independently runtime-qualified Blizzard
    // string layout: TooltipsPanel+0x180. It is {data@+0x00, size@+0x08,
    // capacity/flags@+0x10}, not an MSVC std::string. The previous BuffHud
    // implementation searched FocusableWidget for an MSVC basic_string tuple,
    // which explains the runtime witness tooltipQualFailures>0 with
    // skillNames=ready/name=Fade. Qualify the same blz::basic_string layout
    // here. Accept both char and 16-bit character storage because UI layout
    // fields are not guaranteed to use the same character width.
    for (std::size_t offset = 0;
         offset + Native::Contract::BlizzardStringCapacityFlagsOffset + sizeof(std::uint64_t) <= TooltipNativeScanBytes;
         offset += alignof(std::uintptr_t)) {
        std::uintptr_t dataPointer{};
        std::uint64_t size{};
        std::uint64_t capacityFlags{};
        if (!ReadNativeField(widget, offset + Native::Contract::BlizzardStringDataOffset, dataPointer)
            || !ReadNativeField(widget, offset + Native::Contract::BlizzardStringSizeOffset, size)
            || !ReadNativeField(widget, offset + Native::Contract::BlizzardStringCapacityFlagsOffset, capacityFlags)) {
            continue;
        }

        const auto capacity = capacityFlags & Native::Contract::BlizzardStringCapacityMask;
        const bool embedded = (capacityFlags & Native::Contract::BlizzardStringEmbeddedFlag) != 0;
        if (embedded
            || dataPointer == 0
            || capacity > Native::Contract::MaximumTooltipCapacityBytes) {
            continue;
        }

        TooltipStorageEncoding encoding{TooltipStorageEncoding::Unknown};
        if (size == TooltipReserveLength
            && capacity >= TooltipReserveLength
            && IsWritableRange(reinterpret_cast<void*>(dataPointer), TooltipReserveBytes)
            && Internal::MatchesTooltipReserve(reinterpret_cast<const void*>(dataPointer), expectedUtf8)) {
            encoding = TooltipStorageEncoding::BlizzardUtf8;
        } else if (size == TooltipReserveCodepoints
            && capacity >= TooltipReserveCodepoints
            && IsWritableRange(reinterpret_cast<void*>(dataPointer), Utf16Bytes)
            && std::memcmp(reinterpret_cast<const void*>(dataPointer), expectedUtf16.data(), Utf16Bytes) == 0) {
            encoding = TooltipStorageEncoding::BlizzardUtf16;
        }
        if (encoding == TooltipStorageEncoding::Unknown) continue;

        ++candidateCount;
        candidateOffset = offset;
        candidateBuffer = dataPointer;
        candidateEncoding = encoding;
    }

    if (candidateCount != 1) {
        return false;
    }

    state.qualifiedTooltipWidget = reinterpret_cast<std::uintptr_t>(widget);
    state.qualifiedTooltipBuffer = candidateBuffer;
    state.qualifiedTooltipFieldOffset = candidateOffset;
    state.qualifiedTooltipEncoding = candidateEncoding;
    return true;
}

[[nodiscard]] bool EnsureTooltipQualified(std::size_t slotIndex, SlotRenderState& state) noexcept {
    if (slotIndex >= SlotCount) return false;
    void* widget = ResolveNativeSlotChild(slotIndex, "Tooltip");
    if (widget == nullptr) {
        return false;
    }

    const auto widgetAddress = reinterpret_cast<std::uintptr_t>(widget);
    if (state.qualifiedTooltipWidget == widgetAddress
        && state.qualifiedTooltipBuffer != 0
        && state.qualifiedTooltipFieldOffset != std::numeric_limits<std::size_t>::max()
        && state.qualifiedTooltipEncoding != TooltipStorageEncoding::Unknown) {
        return true;
    }

    RestoreQualifiedTooltipBuffer(slotIndex, state);
    state.qualifiedTooltipWidget = 0;
    state.qualifiedTooltipBuffer = 0;
    state.qualifiedTooltipFieldOffset = std::numeric_limits<std::size_t>::max();
    state.qualifiedTooltipEncoding = TooltipStorageEncoding::Unknown;
    return QualifyTooltipString(slotIndex, widget, state);
}

[[nodiscard]] bool WriteTooltipText(std::size_t slotIndex, const char* text) noexcept {
    if (slotIndex >= SlotCount || text == nullptr) return false;
    const auto utf8Length = std::strlen(text);
    if (utf8Length > TooltipReserveLength) {
        return false;
    }

    auto& state = RenderStates[slotIndex];
    if (!EnsureTooltipQualified(slotIndex, state)) {
        return false;
    }

    const auto stringObject = state.qualifiedTooltipWidget + state.qualifiedTooltipFieldOffset;
    std::uintptr_t dataPointer{};
    std::uint64_t capacityFlags{};
    if (!ReadNativeField(
            reinterpret_cast<void*>(state.qualifiedTooltipWidget),
            state.qualifiedTooltipFieldOffset + Native::Contract::BlizzardStringDataOffset,
            dataPointer)
        || !ReadNativeField(
            reinterpret_cast<void*>(state.qualifiedTooltipWidget),
            state.qualifiedTooltipFieldOffset + Native::Contract::BlizzardStringCapacityFlagsOffset,
            capacityFlags)
        || dataPointer == 0
        || dataPointer != state.qualifiedTooltipBuffer) {
        return false;
    }

    const auto capacity = capacityFlags & Native::Contract::BlizzardStringCapacityMask;
    const bool embedded = (capacityFlags & Native::Contract::BlizzardStringEmbeddedFlag) != 0;
    if (embedded
        || !IsWritableRange(
            reinterpret_cast<void*>(stringObject + Native::Contract::BlizzardStringSizeOffset),
            sizeof(std::uint64_t))) {
        return false;
    }

    std::uint64_t nativeLength{};
    if (state.qualifiedTooltipEncoding == TooltipStorageEncoding::BlizzardUtf8) {
        if (capacity < TooltipReserveLength
            || !IsWritableRange(reinterpret_cast<void*>(dataPointer), TooltipReserveBytes)) {
            return false;
        }
        auto* buffer = reinterpret_cast<char*>(dataPointer);
        if (!Internal::StoreTooltipText(buffer, std::string_view(text, utf8Length))) {
            return false;
        }
        nativeLength = utf8Length;
        if (std::memcmp(buffer, text, utf8Length + 1) != 0) {
            return false;
        }
    } else if (state.qualifiedTooltipEncoding == TooltipStorageEncoding::BlizzardUtf16) {
        if (capacity < TooltipReserveCodepoints) {
            return false;
        }
        std::array<std::uint16_t, TooltipReserveCodepoints + 1> converted{};
        std::size_t convertedLength{};
        if (!Utf8ToUtf16(text, converted, convertedLength)) {
            return false;
        }
        constexpr std::size_t Utf16Bytes = (TooltipReserveCodepoints + 1) * sizeof(std::uint16_t);
        if (!IsWritableRange(reinterpret_cast<void*>(dataPointer), Utf16Bytes)) {
            return false;
        }
        std::memset(reinterpret_cast<void*>(dataPointer), 0, Utf16Bytes);
        if (convertedLength != 0) {
            std::memcpy(reinterpret_cast<void*>(dataPointer), converted.data(), convertedLength * sizeof(std::uint16_t));
        }
        nativeLength = convertedLength;
    } else {
        return false;
    }

    std::memcpy(
        reinterpret_cast<void*>(stringObject + Native::Contract::BlizzardStringSizeOffset),
        &nativeLength,
        sizeof(nativeLength));

    std::uint64_t verifyLength{};
    if (!ReadNativeField(
            reinterpret_cast<void*>(state.qualifiedTooltipWidget),
            state.qualifiedTooltipFieldOffset + Native::Contract::BlizzardStringSizeOffset,
            verifyLength)
        || verifyLength != nativeLength) {
        return false;
    }

    return true;
}

void ClearTooltip(std::size_t slotIndex) noexcept {
    if (slotIndex >= SlotCount) return;
    (void)SetVisible(Handles[slotIndex].tooltip, false);
    if (WriteTooltipText(slotIndex, "")) {
        RenderStates[slotIndex].tooltipVisible = false;
    }
}

void ApplyTooltip(
    std::size_t slotIndex,
    std::int32_t sourceSkillId) noexcept {
    if (slotIndex >= SlotCount || !ResolveWidgetHandles()) return;

    (void)SetVisible(Handles[slotIndex].tooltip, false);
    char localizedName[TooltipReserveBytes]{};
    if (sourceSkillId == Core::NoSourceSkillId
        || !Internal::TryResolveSkillName(sourceSkillId, localizedName, sizeof(localizedName))) {
        ClearTooltip(slotIndex);
        return;
    }

    if (!WriteTooltipText(slotIndex, localizedName)) {
        RenderStates[slotIndex].tooltipVisible = false;
        return;
    }

    // Native hover is optional: a FocusableWidget may intercept world clicks.
    // Preserve the valid-text visibility guard independently of the mouse mode.
    RenderStates[slotIndex].tooltipVisible = SetVisible(Handles[slotIndex].tooltip, true);
}

[[nodiscard]] bool ResolveEntryIcon(
    const Core::BuffDisplayEntry& entry,
    SkillIconDescriptor& descriptor) noexcept {
    descriptor = {};
    if (entry.iconFrame != Core::AutoIconFrame) {
        descriptor.frame = entry.iconFrame;
        descriptor.atlas = entry.iconAtlas == Core::BuffIconAtlas::Auto
            ? Core::BuffIconAtlas::Global
            : entry.iconAtlas;
        return true;
    }
    if (entry.sourceSkillId != Core::NoSourceSkillId
        && Internal::TryResolveSkillIcon(entry.sourceSkillId, descriptor)) {
        return true;
    }
    descriptor = {.atlas = Core::BuffIconAtlas::Global, .frame = 0};
    return false;
}

[[nodiscard]] bool IsFrameInFuture(std::uint32_t expire, std::uint32_t now) noexcept {
    return static_cast<std::int32_t>(expire - now) > 0;
}

[[nodiscard]] std::uint32_t RemainingSeconds(std::uint32_t expire, std::uint32_t now) noexcept {
    const auto delta = static_cast<std::uint32_t>(expire - now);
    return (delta + FramesPerSecond - 1) / FramesPerSecond;
}

void FormatSeconds(std::uint32_t seconds, char (&output)[16]) noexcept {
    if (seconds >= 3600) {
        const auto hours = seconds / 3600;
        const auto minutes = (seconds % 3600) / 60;
        std::snprintf(output, sizeof(output), "%u:%02u", hours, minutes);
    } else if (seconds >= 60) {
        std::snprintf(output, sizeof(output), "%u:%02u", seconds / 60, seconds % 60);
    } else {
        std::snprintf(output, sizeof(output), "%u", seconds);
    }
}

[[nodiscard]] bool ApplyIcon(std::size_t slotIndex, const SkillIconDescriptor& icon) noexcept {
    if (slotIndex >= SlotCount || !ResolveWidgetHandles()) return false;
    auto& handles = Handles[slotIndex];
    const auto selected = AtlasIndex(icon.atlas);

    for (std::size_t i = 0; i < handles.icons.size(); ++i) {
        const bool selectedIcon = i == selected;
        // BuffHud is display-only. Even when a buff is active, its atlas button
        // must stay click-through, so keep every ButtonWidget disabled and only
        // use visibility to choose the currently rendered atlas.
        if (!SetEnabled(handles.icons[i], false)) {
        }
        (void)SetVisible(handles.icons[i], selectedIcon);
    }

    void* nativeIcon = ResolveNativeSlotChild(slotIndex, AtlasWidgetNames[selected]);
    const auto declaredFrame = static_cast<std::uint16_t>(slotIndex * 2);
    if (Internal::TryApplyIconFrame(nativeIcon, icon.frame, declaredFrame)) return true;

    // Fail closed if the qualified live-frame path cannot be applied. The atlas
    // selection remains valid, but the slot keeps its JSON witness frame and the
    // fallback counter makes the failure visible in `buff-panel status`.
    return false;
}

void HideSlot(std::size_t slotIndex) noexcept {
    if (slotIndex >= SlotCount || !ResolveWidgetHandles()) return;
    auto& state = RenderStates[slotIndex];
    if (!state.visible) return;

    // Visibility alone does not remove ButtonWidget hit targets. Once an
    // occupied slot becomes empty, disable every atlas child before hiding it.
    // Initial empty slots were already disabled by ResolveWidgetHandles().
    for (const auto icon : Handles[slotIndex].icons) {
        if (!SetEnabled(icon, false)) {
        }
    }
    ClearTooltip(slotIndex);
    state.tooltipVisible = false;

    (void)SetVisible(Handles[slotIndex].slot, false);
    const auto qualifiedTimerBuffer = state.qualifiedTimerBuffer;
    const auto qualifiedTooltipWidget = state.qualifiedTooltipWidget;
    const auto qualifiedTooltipBuffer = state.qualifiedTooltipBuffer;
    const auto qualifiedTooltipFieldOffset = state.qualifiedTooltipFieldOffset;
    const auto qualifiedTooltipEncoding = state.qualifiedTooltipEncoding;
    state = {};
    state.qualifiedTimerBuffer = qualifiedTimerBuffer;
    state.qualifiedTooltipWidget = qualifiedTooltipWidget;
    state.qualifiedTooltipBuffer = qualifiedTooltipBuffer;
    state.qualifiedTooltipFieldOffset = qualifiedTooltipFieldOffset;
    state.qualifiedTooltipEncoding = qualifiedTooltipEncoding;
}

void RenderSlot(
    std::size_t slotIndex,
    const Core::BuffDisplayEntry& entry,
    std::uint32_t nowFrame,
    bool hasClock) noexcept {
    if (slotIndex >= SlotCount || !ResolveWidgetHandles()) return;

    SkillIconDescriptor icon{};
    const bool iconResolved = ResolveEntryIcon(entry, icon);
    auto& state = RenderStates[slotIndex];
    const bool assignmentChanged = !state.visible
        || state.key != entry.key
        || state.atlas != icon.atlas
        || state.requestedFrame != icon.frame
        || state.displayMode != entry.displayMode
        || state.sourceSkillId != entry.sourceSkillId;

    if (assignmentChanged) {
        (void)SetVisible(Handles[slotIndex].slot, false);
        const bool iconApplied = ApplyIcon(slotIndex, icon);
        state.key = entry.key;
        state.atlas = icon.atlas;
        state.requestedFrame = icon.frame;
        state.renderedFrame = iconApplied
            ? icon.frame
            : static_cast<std::uint16_t>(slotIndex * 2);
        state.displayMode = entry.displayMode;
        state.sourceSkillId = entry.sourceSkillId;
        state.seconds = NoRenderedSeconds;
        state.currentValue = NoRenderedValue;
        state.maximumValue = NoRenderedValue;
        state.timerVisible = false;
        state.tooltipVisible = false;
        ApplyTooltip(slotIndex, entry.sourceSkillId);
        state.visible = true;
        (void)SetVisible(Handles[slotIndex].slot, true);
    }

    if (entry.displayMode == Core::BuffDisplayMode::Resource) {
        if (state.timerVisible && state.currentValue == entry.currentValue) {
            return;
        }

        char text[32]{};
        std::snprintf(text, sizeof(text), "%d", entry.currentValue);
        if (WriteCountdownText(slotIndex, text)) {
            (void)SetVisible(Handles[slotIndex].countdown, true);
            state.timerVisible = true;
            state.seconds = NoRenderedSeconds;
            state.currentValue = entry.currentValue;
            state.maximumValue = entry.maximumValue;
        }
        return;
    }

    const bool wantsTimer = entry.expireGameFrame != 0;
    if (!wantsTimer) {
        if (state.timerVisible) {
            (void)SetVisible(Handles[slotIndex].countdown, false);
            state.timerVisible = false;
            state.seconds = NoRenderedSeconds;
            state.currentValue = NoRenderedValue;
            state.maximumValue = NoRenderedValue;
        }
        return;
    }

    if (!hasClock) {
        if (state.seconds != NoRenderedSeconds || !state.timerVisible) {
            if (WriteCountdownText(slotIndex, "--")) {
                (void)SetVisible(Handles[slotIndex].countdown, true);
                state.timerVisible = true;
                state.seconds = NoRenderedSeconds;
            }
        }
        return;
    }

    const auto seconds = RemainingSeconds(entry.expireGameFrame, nowFrame);
    if (state.timerVisible && state.seconds == seconds) return;

    char text[16]{};
    FormatSeconds(seconds, text);
    if (WriteCountdownText(slotIndex, text)) {
        (void)SetVisible(Handles[slotIndex].countdown, true);
        state.timerVisible = true;
        state.seconds = seconds;
    }
}

[[nodiscard]] bool EffectiveFrame(
    const Core::BuffDisplaySnapshot& snapshot,
    std::uint32_t& frame) noexcept {
    if (!snapshot.hasGameFrame) { frame = 0; return false; }
    frame = snapshot.currentGameFrame;
    return true;
}

void RenderSnapshot() noexcept {
    if (!ResolveWidgetHandles()) return;

    auto snapshot = Core::BuffDisplays().Snapshot();
    std::uint32_t nowFrame{};
    const bool hasClock = EffectiveFrame(snapshot, nowFrame);

    // The display bus is not gameplay state. Once authoritative time says an
    // entry is over, removing it from the display registry is safe and keeps
    // stale producers from occupying HUD slots forever.
    if (hasClock) {
        std::array<std::uint64_t, Core::MaximumPublishedBuffs> expired{};
        std::size_t expiredCount{};
        for (std::size_t i = 0; i < snapshot.count; ++i) {
            const auto& entry = snapshot.entries[i];
            if (entry.expireGameFrame != 0
                && !IsFrameInFuture(entry.expireGameFrame, nowFrame)) {
                expired[expiredCount++] = entry.key;
            }
        }
        if (expiredCount != 0) {
            for (std::size_t i = 0; i < expiredCount; ++i) {
                if (Core::BuffDisplays().Remove(expired[i])) {
                }
            }
            snapshot = Core::BuffDisplays().Snapshot();
            if (snapshot.count == 0 && !snapshot.hasGameFrame) {
            }
        }
    }

    std::array<Core::BuffDisplayEntry, Core::MaximumPublishedBuffs> ordered{};
    std::size_t orderedCount{};
    for (std::size_t i = 0; i < snapshot.count; ++i) ordered[orderedCount++] = snapshot.entries[i];
    std::sort(
        ordered.begin(),
        ordered.begin() + static_cast<std::ptrdiff_t>(orderedCount),
        [](const Core::BuffDisplayEntry& lhs, const Core::BuffDisplayEntry& rhs) noexcept {
            if (lhs.priority != rhs.priority) return lhs.priority > rhs.priority;
            return lhs.sequence < rhs.sequence;
        });

    const auto visibleCount = std::min<std::size_t>(orderedCount, SlotCount);
    for (std::size_t i = 0; i < visibleCount; ++i) {
        RenderSlot(i, ordered[i], nowFrame, hasClock);
    }
    for (std::size_t i = visibleCount; i < SlotCount; ++i) HideSlot(i);

    LastRenderedRevision = snapshot.revision;
}

void QueuePoll() noexcept;

void __cdecl PollOnUiThread(const D2RL::PluginContext* context, void*) noexcept {
    PollScheduled.store(false, std::memory_order_release);
    if (context == nullptr || context != Context
        || CurrentSessionGeneration.load(std::memory_order_acquire) == 0) {
        return;
    }

    RenderSnapshot();
    QueuePoll();
}

void QueuePoll() noexcept {
    if (Context == nullptr || Threads == nullptr
        || CurrentSessionGeneration.load(std::memory_order_acquire) == 0) {
        return;
    }
    bool expected = false;
    if (!PollScheduled.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
        return;
    }
    if (Threads->runOnUiThread(Context, &PollOnUiThread, nullptr)
        != D2RL::Threads::Result::Success) {
        PollScheduled.store(false, std::memory_order_release);
    }
}

void OpenPanel() noexcept {
    if (Context == nullptr || Panels == nullptr
        || RegisteredPanel == D2RL::Panels::InvalidHandle) return;
    const auto result = Panels->openPanel(Context, RegisteredPanel);
    if (result != D2RL::Panels::Result::Success) {
        return;
    }
    InvalidateWidgetHandles();
    if (ResolveWidgetHandles()) {
        for (std::size_t i = 0; i < SlotCount; ++i) {
            (void)SetVisible(Handles[i].slot, false);
            ClearTooltip(i);
        }
    }
}

void ClosePanel() noexcept {
    RestoreQualifiedCountdownBuffers();
    RestoreQualifiedTooltipBuffers();
    if (Context != nullptr && Panels != nullptr
        && RegisteredPanel != D2RL::Panels::InvalidHandle) {
        (void)Panels->closePanel(Context, RegisteredPanel);
    }
    InvalidateWidgetHandles();
}

void __cdecl RebuildIconsOnGameThread(const D2RL::PluginContext*, void*) noexcept {
    const auto previous = Internal::SkillIconStatus();
    const auto revision = previous.tableRevision == 0 ? 1 : previous.tableRevision;
    const bool ready = Internal::RebuildSkillIconCache(revision);
    if (Context != nullptr) {
        const auto status = Internal::SkillIconStatus();
        char line[384]{};
        std::snprintf(
            line,
            sizeof(line),
            "Buff HUD: manual skill presentation cache rebuild %s bank=%u skills=%u desc=%u rowSize=%u/%u candidates(link=%u class=%u name=%u) offsets(link=0x%X class=0x%X icon=0x%X nameId=0x%X) names=%s.",
            ready ? "succeeded" : "FAILED",
            status.bank,
            status.skillCount,
            status.skillDescCount,
            status.skillsRowSize,
            status.skillDescRowSize,
            status.linkCandidateCount,
            status.classCandidateCount,
            status.nameCandidateCount,
            status.skillDescLinkOffset,
            status.skillClassOffset,
            status.iconCelOffset,
            status.skillNameStringIdOffset,
            status.namesReady ? "ready" : "not-qualified");
        ready ? Context->LogInfo(line) : Context->LogWarn(line);
    }
}

void __cdecl OnDataTablesLoaded(
    const D2RL::PluginContext*,
    const D2RL::Lifecycle::DataTablesLoadedEvent* event,
    void*) noexcept {
    if (event == nullptr) return;
    const bool ready = Internal::RebuildSkillIconCache(event->revision);
    if (Context == nullptr) return;
    const auto status = Internal::SkillIconStatus();
    char line[384]{};
    std::snprintf(
        line,
        sizeof(line),
        "Buff HUD: skill presentation table resolver %s revision=%llu bank=%u skills=%u desc=%u rowSize=%u/%u candidates(link=%u class=%u name=%u) offsets(link=0x%X class=0x%X icon=0x%X nameId=0x%X) names=%s.",
        ready ? "ready" : "not-qualified",
        static_cast<unsigned long long>(event->revision),
        status.bank,
        status.skillCount,
        status.skillDescCount,
        status.skillsRowSize,
        status.skillDescRowSize,
        status.linkCandidateCount,
        status.classCandidateCount,
        status.nameCandidateCount,
        status.skillDescLinkOffset,
        status.skillClassOffset,
        status.iconCelOffset,
        status.skillNameStringIdOffset,
        status.namesReady ? "ready" : "not-qualified");
    ready ? Context->LogInfo(line) : Context->LogWarn(line);
}

void __cdecl OnGameplayEvent(
    const D2RL::PluginContext*,
    const D2RL::Lifecycle::GameplayEvent* event,
    void*) noexcept {
    if (event == nullptr) return;

    switch (event->kind) {
    case D2RL::Lifecycle::GameplayEventKind::GameJoined:
        CurrentSessionGeneration.store(event->sessionGeneration, std::memory_order_release);
        Core::BuffDisplays().BeginSession(event->sessionGeneration);
        break;
    case D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady:
        if (CurrentSessionGeneration.load(std::memory_order_acquire) != event->sessionGeneration) {
            CurrentSessionGeneration.store(event->sessionGeneration, std::memory_order_release);
            Core::BuffDisplays().BeginSession(event->sessionGeneration);
        }
        OpenPanel();
        QueuePoll();
        if (!Internal::SkillIconStatus().ready && Threads != nullptr) {
            (void)Threads->runOnGameThread(Context, &RebuildIconsOnGameThread, nullptr);
        }
        break;
    case D2RL::Lifecycle::GameplayEventKind::GameLeft:
        Core::BuffDisplays().EndSession(event->sessionGeneration);
        CurrentSessionGeneration.store(0, std::memory_order_release);
        PollScheduled.store(false, std::memory_order_release);
        ClosePanel();
        break;
    default:
        break;
    }
}

void PrintStatus(const D2RL::PluginContext* context) noexcept {
    if (context == nullptr) return;
    const auto snapshot = Core::BuffDisplays().Snapshot();
    const auto icons = Internal::SkillIconStatus();
    char line[512]{};
    std::snprintf(line, sizeof(line),
        "Buff HUD 1.0.11: displayed=%zu/%zu session=%llu frame=%u panel=%s companion=enabled skillIcons=%s skillNames=%s tableRevision=%llu inputIsolation=enabled.",
        snapshot.count, SlotCount,
        static_cast<unsigned long long>(snapshot.sessionGeneration),
        snapshot.currentGameFrame,
        RegisteredPanel != D2RL::Panels::InvalidHandle ? "registered" : "missing",
        icons.ready ? "ready" : "not-qualified",
        icons.namesReady ? "ready" : "not-qualified",
        static_cast<unsigned long long>(icons.tableRevision));
    context->WriteConsoleMessage(line);
}

D2RL::ConsoleCommandResult __cdecl BuffCommand(
    D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept {
    if (command == nullptr || command->plugin == nullptr) return D2RL::ConsoleCommandResult::Failed;
    const std::string_view args = command->args != nullptr
        ? std::string_view(command->args, command->argsLength) : std::string_view{};
    const auto first = args.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        PrintStatus(command->plugin); return D2RL::ConsoleCommandResult::Handled;
    }
    const auto last = args.find_last_not_of(" \t\r\n");
    if (args.substr(first, last - first + 1) == "status") {
        PrintStatus(command->plugin); return D2RL::ConsoleCommandResult::Handled;
    }
    command->plugin->WriteConsoleMessage("Usage: buff-panel [status]");
    return D2RL::ConsoleCommandResult::InvalidArguments;
}

[[nodiscard]] bool RegisterLayoutAndPanel() noexcept {
    if (Context == nullptr || Panels == nullptr) return false;
    // D2RLoader resolves the layout from our companion MPQ, including packed
    // active-mod overrides. Do not shadow it with a stale in-memory copy.
    const D2RL::Panels::PanelRegistration panel{
        .structSize = D2RL::Panels::PanelRegistrationSize,
        .flags = D2RL::Panels::PanelFlags::None,
        .localId = "BuffHud",
    };
    if (Panels->registerPanel(Context, &panel, &RegisteredPanel)
        != D2RL::Panels::Result::Success) {
        Context->LogError("Buff HUD: failed to register plugin-owned BuffHud panel.");
        return false;
    }
    return true;
}

[[nodiscard]] bool RegisterLifecycle() noexcept {
    if (Context == nullptr || Lifecycle == nullptr) return false;
    const D2RL::Lifecycle::DataTablesLoadedListener tableListener{
        .structSize = D2RL::Lifecycle::DataTablesLoadedListenerSize,
        .flags = 0,
        .callback = &OnDataTablesLoaded,
        .userData = nullptr,
    };
    if (Lifecycle->registerDataTablesLoadedListener(Context, &tableListener, &DataTablesListener)
        != D2RL::Lifecycle::Result::Success) {
        return false;
    }

    constexpr std::array kinds{
        D2RL::Lifecycle::GameplayEventKind::GameJoined,
        D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady,
        D2RL::Lifecycle::GameplayEventKind::GameLeft,
    };
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        const D2RL::Lifecycle::GameplayEventListener listener{
            .structSize = D2RL::Lifecycle::GameplayEventListenerSize,
            .flags = 0,
            .kind = kinds[i],
            .reserved = 0,
            .callback = &OnGameplayEvent,
            .userData = nullptr,
        };
        if (Lifecycle->registerGameplayEventListener(Context, &listener, &GameplayListeners[i])
            != D2RL::Lifecycle::Result::Success) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ValidateNativeUiContract() noexcept {
    if (Context == nullptr || Context->exeBase == 0) return false;
    if (!Context->CheckExpectedBytes(
            Native::Contract::FindTopLevelPanelRva,
            Native::Contract::FindTopLevelPanelExpected.data(),
            static_cast<std::uint32_t>(Native::Contract::FindTopLevelPanelExpected.size()))
        || !Context->CheckExpectedBytes(
            Native::Contract::FindChildWidgetByNameRva,
            Native::Contract::FindChildWidgetByNameExpected.data(),
            static_cast<std::uint32_t>(Native::Contract::FindChildWidgetByNameExpected.size()))) {
        Context->LogError("Buff HUD: native UI resolver fingerprint mismatch; refusing BuffHud initialization.");
        return false;
    }
    FindTopLevelPanel = reinterpret_cast<FindTopLevelPanelFn>(
        Context->exeBase + Native::Contract::FindTopLevelPanelRva);
    FindChildWidgetByName = reinterpret_cast<FindChildWidgetByNameFn>(
        Context->exeBase + Native::Contract::FindChildWidgetByNameRva);
    return true;
}

} // namespace

bool Initialize(const D2RL::PluginContext* context) noexcept {
    Shutdown();
    if (context == nullptr) return false;
    Context = context;

    const auto& services = Core::Services();
    Panels = services.panels;
    Widgets = services.widgets;
    Threads = services.threads;
    Lifecycle = services.lifecycle;

    const bool panelsReady = Panels != nullptr
        && D2RL::HasPanelServiceField(Panels, D2RL::PanelServiceRequiredSize);
    const bool widgetsReady = Widgets != nullptr
        && D2RL::HasWidgetServiceField(Widgets, D2RL::WidgetServiceRequiredSize);
    const bool threadsReady = Threads != nullptr
        && D2RL::HasThreadServiceField(Threads, D2RL::ThreadServiceRequiredSize);
    const bool lifecycleReady = Lifecycle != nullptr
        && D2RL::HasLifecycleServiceField(Lifecycle, D2RL::LifecycleServiceRequiredSize);

    if (!panelsReady || !widgetsReady || !threadsReady || !lifecycleReady) {
        char serviceLine[256]{};
        std::snprintf(
            serviceLine,
            sizeof(serviceLine),
            "Buff HUD: required PluginSDK service unavailable/undersized: Panel=%d Widget=%d Thread=%d Lifecycle=%d.",
            panelsReady ? 1 : 0,
            widgetsReady ? 1 : 0,
            threadsReady ? 1 : 0,
            lifecycleReady ? 1 : 0);
        Context->LogError(serviceLine);
        Shutdown();
        return false;
    }

    // D2RLoader ABI 4 does not guarantee that every named service is active for
    // every loader build/runtime phase. Localization is presentation-only here:
    // icons/timers/resources remain fully functional without it. The skill cache
    // retries LocalizationService when data tables are ready, so a service that
    // becomes available later still restores localized hover names.
    const bool localizationReady = Core::LocalizationService() != nullptr;
    if (!localizationReady) {
        Context->LogWarn(
            "Buff HUD: LocalizationService unavailable at plugin initialization; BuffHud remains enabled and localized hover names will be retried when the skill cache is rebuilt.");
    }

    if (!ValidateNativeUiContract()
        || !Internal::InitializeIconFrameBackend(Context)
        || !RegisterLayoutAndPanel()
        || !RegisterLifecycle()) {
        Shutdown();
        return false;
    }

    if (!Context->RegisterConsoleCommand(
            "buff-panel",
            &BuffCommand,
            "Show Buff HUD status.")) {
        Context->LogWarn("Buff HUD: console command 'buff-panel' could not be registered.");
    }
    Context->LogInfo(
        "Buff HUD 1.0.11 BuffHud initialized: 21 display-only slots, companion-resource layout, fixed gameplay input isolation, timer/resource rendering, and runtime skill icon/name resolution.");
    return true;
}

void Shutdown() noexcept {
    Core::BuffDisplays().EndSession();
    Internal::ResetSkillIconCache();
    Internal::ShutdownIconFrameBackend();
    CurrentSessionGeneration.store(0, std::memory_order_release);
    PollScheduled.store(false, std::memory_order_release);
    // Restore any native countdown backing buffers we qualified before dropping
    // their pointers. This is bounded/readability-checked and does not call UI
    // services from the loader shutdown thread.
    RestoreQualifiedCountdownBuffers();
    RestoreQualifiedTooltipBuffers();
    InvalidateWidgetHandles();

    RegisteredPanel = D2RL::Panels::InvalidHandle;
    DataTablesListener = D2RL::Lifecycle::InvalidHandle;
    GameplayListeners = {};
    FindTopLevelPanel = nullptr;
    FindChildWidgetByName = nullptr;
    Panels = nullptr;
    Widgets = nullptr;
    Threads = nullptr;
    Lifecycle = nullptr;
    Context = nullptr;
}

} // namespace BuffPanel::Systems::BuffHud

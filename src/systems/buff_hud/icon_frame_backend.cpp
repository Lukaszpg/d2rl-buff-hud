#include "icon_frame_backend.hpp"

#include "native/native_contract.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace BuffPanel::Systems::BuffHud::Internal {
namespace {

namespace Native = BuffPanel::Native::Contract;

using LiveFrameSetterFn = void(__fastcall*)(void* renderSubobject, std::int32_t frame) noexcept;
using ButtonRefreshFn = void(__fastcall*)(void* widget) noexcept;

std::atomic<bool> Ready{};
std::uintptr_t ExeBase{};
LiveFrameSetterFn LiveFrameSetter{};
ButtonRefreshFn ButtonRefresh{};

[[nodiscard]] bool IsReadableRange(const void* address, std::size_t size) noexcept {
    if (address == nullptr || size == 0) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    if (start < 0x10000u || end < start) return false;
    __try {
        const auto* bytes = static_cast<const volatile std::uint8_t*>(address);
        for (std::size_t offset = 0; offset < size; offset += 4096) {
            (void)bytes[offset];
        }
        (void)bytes[size - 1];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

[[nodiscard]] bool IsWritableRange(void* address, std::size_t size) noexcept {
    if (address == nullptr || size == 0) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    if (start < 0x10000u || end < start) return false;
    __try {
        auto* bytes = static_cast<volatile std::uint8_t*>(address);
        for (std::size_t offset = 0; offset < size; offset += 4096) {
            const std::uint8_t value = bytes[offset];
            bytes[offset] = value;
        }
        const std::uint8_t last = bytes[size - 1];
        bytes[size - 1] = last;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
[[nodiscard]] bool ReadValue(std::uintptr_t address, T& value) noexcept {
    if (address < 0x10000u) return false;
    const auto* source = reinterpret_cast<const void*>(address);
    __try {
        std::memcpy(&value, source, sizeof(T));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
[[nodiscard]] bool WriteWidgetValue(void* widget, std::size_t offset, const T& value) noexcept {
    if (widget == nullptr) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(widget);
    if (offset > static_cast<std::size_t>(std::numeric_limits<std::uintptr_t>::max() - base)) return false;
    auto* destination = reinterpret_cast<void*>(base + offset);
    if (!IsWritableRange(destination, sizeof(T))) return false;
    std::memcpy(destination, &value, sizeof(T));
    T verify{};
    std::memcpy(&verify, destination, sizeof(T));
    return std::memcmp(&verify, &value, sizeof(T)) == 0;
}

[[nodiscard]] bool DecodeRelativeCallTarget(
    std::uintptr_t callRva,
    std::uintptr_t& targetRva) noexcept {
    targetRva = 0;
    if (ExeBase == 0) return false;
    const auto address = ExeBase + callRva;
    std::array<std::uint8_t, 5> call{};
    if (!ReadValue(address, call) || call[0] != 0xE8U) return false;
    std::int32_t displacement{};
    std::memcpy(&displacement, call.data() + 1, sizeof(displacement));
    const auto target = static_cast<std::int64_t>(callRva + call.size())
        + static_cast<std::int64_t>(displacement);
    if (target < 0) return false;
    targetRva = static_cast<std::uintptr_t>(target);
    return true;
}

[[nodiscard]] bool ValidateDescriptorContract() noexcept {
    if (ExeBase == 0) return false;

    std::uintptr_t normalName{};
    std::uintptr_t pressedName{};
    std::uint64_t normalMember{};
    std::uint64_t pressedMember{};
    std::uint32_t normalWidth{};
    std::uint32_t pressedWidth{};

    return ReadValue(ExeBase + Native::ButtonNormalFrameDescriptorRva, normalName)
        && ReadValue(
            ExeBase + Native::ButtonPressedFrameDescriptorRva,
            pressedName)
        && ReadValue(
            ExeBase + Native::ButtonNormalFrameDescriptorRva
                + Native::ButtonFrameDescriptorMemberOffsetField,
            normalMember)
        && ReadValue(
            ExeBase + Native::ButtonPressedFrameDescriptorRva
                + Native::ButtonFrameDescriptorMemberOffsetField,
            pressedMember)
        && ReadValue(
            ExeBase + Native::ButtonNormalFrameDescriptorRva
                + Native::ButtonFrameDescriptorStorageWidthField,
            normalWidth)
        && ReadValue(
            ExeBase + Native::ButtonPressedFrameDescriptorRva
                + Native::ButtonFrameDescriptorStorageWidthField,
            pressedWidth)
        && normalName == ExeBase + Native::ButtonNormalFrameNameRva
        && pressedName == ExeBase + Native::ButtonPressedFrameNameRva
        && Native::ButtonPressedFrameDescriptorRva - Native::ButtonNormalFrameDescriptorRva
            == Native::ButtonFrameDescriptorStride
        && normalMember == Native::ButtonNormalFrameOffset
        && pressedMember == Native::ButtonPressedFrameOffset
        && normalWidth == sizeof(std::uint32_t)
        && pressedWidth == sizeof(std::uint32_t);
}

[[nodiscard]] bool ValidateLiveApplyContract(
    const D2RL::PluginContext* context) noexcept {
    if (context == nullptr || context->exeBase == 0) return false;
    if (!context->CheckExpectedBytes(
            Native::ButtonPressedApplyBlockRva,
            Native::ButtonPressedApplyExpected.data(),
            static_cast<std::uint32_t>(Native::ButtonPressedApplyExpected.size()))
        || !context->CheckExpectedBytes(
            Native::ButtonNormalApplyBlockRva,
            Native::ButtonNormalApplyExpected.data(),
            static_cast<std::uint32_t>(Native::ButtonNormalApplyExpected.size()))) {
        return false;
    }

    std::uintptr_t pressedSetter{};
    std::uintptr_t normalSetter{};
    std::uintptr_t pressedRefresh{};
    std::uintptr_t normalRefresh{};
    return DecodeRelativeCallTarget(Native::ButtonPressedSetterCallRva, pressedSetter)
        && DecodeRelativeCallTarget(Native::ButtonNormalSetterCallRva, normalSetter)
        && DecodeRelativeCallTarget(Native::ButtonPressedRefreshCallRva, pressedRefresh)
        && DecodeRelativeCallTarget(Native::ButtonNormalRefreshCallRva, normalRefresh)
        && pressedSetter == Native::ButtonLiveFrameSetterRva
        && normalSetter == Native::ButtonLiveFrameSetterRva
        && pressedRefresh == Native::ButtonRefreshRva
        && normalRefresh == Native::ButtonRefreshRva;
}

} // namespace

bool InitializeIconFrameBackend(const D2RL::PluginContext* context) noexcept {
    ShutdownIconFrameBackend();
    if (context == nullptr || context->exeBase == 0) return false;

    ExeBase = context->exeBase;
    if (!ValidateDescriptorContract() || !ValidateLiveApplyContract(context)) {
        ShutdownIconFrameBackend();
        return false;
    }

    LiveFrameSetter = reinterpret_cast<LiveFrameSetterFn>(
        ExeBase + Native::ButtonLiveFrameSetterRva);
    ButtonRefresh = reinterpret_cast<ButtonRefreshFn>(
        ExeBase + Native::ButtonRefreshRva);
    Ready.store(true, std::memory_order_release);
    return true;
}

void ShutdownIconFrameBackend() noexcept {
    Ready.store(false, std::memory_order_release);
    LiveFrameSetter = nullptr;
    ButtonRefresh = nullptr;
    ExeBase = 0;
}

IconFrameBackendState IconFrameBackendStatus() noexcept {
    return Ready.load(std::memory_order_acquire)
        ? IconFrameBackendState::Ready
        : IconFrameBackendState::Unavailable;
}

bool TryApplyIconFrame(
    void* buttonWidget,
    std::uint16_t requestedFrame,
    std::uint16_t declaredFrame) noexcept {
    (void)declaredFrame;
    if (buttonWidget == nullptr
        || requestedFrame == std::numeric_limits<std::uint16_t>::max()
        || !Ready.load(std::memory_order_acquire)
        || LiveFrameSetter == nullptr
        || ButtonRefresh == nullptr) {
        return false;
    }

    // IconCel is the normal-frame index. D2R's native skill-icon atlases store
    // the pressed variant immediately after it. Keep the parsed ButtonWidget
    // configuration coherent so later stock refresh/input paths do not restore
    // the JSON witness frame, then invoke the visually-qualified live path.
    const std::uint32_t normal = requestedFrame;
    const std::uint32_t pressed = normal + 1U;
    if (!WriteWidgetValue(buttonWidget, Native::ButtonNormalFrameOffset, normal)
        || !WriteWidgetValue(buttonWidget, Native::ButtonPressedFrameOffset, pressed)) {
        return false;
    }

    auto* renderSubobject = static_cast<std::uint8_t*>(buttonWidget)
        + Native::ButtonFrameRenderSubobjectOffset;
    LiveFrameSetter(renderSubobject, static_cast<std::int32_t>(normal));
    ButtonRefresh(buttonWidget);
    return true;
}

} // namespace BuffPanel::Systems::BuffHud::Internal

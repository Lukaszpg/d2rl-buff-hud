#include "systems/buff_hud/tooltip_text.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace BuffPanel::Systems::BuffHud::Internal;

int main() {
    assert(!StoreTooltipText(nullptr, "Bone Armor"));
    std::array<char, TooltipReserveBytes> reserve{};
    for (std::size_t n = 0; n < 42; ++n) std::memcpy(reserve.data() + n * 3, "\xE2\x80\x8B", 3);
    // Live native buffer: 126 bytes + NUL, followed by an unrelated byte.
    std::array<unsigned char, TooltipReserveBytes + 1> live{};
    std::memcpy(live.data(), reserve.data(), reserve.size());
    live.back() = 0xFF;
    assert(MatchesTooltipReserve(live.data(), reserve.data()));
    assert(StoreTooltipText(live.data(), "Bone Armor"));
    assert(std::strcmp(reinterpret_cast<char*>(live.data()), "Bone Armor") == 0);
    assert(live.back() == 0xFF);
    assert(!MatchesTooltipReserve(live.data(), reserve.data()));
    assert(!StoreTooltipText(live.data(), std::string(127, 'x')));
    assert(live.back() == 0xFF);

    // Exact-size allocation touching a guard page catches a one-byte overflow.
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto pageSize = static_cast<std::size_t>(info.dwPageSize);
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(pages);
    DWORD old{};
    assert(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &old));
#else
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* pages = static_cast<char*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    assert(pages != MAP_FAILED);
    assert(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0);
#endif
    auto* exact = pages + pageSize - TooltipReserveBytes;
    assert(StoreTooltipText(exact, std::string_view(reserve.data(), TooltipReserveLength)));
    assert(MatchesTooltipReserve(exact, reserve.data()));
    assert(StoreTooltipText(exact, "Battle Orders"));
    assert(std::strcmp(exact, "Battle Orders") == 0);
    assert(StoreTooltipText(exact, ""));
    assert(exact[0] == 0);
#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 2);
#endif
    std::cout << "PASS: native tooltip sentinel, exact-capacity guard page, restore/write/clear\n";
}

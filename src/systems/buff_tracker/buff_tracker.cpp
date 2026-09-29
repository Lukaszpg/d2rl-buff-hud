#include "buff_tracker.hpp"
#include "timer_metadata.hpp"

#include "core/buff_display_bus.hpp"
#include "core/services.hpp"
#include "core/stat_read_bus.hpp"
#include "core/stat_list_post_bus.hpp"
#include "native/native_contract.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>


namespace BuffPanel::Systems::BuffTracker {
namespace {

using GetGameFromUnitFn = void*(__fastcall*)(void* unit) noexcept;
using GetStatListFromUnitAndStateFn = void*(__fastcall*)(void* unit, std::int32_t state) noexcept;

// StatList semantic field layout is runtime-qualified on D2R build 93847. buff-hud.txt is the authoritative whitelist and
// also declares how each state is presented: finite timer or resource pool.
// Native CURSE lists remain excluded as a safety boundary. Timer rows require
// finite future expiry metadata. Native skill ID is preferred; timer skill_id
// is an optional fallback when the StatList exposes skill=0. skillLevel is not
// used for qualification. Resource rows read their current/max values through
// the already-owned Core unit-stat reader and do not require an expiry.
constexpr std::uint32_t UnitTypeOffset = 0x00;
constexpr std::uint32_t PlayerUnitType = 0;
constexpr std::uint32_t StatListCurseFlag = 0x00000020u;
constexpr std::uint32_t SharedStashProxyState = 186u; // states.txt: sharedstash; D2RLoader 1.3 proxy marker

const D2RL::PluginContext* Context{};
const D2RL::CustomTableService* CustomTables{};
const D2RL::ThreadService* Threads{};
const D2RL::LifecycleService* Lifecycle{};
GetGameFromUnitFn GetGameFromUnit{};
GetStatListFromUnitAndStateFn GetStatListFromUnitAndState{};
D2RL::CustomTables::TableHandle BuffHudTable{D2RL::CustomTables::InvalidHandle};
D2RL::Lifecycle::ListenerHandle DataTablesListener{D2RL::Lifecycle::InvalidHandle};
std::array<D2RL::Lifecycle::ListenerHandle, 3> GameplayListeners{};

struct BuffHudRow final {
    char name[48]{};
    std::uint32_t stateId{};
    char displayType[16]{};
    std::uint32_t valueStat{};
    std::uint32_t maxStat{};
    std::uint32_t skillId{};
    std::uint8_t valueShift{};
    std::uint8_t enabled{};
};

struct BuffDefinition final {
    std::uint32_t stateId{};
    Core::BuffDisplayMode displayMode{Core::BuffDisplayMode::Timer};
    std::int32_t valueStatId{};
    std::int32_t maxStatId{};
    std::int32_t sourceSkillId{};
    std::uint8_t valueShift{};
};

struct WhitelistCache final {
    std::uint64_t revision{};
    std::vector<BuffDefinition> definitions;
};

std::atomic<std::shared_ptr<const WhitelistCache>> Whitelist{};

struct TimerPresenceRecord final {
    std::uint64_t key{};
    std::uint32_t stateId{};
    std::uint32_t skillId{};
    std::uint32_t expireFrame{};
    std::uint32_t lastPostFrame{};
};

std::mutex TimerPresenceMutex;
std::array<TimerPresenceRecord, Core::MaximumPublishedBuffs> TimerPresenceRecords{};

std::atomic<std::uint64_t> CurrentSessionGeneration{};
std::atomic<void*> AuthoritativeGame{};
std::atomic<void*> AuthoritativePlayer{};
std::atomic<bool> FramePumpScheduled{};

std::atomic<std::uint32_t> LastTimerDiscoveryFrame{}; // once per game frame

constexpr std::array<D2RL::CustomTables::ColumnDefinition, 8> BuffHudColumns{{
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "name",
        .type = D2RL::CustomTables::ColumnType::Ascii,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, name)),
        .length = sizeof(BuffHudRow::name),
    },
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "state_id",
        .type = D2RL::CustomTables::ColumnType::Dword,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, stateId)),
    },
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "display_type",
        .type = D2RL::CustomTables::ColumnType::Ascii,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, displayType)),
        .length = sizeof(BuffHudRow::displayType),
    },
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "value_stat",
        .type = D2RL::CustomTables::ColumnType::Dword,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, valueStat)),
    },
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "max_stat",
        .type = D2RL::CustomTables::ColumnType::Dword,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, maxStat)),
    },
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "skill_id",
        .type = D2RL::CustomTables::ColumnType::Dword,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, skillId)),
    },
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "value_shift",
        .type = D2RL::CustomTables::ColumnType::Byte,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, valueShift)),
    },
    {
        .structSize = D2RL::CustomTables::ColumnDefinitionSize,
        .name = "enabled",
        .type = D2RL::CustomTables::ColumnType::Byte,
        .offset = static_cast<std::uint32_t>(offsetof(BuffHudRow, enabled)),
    },
}};

[[nodiscard]] bool IsReadableRange(const void* address, std::size_t size) noexcept {
    if (address == nullptr || size == 0) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    if (end < start) return false;

    auto current = start;
    while (current < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &info, sizeof(info)) == 0
            || info.State != MEM_COMMIT
            || (info.Protect & PAGE_GUARD) != 0
            || (info.Protect & PAGE_NOACCESS) != 0) {
            return false;
        }
        const DWORD basic = info.Protect & 0xFFu;
        if (basic != PAGE_READONLY
            && basic != PAGE_READWRITE
            && basic != PAGE_WRITECOPY
            && basic != PAGE_EXECUTE
            && basic != PAGE_EXECUTE_READ
            && basic != PAGE_EXECUTE_READWRITE
            && basic != PAGE_EXECUTE_WRITECOPY) {
            return false;
        }
        const auto regionBase = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        const auto regionEnd = regionBase + info.RegionSize;
        if (regionEnd <= current) return false;
        current = regionEnd < end ? regionEnd : end;
    }
    return true;
}

template <typename T>
[[nodiscard]] bool ReadField(const void* base, std::size_t offset, T& value) noexcept {
    if (base == nullptr) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(base);
    if (offset > static_cast<std::size_t>(UINTPTR_MAX - start)) return false;
    const auto* address = reinterpret_cast<const void*>(start + offset);
    if (!IsReadableRange(address, sizeof(T))) return false;
    std::memcpy(&value, address, sizeof(T));
    return true;
}

[[nodiscard]] bool ReadGameFrame(void* game, std::uint32_t& frame) noexcept {
    frame = 0;
    if (reinterpret_cast<std::uintptr_t>(game) < 0x10000u) return false;
    return ReadField(game, Native::Contract::GameFrameOffset, frame) && frame != 0;
}

[[nodiscard]] const BuffDefinition* FindBuffDefinition(
    const std::shared_ptr<const WhitelistCache>& cache,
    std::uint32_t state) noexcept {
    if (cache == nullptr) return nullptr;
    const auto it = std::lower_bound(
        cache->definitions.begin(),
        cache->definitions.end(),
        state,
        [](const BuffDefinition& definition, std::uint32_t value) noexcept {
            return definition.stateId < value;
        });
    return it != cache->definitions.end() && it->stateId == state ? &*it : nullptr;
}

[[nodiscard]] bool CopyWhitelistRows(
    std::uint64_t& revision,
    std::uint32_t& rowCount,
    std::vector<std::uint8_t>& bytes) noexcept {
    if (Context == nullptr || CustomTables == nullptr
        || BuffHudTable == D2RL::CustomTables::InvalidHandle) {
        return false;
    }
    D2RL::CustomTables::TableInfo info{};
    info.structSize = D2RL::CustomTables::TableInfoSize;
    if (CustomTables->getTableInfo(
            Context, BuffHudTable, D2RL::CustomTables::TableBank::Rotw, &info)
            != D2RL::CustomTables::Result::Success
        || info.state != D2RL::CustomTables::TableState::Ready
        || info.rowSize != sizeof(BuffHudRow)
        || info.byteCount != static_cast<std::uint64_t>(info.rowCount) * sizeof(BuffHudRow)) {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(info.byteCount));
    if (CustomTables->copyRows(
            Context, BuffHudTable, D2RL::CustomTables::TableBank::Rotw,
            info.revision, bytes.empty() ? nullptr : bytes.data(), info.byteCount)
        != D2RL::CustomTables::Result::Success) {
        return false;
    }
    revision = info.revision;
    rowCount = info.rowCount;
    return true;
}

[[nodiscard]] std::shared_ptr<const WhitelistCache> BuildWhitelist() noexcept {
    std::uint64_t revision{};
    std::uint32_t rowCount{};
    std::vector<std::uint8_t> bytes;
    if (!CopyWhitelistRows(revision, rowCount, bytes)) return {};

    auto cache = std::make_shared<WhitelistCache>();
    cache->revision = revision;
    cache->definitions.reserve(rowCount);
    for (std::uint32_t index = 0; index < rowCount; ++index) {
        BuffHudRow row{};
        std::memcpy(&row, bytes.data() + static_cast<std::size_t>(index) * sizeof(row), sizeof(row));
        if (row.enabled == 0) continue;
        if (row.name[0] == '\0' || row.stateId == 0 || row.stateId > 4095 || row.displayType[0] == '\0') {
            return {};
        }

        BuffDefinition definition{};
        definition.stateId = row.stateId;
        const std::string_view displayType{row.displayType, strnlen(row.displayType, sizeof(row.displayType))};
        if (displayType == "timer") {
            if (row.valueStat != 0 || row.maxStat != 0 || row.skillId > 4095 || row.valueShift != 0) return {};
            definition.displayMode = Core::BuffDisplayMode::Timer;
            definition.sourceSkillId = static_cast<std::int32_t>(row.skillId);
        } else if (displayType == "resource") {
            if (row.valueStat == 0 || row.maxStat == 0 || row.skillId == 0
                || row.valueStat > 4095 || row.maxStat > 4095 || row.skillId > 4095
                || row.valueShift > 30) {
                return {};
            }
            definition.displayMode = Core::BuffDisplayMode::Resource;
            definition.valueStatId = static_cast<std::int32_t>(row.valueStat);
            definition.maxStatId = static_cast<std::int32_t>(row.maxStat);
            definition.sourceSkillId = static_cast<std::int32_t>(row.skillId);
            definition.valueShift = row.valueShift;
        } else {
            return {};
        }
        cache->definitions.push_back(definition);
    }

    std::sort(
        cache->definitions.begin(),
        cache->definitions.end(),
        [](const BuffDefinition& lhs, const BuffDefinition& rhs) noexcept {
            return lhs.stateId < rhs.stateId;
        });
    if (std::adjacent_find(
            cache->definitions.begin(),
            cache->definitions.end(),
            [](const BuffDefinition& lhs, const BuffDefinition& rhs) noexcept {
                return lhs.stateId == rhs.stateId;
            }) != cache->definitions.end()) {
        return {};
    }
    return cache;
}

void __cdecl OnTablesLoaded(
    const D2RL::PluginContext* context,
    const D2RL::Lifecycle::DataTablesLoadedEvent* event,
    void*) noexcept {
    if (context == nullptr
        || !D2RL::Lifecycle::HasDataTablesLoadedEventField(
            event, D2RL::Lifecycle::DataTablesLoadedEventRequiredSize)) {
        return;
    }
    const auto cache = BuildWhitelist();
    if (!cache) {
        Whitelist.store({}, std::memory_order_release);
        context->LogError("Buff HUD: buff-hud.txt rejected; whitelist-driven BuffHud publishing is disabled.");
        return;
    }
    const auto count = cache->definitions.size();
    const auto revision = cache->revision;
    Whitelist.store(cache, std::memory_order_release);
    char line[256]{};
    std::snprintf(line, sizeof(line),
        "Buff HUD: buff-hud.txt ready; enabledDefinitions=%zu revision=%llu.",
        count, static_cast<unsigned long long>(revision));
    context->LogInfo(line);
}

[[nodiscard]] std::uint64_t AutomaticBuffKey(std::uint32_t state, std::uint32_t skill) noexcept {
    // Numeric FNV-1a namespace so automatic StatList entries cannot collide
    // with the readable keys used by explicit gameplay producers.
    std::uint64_t hash = 14695981039346656037ULL;
    constexpr char tag[] = "buff-panel:auto-statlist-buff";
    for (const unsigned char ch : tag) {
        if (ch == 0) break;
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>(state >> shift);
        hash *= 1099511628211ULL;
    }
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>(skill >> shift);
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1 : hash;
}

[[nodiscard]] bool IsFrameInFuture(std::uint32_t future, std::uint32_t now) noexcept {
    return static_cast<std::int32_t>(future - now) > 0;
}

void ClearTimerPresenceRecords() noexcept {
    std::lock_guard lock(TimerPresenceMutex);
    TimerPresenceRecords = {};
}

void RecordTimerPresence(
    std::uint64_t key,
    std::uint32_t stateId,
    std::uint32_t skillId,
    std::uint32_t expireFrame,
    std::uint32_t postFrame) noexcept {
    if (key == 0 || stateId == 0 || skillId == 0 || expireFrame == 0) return;
    std::lock_guard lock(TimerPresenceMutex);
    TimerPresenceRecord* empty = nullptr;
    for (auto& record : TimerPresenceRecords) {
        if (record.key == key) {
            record.stateId = stateId;
            record.skillId = skillId;
            record.expireFrame = expireFrame;
            record.lastPostFrame = postFrame;
            return;
        }
        if (record.key == 0 && empty == nullptr) empty = &record;
    }
    if (empty != nullptr) {
        *empty = TimerPresenceRecord{
            .key = key,
            .stateId = stateId,
            .skillId = skillId,
            .expireFrame = expireFrame,
            .lastPostFrame = postFrame,
        };
    }
}

// The native list can be re-used when a skill such as Venom is recast. A
// STATLIST_PostStatList notification is not guaranteed for an in-place update:
// read the *currently attached* state every frame, not only its presence.
// Keep the original 0x30 native readable-range qualification, while the logical
// timer model intentionally stops before the unused skillLevel field at +0x2C.
using NativeTimerMetadata = Internal::TimerMetadata;

[[nodiscard]] bool ReadLiveTimerExpiry(
    void* statList,
    const TimerPresenceRecord& record,
    std::uint32_t currentFrame,
    std::uint32_t& liveExpiry) noexcept {
    NativeTimerMetadata metadata{};
    std::uint32_t skill{};
    return IsReadableRange(statList, Native::Contract::StatListBuffMetadataBytes)
        && ReadField(statList, Native::Contract::StatListBuffFlagsOffset, metadata)
        && Internal::ResolveTimer(
            metadata, record.stateId, record.skillId, currentFrame, skill, liveExpiry)
        && skill == record.skillId;
}

// Retain the established expiry/presence lifecycle. The renewal path is
// strictly additive: a verified *later* native expiry can extend a published
// timer, but an unreadable or mismatched native metadata block must never
// shorten or remove a countdown. A posted state gets a full frame to attach.
void RefreshTimerPresence(std::uint32_t currentFrame) noexcept {
    auto* player = AuthoritativePlayer.load(std::memory_order_acquire);
    if (player == nullptr || GetStatListFromUnitAndState == nullptr || currentFrame == 0) return;

    struct Removal final {
        std::uint64_t key{};
        std::uint32_t stateId{};
        std::uint32_t skillId{};
        std::uint32_t expireFrame{};
    };
    struct Refresh final {
        std::uint64_t key{};
        std::uint32_t stateId{};
        std::uint32_t skillId{};
        std::uint32_t oldExpireFrame{};
        std::uint32_t newExpireFrame{};
    };
    std::array<Removal, Core::MaximumPublishedBuffs> removals{};
    std::size_t removalCount{};
    std::array<Refresh, Core::MaximumPublishedBuffs> refreshes{};
    std::size_t refreshCount{};

    {
        std::lock_guard lock(TimerPresenceMutex);
        for (auto& record : TimerPresenceRecords) {
            if (record.key == 0) continue;

            // Preserve the original countdown lifecycle: natural expiration
            // belongs to BuffHud; never reinterpret it as early removal.
            if (!IsFrameInFuture(record.expireFrame, currentFrame)) {
                record = {};
                continue;
            }
            if (record.lastPostFrame == currentFrame) continue;

            void* nativeState = GetStatListFromUnitAndState(
                player, static_cast<std::int32_t>(record.stateId));
            if (nativeState != nullptr) {
                std::uint32_t liveExpire{};
                if (ReadLiveTimerExpiry(nativeState, record, currentFrame, liveExpire)) {
                    // Only a strictly later expiry is evidence of a renewal.
                    // Never overwrite the original deadline with a shorter
                    // value or reinterpret state-lookup failure as expiry.
                    if (IsFrameInFuture(liveExpire, record.expireFrame)
                        && liveExpire - record.expireFrame >= 3u
                        && refreshCount < refreshes.size()) {
                        refreshes[refreshCount++] = Refresh{
                            .key = record.key,
                            .stateId = record.stateId,
                            .skillId = record.skillId,
                            .oldExpireFrame = record.expireFrame,
                            .newExpireFrame = liveExpire,
                        };
                        record.expireFrame = liveExpire;
                        record.lastPostFrame = currentFrame;
                    }
                } else {
                }
                continue;
            }

            // Preserve the existing state-removal policy.
            if (removalCount < removals.size()) {
                removals[removalCount++] = Removal{
                    .key = record.key,
                    .stateId = record.stateId,
                    .skillId = record.skillId,
                    .expireFrame = record.expireFrame,
                };
            }
            record = {};
        }
    }

    for (std::size_t i = 0; i < refreshCount; ++i) {
        const auto& refresh = refreshes[i];
        const auto snapshot = Core::BuffDisplays().Snapshot();
        for (std::size_t j = 0; j < snapshot.count; ++j) {
            const auto& entry = snapshot.entries[j];
            if (entry.key != refresh.key || entry.displayMode != Core::BuffDisplayMode::Timer
                || entry.expireGameFrame != refresh.oldExpireFrame) continue;
            auto updated = entry;
            updated.expireGameFrame = refresh.newExpireFrame;
            if (!Core::BuffDisplays().Upsert(updated)) {
                break;
            }
            break;
        }
    }

    for (std::size_t i = 0; i < removalCount; ++i) {
        const auto& removal = removals[i];
        if (Core::BuffDisplays().Remove(removal.key)) {
        } else {
        }
    }
}

[[nodiscard]] std::int32_t NormalizeResourceValue(std::int32_t rawValue, std::uint8_t valueShift) noexcept {
    if (valueShift == 0) return rawValue;
    const auto divisor = static_cast<std::int64_t>(1) << valueShift;
    return static_cast<std::int32_t>(static_cast<std::int64_t>(rawValue) / divisor);
}

void RefreshResourceBuffs() noexcept {
    auto* player = AuthoritativePlayer.load(std::memory_order_acquire);
    const auto getter = Core::StatReads().RawGetter();
    const auto cache = Whitelist.load(std::memory_order_acquire);
    if (player == nullptr || getter == nullptr || cache == nullptr) return;

    // D2RLoader 1.3 materializes the shared stash as a UNIT_PLAYER-shaped
    // proxy carrying state 186 (sharedstash). Never poll player stats from that
    // proxy: Loader deliberately asserts if its proxy is queried before/without
    // the marker state, and the proxy is not the gameplay player anyway.
    if (GetStatListFromUnitAndState != nullptr
        && GetStatListFromUnitAndState(player, SharedStashProxyState) != nullptr) {
        AuthoritativePlayer.store(nullptr, std::memory_order_release);
        return;
    }

    const auto snapshot = Core::BuffDisplays().Snapshot();
    for (const auto& definition : cache->definitions) {
        if (definition.displayMode != Core::BuffDisplayMode::Resource
            || definition.valueStatId <= 0
            || definition.maxStatId <= 0
            || definition.sourceSkillId <= 0) {
            continue;
        }

        const auto key = AutomaticBuffKey(
            definition.stateId,
            static_cast<std::uint32_t>(definition.sourceSkillId));
        const Core::BuffDisplayEntry* existing = nullptr;
        for (std::size_t index = 0; index < snapshot.count; ++index) {
            if (snapshot.entries[index].key == key) {
                existing = &snapshot.entries[index];
                break;
            }
        }

        // A positive stat pool alone does not prove that its buff is active.
        // Mods can reuse stat IDs (Reimagined uses stock Psychic Ward IDs for
        // corruption flags), and a depleted buff can leave its maximum behind.
        // Require the attached native state for EVERY resource definition.
        const bool isAttached = GetStatListFromUnitAndState != nullptr
            && GetStatListFromUnitAndState(
                player, static_cast<std::int32_t>(definition.stateId)) != nullptr;
        if (!isAttached) {
            if (existing != nullptr && Core::BuffDisplays().Remove(key)) {
            }
            continue;
        }

        const auto rawCurrent = getter(player, definition.valueStatId, 0);
        const auto rawMaximum = getter(player, definition.maxStatId, 0);
        const auto current = NormalizeResourceValue(rawCurrent, definition.valueShift);
        const auto maximum = NormalizeResourceValue(rawMaximum, definition.valueShift);

        if (rawCurrent <= 0 || rawMaximum <= 0) {
            if (existing != nullptr && Core::BuffDisplays().Remove(key)) {
            }
            if (rawCurrent > 0 && rawMaximum <= 0) {
            }
            continue;
        }

        if (existing != nullptr
            && existing->displayMode == Core::BuffDisplayMode::Resource
            && existing->sourceSkillId == definition.sourceSkillId
            && existing->valueStatId == definition.valueStatId
            && existing->maxStatId == definition.maxStatId
            && existing->currentValue == current
            && existing->maximumValue == maximum) {
            continue;
        }

        Core::BuffDisplayEntry entry{};
        entry.key = key;
        entry.sourceSkillId = definition.sourceSkillId;
        entry.displayMode = Core::BuffDisplayMode::Resource;
        entry.valueStatId = definition.valueStatId;
        entry.maxStatId = definition.maxStatId;
        entry.currentValue = current;
        entry.maximumValue = maximum;
        entry.stacks = 1;
        entry.priority = 100;
        if (!Core::BuffDisplays().Upsert(entry)) {
            continue;
        }

        if (existing == nullptr) {
        } else {
        }
    }
}

// Some duration effects renew an attached StatList in place, without a new
// STATLIST_PostStatList event. Discover only explicitly whitelisted timer
// states from the authoritative player's currently attached native lists.
// This is a read-only fallback, not a second native hook or a new buff source.
void DiscoverAttachedTimers(std::uint32_t currentFrame) noexcept {
    if (currentFrame == 0 || LastTimerDiscoveryFrame.load(std::memory_order_relaxed) == currentFrame) return;
    LastTimerDiscoveryFrame.store(currentFrame, std::memory_order_relaxed);
    auto* player = AuthoritativePlayer.load(std::memory_order_acquire);
    const auto cache = Whitelist.load(std::memory_order_acquire);
    if (player == nullptr || GetStatListFromUnitAndState == nullptr || cache == nullptr) return;
    // The loader's UNIT_PLAYER-shaped shared stash proxy must never be treated
    // as a gameplay player. This matches RefreshResourceBuffs' safety guard.
    if (GetStatListFromUnitAndState(player, SharedStashProxyState) != nullptr) return;

    auto snapshot = Core::BuffDisplays().Snapshot();
    for (const auto& definition : cache->definitions) {
        if (definition.displayMode != Core::BuffDisplayMode::Timer) continue;
        const auto state = definition.stateId;
        auto* nativeState = GetStatListFromUnitAndState(player, static_cast<std::int32_t>(state));
        if (nativeState == nullptr) continue;

        NativeTimerMetadata metadata{};
        std::uint32_t skill{}, expiry{};
        if (!IsReadableRange(nativeState, Native::Contract::StatListBuffMetadataBytes)
            || !ReadField(nativeState, Native::Contract::StatListBuffFlagsOffset, metadata)
            || !Internal::ResolveTimer(
                metadata,
                state,
                definition.sourceSkillId > 0
                    ? static_cast<std::uint32_t>(definition.sourceSkillId) : 0u,
                currentFrame,
                skill,
                expiry)) {
            continue;
        }
        const TimerPresenceRecord witness{
            .key = AutomaticBuffKey(state, skill),
            .stateId = state,
            .skillId = skill,
        };
        bool alreadyPublished = false;
        for (std::size_t i = 0; i < snapshot.count; ++i) {
            if (snapshot.entries[i].key == witness.key
                && snapshot.entries[i].displayMode == Core::BuffDisplayMode::Timer) {
                alreadyPublished = true;
                break;
            }
        }
        if (alreadyPublished) {
            continue;
        }
        Core::BuffDisplayEntry entry{};
        entry.key = witness.key;
        entry.sourceSkillId = static_cast<std::int32_t>(skill);
        entry.displayMode = Core::BuffDisplayMode::Timer;
        entry.expireGameFrame = expiry;
        entry.stacks = 1;
        entry.priority = 100;
        if (!Core::BuffDisplays().Upsert(entry)) {
            continue;
        }
        RecordTimerPresence(witness.key, state, skill, expiry, currentFrame);
        // Keep the per-frame snapshot in step so a duplicate table row or key
        // cannot consume two HUD positions during this scan.
        snapshot = Core::BuffDisplays().Snapshot();
    }
}

void QueueFramePump() noexcept;

void __cdecl FramePumpOnGameThread(const D2RL::PluginContext* context, void*) noexcept {
    FramePumpScheduled.store(false, std::memory_order_release);
    if (context == nullptr || context != Context) return;
    const auto session = CurrentSessionGeneration.load(std::memory_order_acquire);
    if (session == 0) return;

    auto* game = AuthoritativeGame.load(std::memory_order_acquire);
    std::uint32_t frame{};
    if (ReadGameFrame(game, frame)) {
        Core::BuffDisplays().PublishGameFrame(session, frame);
        RefreshResourceBuffs();
        RefreshTimerPresence(frame);
        DiscoverAttachedTimers(frame);
        QueueFramePump();
    }
}

void QueueFramePump() noexcept {
    if (Context == nullptr || Threads == nullptr
        || CurrentSessionGeneration.load(std::memory_order_acquire) == 0
        || AuthoritativeGame.load(std::memory_order_acquire) == nullptr) {
        return;
    }
    bool expected = false;
    if (!FramePumpScheduled.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
        return;
    }
    if (Threads->runOnGameThread(Context, &FramePumpOnGameThread, nullptr)
        != D2RL::Threads::Result::Success) {
        FramePumpScheduled.store(false, std::memory_order_release);
    }
}

void OnStatListPost(
    const Core::StatListPostEvent& event,
    Core::StatListPostPhase phase,
    void*) noexcept {
    if (event.unit == nullptr || event.statList == nullptr || GetGameFromUnit == nullptr) return;

    std::uint32_t unitType{};
    if (!ReadField(event.unit, UnitTypeOffset, unitType) || unitType != PlayerUnitType) return;

    // Read StatList metadata before any native unit helper. D2RLoader 1.3
    // creates its shared-stash proxy by posting state 186 to a UNIT_PLAYER-
    // shaped proxy. During BeforeNative that marker is not attached yet.
    if (!IsReadableRange(event.statList, Native::Contract::StatListBuffMetadataBytes)) {
        if (phase == Core::StatListPostPhase::BeforeNative) {
        }
        return;
    }

    std::uint32_t flags{};
    std::uint32_t state{};
    float expireFrameFloat{};
    std::uint32_t skill{};
    if (!ReadField(event.statList, Native::Contract::StatListBuffFlagsOffset, flags)
        || !ReadField(event.statList, Native::Contract::StatListBuffStateOffset, state)
        || !ReadField(event.statList, Native::Contract::StatListBuffExpireFrameFloatOffset, expireFrameFloat)
        || !ReadField(event.statList, Native::Contract::StatListBuffSkillIdOffset, skill)) {
        if (phase == Core::StatListPostPhase::BeforeNative) {
        }
        return;
    }

    const bool sharedStashProxy = state == SharedStashProxyState
        || (GetStatListFromUnitAndState != nullptr
            && GetStatListFromUnitAndState(event.unit, SharedStashProxyState) != nullptr);
    if (sharedStashProxy) {
        return;
    }

    void* game = GetGameFromUnit(event.unit);
    std::uint32_t currentFrame{};
    if (!ReadGameFrame(game, currentFrame)) return;

    if (phase == Core::StatListPostPhase::BeforeNative) {
        AuthoritativeGame.store(game, std::memory_order_release);
        AuthoritativePlayer.store(event.unit, std::memory_order_release);
        const auto session = CurrentSessionGeneration.load(std::memory_order_acquire);
        if (session != 0) {
            Core::BuffDisplays().PublishGameFrame(session, currentFrame);
            QueueFramePump();
        }
    }

    if (phase == Core::StatListPostPhase::BeforeNative) {
    }

    const auto cache = Whitelist.load(std::memory_order_acquire);
    const auto* definition = FindBuffDefinition(cache, state);
    if (definition == nullptr) {
        if (phase == Core::StatListPostPhase::BeforeNative) {
        }
        return;
    }
    if ((flags & StatListCurseFlag) != 0) {
        if (phase == Core::StatListPostPhase::BeforeNative) {
        }
        return;
    }
    const auto session = CurrentSessionGeneration.load(std::memory_order_acquire);
    if (session == 0) return;

    if (definition->displayMode == Core::BuffDisplayMode::Timer) {
        if (phase != Core::StatListPostPhase::BeforeNative) return;
        const NativeTimerMetadata metadata{flags, state, expireFrameFloat, skill};
        std::uint32_t resolvedSkill{};
        std::uint32_t expireFrame{};
        if (!Internal::ResolveTimer(
                metadata,
                definition->stateId,
                definition->sourceSkillId > 0
                    ? static_cast<std::uint32_t>(definition->sourceSkillId) : 0u,
                currentFrame,
                resolvedSkill,
                expireFrame)) {
            return;
        }

        Core::BuffDisplayEntry entry{};
        entry.key = AutomaticBuffKey(state, resolvedSkill);
        entry.sourceSkillId = static_cast<std::int32_t>(resolvedSkill);
        entry.displayMode = Core::BuffDisplayMode::Timer;
        entry.expireGameFrame = expireFrame;
        entry.stacks = 1;
        entry.priority = 100;
        if (!Core::BuffDisplays().Upsert(entry)) {
            return;
        }
        RecordTimerPresence(entry.key, state, resolvedSkill, expireFrame, currentFrame);
        return;
    }

    // Resource-mode rows are not classified from StatList metadata. Some
    // absorb-pool skills (notably Bone Armor) do not expose a reliable
    // state/skill/level tuple on the posted list. The frame pump therefore
    // discovers resource buffs directly from their configured current/max unit
    // stats and uses buff-hud.txt skill_id only to resolve the icon/name.
    return;

}

void __cdecl OnGameplayEvent(
    const D2RL::PluginContext*,
    const D2RL::Lifecycle::GameplayEvent* event,
    void*) noexcept {
    if (event == nullptr) return;
    switch (event->kind) {
    case D2RL::Lifecycle::GameplayEventKind::GameJoined:
        ClearTimerPresenceRecords();
        LastTimerDiscoveryFrame.store(0, std::memory_order_relaxed);
        CurrentSessionGeneration.store(event->sessionGeneration, std::memory_order_release);
        QueueFramePump();
        break;
    case D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady:
        CurrentSessionGeneration.store(event->sessionGeneration, std::memory_order_release);
        QueueFramePump();
        break;
    case D2RL::Lifecycle::GameplayEventKind::GameLeft:
        CurrentSessionGeneration.store(0, std::memory_order_release);
        AuthoritativeGame.store(nullptr, std::memory_order_release);
        AuthoritativePlayer.store(nullptr, std::memory_order_release);
        FramePumpScheduled.store(false, std::memory_order_release);
        ClearTimerPresenceRecords();
        break;
    default:
        break;
    }
}

[[nodiscard]] bool RegisterTable() noexcept {
    if (Context == nullptr || CustomTables == nullptr) return false;
    // The default TXT lives in d2rl-buff-panel.mpq. D2RLoader owns resource
    // selection and custom-table compilation; no mod-side file is required.
    const D2RL::CustomTables::TableRegistration registration{
        .structSize = D2RL::CustomTables::TableRegistrationSize,
        .name = "buff-hud",
        .banks = D2RL::CustomTables::TableBank::Rotw,
        .rowSize = sizeof(BuffHudRow),
        .columns = BuffHudColumns.data(),
        .columnCount = static_cast<std::uint32_t>(BuffHudColumns.size()),
        .columnStride = D2RL::CustomTables::ColumnDefinitionSize,
    };
    if (CustomTables->registerTable(Context, &registration, &BuffHudTable)
            != D2RL::CustomTables::Result::Success
        || BuffHudTable == D2RL::CustomTables::InvalidHandle) {
        Context->LogError("Buff HUD: unable to register selected buff-hud.txt custom table.");
        return false;
    }
    Context->LogInfo(
        "Buff HUD: registered buff-hud custom table from companion resources; active-mod overrides take priority.");
    return true;
}

[[nodiscard]] bool RegisterLifecycle() noexcept {
    if (Context == nullptr || Lifecycle == nullptr) return false;
    const D2RL::Lifecycle::DataTablesLoadedListener tableListener{
        .structSize = D2RL::Lifecycle::DataTablesLoadedListenerSize,
        .flags = 0,
        .callback = &OnTablesLoaded,
        .userData = nullptr,
    };
    if (Lifecycle->registerDataTablesLoadedListener(Context, &tableListener, &DataTablesListener)
            != D2RL::Lifecycle::Result::Success
        || DataTablesListener == D2RL::Lifecycle::InvalidHandle) {
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

} // namespace

bool Initialize(const D2RL::PluginContext* context) noexcept {
    Shutdown();
    if (context == nullptr || context->exeBase == 0) return false;
    Context = context;
    const auto& services = Core::Services();
    CustomTables = services.customTables;
    Threads = services.threads;
    Lifecycle = services.lifecycle;
    if (CustomTables == nullptr || Threads == nullptr || Lifecycle == nullptr
        || !D2RL::HasThreadServiceField(Threads, D2RL::ThreadServiceRequiredSize)
        || !D2RL::HasLifecycleServiceField(Lifecycle, D2RL::LifecycleServiceRequiredSize)) {
        Context->LogError("Buff HUD: required CustomTable/Thread/Lifecycle service unavailable.");
        Shutdown();
        return false;
    }

    if (!Context->CheckExpectedBytes(
            Native::Contract::GetGameFromUnitRva,
            Native::Contract::GetGameFromUnitExpected.data(),
            static_cast<std::uint32_t>(Native::Contract::GetGameFromUnitExpected.size()))) {
        Context->LogError("Buff HUD: UNITS_GetGame native fingerprint mismatch; refusing whitelist-driven buff tracking.");
        Shutdown();
        return false;
    }
    GetGameFromUnit = reinterpret_cast<GetGameFromUnitFn>(
        context->exeBase + Native::Contract::GetGameFromUnitRva);

    if (!Context->CheckExpectedBytes(
            Native::Contract::GenericCurseStateLookupCallRva,
            Native::Contract::GenericCurseStateLookupCallExpected.data(),
            static_cast<std::uint32_t>(Native::Contract::GenericCurseStateLookupCallExpected.size()))) {
        Context->LogError("Buff HUD: exact-state StatList lookup native fingerprint mismatch; refusing premature timer-removal tracking.");
        Shutdown();
        return false;
    }
    GetStatListFromUnitAndState = reinterpret_cast<GetStatListFromUnitAndStateFn>(
        context->exeBase + Native::Contract::GetStatListFromUnitAndStateRva);

    if (Core::StatReads().RawGetter() == nullptr) {
        Context->LogError("Buff HUD: shared Core unit-stat reader is unavailable; resource-mode BuffHud entries cannot be supported.");
        Shutdown();
        return false;
    }

    if (!Core::StatListPosts().EnsureInstalled(
            Native::Contract::PostStatListRva,
            Native::Contract::PostStatListExpected,
            Native::Contract::PostStatListBridgeSlotRva)
        || !Core::StatListPosts().Register({
            .owner = "buff-tracker.player-buffs",
            .priority = 50,
            .callback = &OnStatListPost,
            .userData = nullptr,
        })) {
        Context->LogError("Buff HUD: failed to register with the shared Core STATLIST_PostStatList bus.");
        Shutdown();
        return false;
    }

    if (!RegisterTable() || !RegisterLifecycle()) {
        Context->LogError("Buff HUD: failed to register buff-hud table/lifecycle listeners.");
        Shutdown();
        return false;
    }
    Context->LogInfo(
        "Buff HUD 1.1.0 BuffTracker initialized: companion whitelist tracking with native skill attribution, configured skill fallback, exact-state lifetime checks, and finite native expiry validation.");
    return true;
}

void Shutdown() noexcept {
    CurrentSessionGeneration.store(0, std::memory_order_release);
    AuthoritativeGame.store(nullptr, std::memory_order_release);
    AuthoritativePlayer.store(nullptr, std::memory_order_release);
    FramePumpScheduled.store(false, std::memory_order_release);
    Whitelist.store({}, std::memory_order_release);
    BuffHudTable = D2RL::CustomTables::InvalidHandle;
    DataTablesListener = D2RL::Lifecycle::InvalidHandle;
    GameplayListeners = {};
    ClearTimerPresenceRecords();
    GetGameFromUnit = nullptr;
    GetStatListFromUnitAndState = nullptr;
    CustomTables = nullptr;
    Threads = nullptr;
    Lifecycle = nullptr;
    Context = nullptr;
}

} // namespace BuffPanel::Systems::BuffTracker

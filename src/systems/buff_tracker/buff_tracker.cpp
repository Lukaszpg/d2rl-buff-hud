#include "buff_tracker.hpp"
#include "timer_metadata.hpp"

#include "core/buff_display_bus.hpp"
#include "core/services.hpp"
#include "core/stat_read_bus.hpp"
#include "core/stat_list_post_bus.hpp"
#include "native/native_contract.hpp"

#include "loose_buff_hud.hpp"

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

// Compile-time defaults are kept here, not in generated headers or
// authoring TXT/JSON. Charsi may strip non-C++ asset files.
namespace BuffPanel::Systems::BuffTracker::DefaultTables {
constexpr char BuffHud[] = R"BUFFPANELHUD(name	state_id	display_type	value_stat	max_stat	skill_id	value_shift	enabled
resist_fire_aura	3	timer	0	0	0	0	0
resist_cold_aura	4	timer	0	0	0	0	0
resist_lightning_aura	5	timer	0	0	0	0	0
salvation_aura	8	timer	0	0	0	0	0
frozen_armor	10	timer	0	0	0	0	1
blaze	13	timer	0	0	0	0	1
bone_armor	14	resource	132	133	68	8	1
concentrate_attack_state	15	timer	0	0	0	0	0
enchant	16	timer	0	0	0	0	1
chilling_armor	20	timer	0	0	0	0	1
shout	26	timer	0	0	138	0	1
conviction_aura	28	timer	0	0	0	0	0
energy_shield	30	timer	0	0	0	0	1
venom	31	timer	0	0	0	0	1
battle_orders	32	timer	0	0	149	0	1
might_aura	33	timer	0	0	0	0	0
prayer_aura	34	timer	0	0	0	0	0
holy_fire_aura	35	timer	0	0	0	0	0
thorns_aura	36	timer	0	0	0	0	0
defiance_aura	37	timer	0	0	0	0	0
thunder_storm	38	timer	0	0	0	0	1
blessed_aim_aura	40	timer	0	0	0	0	0
vigor_aura	41	timer	0	0	0	0	0
concentration_aura	42	timer	0	0	0	0	0
holy_freeze_caster_aura	43	timer	0	0	0	0	0
cleansing_aura	45	timer	0	0	0	0	0
holy_shock_aura	46	timer	0	0	0	0	0
sanctuary_aura	47	timer	0	0	0	0	0
meditation_aura	48	timer	0	0	0	0	0
fanaticism_aura	49	timer	0	0	0	0	0
redemption_aura	50	timer	0	0	0	0	0
battle_command	51	timer	0	0	155	0	1
critical_strike_passive	64	timer	0	0	0	0	0
dodge_passive	65	timer	0	0	0	0	0
avoid_passive	66	timer	0	0	0	0	0
penetrate_passive	67	timer	0	0	0	0	0
evade_passive	68	timer	0	0	0	0	0
pierce_passive	69	timer	0	0	0	0	0
warmth_passive	70	timer	0	0	0	0	0
fire_mastery_passive	71	timer	0	0	0	0	0
lightning_mastery_passive	72	timer	0	0	0	0	0
cold_mastery_passive	73	timer	0	0	0	0	0
blade_mastery_passive	74	timer	0	0	0	0	0
axe_mastery_passive	75	timer	0	0	0	0	0
mace_mastery_passive	76	timer	0	0	0	0	0
polearm_mastery_passive	77	timer	0	0	0	0	0
throwing_mastery_passive	78	timer	0	0	0	0	0
spear_mastery_passive	79	timer	0	0	0	0	0
increased_stamina_passive	80	timer	0	0	0	0	0
iron_skin_passive	81	timer	0	0	0	0	0
increased_speed_passive	82	timer	0	0	0	0	0
natural_resistance_passive	83	timer	0	0	0	0	0
shiver_armor	88	timer	0	0	0	0	1
frenzy	94	timer	0	0	0	0	1
berserk_attack_state	95	timer	0	0	0	0	0
skeleton_mastery_passive	97	timer	0	0	0	0	0
holy_shield	101	timer	0	0	0	0	1
golem_mastery_passive	111	timer	0	0	0	0	0
maul	117	timer	0	0	0	0	1
feral_rage	120	timer	0	0	0	0	1
tiger_strike_charges	122	timer	0	0	0	0	1
cobra_strike_charges	123	timer	0	0	0	0	1
phoenix_strike_charges	124	timer	0	0	0	0	1
fists_of_fire_charges	125	timer	0	0	0	0	1
blades_of_ice_charges	126	timer	0	0	0	0	1
claws_of_thunder_charges	127	timer	0	0	0	0	1
armor_shrine	128	timer	0	0	0	0	1
combat_shrine	129	timer	0	0	0	0	1
lightning_resist_shrine	130	timer	0	0	0	0	1
fire_resist_shrine	131	timer	0	0	0	0	1
cold_resist_shrine	132	timer	0	0	0	0	1
poison_resist_shrine	133	timer	0	0	0	0	1
skill_shrine	134	timer	0	0	0	0	1
mana_regen_shrine	135	timer	0	0	0	0	1
stamina_shrine	136	timer	0	0	0	0	1
experience_shrine	137	timer	0	0	0	0	1
werewolf	139	timer	0	0	0	0	1
werebear	140	timer	0	0	0	0	1
bloodlust	141	timer	0	0	0	0	0
hurricane	144	timer	0	0	0	0	1
armageddon	145	timer	0	0	0	0	1
spirit_of_barbs_aura	147	timer	0	0	0	0	0
heart_of_wolverine_aura	148	timer	0	0	0	0	0
oak_sage_aura	149	timer	0	0	0	0	0
cyclone_armor	151	resource	132	133	235	8	1
claw_mastery_passive	152	timer	0	0	0	0	0
cloak_of_shadows_caster	153	timer	0	0	0	0	1
weapon_block_passive	155	timer	0	0	0	0	0
burst_of_speed	157	timer	0	0	0	0	1
blade_shield	158	timer	0	0	0	0	1
fade	159	timer	0	0	0	0	1
summon_resist_passive	160	timer	0	0	0	0	0
whirlwind_action_state	174	timer	0	0	0	0	0
delirium	177	timer	0	0	0	0	1
antidote_potion	178	timer	0	0	0	0	1
thawing_potion	179	timer	0	0	0	0	1
stamina_potion	180	timer	0	0	0	0	1
resist_fire_passive	181	timer	0	0	0	0	0
resist_cold_passive	182	timer	0	0	0	0	0
resist_lightning_passive	183	timer	0	0	0	0	0
mark_of_the_bear	190	timer	0	0	0	0	1
mark_of_the_wolf	191	timer	0	0	0	0	1
blood_oath_passive	195	timer	0	0	0	0	0
demonic_mastery_passive	196	timer	0	0	0	0	0
levitate_mastery_passive	197	timer	0	0	0	0	0
hex_bane_caster	198	timer	0	0	0	0	1
hex_siphon_caster	200	timer	0	0	0	0	1
hex_purge_caster	202	timer	0	0	0	0	1
mind_barrier	203	timer	0	0	0	0	0
enhanced_entropy_passive	204	timer	0	0	0	0	0
sigil_caster	205	timer	0	0	0	0	0
psychic_ward	206	resource	361	362	387	8	1
bind_demon_caster	207	timer	0	0	0	0	0
consume	208	timer	0	0	0	0	1
engorge	211	timer	0	0	0	0	1
lightning_enchant	222	timer	0	0	0	0	0
cold_enchant	223	timer	0	0	0	0	0
)BUFFPANELHUD";
} // namespace BuffPanel::Systems::BuffTracker::DefaultTables

namespace BuffPanel::Systems::BuffTracker {
namespace {

using GetGameFromUnitFn = void*(__fastcall*)(void* unit) noexcept;
using GetStatListFromUnitAndStateFn = void*(__fastcall*)(void* unit, std::int32_t state) noexcept;

// StatList semantic fields are runtime-qualified for D2R build 93847. buff-hud.txt is the authoritative whitelist and
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
constexpr std::uint32_t FramesPerSecond = 25;

const D2RL::PluginContext* Context{};
const D2RL::ResourceService* Resources{};
const D2RL::CustomTableService* CustomTables{};
const D2RL::ThreadService* Threads{};
const D2RL::LifecycleService* Lifecycle{};
GetGameFromUnitFn GetGameFromUnit{};
GetStatListFromUnitAndStateFn GetStatListFromUnitAndState{};
D2RL::CustomTables::TableHandle BuffHudTable{D2RL::CustomTables::InvalidHandle};
LooseBuffHud::Source TableSource{LooseBuffHud::Source::Embedded};
std::string TableSourcePath{};
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
        context->LogError("BuffPanel BuffTracker: buff-hud.txt rejected; whitelist-driven BuffHud publishing is disabled.");
        return;
    }
    const auto count = cache->definitions.size();
    const auto revision = cache->revision;
    Whitelist.store(cache, std::memory_order_release);
    char line[256]{};
    std::snprintf(line, sizeof(line),
        "BuffPanel BuffTracker: buff-hud.txt ready; enabledDefinitions=%zu revision=%llu.",
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
    };
    struct Refresh final {
        std::uint64_t key{};
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
                            .oldExpireFrame = record.expireFrame,
                            .newExpireFrame = liveExpire,
                        };
                        record.expireFrame = liveExpire;
                        record.lastPostFrame = currentFrame;
                    }
                }
                continue;
            }

            // Preserve the existing state-removal policy.
            if (removalCount < removals.size()) {
                removals[removalCount++] = Removal{.key = record.key};
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
        (void)Core::BuffDisplays().Remove(removal.key);
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

        // Multiple enabled resource states can share one unit-stat pool:
        // Bone Armor and Cyclone Armor both use bonearmor/bonearmormax.
        // Resolve the *actual* attached native state in that case so a single
        // pool never creates two different skill icons. Keep the original
        // stat-only fallback for an unambiguous resource definition.
        bool sharesResourcePair = false;
        for (const auto& other : cache->definitions) {
            if (other.displayMode == Core::BuffDisplayMode::Resource
                && other.stateId != definition.stateId
                && other.valueStatId == definition.valueStatId
                && other.maxStatId == definition.maxStatId) {
                sharesResourcePair = true;
                break;
            }
        }
        if (sharesResourcePair) {
            const bool isAttached = GetStatListFromUnitAndState != nullptr
                && GetStatListFromUnitAndState(
                    player, static_cast<std::int32_t>(definition.stateId)) != nullptr;
            if (!isAttached) {
                if (existing != nullptr) (void)Core::BuffDisplays().Remove(key);
                continue;
            }
        }

        const auto rawCurrent = getter(player, definition.valueStatId, 0);
        const auto rawMaximum = getter(player, definition.maxStatId, 0);
        const auto current = NormalizeResourceValue(rawCurrent, definition.valueShift);
        const auto maximum = NormalizeResourceValue(rawMaximum, definition.valueShift);

        if (rawCurrent <= 0 || rawMaximum <= 0) {
            if (existing != nullptr) (void)Core::BuffDisplays().Remove(key);
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

    const auto cache = Whitelist.load(std::memory_order_acquire);
    const auto* definition = FindBuffDefinition(cache, state);
    if (definition == nullptr) {
        return;
    }
    if ((flags & StatListCurseFlag) != 0) {
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
    // absorb-pool skills (notably Bone Armor) do not expose reliable source
    // skill metadata on the posted list. The frame pump therefore
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
    if (Context == nullptr || Resources == nullptr || CustomTables == nullptr) return false;
    // Do not depend on opaque resource-overlay priority. Explicitly select the
    // active mod's loose file BEFORE registering a single in-memory resource.
    // D2RLoader copies the selected bytes during registerResource().
    const auto selection = LooseBuffHud::Select(
        Context->modDirectory, Context->activeMod,
        std::string_view(DefaultTables::BuffHud, sizeof(DefaultTables::BuffHud) - 1));
    if (selection.source == LooseBuffHud::Source::InvalidOverride) {
        char line[512]{};
        std::snprintf(line, sizeof(line),
            "Buff Panel: external buff-hud.txt rejected: %s. Correct or delete it; no silent fallback.",
            selection.error.c_str());
        Context->LogError(line);
        return false;
    }
    TableSource = selection.source;
    TableSourcePath.clear();
    if (TableSource == LooseBuffHud::Source::ActiveMod) {
        try {
            const auto utf8 = selection.path.u8string();
            TableSourcePath.assign(utf8.begin(), utf8.end());
        } catch (...) {
            TableSourcePath = "<active mod file>";
        }
    }
    const D2RL::Resources::ResourceRegistration resource{
        .structSize = D2RL::Resources::ResourceRegistrationSize,
        .flags = 0,
        .path = "data/global/excel/d2rloader/buff-panel/buff-hud.txt",
        .bytes = selection.bytes.data(),
        .byteCount = selection.bytes.size(),
    };
    D2RL::Resources::RegistrationHandle resourceHandle{D2RL::Resources::InvalidHandle};
    if (Resources->registerResource(Context, &resource, &resourceHandle)
            != D2RL::Resources::Result::Success
        || resourceHandle == D2RL::Resources::InvalidHandle) {
        Context->LogError("Buff Panel: unable to register selected buff-hud.txt as a loader resource.");
        return false;
    }
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
        Context->LogError("Buff Panel: unable to register selected buff-hud.txt custom table.");
        return false;
    }
    char line[768]{};
    if (TableSource == LooseBuffHud::Source::ActiveMod) {
        std::snprintf(line, sizeof(line),
            "Buff Panel: buff-hud.txt source=active-mod file=%s; override loaded; restart D2R after edits.",
            TableSourcePath.c_str());
    } else {
        std::snprintf(line, sizeof(line),
            "Buff Panel: buff-hud.txt source=embedded (active mod has no loose override)."
            " No loose file is required; restart D2R after adding one.");
    }
    Context->LogInfo(line);
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
    Resources = services.resources;
    CustomTables = services.customTables;
    Threads = services.threads;
    Lifecycle = services.lifecycle;
    if (Resources == nullptr || CustomTables == nullptr || Threads == nullptr || Lifecycle == nullptr
        || !D2RL::HasThreadServiceField(Threads, D2RL::ThreadServiceRequiredSize)
        || !D2RL::HasLifecycleServiceField(Lifecycle, D2RL::LifecycleServiceRequiredSize)) {
        Context->LogError("BuffPanel BuffTracker: required Resource/CustomTable/Thread/Lifecycle service unavailable.");
        Shutdown();
        return false;
    }

    if (!Context->CheckExpectedBytes(
            Native::Contract::GetGameFromUnitRva,
            Native::Contract::GetGameFromUnitExpected.data(),
            static_cast<std::uint32_t>(Native::Contract::GetGameFromUnitExpected.size()))) {
        Context->LogError("BuffPanel BuffTracker: UNITS_GetGame native fingerprint mismatch; refusing whitelist-driven buff tracking.");
        Shutdown();
        return false;
    }
    GetGameFromUnit = reinterpret_cast<GetGameFromUnitFn>(
        context->exeBase + Native::Contract::GetGameFromUnitRva);

    if (!Context->CheckExpectedBytes(
            Native::Contract::GenericCurseStateLookupCallRva,
            Native::Contract::GenericCurseStateLookupCallExpected.data(),
            static_cast<std::uint32_t>(Native::Contract::GenericCurseStateLookupCallExpected.size()))) {
        Context->LogError("BuffPanel BuffTracker: exact-state StatList lookup native fingerprint mismatch; refusing premature timer-removal tracking.");
        Shutdown();
        return false;
    }
    GetStatListFromUnitAndState = reinterpret_cast<GetStatListFromUnitAndStateFn>(
        context->exeBase + Native::Contract::GetStatListFromUnitAndStateRva);

    if (Core::StatReads().RawGetter() == nullptr) {
        Context->LogError("BuffPanel BuffTracker: shared Core unit-stat reader is unavailable; resource-mode BuffHud entries cannot be supported.");
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
        Context->LogError("BuffPanel BuffTracker: failed to register with the shared Core STATLIST_PostStatList bus.");
        Shutdown();
        return false;
    }

    if (!RegisterTable() || !RegisterLifecycle()) {
        Context->LogError("BuffPanel BuffTracker: failed to register buff-hud table/lifecycle listeners.");
        Shutdown();
        return false;
    }

    Context->LogInfo(
        "Buff Panel 1.0.11 BuffTracker initialized: whitelist-driven timer/resource tracking with native skill attribution, configured skill fallback, and finite native expiry validation.");
    return true;
}

void Shutdown() noexcept {
    CurrentSessionGeneration.store(0, std::memory_order_release);
    AuthoritativeGame.store(nullptr, std::memory_order_release);
    AuthoritativePlayer.store(nullptr, std::memory_order_release);
    FramePumpScheduled.store(false, std::memory_order_release);
    Whitelist.store({}, std::memory_order_release);
    BuffHudTable = D2RL::CustomTables::InvalidHandle;
    TableSource = LooseBuffHud::Source::Embedded;
    TableSourcePath.clear();
    DataTablesListener = D2RL::Lifecycle::InvalidHandle;
    GameplayListeners = {};
    ClearTimerPresenceRecords();
    GetGameFromUnit = nullptr;
    GetStatListFromUnitAndState = nullptr;
    Resources = nullptr;
    CustomTables = nullptr;
    Threads = nullptr;
    Lifecycle = nullptr;
    Context = nullptr;
}

} // namespace BuffPanel::Systems::BuffTracker

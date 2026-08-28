// OBR Playable Races (Extended Races)
//
// Three writes into the running executable, none of which content can make:
//
//  1. The character-creation Confirm handler looks the selected RaceName up
//     in a TMap<FString,int32> compiled into the executable and dereferences
//     a miss. The map is rebuilt with the four added races.
//  2. GetIsRace is aliased so the four races satisfy every condition Imperial
//     satisfies, for the player only; race-gated dialogue (the tutorial
//     included) otherwise never matches them.
//  3. A female Dremora player is kept in AltVoiceFaction, which is how the
//     engine selects the AltVoice folder her combat lines are recorded in.
//
// Every structure is verified before it is written; on a mismatch the mod
// logs and changes nothing. Offsets are for OblivionRemastered-Win64-Shipping
// 1.512.105. See docs/findings/ in the Extended Races repository.

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include <Windows.h>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>

using namespace RC;

namespace
{
    // Offsets measured against OblivionRemastered-Win64-Shipping.exe 1.512.105.
    constexpr uintptr_t RaceIdMapRva = 0x09309240;
    constexpr uintptr_t StrihashRva = 0x00D83D50;

    // GetIsRace's CommandInfo row (stride 0x50, eval at +0x40) and the player
    // global. The eval compares race pointers; form ids only identify races.
    constexpr uintptr_t GetIsRaceCommandRva = 0x08FBBA30;
    constexpr size_t CommandLongNameOffset = 0x00;
    constexpr size_t CommandOpcodeOffset = 0x10;
    constexpr size_t CommandEvalOffset = 0x40;
    constexpr uint32_t GetIsRaceOpcode = 0x1045;
    constexpr uintptr_t PlayerPointerRva = 0x0941F708;

    // All five RACE records live in Oblivion.esm, whose mod index is always 0x00,
    // so these are the runtime form ids too - no load-order adjustment needed.
    constexpr uint32_t ImperialRaceFormId = 0x00000907;
    constexpr std::array<uint32_t, 4> AliasedRaceFormIds{
            0x0001208E,  // Dark Seducer
            0x0001208F,  // Golden Saint
            0x00038010,  // Dremora
            0x0005308E,  // Sheogorath
    };

    constexpr size_t FormTypeOffset = 0x08;
    constexpr size_t FormIdOffset = 0x10;
    constexpr size_t NpcRaceOffset = 0x1B0;
    constexpr uint8_t FormTypeNpc = 0x23;
    constexpr uint8_t FormTypeRace = 0x09;
    constexpr size_t BaseFormVFuncOffset = 0x310;

    // RaceId is an ordinal position in the alphabetical playable-race list,
    // so the shipped ten are renumbered around the four. Must match
    // RACE_ORDER in tools/build_pak.py.
    struct FRaceEntry
    {
        const wchar_t* Name;
        int32_t RaceId;
    };

    constexpr std::array<FRaceEntry, 14> RaceTable{{
            {L"Argonian", 0},     {L"Breton", 1},      {L"Dark Elf", 2},
            {L"Dark Seducer", 3}, {L"Dremora", 4},     {L"Golden Saint", 5},
            {L"High Elf", 6},     {L"Imperial", 7},    {L"Khajiit", 8},
            {L"Nord", 9},         {L"Orc", 10},        {L"Redguard", 11},
            {L"Sheogorath", 12},  {L"Wood Elf", 13},
    }};

    using FStrihash = uint32_t (*)(int32_t Length, const wchar_t* Str);

    // TSetElement<TPair<FString,int32>>
    struct FRaceIdElement
    {
        const wchar_t* Key;   // +0x00  FString::Data
        int32_t KeyNum;       // +0x08  length including the null terminator
        int32_t KeyMax;       // +0x0C
        int32_t Value;        // +0x10  the RaceId; the slot the crash reads
        int32_t Padding;      // +0x14
        int32_t HashNextId;   // +0x18  next element in this bucket, or -1
        int32_t HashIndex;    // +0x1C  which bucket this element is in
    };
    static_assert(sizeof(FRaceIdElement) == 32, "sparse-array element stride is 32 bytes");

    // TMap<FString,int32> == TSet<TSetElement<TPair<FString,int32>>>.
    struct FRaceIdMap
    {
        uint8_t* Base;

        auto Elements() -> FRaceIdElement*& { return *At<FRaceIdElement*>(0x00); }
        auto ArrayNum() -> int32_t& { return *At<int32_t>(0x08); }
        auto ArrayMax() -> int32_t& { return *At<int32_t>(0x0C); }
        auto AllocFlagsInline() -> uint32_t* { return At<uint32_t>(0x10); }
        auto AllocFlagsSecondary() -> uint32_t*& { return *At<uint32_t*>(0x20); }
        auto NumBits() -> int32_t& { return *At<int32_t>(0x28); }
        auto MaxBits() -> int32_t& { return *At<int32_t>(0x2C); }
        auto FirstFreeIndex() -> int32_t& { return *At<int32_t>(0x30); }
        auto NumFreeIndices() -> int32_t& { return *At<int32_t>(0x34); }
        auto HashSecondary() -> int32_t*& { return *At<int32_t*>(0x40); }
        auto HashSize() -> int32_t& { return *At<int32_t>(0x48); }

        // TInlineAllocator: the secondary pointer wins once it is set.
        auto Hash() -> int32_t* { return HashSecondary() ? HashSecondary() : At<int32_t>(0x38); }

      private:
        template <typename T> auto At(size_t offset) -> T*
        {
            return reinterpret_cast<T*>(Base + offset);
        }
    };

    auto Hex(uint32_t value) -> std::wstring
    {
        wchar_t buffer[16]{};
        std::swprintf(buffer, std::size(buffer), L"0x%08X", value);
        return buffer;
    }

    auto Log(const std::wstring& line) -> void
    {
        Output::send<LogLevel::Default>(std::wstring(STR("[OBRPlayableRaces] ")) + line + STR("\n"));
    }

    auto LogError(const std::wstring& line) -> void
    {
        Output::send<LogLevel::Error>(std::wstring(STR("[OBRPlayableRaces] ")) + line + STR("\n"));
    }

    // Every entry hashed with the game's own function and found through its
    // bucket chain: the layout is understood, or nothing is written.
    auto VerifyModel(FRaceIdMap& map, FStrihash Strihash) -> bool
    {
        const auto count = map.ArrayNum();
        const auto size = map.HashSize();
        auto* hash = map.Hash();
        auto* elements = map.Elements();

        if (!elements || !hash || count <= 0 || count > 128 || size <= 0 ||
            (size & (size - 1)) != 0)
        {
            LogError(STR("map shape is not recognisable: num=") + std::to_wstring(count) +
                     STR(" hashSize=") + std::to_wstring(size));
            return false;
        }

        for (int32_t i = 0; i < count; ++i)
        {
            const auto& element = elements[i];
            if (!element.Key || element.KeyNum <= 0)
            {
                LogError(STR("element ") + std::to_wstring(i) + STR(" has no key"));
                return false;
            }

            const auto bucket =
                    static_cast<int32_t>(Strihash(element.KeyNum - 1, element.Key) & (size - 1));
            if (element.HashIndex != bucket)
            {
                LogError(std::wstring(STR("bucket mismatch for \"")) + element.Key +
                         STR("\": stored ") + std::to_wstring(element.HashIndex) +
                         STR(", computed ") + std::to_wstring(bucket));
                return false;
            }

            bool found = false;
            int32_t id = hash[bucket];
            for (int32_t guard = 0; id >= 0 && guard <= count; ++guard)
            {
                if (id == i) { found = true; break; }
                id = elements[id].HashNextId;
            }
            if (!found)
            {
                LogError(std::wstring(STR("bucket chain does not reach \"")) + element.Key +
                         STR("\""));
                return false;
            }
        }
        Log(STR("model verified against all ") + std::to_wstring(count) + STR(" entries"));
        return true;
    }

    // ---------------------------------------------------------------------
    // GetIsRace alias: the four races satisfy every check Imperial satisfies,
    // player only. Race-gated dialogue names the ten shipped races and nothing
    // else, and the tutorial cannot be completed without a match.
    using FGetIsRaceEval = bool (*)(void* Subject, void* RaceParam, void* Param2, double* Result);
    FGetIsRaceEval g_OriginalGetIsRace = nullptr;
    void** g_PlayerPointer = nullptr;

    template <typename T> auto CallVFunc(void* object, size_t byte_offset) -> T
    {
        auto* vtable = *reinterpret_cast<uint8_t***>(object);
        auto fn = reinterpret_cast<T (*)(void*)>(vtable[byte_offset / sizeof(void*)]);
        return fn(object);
    }

    bool g_AliasReported = false;

    auto GetIsRaceAlias(void* subject, void* race_param, void* param2, double* result) -> bool
    {
        const bool ok = g_OriginalGetIsRace(subject, race_param, param2, result);

        // Only ever turn a miss into a match.
        if (!ok || !result || *result != 0.0 || !subject || !race_param) { return ok; }

        // Player only, by pointer identity; most race conditions run on the
        // speaker, and NPCs of these races must keep their own dialogue.
        if (!g_PlayerPointer || subject != *g_PlayerPointer) { return ok; }

        if (*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(race_param) + FormIdOffset) !=
            ImperialRaceFormId)
        {
            return ok;
        }

        // subject -> base form -> NPC_ -> race, the walk the original made.
        auto* npc = CallVFunc<void*>(subject, BaseFormVFuncOffset);
        if (!npc) { return ok; }
        if (*(static_cast<uint8_t*>(npc) + FormTypeOffset) != FormTypeNpc) { return ok; }
        auto* race = *reinterpret_cast<void**>(static_cast<uint8_t*>(npc) + NpcRaceOffset);
        if (!race) { return ok; }

        const auto type = *(static_cast<uint8_t*>(race) + FormTypeOffset);
        const auto id = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(race) + FormIdOffset);
        bool aliased = false;
        for (const auto candidate : AliasedRaceFormIds)
        {
            if (id == candidate) { aliased = true; break; }
        }
        if (aliased) { *result = 1.0; }

        if (!g_AliasReported)
        {
            g_AliasReported = true;
            // formType 9 is RACE. Anything else means FormTypeOffset or
            // NpcRaceOffset is wrong for this build and the id below is noise.
            Log(STR("player race resolved: formType=") + std::to_wstring(type) +
                STR(" formId=") + Hex(id) +
                (type != FormTypeRace ? STR(" (EXPECTED 9 - offsets do not match this build)")
                                      : (aliased ? STR(" - aliased to Imperial")
                                                 : STR(" - not an aliased race, left alone"))));
        }
        return ok;
    }

    auto InstallRaceConditionAlias(uintptr_t base) -> void
    {
        auto* command = reinterpret_cast<uint8_t*>(base + GetIsRaceCommandRva);

        const auto* name = *reinterpret_cast<const char**>(command + CommandLongNameOffset);
        const auto opcode = *reinterpret_cast<uint32_t*>(command + CommandOpcodeOffset);
        if (!name || std::strcmp(name, "GetIsRace") != 0 || opcode != GetIsRaceOpcode)
        {
            LogError(std::wstring(STR("GetIsRace command entry did not verify (name=")) +
                     (name ? std::wstring(name, name + std::strlen(name)) : std::wstring(STR("<null>"))) +
                     STR(" opcode=") + std::to_wstring(opcode) + STR("); NOT aliasing"));
            return;
        }

        g_PlayerPointer = reinterpret_cast<void**>(base + PlayerPointerRva);
        auto** slot = reinterpret_cast<FGetIsRaceEval*>(command + CommandEvalOffset);
        if (*slot == &GetIsRaceAlias) { Log(STR("race condition alias already installed")); return; }

        g_OriginalGetIsRace = *slot;
        if (!g_OriginalGetIsRace) { LogError(STR("GetIsRace eval slot is null; NOT aliasing")); return; }

        DWORD previous = 0;
        if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &previous))
        {
            LogError(STR("could not make the eval slot writable"));
            return;
        }
        *slot = &GetIsRaceAlias;
        VirtualProtect(slot, sizeof(*slot), previous, &previous);

        Log(STR("race conditions aliased to Imperial for the four added races (player only)"));
    }

    // ---------------------------------------------------------------------
    // AltVoiceFaction membership for a female Dremora player. The engine
    // derives an actor's alt-voice flag (+0x228) from a faction whose display
    // name contains "AltVoiceFaction", and files her combat lines under the
    // AltVoice folder only when it is set.
    //
    //   TESDataHandler global 0x9480018, FACT list at +0xB8 {item, next};
    //   faction display name char* at +0x38; TESNPC: TESActorBaseData at
    //   +0x48, female flag bit 0 at +0x50, race at +0x1B0;
    //   TESActorBaseData::SetFactionRank 0x683EE00 (rank -1 removes),
    //   GetFactionRank 0x683E160 (-1 when not a member).
    constexpr uintptr_t DataHandlerPointerRva = 0x09480018;
    constexpr size_t DataHandlerFactionListOffset = 0xB8;
    constexpr uint8_t FormTypeFaction = 0x06;
    constexpr size_t FactionEditorIdOffset = 0x38;
    constexpr size_t FactionEditorIdLengthOffset = 0x40;
    constexpr size_t NpcActorBaseDataOffset = 0x48;
    constexpr size_t NpcFemaleFlagOffset = 0x50;
    constexpr size_t ActorAltVoiceOffset = 0x228;
    constexpr uintptr_t SetFactionRankRva = 0x0683EE00;
    constexpr uintptr_t GetFactionRankRva = 0x0683E160;
    constexpr uint32_t DremoraRaceFormId = 0x00038010;
    constexpr const char* AltVoiceFactionEditorId = "AltVoiceFaction";

    using FSetFactionRank = void (*)(void* ActorBaseData, void* Faction, int8_t Rank);
    using FGetFactionRank = int32_t (*)(void* ActorBaseData, void* Faction, bool IsPlayer);

    struct FListNode
    {
        void* Item;
        FListNode* Next;
    };

    void* g_AltVoiceFaction = nullptr;
    int g_FactionSearches = 0;
    bool g_FactionSearchWarned = false;
    bool g_VoiceStateKnown = false;
    bool g_VoiceStateLast = false;

    auto FindAltVoiceFaction(uintptr_t base, int* seenNodes, int* seenFactions,
                             std::wstring* firstNames) -> void*
    {
        auto* handler = *reinterpret_cast<uint8_t**>(base + DataHandlerPointerRva);
        *seenNodes = 0;
        *seenFactions = 0;
        if (!handler) { return nullptr; }
        auto* node = reinterpret_cast<FListNode*>(handler + DataHandlerFactionListOffset);
        for (; node && *seenNodes < 100000; node = node->Next)
        {
            ++*seenNodes;
            auto* form = static_cast<uint8_t*>(node->Item);
            if (!form || form[FormTypeOffset] != FormTypeFaction) { continue; }
            ++*seenFactions;
            const auto* edid = *reinterpret_cast<const char**>(form + FactionEditorIdOffset);
            if (!edid) { continue; }
            if (*seenFactions <= 3)
            {
                *firstNames += std::wstring(edid, edid + std::strlen(edid)) + STR(" ");
            }
            if (std::strstr(edid, AltVoiceFactionEditorId) != nullptr) { return form; }
        }
        return nullptr;
    }

    // Runs on the game thread from the tick; a few reads when nothing changes.
    auto ApplyVoiceFaction(uintptr_t base) -> void
    {
        if (!g_PlayerPointer) { return; }
        auto* player = static_cast<uint8_t*>(*g_PlayerPointer);
        if (!player) { return; }
        auto* npc = static_cast<uint8_t*>(CallVFunc<void*>(player, BaseFormVFuncOffset));
        if (!npc || npc[FormTypeOffset] != FormTypeNpc) { return; }

        if (!g_AltVoiceFaction)
        {
            int nodes = 0, factions = 0;
            std::wstring first;
            g_AltVoiceFaction = FindAltVoiceFaction(base, &nodes, &factions, &first);
            if (!g_AltVoiceFaction)
            {
                // The faction list fills as plugins load; say what was seen
                // after a few seconds so a miss is visible.
                if (++g_FactionSearches == 20 && !g_FactionSearchWarned)
                {
                    g_FactionSearchWarned = true;
                    LogError(STR("AltVoiceFaction not found yet: walked ") + std::to_wstring(nodes) +
                             STR(" list nodes, ") + std::to_wstring(factions) +
                             STR(" FACT forms; first names: ") + first);
                }
                return;
            }
            Log(STR("faction list: ") + std::to_wstring(nodes) + STR(" nodes, ") +
                std::to_wstring(factions) + STR(" FACT forms walked"));
            Log(std::wstring(STR("AltVoiceFaction found: formId ")) +
                Hex(*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(g_AltVoiceFaction) +
                                                 FormIdOffset)));
        }

        auto* race = *reinterpret_cast<uint8_t**>(npc + NpcRaceOffset);
        const bool female = (npc[NpcFemaleFlagOffset] & 1) != 0;
        const bool dremora = race && race[FormTypeOffset] == FormTypeRace &&
                             *reinterpret_cast<uint32_t*>(race + FormIdOffset) == DremoraRaceFormId;
        const bool wanted = dremora && female;

        auto SetFactionRank = reinterpret_cast<FSetFactionRank>(base + SetFactionRankRva);
        auto GetFactionRank = reinterpret_cast<FGetFactionRank>(base + GetFactionRankRva);

        void* baseData = npc + NpcActorBaseDataOffset;
        const bool member = GetFactionRank(baseData, g_AltVoiceFaction, true) != -1;

        bool changed = false;
        if (wanted && !member)
        {
            SetFactionRank(baseData, g_AltVoiceFaction, 0);
            changed = true;
            Log(STR("female Dremora player: added to AltVoiceFaction"));
        }
        else if (!wanted && member)
        {
            SetFactionRank(baseData, g_AltVoiceFaction, -1);
            changed = true;
            Log(STR("player is no longer a female Dremora: removed from AltVoiceFaction"));
        }

        // The engine recomputes the flag when the pawn pairs; for a save that
        // is already loaded, write the byte directly. (Calling the engine's
        // recompute routine from the tick crashes inside the pairing send.)
        const bool flag = player[ActorAltVoiceOffset] != 0;
        if (flag != wanted)
        {
            player[ActorAltVoiceOffset] = wanted ? 1 : 0;
        }
        const bool now = player[ActorAltVoiceOffset] != 0;
        if (!g_VoiceStateKnown || now != g_VoiceStateLast)
        {
            g_VoiceStateKnown = true;
            g_VoiceStateLast = now;
            Log(std::wstring(STR("player alt-voice flag is now ")) + (now ? STR("1") : STR("0")));
        }
    }

    auto ExtendMap() -> bool
    {
        auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!base) { LogError(STR("no module base")); return true; }

        FRaceIdMap map{reinterpret_cast<uint8_t*>(base + RaceIdMapRva)};
        auto Strihash = reinterpret_cast<FStrihash>(base + StrihashRva);

        const auto original = map.ArrayNum();
        Log(STR("race-id map: num=") + std::to_wstring(original) + STR(" max=") +
            std::to_wstring(map.ArrayMax()) + STR(" free=") +
            std::to_wstring(map.NumFreeIndices()) + STR(" hashSize=") +
            std::to_wstring(map.HashSize()) + STR(" hashSecondary=") +
            (map.HashSecondary() ? STR("yes") : STR("no")) + STR(" flagsSecondary=") +
            (map.AllocFlagsSecondary() ? STR("yes") : STR("no")));

        if (original <= 0 || original > 128)
        {
            LogError(STR("entry count is implausible; wrong address for this build"));
            return true;
        }
        for (int32_t i = 0; i < original; ++i)
        {
            const auto& element = map.Elements()[i];
            Log(std::wstring(STR("  [")) + std::to_wstring(i) + STR("] \"") +
                (element.Key ? element.Key : STR("<null>")) + STR("\" -> ") +
                std::to_wstring(element.Value));
        }

        if (map.NumFreeIndices() != 0)
        {
            LogError(STR("sparse array has holes; a rebuild would need free-list handling"));
            return true;
        }
        if (map.AllocFlagsSecondary() != nullptr)
        {
            LogError(STR("allocation flags are heap-allocated; not handled"));
            return true;
        }
        if (!VerifyModel(map, Strihash))
        {
            LogError(STR("NOT modifying the map"));
            return true;
        }

        const auto total = static_cast<int32_t>(RaceTable.size());

        auto* elements = static_cast<FRaceIdElement*>(
                std::calloc(static_cast<size_t>(total), sizeof(FRaceIdElement)));
        if (!elements) { LogError(STR("element allocation failed")); return true; }

        for (int32_t i = 0; i < total; ++i)
        {
            auto& element = elements[i];
            const auto length = static_cast<int32_t>(std::wcslen(RaceTable[i].Name));
            element.Key = RaceTable[i].Name;  // static storage, outlives the process
            element.KeyNum = length + 1;
            element.KeyMax = length + 1;
            element.Value = RaceTable[i].RaceId;
        }

        int32_t hashSize = map.HashSize();
        while (hashSize < total * 2) { hashSize *= 2; }
        auto* hash =
                static_cast<int32_t*>(std::malloc(static_cast<size_t>(hashSize) * sizeof(int32_t)));
        if (!hash)
        {
            LogError(STR("hash allocation failed"));
            std::free(elements);
            return true;
        }
        for (int32_t i = 0; i < hashSize; ++i) { hash[i] = -1; }
        for (int32_t i = 0; i < total; ++i)
        {
            auto& element = elements[i];
            const auto bucket =
                    static_cast<int32_t>(Strihash(element.KeyNum - 1, element.Key) & (hashSize - 1));
            element.HashIndex = bucket;
            element.HashNextId = hash[bucket];
            hash[bucket] = i;
        }

        map.Elements() = elements;
        map.ArrayNum() = total;
        map.ArrayMax() = total;
        map.AllocFlagsInline()[0] = (total >= 32) ? 0xFFFFFFFFu : ((1u << total) - 1u);
        map.NumBits() = total;
        if (map.MaxBits() < total) { map.MaxBits() = total; }
        map.FirstFreeIndex() = -1;
        map.NumFreeIndices() = 0;
        map.HashSecondary() = hash;
        map.HashSize() = hashSize;

        FRaceIdMap written{reinterpret_cast<uint8_t*>(base + RaceIdMapRva)};
        if (!VerifyModel(written, Strihash))
        {
            LogError(STR("post-write verification FAILED - the map is now inconsistent"));
            return true;
        }
        Log(STR("rebuilt with ") + std::to_wstring(total) + STR(" entries, hashSize=") +
            std::to_wstring(hashSize) + STR("; all re-verified"));
        return true;
    }
} // namespace

class OBRPlayableRaces : public CppUserModBase
{
  public:
    OBRPlayableRaces()
    {
        ModName = STR("OBRPlayableRaces");
        ModVersion = STR("0.3.0");
        ModDescription = STR("Extends the engine's built-in race-name table so races added by a "
                             "content mod can be confirmed in character creation, aliases their "
                             "race conditions to Imperial, and keeps a female Dremora on her "
                             "alt-voice recordings.");
        ModAuthors = STR("Extended Races");
    }

    ~OBRPlayableRaces() override = default;

    // The legacy engine is not up at init; poll until the map and alias are
    // in, then keep ticking for the faction check.
    auto on_unreal_init() -> void override
    {
        m_tick = Unreal::Hook::RegisterEngineTickPreCallback(
                [this](auto&, Unreal::UEngine*, float, bool) {
                    if (++m_ticks < TicksBetweenAttempts) { return; }
                    m_ticks = 0;
                    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
                    if (!m_done)
                    {
                        if (!ExtendMap()) { return; }
                        InstallRaceConditionAlias(base);
                        m_done = true;
                    }
                    ApplyVoiceFaction(base);
                },
                Unreal::Hook::FCallbackOptions{
                        .bOnce = false,
                        .bReadonly = true,
                        .OwnerModName = STR("OBRPlayableRaces"),
                        .HookName = STR("RaceIdMapRebuild"),
                });
        Output::send<LogLevel::Verbose>(
                STR("[OBRPlayableRaces] waiting for the legacy race list\n"));
    }

  private:
    static constexpr int TicksBetweenAttempts = 30;
    Unreal::Hook::GlobalCallbackId m_tick{Unreal::Hook::ERROR_ID};
    int m_ticks{};
    bool m_done{};
};

#define OBR_PLAYABLE_RACES_API __declspec(dllexport)
extern "C"
{
    OBR_PLAYABLE_RACES_API CppUserModBase* start_mod() { return new OBRPlayableRaces(); }
    OBR_PLAYABLE_RACES_API void uninstall_mod(CppUserModBase* mod) { delete mod; }
}

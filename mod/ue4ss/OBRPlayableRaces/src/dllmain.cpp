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
//     engine selects the AltVoice folder her combat lines are recorded in,
//     and her alt-voice flag is kept set. The engine derives that flag for
//     every actor once, when it pairs with its pawn - except the player, whose
//     pairing override is a stub - so both are checked from the engine tick:
//     every half second, plus immediately after a map load and the first time
//     a race condition is evaluated for the player (which follows Confirm).
//
// This is a UNBSE add-on. It registers with the UNBSE host, declaring that it
// reads and writes the running executable, verifies the executable's identity
// through UNBSE's runtime-info service, resolves every address through its
// bounded relocation service, listens for the host's lifecycle messages, and
// publishes a read-only status function to UE4SS Lua as
// UNBSE.Invoke("obr_playable_races", "status"). Without UNBSE the same checks
// run against the PE header directly (UNBSE's report-and-attempt policy).
//
// Every structure is verified before it is written; on a mismatch the mod
// logs and changes nothing. Offsets are for OblivionRemastered-Win64-Shipping
// 1.512.105. See docs/findings/ in the Extended Races repository.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include <Windows.h>
#include <Psapi.h>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>

#include <UNBSEAddonHostV1.h>
#include <UNBSEMessagingV1.h>
#include <UNBSERelocationV1.h>
#include <UNBSERuntimeInfoV1.h>

using namespace RC;

namespace
{
    constexpr auto ModVersionString = STR("0.5.0");
    constexpr auto AddonId = "obr.playable-races";
    constexpr auto AddonVersion = "0.5.0";
    constexpr auto ScriptNamespace = "obr_playable_races";

    // OblivionRemastered-Win64-Shipping.exe 1.512.105: PE link timestamp and
    // SizeOfImage. The same two values UNBSE pins for this game version. Every
    // offset below is only meaningful in that image.
    constexpr uint32_t ExpectedPeTimestamp = 0xF19077A4u;
    constexpr uint64_t ExpectedImageSize = 0x09E1E000u;

    // Offsets measured against OblivionRemastered-Win64-Shipping.exe 1.512.105.
    constexpr uintptr_t RaceIdMapRva = 0x09309240;
    constexpr uintptr_t StrihashRva = 0x00D83D50;

    // GetIsRace's CommandInfo row (stride 0x50, eval at +0x40) and the player
    // global. The eval compares race pointers; form ids only identify races.
    constexpr uintptr_t GetIsRaceCommandRva = 0x08FBBA30;
    constexpr size_t CommandLongNameOffset = 0x00;
    constexpr size_t CommandOpcodeOffset = 0x10;
    constexpr size_t CommandEvalOffset = 0x40;
    constexpr size_t CommandStride = 0x50;
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
        static constexpr size_t Size = 0x50;

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

    auto Hex(uint64_t value) -> std::wstring
    {
        wchar_t buffer[24]{};
        std::swprintf(buffer, std::size(buffer), L"0x%08llX", static_cast<unsigned long long>(value));
        return buffer;
    }

    auto Widen(const char* text) -> std::wstring
    {
        return text ? std::wstring(text, text + std::strlen(text)) : std::wstring(STR("<null>"));
    }

    auto Log(const std::wstring& line) -> void
    {
        Output::send<LogLevel::Default>(std::wstring(STR("[OBRPlayableRaces] ")) + line + STR("\n"));
    }

    auto LogError(const std::wstring& line) -> void
    {
        Output::send<LogLevel::Error>(std::wstring(STR("[OBRPlayableRaces] ")) + line + STR("\n"));
    }

    // ---------------------------------------------------------------------
    // State the Lua-visible status function reads. Only ever atomics: that
    // function runs on whichever thread holds the UE4SS Lua VM, never the
    // game thread.
    enum : int { StatePending = 0, StateDone = 1, StateRefused = 2 };
    std::atomic<int> g_RuntimeState{StatePending};   // executable identity
    std::atomic<int> g_MapState{StatePending};
    std::atomic<int> g_AliasState{StatePending};
    std::atomic<int> g_VoiceFlag{-1};                 // -1 unknown
    std::atomic<int> g_FactionMember{-1};             // -1 unknown
    std::atomic<int> g_HostState{0};                  // 0 absent, 1 verified, 2 unverified attempt
    std::atomic<bool> g_HostStopping{false};

    auto StateName(int state) -> const char*
    {
        return state == StateDone ? "done" : state == StateRefused ? "refused" : "pending";
    }

    // ---------------------------------------------------------------------
    // UNBSE host link. Every export is looked up in the loaded modules rather
    // than by DLL name, so the mod does not depend on where UNBSE lives.
    template <typename Function>
    auto FindExport(const char* name) -> Function
    {
        std::array<HMODULE, 1024> modules{};
        DWORD bytesNeeded = 0;
        if (!EnumProcessModules(GetCurrentProcess(), modules.data(),
                                static_cast<DWORD>(sizeof(modules)), &bytesNeeded))
        {
            return nullptr;
        }
        const auto count = std::min<size_t>(modules.size(), bytesNeeded / sizeof(HMODULE));
        for (size_t i = 0; i < count; ++i)
        {
            const auto address = GetProcAddress(modules[i], name);
            if (!address) { continue; }
            static_assert(sizeof(address) == sizeof(Function));
            Function result{};
            std::memcpy(&result, &address, sizeof(result));
            return result;
        }
        return nullptr;
    }

    struct FHostLink
    {
        UNBSEAddonHostV1 Host{};
        UNBSERelocationV1 Relocation{};
        UNBSEMessagingV1 Messaging{};
        UNBSEScriptServiceV1 Script{};
        uint32_t Owner{};
        bool Registered{};
        bool RelocationReady{};
        bool MessagingReady{};
        bool ScriptReady{};
    };

    // What is loaded, from UNBSE's runtime-info service when it is present and
    // from the PE header otherwise. The same two fields either way.
    struct FRuntimeIdentity
    {
        bool Known{};
        uint64_t ImageBase{};
        uint64_t ImageSize{};
        uint32_t PeTimestamp{};
        std::wstring Source{STR("none")};
        std::wstring HostVersion{};
        std::wstring FoundationId{};
    };

    auto ReadPeIdentity(FRuntimeIdentity* identity) -> bool
    {
        const auto module = GetModuleHandleW(nullptr);
        if (!module) { return false; }
        const auto* base = reinterpret_cast<const uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 1024 * 1024)
        {
            return false;
        }
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt->OptionalHeader.SizeOfImage == 0)
        {
            return false;
        }
        identity->Known = true;
        identity->ImageBase = reinterpret_cast<uint64_t>(base);
        identity->ImageSize = nt->OptionalHeader.SizeOfImage;
        identity->PeTimestamp = nt->FileHeader.TimeDateStamp;
        identity->Source = STR("PE header");
        return true;
    }

    auto QueryRuntimeIdentity(FRuntimeIdentity* identity) -> bool
    {
        const auto query = FindExport<UNBSEQueryRuntimeInfoV1Function>("UNBSE_QueryRuntimeInfoV1");
        if (query)
        {
            UNBSERuntimeInfoV1 info{};
            info.structSize = sizeof(info);
            info.apiVersion = UNBSE_RUNTIME_INFO_ABI_VERSION;
            if (query(UNBSE_RUNTIME_INFO_ABI_VERSION, &info) &&
                (info.identityFlags & UNBSE_RUNTIME_IDENTITY_PE_OBSERVED) != 0 &&
                info.imageBase != 0 && info.imageSize != 0)
            {
                identity->Known = true;
                identity->ImageBase = info.imageBase;
                identity->ImageSize = info.imageSize;
                identity->PeTimestamp = info.peTimestamp;
                identity->Source = STR("UNBSE runtime-info v1");
                identity->HostVersion = Widen(info.hostVersion);
                identity->FoundationId = Widen(info.foundationId);
                return true;
            }
        }
        return ReadPeIdentity(identity);
    }

    FHostLink g_Link{};
    FRuntimeIdentity g_Runtime{};

    // Absolute address of an executable RVA. Through the host's bounded
    // resolver when registered; otherwise the same bounds check, locally.
    auto Resolve(uintptr_t rva, size_t bytes) -> uintptr_t
    {
        if (g_Link.Registered && g_Link.RelocationReady)
        {
            UNBSERelocationRequestV1 request{};
            request.structSize = sizeof(request);
            request.apiVersion = UNBSE_RELOCATION_ABI_VERSION;
            request.ownerHandle = g_Link.Owner;
            request.module = UNBSE_RELOCATION_MODULE_EXECUTABLE;
            request.relativeAddress = rva;
            request.minimumBytes = bytes;
            UNBSERelocationResultV1 result{};
            result.structSize = sizeof(result);
            result.apiVersion = UNBSE_RELOCATION_ABI_VERSION;
            const auto status = g_Link.Relocation.resolve(&request, &result);
            if (status != UNBSE_RELOCATION_RESULT_OK || result.relativeAddress != rva)
            {
                LogError(STR("UNBSE could not resolve RVA ") + Hex(rva) + STR(": ") +
                         Widen(g_Link.Relocation.resultName ? g_Link.Relocation.resultName(status)
                                                             : "unknown"));
                return 0;
            }
            return static_cast<uintptr_t>(result.absoluteAddress);
        }
        if (!g_Runtime.Known || rva >= g_Runtime.ImageSize || bytes > g_Runtime.ImageSize - rva)
        {
            LogError(STR("RVA ") + Hex(rva) + STR(" is outside the executable image"));
            return 0;
        }
        return static_cast<uintptr_t>(g_Runtime.ImageBase + rva);
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
    // Raised by the alias the first time it sees the player; the tick turns it
    // into a voice check. Confirm is followed by race-gated condition
    // evaluation, so this lands right after the character exists.
    std::atomic<bool> g_PlayerRaceSeen{false};

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
            g_PlayerRaceSeen.store(true);
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

    auto InstallRaceConditionAlias() -> void
    {
        const auto commandAddress = Resolve(GetIsRaceCommandRva, CommandStride);
        const auto playerAddress = Resolve(PlayerPointerRva, sizeof(void*));
        if (!commandAddress || !playerAddress)
        {
            g_AliasState.store(StateRefused);
            LogError(STR("GetIsRace command or player global did not resolve; NOT aliasing"));
            return;
        }
        auto* command = reinterpret_cast<uint8_t*>(commandAddress);

        const auto* name = *reinterpret_cast<const char**>(command + CommandLongNameOffset);
        const auto opcode = *reinterpret_cast<uint32_t*>(command + CommandOpcodeOffset);
        if (!name || std::strcmp(name, "GetIsRace") != 0 || opcode != GetIsRaceOpcode)
        {
            g_AliasState.store(StateRefused);
            LogError(std::wstring(STR("GetIsRace command entry did not verify (name=")) + Widen(name) +
                     STR(" opcode=") + std::to_wstring(opcode) + STR("); NOT aliasing"));
            return;
        }

        g_PlayerPointer = reinterpret_cast<void**>(playerAddress);
        auto** slot = reinterpret_cast<FGetIsRaceEval*>(command + CommandEvalOffset);
        if (*slot == &GetIsRaceAlias)
        {
            g_AliasState.store(StateDone);
            Log(STR("race condition alias already installed"));
            return;
        }

        g_OriginalGetIsRace = *slot;
        if (!g_OriginalGetIsRace)
        {
            g_AliasState.store(StateRefused);
            LogError(STR("GetIsRace eval slot is null; NOT aliasing"));
            return;
        }

        DWORD previous = 0;
        if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &previous))
        {
            g_AliasState.store(StateRefused);
            LogError(STR("could not make the eval slot writable"));
            return;
        }
        *slot = &GetIsRaceAlias;
        VirtualProtect(slot, sizeof(*slot), previous, &previous);

        g_AliasState.store(StateDone);
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
    constexpr size_t NpcActorBaseDataOffset = 0x48;
    constexpr size_t NpcFemaleFlagOffset = 0x50;
    constexpr size_t ActorAltVoiceOffset = 0x228;
    constexpr uintptr_t SetFactionRankRva = 0x0683EE00;
    constexpr uintptr_t GetFactionRankRva = 0x0683E160;
    constexpr size_t FunctionProbeBytes = 16;
    constexpr uint32_t DremoraRaceFormId = 0x00038010;
    constexpr const char* AltVoiceFactionEditorId = "AltVoiceFaction";

    using FSetFactionRank = void (*)(void* ActorBaseData, void* Faction, int8_t Rank);
    using FGetFactionRank = int32_t (*)(void* ActorBaseData, void* Faction, bool IsPlayer);

    struct FListNode
    {
        void* Item;
        FListNode* Next;
    };

    uint8_t** g_DataHandlerPointer = nullptr;
    FSetFactionRank g_SetFactionRank = nullptr;
    FGetFactionRank g_GetFactionRank = nullptr;
    void* g_AltVoiceFaction = nullptr;
    int g_FactionSearches = 0;
    bool g_FactionSearchWarned = false;
    bool g_VoiceStateKnown = false;
    bool g_VoiceStateLast = false;

    auto ResolveVoiceRoutines() -> bool
    {
        if (g_DataHandlerPointer && g_SetFactionRank && g_GetFactionRank) { return true; }
        const auto handler = Resolve(DataHandlerPointerRva, sizeof(void*));
        const auto set = Resolve(SetFactionRankRva, FunctionProbeBytes);
        const auto get = Resolve(GetFactionRankRva, FunctionProbeBytes);
        if (!handler || !set || !get)
        {
            LogError(STR("faction routines did not resolve; the voice check is off"));
            return false;
        }
        g_DataHandlerPointer = reinterpret_cast<uint8_t**>(handler);
        g_SetFactionRank = reinterpret_cast<FSetFactionRank>(set);
        g_GetFactionRank = reinterpret_cast<FGetFactionRank>(get);
        return true;
    }

    auto FindAltVoiceFaction(int* seenNodes, int* seenFactions, std::wstring* firstNames) -> void*
    {
        auto* handler = *g_DataHandlerPointer;
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
                *firstNames += Widen(edid) + STR(" ");
            }
            if (std::strstr(edid, AltVoiceFactionEditorId) != nullptr) { return form; }
        }
        return nullptr;
    }

    // Runs on the game thread from the tick. Returns false while the player,
    // its NPC record or the faction cannot be read yet, so the caller retries.
    // Cheap when nothing has changed: a handful of reads and one faction-rank
    // lookup; it only writes, and only logs, on a change.
    auto ApplyVoiceFaction(const wchar_t* reason) -> bool
    {
        if (!g_PlayerPointer || !ResolveVoiceRoutines()) { return false; }
        auto* player = static_cast<uint8_t*>(*g_PlayerPointer);
        if (!player) { return false; }
        auto* npc = static_cast<uint8_t*>(CallVFunc<void*>(player, BaseFormVFuncOffset));
        if (!npc || npc[FormTypeOffset] != FormTypeNpc) { return false; }

        if (!g_AltVoiceFaction)
        {
            int nodes = 0, factions = 0;
            std::wstring first;
            g_AltVoiceFaction = FindAltVoiceFaction(&nodes, &factions, &first);
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
                return false;
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

        void* baseData = npc + NpcActorBaseDataOffset;
        const bool member = g_GetFactionRank(baseData, g_AltVoiceFaction, true) != -1;

        if (wanted && !member)
        {
            g_SetFactionRank(baseData, g_AltVoiceFaction, 0);
            Log(std::wstring(STR("female Dremora player: added to AltVoiceFaction (")) + reason + STR(")"));
        }
        else if (!wanted && member)
        {
            g_SetFactionRank(baseData, g_AltVoiceFaction, -1);
            Log(std::wstring(STR("player is not a female Dremora: removed from AltVoiceFaction (")) +
                reason + STR(")"));
        }
        g_FactionMember.store(g_GetFactionRank(baseData, g_AltVoiceFaction, true) != -1 ? 1 : 0);

        // Nothing native ever derives the player's flag (PlayerCharacter stubs
        // the pairing routine that does it for NPCs), so write the byte. The
        // engine's own recompute cannot be called on the player: it sends a
        // pairing the player path never set up, and crashes.
        const bool flag = player[ActorAltVoiceOffset] != 0;
        if (flag != wanted)
        {
            player[ActorAltVoiceOffset] = wanted ? 1 : 0;
        }
        const bool now = player[ActorAltVoiceOffset] != 0;
        g_VoiceFlag.store(now ? 1 : 0);
        if (!g_VoiceStateKnown || now != g_VoiceStateLast)
        {
            g_VoiceStateKnown = true;
            g_VoiceStateLast = now;
            Log(std::wstring(STR("player alt-voice flag is now ")) + (now ? STR("1") : STR("0")) +
                STR(" (") + reason + STR(")"));
        }
        return true;
    }

    // Returns true when the map is settled - rebuilt, or refused for good.
    // False means the legacy engine has not populated it yet: try again.
    auto ExtendMap() -> bool
    {
        const auto mapAddress = Resolve(RaceIdMapRva, FRaceIdMap::Size);
        const auto hashAddress = Resolve(StrihashRva, FunctionProbeBytes);
        if (!mapAddress || !hashAddress)
        {
            g_MapState.store(StateRefused);
            LogError(STR("race-id map or strihash did not resolve; NOT modifying the map"));
            return true;
        }

        FRaceIdMap map{reinterpret_cast<uint8_t*>(mapAddress)};
        auto Strihash = reinterpret_cast<FStrihash>(hashAddress);

        const auto original = map.ArrayNum();
        Log(STR("race-id map: num=") + std::to_wstring(original) + STR(" max=") +
            std::to_wstring(map.ArrayMax()) + STR(" free=") +
            std::to_wstring(map.NumFreeIndices()) + STR(" hashSize=") +
            std::to_wstring(map.HashSize()) + STR(" hashSecondary=") +
            (map.HashSecondary() ? STR("yes") : STR("no")) + STR(" flagsSecondary=") +
            (map.AllocFlagsSecondary() ? STR("yes") : STR("no")));

        if (original <= 0 || original > 128)
        {
            g_MapState.store(StateRefused);
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
            g_MapState.store(StateRefused);
            LogError(STR("sparse array has holes; a rebuild would need free-list handling"));
            return true;
        }
        if (map.AllocFlagsSecondary() != nullptr)
        {
            g_MapState.store(StateRefused);
            LogError(STR("allocation flags are heap-allocated; not handled"));
            return true;
        }
        if (!VerifyModel(map, Strihash))
        {
            g_MapState.store(StateRefused);
            LogError(STR("NOT modifying the map"));
            return true;
        }

        const auto total = static_cast<int32_t>(RaceTable.size());

        auto* elements = static_cast<FRaceIdElement*>(
                std::calloc(static_cast<size_t>(total), sizeof(FRaceIdElement)));
        if (!elements)
        {
            g_MapState.store(StateRefused);
            LogError(STR("element allocation failed"));
            return true;
        }

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
            g_MapState.store(StateRefused);
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

        FRaceIdMap written{reinterpret_cast<uint8_t*>(mapAddress)};
        if (!VerifyModel(written, Strihash))
        {
            g_MapState.store(StateRefused);
            LogError(STR("post-write verification FAILED - the map is now inconsistent"));
            return true;
        }
        g_MapState.store(StateDone);
        Log(STR("rebuilt with ") + std::to_wstring(total) + STR(" entries, hashSize=") +
            std::to_wstring(hashSize) + STR("; all re-verified"));
        return true;
    }

    // ---------------------------------------------------------------------
    // The Lua-visible status function: UNBSE.Invoke("obr_playable_races", "status").
    // Read-only, no arguments, one UTF-8 line. Runs on the Lua VM's thread.
    void UNBSE_SCRIPT_CALL StatusFunction(const UNBSEScriptInvocationV1*,
                                          UNBSEScriptResultV1* result, void*)
    {
        result->code = UNBSE_SCRIPT_RESULT_OK;
        result->value.structSize = sizeof(UNBSEScriptValueV1);
        result->value.type = UNBSE_SCRIPT_VALUE_UTF8;
        const auto flag = g_VoiceFlag.load();
        const auto member = g_FactionMember.load();
        const auto host = g_HostState.load();
        const auto written = std::snprintf(
                result->value.utf8Value, sizeof(result->value.utf8Value),
                "{\"schema\":\"OBRPlayableRaces.Status\",\"schemaVersion\":1,"
                "\"version\":\"%s\",\"runtime\":\"%s\",\"raceIdMap\":\"%s\","
                "\"raceConditionAlias\":\"%s\",\"altVoiceFlag\":%s,\"altVoiceFaction\":%s,"
                "\"host\":\"%s\"}",
                AddonVersion, StateName(g_RuntimeState.load()), StateName(g_MapState.load()),
                StateName(g_AliasState.load()),
                flag < 0 ? "null" : flag ? "true" : "false",
                member < 0 ? "null" : member ? "\"member\"" : "\"not-member\"",
                host == 1 ? "verified" : host == 2 ? "unverified-attempt" : "absent");
        if (written < 0 || static_cast<size_t>(written) >= sizeof(result->value.utf8Value))
        {
            result->code = UNBSE_SCRIPT_RESULT_CALLBACK_FAILED;
            result->value = {};
            result->value.structSize = sizeof(UNBSEScriptValueV1);
            result->value.type = UNBSE_SCRIPT_VALUE_NONE;
        }
    }
} // namespace

class OBRPlayableRaces : public CppUserModBase
{
  public:
    OBRPlayableRaces()
    {
        ModName = STR("OBRPlayableRaces");
        ModVersion = ModVersionString;
        ModDescription = STR("UNBSE add-on. Extends the engine's built-in race-name table so races "
                             "added by a content mod can be confirmed in character creation, "
                             "aliases their race conditions to Imperial, and keeps a female "
                             "Dremora on her alt-voice recordings.");
        ModAuthors = STR("Extended Races");
    }

    ~OBRPlayableRaces() override
    {
        if (m_tick != Unreal::Hook::ERROR_ID) { Unreal::Hook::UnregisterCallback(m_tick); }
        if (m_loadMap != Unreal::Hook::ERROR_ID) { Unreal::Hook::UnregisterCallback(m_loadMap); }
        if (g_Link.Registered && g_Link.Host.retireAddon)
        {
            const auto result = g_Link.Host.retireAddon(g_Link.Owner, 50);
            Log(std::wstring(STR("UNBSE add-on ")) +
                (result == UNBSE_ADDON_RESULT_OK ? STR("retired") : STR("retirement failed")));
            g_Link = {};
        }
    }

    // UNBSE is a sibling C++ mod that publishes its host from its constructor.
    // C++ mods start in directory order, which puts this one first, so the
    // link is made once every C++ mod has started.
    auto on_cpp_mods_loaded() -> void override { ConnectHost(); }

    // The legacy engine is not up at init, so the map rebuild and the alias
    // are retried from the engine tick until they take. The tick then keeps
    // the voice state right: every half second, and at once after a map
    // load or the first race condition evaluated for the player.
    auto on_unreal_init() -> void override
    {
        if (!g_Link.Registered) { ConnectHost(); }
        VerifyRuntime();

        m_tick = Unreal::Hook::RegisterEngineTickPreCallback(
                [this](auto&, Unreal::UEngine*, float, bool) { OnTick(); },
                Unreal::Hook::FCallbackOptions{
                        .bOnce = false,
                        .bReadonly = true,
                        .OwnerModName = STR("OBRPlayableRaces"),
                        .HookName = STR("RaceIdMapRebuild"),
                });
        m_loadMap = Unreal::Hook::RegisterLoadMapPostCallback(
                [this](auto&, auto*, auto&, auto, auto*, auto&) { RequestVoiceCheck(STR("map loaded")); },
                Unreal::Hook::FCallbackOptions{
                        .bOnce = false,
                        .bReadonly = true,
                        .OwnerModName = STR("OBRPlayableRaces"),
                        .HookName = STR("VoiceCheckOnLoad"),
                });
        Output::send<LogLevel::Verbose>(
                STR("[OBRPlayableRaces] waiting for the legacy race list\n"));
    }

  private:
    static constexpr int TicksBetweenAttempts = 30;

    auto ConnectHost() -> void
    {
        if (g_Link.Registered || m_hostAttempted) { return; }
        m_hostAttempted = true;

        const auto query = FindExport<UNBSEQueryAddonHostV1Function>("UNBSE_QueryAddonHostV1");
        if (!query)
        {
            g_HostState.store(2);
            LogError(STR("UNBSE host not found; running as an unverified attempt against the PE header"));
            return;
        }
        g_Link.Host.structSize = sizeof(g_Link.Host);
        if (!query(UNBSE_ADDON_HOST_ABI_VERSION, &g_Link.Host))
        {
            g_Link.Host = {};
            g_HostState.store(2);
            LogError(STR("UNBSE host does not offer add-on ABI v1; running as an unverified attempt"));
            return;
        }

        UNBSEAddonDescriptorV1 descriptor{};
        descriptor.structSize = sizeof(descriptor);
        descriptor.apiVersion = UNBSE_ADDON_HOST_ABI_VERSION;
        descriptor.declaredEffects = UNBSE_ADDON_EFFECT_RUNTIME_READ | UNBSE_ADDON_EFFECT_RUNTIME_WRITE;
        descriptor.requiredHostCapabilities = UNBSE_ADDON_HOST_CAPABILITY_RUNTIME_INFO_V1 |
                                              UNBSE_ADDON_HOST_CAPABILITY_RELOCATION_V1;
        std::memcpy(descriptor.addonId, AddonId, std::strlen(AddonId) + 1);
        std::memcpy(descriptor.addonVersion, AddonVersion, std::strlen(AddonVersion) + 1);

        UNBSEAddonRegistrationV1 registration{};
        registration.structSize = sizeof(registration);
        registration.apiVersion = UNBSE_ADDON_HOST_ABI_VERSION;
        const auto result = g_Link.Host.registerAddon(&descriptor, &registration);
        if (result != UNBSE_ADDON_RESULT_OK)
        {
            LogError(STR("UNBSE registration failed: ") +
                     Widen(g_Link.Host.resultName ? g_Link.Host.resultName(result) : "unknown") +
                     STR("; running as an unverified attempt"));
            g_Link.Host = {};
            g_HostState.store(2);
            return;
        }
        g_Link.Owner = registration.ownerHandle;
        g_Link.Registered = true;
        g_HostState.store(registration.compatibility == UNBSE_ADDON_COMPATIBILITY_VERIFIED ? 1 : 2);

        // Bounded RVA resolution: the host checks owner, image bounds and
        // arithmetic for every address this mod touches.
        const auto queryRelocation =
                FindExport<UNBSEQueryRelocationV1Function>("UNBSE_QueryRelocationV1");
        g_Link.Relocation.structSize = sizeof(g_Link.Relocation);
        g_Link.RelocationReady = queryRelocation &&
                                 queryRelocation(UNBSE_RELOCATION_ABI_VERSION, &g_Link.Relocation) &&
                                 g_Link.Relocation.resolve;
        if (!g_Link.RelocationReady) { g_Link.Relocation = {}; }

        // Core lifecycle: runtime-ready is informational; runtime-stopping
        // ends every write to the process.
        const auto queryMessaging =
                FindExport<UNBSEQueryMessagingV1Function>("UNBSE_QueryMessagingV1");
        g_Link.Messaging.structSize = sizeof(g_Link.Messaging);
        g_Link.MessagingReady = queryMessaging &&
                                queryMessaging(UNBSE_MESSAGING_ABI_VERSION, &g_Link.Messaging) &&
                                g_Link.Messaging.registerListener &&
                                g_Link.Messaging.registerListener(g_Link.Owner, "unbse.core",
                                                                  &OnCoreMessage, this) ==
                                        UNBSE_MESSAGING_RESULT_OK;
        if (!g_Link.MessagingReady) { g_Link.Messaging = {}; }

        // The read-only status line for Lua.
        g_Link.Script.structSize = sizeof(g_Link.Script);
        g_Link.ScriptReady = g_Link.Host.queryScriptService &&
                             g_Link.Host.queryScriptService(UNBSE_SCRIPT_SERVICE_ABI_VERSION,
                                                            &g_Link.Script) &&
                             g_Link.Script.registerFunction;
        if (g_Link.ScriptReady)
        {
            UNBSEScriptFunctionDeclarationV1 declaration{};
            declaration.structSize = sizeof(declaration);
            declaration.apiVersion = UNBSE_SCRIPT_SERVICE_ABI_VERSION;
            declaration.flags = UNBSE_SCRIPT_FUNCTION_READ_ONLY;
            declaration.argumentCount = 0;
            declaration.resultType = UNBSE_SCRIPT_VALUE_UTF8;
            std::memcpy(declaration.namespaceToken, ScriptNamespace, std::strlen(ScriptNamespace) + 1);
            std::memcpy(declaration.functionToken, "status", sizeof("status"));
            declaration.callback = &StatusFunction;
            g_Link.ScriptReady = g_Link.Script.registerFunction(g_Link.Owner, &declaration) ==
                                 UNBSE_SCRIPT_RESULT_OK;
        }
        if (!g_Link.ScriptReady) { g_Link.Script = {}; }

        Log(std::wstring(STR("registered with UNBSE as ")) + Widen(AddonId) + STR(" ") +
            Widen(AddonVersion) + STR(" (owner ") + std::to_wstring(g_Link.Owner) + STR(", ") +
            (registration.compatibility == UNBSE_ADDON_COMPATIBILITY_VERIFIED
                     ? STR("verified")
                     : STR("unverified attempt")) +
            STR("; relocation ") + (g_Link.RelocationReady ? STR("on") : STR("off")) +
            STR(", messaging ") + (g_Link.MessagingReady ? STR("on") : STR("off")) +
            STR(", script status ") + (g_Link.ScriptReady ? STR("on") : STR("off")) + STR(")"));
    }

    static void UNBSE_SCRIPT_CALL OnCoreMessage(const UNBSEMessageV1* message, void*)
    {
        if (!message) { return; }
        switch (message->messageType)
        {
        case UNBSE_CORE_MESSAGE_RUNTIME_READY:
            Log(STR("UNBSE runtime ready"));
            break;
        case UNBSE_CORE_MESSAGE_RUNTIME_STOPPING:
            g_HostStopping.store(true);
            Log(STR("UNBSE runtime stopping; no further writes"));
            break;
        default:
            break;
        }
    }

    // The executable must be the one every offset here was measured in.
    // Anything else and the mod does nothing at all.
    auto VerifyRuntime() -> void
    {
        if (!QueryRuntimeIdentity(&g_Runtime))
        {
            g_RuntimeState.store(StateRefused);
            LogError(STR("could not read the executable's identity; doing nothing"));
            return;
        }
        const bool match = g_Runtime.PeTimestamp == ExpectedPeTimestamp &&
                           g_Runtime.ImageSize == ExpectedImageSize;
        g_RuntimeState.store(match ? StateDone : StateRefused);
        Log(STR("executable identity via ") + g_Runtime.Source + STR(": timestamp ") +
            Hex(g_Runtime.PeTimestamp) + STR(", image ") + Hex(g_Runtime.ImageSize) +
            STR(" at ") + Hex(g_Runtime.ImageBase) +
            (match ? STR(" - OblivionRemastered-Win64-Shipping 1.512.105")
                   : STR(" - NOT the build this mod was measured against; doing nothing")) +
            (g_Runtime.FoundationId.empty()
                     ? std::wstring()
                     : STR("; host ") + g_Runtime.HostVersion + STR(" on ") + g_Runtime.FoundationId));
    }

    auto RequestVoiceCheck(const wchar_t* reason) -> void
    {
        m_pending = true;
        m_reason = reason;
    }

    auto OnTick() -> void
    {
        if (++m_ticks < TicksBetweenAttempts) { return; }
        m_ticks = 0;
        if (g_RuntimeState.load() != StateDone || g_HostStopping.load()) { return; }

        if (!m_done)
        {
            if (!ExtendMap()) { return; }
            if (g_MapState.load() != StateDone)
            {
                // A refused map means the executable is not understood; the
                // alias and the voice check would be guessing too.
                m_done = true;
                g_AliasState.store(StateRefused);
                LogError(STR("race-id map refused; alias and voice check are off"));
                return;
            }
            InstallRaceConditionAlias();
            m_done = true;
            RequestVoiceCheck(STR("startup"));
        }
        if (g_AliasState.load() != StateDone) { return; }

        if (g_PlayerRaceSeen.exchange(false)) { RequestVoiceCheck(STR("player race condition evaluated")); }
        if (ApplyVoiceFaction(m_pending ? m_reason : STR("poll"))) { m_pending = false; }
    }

    Unreal::Hook::GlobalCallbackId m_tick{Unreal::Hook::ERROR_ID};
    Unreal::Hook::GlobalCallbackId m_loadMap{Unreal::Hook::ERROR_ID};
    int m_ticks{};
    bool m_done{};
    bool m_hostAttempted{};
    bool m_pending{};
    const wchar_t* m_reason{STR("startup")};
};

#define OBR_PLAYABLE_RACES_API __declspec(dllexport)
extern "C"
{
    OBR_PLAYABLE_RACES_API CppUserModBase* start_mod() { return new OBRPlayableRaces(); }
    OBR_PLAYABLE_RACES_API void uninstall_mod(CppUserModBase* mod) { delete mod; }
}

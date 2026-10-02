#pragma once

#include <cstdint>
#include <cstring>

namespace PlayerVoiceRace
{
    constexpr uint32_t Sheogorath = 0x0005308E;
    constexpr uint32_t Imperial = 0x00000907;
    using LookupForm = void* (*)(uint32_t);

    // Only the voice-path call site uses this result. Never write the NPC's
    // race pointer or the shared RACE's VNAM, and never cache a player's race.
    inline auto Select(void* speaker, void* player, void* originalRace, LookupForm lookup) -> void*
    {
        if (!speaker || speaker != player || !originalRace || !lookup) { return originalRace; }
        const auto isRace = [](void* form, uint32_t expected) {
            if (!form) { return false; }
            auto* bytes = static_cast<const uint8_t*>(form);
            uint32_t id{};
            std::memcpy(&id, bytes + 0x10, sizeof(id));
            return bytes[0x08] == 9 && id == expected;
        };
        if (!isRace(originalRace, Sheogorath)) { return originalRace; }
        void* donor = lookup(Imperial);
        return isRace(donor, Imperial) ? donor : originalRace;
    }
}

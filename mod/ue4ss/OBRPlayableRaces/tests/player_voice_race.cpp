#include "../src/PlayerVoiceRace.hpp"
#include <array>
#include <cassert>
#include <cstdio>

using Form = std::array<uint8_t, 32>;
Form sheo{}, imperial{}, other{};
void* donor = imperial.data();
int lookups = 0;
void* Lookup(uint32_t id)
{
    assert(id == PlayerVoiceRace::Imperial);
    ++lookups;
    return donor;
}
void SetForm(Form& form, uint32_t id, uint8_t type = 9)
{
    form[8] = type;
    std::memcpy(form.data() + 0x10, &id, sizeof(id));
}
int main()
{
    SetForm(sheo, PlayerVoiceRace::Sheogorath);
    SetForm(imperial, PlayerVoiceRace::Imperial);
    SetForm(other, 0x38010);
    int player{}, npc{};
    const auto before = sheo;
    using PlayerVoiceRace::Select;
    assert(Select(&player, &player, sheo.data(), Lookup) == imperial.data());
    assert(lookups == 1);
    assert(Select(&npc, &player, sheo.data(), Lookup) == sheo.data());
    assert(Select(&npc, &player, reinterpret_cast<void*>(1), Lookup) == reinterpret_cast<void*>(1));
    assert(Select(nullptr, nullptr, sheo.data(), Lookup) == sheo.data());
    assert(Select(&player, nullptr, sheo.data(), Lookup) == sheo.data());
    assert(Select(&player, &player, nullptr, Lookup) == nullptr);
    assert(Select(&player, &player, other.data(), Lookup) == other.data());
    assert(Select(&player, &player, imperial.data(), Lookup) == imperial.data());
    assert(lookups == 1); // NPCs, other races and missing player never query a donor.
    donor = nullptr;
    assert(Select(&player, &player, sheo.data(), Lookup) == sheo.data());
    donor = other.data();
    assert(Select(&player, &player, sheo.data(), Lookup) == sheo.data());
    donor = imperial.data();
    imperial[8] = 0x23;
    assert(Select(&player, &player, sheo.data(), Lookup) == sheo.data());
    imperial[8] = 9;
    sheo[8] = 0x23;
    assert(Select(&player, &player, sheo.data(), Lookup) == sheo.data());
    sheo = before;
    assert(Select(&player, &player, sheo.data(), nullptr) == sheo.data());
    assert(Select(&player, &player, sheo.data(), Lookup) == imperial.data());
    assert(sheo == before); // No shared data mutation, including after race changes.
    std::puts("PASS: player/NPC voice split, race changes, missing/wrong donors, immutable race data");
}

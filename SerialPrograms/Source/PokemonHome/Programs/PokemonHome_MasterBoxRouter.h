/*  Master Box Router
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure routing logic: decides which box category a given Pokémon belongs in.
 *  No Qt, no hardware, no file IO — intentionally pure so it can be unit-tested
 *  in isolation (Task 4 will load the dex-number sets from JSON and wire them in).
 */

#ifndef PokemonAutomation_PokemonHome_MasterBoxRouter_H
#define PokemonAutomation_PokemonHome_MasterBoxRouter_H

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Box category — the target box for a given Pokémon.
// Matches the spec §4 category list exactly (order of enum values is
// intentionally meaningful for future display but routing uses first-match).
// ---------------------------------------------------------------------------
enum class BoxCategory{
    LivingDex,
    Competitive,
    Breeding,
    Breedject,
    Events,
    GoodTrades,
    DuplicateShiny,
    Legendary,
    Mythical,
    UltraBeast,
    Paradox,
    ManualForms,
    ManualOther,
    Utility,        // v2: any mon matched by a UtilityRule
    // v3 additions
    ShinyDex,       // shiny copy of each shiny-able species in National Dex order
    RegularDex,     // best non-shiny of each species in National Dex order
    ShinyTrades,    // shiny duplicates beyond the single Shiny Dex slot
    RegularTrades,  // non-shiny duplicates with trade value
    Junk,           // low-value duplicates (review bucket — never auto-released)
};


// ---------------------------------------------------------------------------
// Utility rule — matches a mon by ability, held item, or move slug.  // v2
// ---------------------------------------------------------------------------
struct UtilityRule{
    enum Kind { Ability, Item, Move };
    Kind        kind;
    std::string target_slug;
};


// ---------------------------------------------------------------------------
// Router configuration — all thresholds and the four special-dex sets.
// The dex-number sets are owned externally (Task 4 loads them); the router
// only borrows const pointers so it stays pure and zero-copy.
// ---------------------------------------------------------------------------
struct RouterConfig{
    // OT names that count as "owner" (already lowercase, e.g. {"nicole","cole"}).
    std::set<std::string> owner_ot_names;

    // IV thresholds.
    uint8_t competitive_min31 = 6;          // iv_best_count >= this → Competitive
    std::pair<uint8_t,uint8_t> breeding_range  = {3, 5}; // inclusive
    std::pair<uint8_t,uint8_t> breedject_range = {1, 2}; // inclusive

    // Dex-number sets for special species.  Null pointers are treated as empty
    // sets (the router null-checks each pointer before dereferencing it).
    const std::set<uint16_t>* legendary  = nullptr;
    const std::set<uint16_t>* mythical   = nullptr;
    const std::set<uint16_t>* ultra_beast = nullptr;
    const std::set<uint16_t>* paradox    = nullptr;

    // v2: ability/item/move rules for the Utility box.
    std::vector<UtilityRule> utility_rules;
};


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Returns true if p.ot_name is in the owner set (already normalized/lowercased).
bool is_owner_ot(const Pokemon::CollectedPokemonInfo& p, const std::set<std::string>& owners);

// Returns true if p matches any UtilityRule (ability, held item, or move slug).  // v2
bool p_matches_utility(const Pokemon::CollectedPokemonInfo& p, const std::vector<UtilityRule>& rules);


// ---------------------------------------------------------------------------
// Primary routing function — first matching rule wins (see brief §4).
//
// species_slot_taken_by_shiny:   true when the living-dex shiny slot for this
//     species is already occupied (determines DuplicateShiny routing).
// base_form_signature_matches:   true when the Pokémon matches the expected
//     "base form" for its species slot; false → ManualForms.
// ---------------------------------------------------------------------------
BoxCategory route(
    const Pokemon::CollectedPokemonInfo& p,
    const RouterConfig& cfg,
    bool species_slot_taken_by_shiny,
    bool base_form_signature_matches
);


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

#endif

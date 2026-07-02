/*  Master Box Router V3
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  v3 routing logic: species grouping, dex-first best-copy dedup,
 *  duplicate priority chain, overqualified detection.
 *  No Qt, no hardware, no file IO — purely testable.
 */

#ifndef PokemonAutomation_PokemonHome_MasterBoxRouterV3_H
#define PokemonAutomation_PokemonHome_MasterBoxRouterV3_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Options/Pokemon_StatsHuntFilter.h"
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "Pokemon/Pokemon_Types.h"
#include "PokemonHome_MasterBoxLayout.h"
#include "PokemonHome_MasterBoxRouter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// DexAssign — how a copy is used within its species group.
// ---------------------------------------------------------------------------
enum class DexAssign {
    RegularDexSlot,  // best non-shiny → fills the Regular Dex slot
    ShinyDexSlot,    // best shiny → fills the Shiny Dex slot
    Duplicate,       // every other copy → routed by route_duplicate_v3
};


// ---------------------------------------------------------------------------
// SpeciesKey — groups copies of the "same species/form" together.
//
// Mirrors v1's base-form signature comparison: species with the same dex
// number but different type/gmax/gender are detectable variants and get
// their own group (they can populate the Forms box if they end up as
// duplicates).
//
// Limitation (matches v2): cosmetic-only forms (same type/gmax/gender) are
// indistinguishable by the reader and land in the same group.
// ---------------------------------------------------------------------------
struct SpeciesKey {
    uint16_t                      dex_number;
    Pokemon::PokemonType          t1;
    Pokemon::PokemonType          t2;
    bool                          gmax;
    Pokemon::StatsHuntGenderFilter gender;

    bool operator==(const SpeciesKey& o) const noexcept {
        return dex_number == o.dex_number &&
               t1         == o.t1         &&
               t2         == o.t2         &&
               gmax       == o.gmax       &&
               gender     == o.gender;
    }
    bool operator<(const SpeciesKey& o) const noexcept {
        if (dex_number != o.dex_number) return dex_number < o.dex_number;
        if (t1         != o.t1)         return t1         < o.t1;
        if (t2         != o.t2)         return t2         < o.t2;
        if (gmax       != o.gmax)       return gmax       < o.gmax;
        return gender < o.gender;
    }
};


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

// ---------------------------------------------------------------------------
// std::hash specialisation for SpeciesKey so it can be used in
// std::unordered_map if desired (map<SpeciesKey,...> uses operator< above).
// ---------------------------------------------------------------------------
namespace std {
template<>
struct hash<PokemonAutomation::NintendoSwitch::PokemonHome::SpeciesKey> {
    size_t operator()(
        const PokemonAutomation::NintendoSwitch::PokemonHome::SpeciesKey& k
    ) const noexcept {
        size_t h = std::hash<uint16_t>{}(k.dex_number);
        h ^= std::hash<int>{}(static_cast<int>(k.t1))  + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(static_cast<int>(k.t2))  + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<bool>{}(k.gmax)                 + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(static_cast<int>(k.gender)) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};
}  // namespace std


namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// RouteResultV3 — per-catalogue-slot result returned by route_all_v3.
//
// For empty (nullopt) catalogue slots, route_all_v3 returns a default result
// with category=Junk and is_dex_keeper=false (also_qualifies empty).
// Callers must check the original catalogue slot for nullopt before using
// the result.
// ---------------------------------------------------------------------------
struct RouteResultV3 {
    BoxCategory              category      = BoxCategory::Junk;
    bool                     is_dex_keeper = false;
    std::vector<std::string> also_qualifies;  // non-empty only for dex keepers
};


// ---------------------------------------------------------------------------
// route_duplicate_v3 — priority chain for copies that are not dex keepers.
//
// Priority (first match wins):
//   1. ManualForms  — detectable type/gmax/gender variant
//   2. Legendary    — dex# in layout.legendary
//   3. Mythical     — dex# in layout.mythical
//   4. UltraBeast   — dex# in layout.ultra_beast
//   5. Paradox      — dex# in layout.paradox
//   6. Events       — cherish-ball or GO/LGPE/GAMEBOY origin mark
//   7. Utility      — matches any UtilityRule (ability/item/move)
//   8. Breeding     — iv_best_count in cfg.breeding_range
//   9. Competitive  — iv_best_count >= cfg.competitive_min31
//  10. ShinyTrades  — shiny
//  11. RegularTrades — foreign OT (not in cfg.owner_ot_names)
//  12. Junk         — default; never Junk a shiny / owner-OT / legendary /
//                     mythical / event / perfect-IV unless already caught by
//                     a higher step (auto-protections).
//
// Note on Forms: a duplicate is flagged as ManualForms when its SpeciesKey
// differs from the canonical base (t1/t2/gmax/gender differ from a majority
// of same-dex copies). Since the reader cannot distinguish cosmetic variants,
// this only fires for type-distinct / gmax / gender-distinct forms — matches
// v2's documented limitation.
// ---------------------------------------------------------------------------
BoxCategory route_duplicate_v3(
    const Pokemon::CollectedPokemonInfo& p,
    const MasterBoxLayoutV3&             layout,
    const RouterConfig&                  cfg
);


// ---------------------------------------------------------------------------
// route_all_v3 — group the catalogue by SpeciesKey, select the best keeper
// for each dex slot, and route every other copy as a duplicate.
//
// Returns one RouteResultV3 per catalogue index (parallel to `catalogue`).
// Empty (nullopt) slots receive a default result (Junk, is_dex_keeper=false).
//
// Algorithm:
//  1. Build groups keyed by SpeciesKey over non-empty catalogue entries.
//  2. In each group:
//     a. Best non-shiny → RegularDexSlot keeper (category = RegularDex).
//     b. Best shiny     → ShinyDexSlot keeper (category = ShinyDex) UNLESS
//        the dex# is in layout.shiny_locked — then that shiny is a Duplicate.
//     c. All other copies → Duplicate; routed by route_duplicate_v3.
//  3. For each dex keeper, populate also_qualifies with the higher categories
//     it would also match (for the overqualified-dex report):
//       "Forms"       — if the key is a detectable variant (type/gmax/gender
//                       differs from the most-common key for that dex#)
//       "Legendary"/"Mythical"/"UltraBeast"/"Paradox" — if dex# in the set
//       "Events"      — cherish-ball or GO/LGPE/GAMEBOY origin mark
//       "Utility"     — p_matches_utility
//       "Breeding"    — iv_best_count in cfg.breeding_range
//       "Competitive" — iv_best_count >= cfg.competitive_min31
// ---------------------------------------------------------------------------
std::vector<RouteResultV3> route_all_v3(
    const std::vector<std::optional<Pokemon::CollectedPokemonInfo>>& catalogue,
    const MasterBoxLayoutV3&                                         layout,
    const RouterConfig&                                              cfg
);


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

#endif

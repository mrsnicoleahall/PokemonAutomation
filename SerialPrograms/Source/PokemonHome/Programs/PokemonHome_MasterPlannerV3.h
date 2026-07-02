/*  Master Planner V3
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Assembles the full v3 box layout, assigns each Pokémon a target slot
 *  (ShinyDex / RegularDex with gaps + buffers reserved + category boxes),
 *  emits the box-map legend, overqualified rows, and a PlannedMove list.
 *  No Qt, no hardware, no file IO — pure testable logic.
 */

#ifndef PokemonAutomation_PokemonHome_MasterPlannerV3_H
#define PokemonAutomation_PokemonHome_MasterPlannerV3_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "PokemonHome_MasterBoxLayout.h"
#include "PokemonHome_MasterBoxPlanner.h"
#include "PokemonHome_MasterBoxRouter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// BoxMapEntry — one labeled box range in the §1 box-map legend.
// box_start and box_end are 1-indexed, inclusive HOME box numbers.
// ---------------------------------------------------------------------------
struct BoxMapEntry {
    uint16_t    box_start;
    uint16_t    box_end;
    std::string label;
};


// ---------------------------------------------------------------------------
// build_box_map — produce the labeled legend in §1 order:
//   "Shiny Dex", "(buffer)", "Regular Dex", "(buffer)",
//   "Legendary", "Mythical", "Ultra Beasts", "Paradox", "Events", "Forms",
//   "Breeding", "Utility", "Competitive", "Shiny Trades", "Regular Trades", "Junk"
//
// The two dex spans are computed from layout.shiny_dex_start /
// layout.regular_dex_start and the buffer sizes.
// The category entries are sourced from layout.category_box_ranges in §1 order.
// ---------------------------------------------------------------------------
std::vector<BoxMapEntry> build_box_map(const MasterBoxLayoutV3& layout);


// ---------------------------------------------------------------------------
// MasterPlanV3 — full result returned by build_master_plan_v3.
// ---------------------------------------------------------------------------
struct MasterPlanV3 {
    // Ordered list of swaps to perform in HOME.
    std::vector<PlannedMove> moves;

    // Warning strings.  Entries beginning with "[BLOCKING]" mean Execute must
    // abort — no room for an intermediary or category overflow with no spill.
    std::vector<std::string> warnings;

    // One entry per dex keeper that also qualifies for a higher category.
    // Format: "<RegularDex|ShinyDex> slot <N>: <species dex#> also qualifies for <list>"
    std::vector<std::string> overqualified_rows;

    // Box-map legend (same content as build_box_map).
    std::vector<BoxMapEntry> box_map;

    // Per catalogue-entry routing result (parallel to the catalogue vector).
    // Index ci → {category name string, dest_box (1-indexed absolute HOME box, or -1 if unplaceable)}.
    // Empty-slot entries have category="" and dest_box=-1.
    struct SlotRoute {
        std::string category;  // e.g. "RegularDex", "ShinyDex", "Junk"
        int dest_box = -1;     // 1-indexed absolute HOME box number (-1 = unplaceable or empty slot)
    };
    std::vector<SlotRoute> slot_routes;  // size == catalogue.size()
};


// ---------------------------------------------------------------------------
// build_master_plan_v3
//
// catalogue   — one entry per scanned HOME slot (nullopt = empty slot).
//               Index 0 = scan_start box, slot 0.
// layout      — loaded MasterBoxLayoutV3.
// cfg         — router configuration (thresholds + dex sets).
// scan_start  — 0-indexed absolute HOME box number of catalogue[0].
//               Precondition: scan_start == layout.shiny_dex_start - 1 (= 0).
//               Program enforces this with UserSetupError before calling.
//
// Algorithm:
//   1. route_all_v3(catalogue, layout, cfg) → per-slot RouteResultV3.
//   2. Map each keeper to its absolute flat slot:
//        RegularDex keeper → (regular_dex_start-1)*30 + regular_dex_slot(dex)
//        ShinyDex   keeper → (shiny_dex_start-1)*30 + *shiny_dex_slot(dex, shiny_locked)
//   3. Map duplicates to next free slot in their category box range (append order).
//      Overflow spills to Junk; if Junk is also full, adds [BLOCKING] warning.
//   4. NEVER assign any target inside the two buffer regions.
//   5. Compute moves (never-overwrite / cycle-break logic from v1 planner).
//   6. Collect overqualified_rows from each keeper's also_qualifies.
//   7. box_map = build_box_map(layout).
// ---------------------------------------------------------------------------
MasterPlanV3 build_master_plan_v3(
    const std::vector<std::optional<Pokemon::CollectedPokemonInfo>>& catalogue,
    const MasterBoxLayoutV3& layout,
    const RouterConfig& cfg,
    uint16_t scan_start
);


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

#endif

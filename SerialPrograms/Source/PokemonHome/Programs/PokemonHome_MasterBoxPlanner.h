/*  Master Box Planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure planning logic: given the catalogue and layout, computes the full
 *  move list needed to sort all Pokémon into their target category boxes.
 *  No Qt, no hardware, no file IO — intentionally pure so it can be unit-tested
 *  in isolation.
 */

#ifndef PokemonAutomation_PokemonHome_MasterBoxPlanner_H
#define PokemonAutomation_PokemonHome_MasterBoxPlanner_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Pokemon_BoxCursor.h"
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "PokemonHome_MasterBoxLayout.h"
#include "PokemonHome_MasterBoxRouter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


// ---------------------------------------------------------------------------
// A single planned swap: pick up from `from`, place at `to`.
// The HOME Y-button idiom picks up whatever is at `from`, then Y at `to`
// swaps in-hand with whatever is at `to`.  When `to` is empty the pick is
// simply placed.  When `to` is occupied the two slots swap.
// ---------------------------------------------------------------------------
struct PlannedMove{
    BoxCursor from;
    BoxCursor to;
};


// ---------------------------------------------------------------------------
// The full plan returned by build_master_plan.
// ---------------------------------------------------------------------------
struct MasterPlan{
    std::vector<PlannedMove> moves;

    // Non-empty entries are warning strings.  A warning that begins with
    // "[BLOCKING]" means Execute must abort with UserSetupError rather than
    // proceed (no space for an intermediary, or category box overflow with no
    // ManualOther space left).
    std::vector<std::string> warnings;

    // Per catalogue-entry routing result (parallel to the catalogue vector).
    // Index ci → {category name string, dest_box (0-indexed absolute HOME box, or -1 if unplaceable)}
    struct SlotRoute{
        std::string category;   // e.g. "LivingDex", "Breeding"
        int dest_box = -1;      // 0-indexed absolute HOME box number (-1 = unplaceable)
    };
    std::vector<SlotRoute> slot_routes;  // size == catalogue.size()
};


// ---------------------------------------------------------------------------
// Contention resolution: returns true when `challenger` should take the
// living-dex slot over `incumbent`.
// Priority (spec §4.1):
//   1. shiny beats non-shiny
//   2. owner-OT beats non-owner-OT (among same shiny status)
//   3. higher iv_best_count beats lower
//   4. higher iv_total_estimate beats lower
//   5. rarer/more-distinct form (event/rarer ball) — not computable without
//      additional data, so treated as no-preference (incumbent wins tie)
// ---------------------------------------------------------------------------
bool wins_slot(
    const CollectedPokemonInfo& challenger,
    const CollectedPokemonInfo& incumbent,
    const RouterConfig& cfg
);


// ---------------------------------------------------------------------------
// Build the full move plan.
//
// catalogue            — one entry per scanned slot (nullopt = empty).
//                        Index 0 = box scan_start slot 0; use BoxCursor arithmetic.
// layout               — loaded master_box_layout.json.
// cfg                  — router configuration (thresholds + dex sets).
// scan_start           — 0-indexed absolute HOME box number of catalogue[0].
//                        Precondition: scan_start == layout.living_dex_start_box - 1.
//                        program() enforces this with a UserSetupError before calling.
// scratch_box_start    — 0-indexed absolute box number of the first scratch box.
// scratch_box_count    — number of scratch boxes available as temporary staging.
//
// The planner:
//  1. Routes each catalogue entry to a category.
//  2. Assigns target slots within the category's box range.
//  3. Resolves same-slot contention with wins_slot (loser re-routes).
//  4. When a category overflows, places excess in ManualOther and adds warning.
//  5. Computes a greedy move list: for each slot whose occupant needs to move,
//     uses empty slots or scratch boxes as intermediaries.
//  6. If no intermediary exists, adds a [BLOCKING] warning.
// ---------------------------------------------------------------------------
MasterPlan build_master_plan(
    const std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    const MasterBoxLayout& layout,
    const RouterConfig& cfg,
    size_t scan_start,
    uint16_t scratch_box_start,
    uint16_t scratch_box_count
);


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

#endif

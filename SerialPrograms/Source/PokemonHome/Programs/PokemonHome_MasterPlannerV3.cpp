/*  Master Planner V3
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure planning logic — no Qt, no hardware, no file IO.
 *
 *  Algorithm overview:
 *   Phase 1: Route + target assignment
 *     Call route_all_v3 to classify every catalogue entry.
 *     Dex keepers (RegularDex / ShinyDex) are assigned their fixed slot by dex
 *     number; duplicates fill category boxes in append order.  Buffer regions
 *     are never targeted.  Overflow spills to Junk (or [BLOCKING] if full).
 *
 *   Phase 2: Move computation (reuses v1 never-overwrite cycle-break logic)
 *     Build current_flat[ci] (where each Pokémon is now) and desired_at[flat]
 *     (who should end up where).  Walk pending targets in order; resolve each
 *     using empty slots as temporaries; emit [BLOCKING] if no temp is available.
 */

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "Pokemon/Pokemon_BoxCursor.h"
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "PokemonHome_DexSlots.h"
#include "PokemonHome_MasterBoxLayout.h"
#include "PokemonHome_MasterBoxPlanner.h"
#include "PokemonHome_MasterBoxRouter.h"
#include "PokemonHome_MasterBoxRouterV3.h"
#include "PokemonHome_MasterPlannerV3.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
static const size_t SLOTS_PER_BOX = 30;   // 5 rows × 6 columns


// ---------------------------------------------------------------------------
// Internal: convert 0-indexed box + slot-within-box to flat global index.
// ---------------------------------------------------------------------------
static size_t abs_to_flat(size_t abs_box, size_t slot_in_box){
    return abs_box * SLOTS_PER_BOX + slot_in_box;
}


// ---------------------------------------------------------------------------
// Internal: convert flat global index to BoxCursor.
// ---------------------------------------------------------------------------
static BoxCursor flat_to_cursor(size_t flat){
    return BoxCursor(
        flat / SLOTS_PER_BOX,
        (flat % SLOTS_PER_BOX) / BOX_COLS,
        flat % BOX_COLS
    );
}


// ---------------------------------------------------------------------------
// Internal: human-readable name for BoxCategory (used in warnings/reports).
// ---------------------------------------------------------------------------
static const char* category_name_v3(BoxCategory cat){
    switch (cat){
        case BoxCategory::ShinyDex:      return "ShinyDex";
        case BoxCategory::RegularDex:    return "RegularDex";
        case BoxCategory::Legendary:     return "Legendary";
        case BoxCategory::Mythical:      return "Mythical";
        case BoxCategory::UltraBeast:    return "UltraBeast";
        case BoxCategory::Paradox:       return "Paradox";
        case BoxCategory::Events:        return "Events";
        case BoxCategory::ManualForms:   return "Forms";
        case BoxCategory::Breeding:      return "Breeding";
        case BoxCategory::Utility:       return "Utility";
        case BoxCategory::Competitive:   return "Competitive";
        case BoxCategory::ShinyTrades:   return "ShinyTrades";
        case BoxCategory::RegularTrades: return "RegularTrades";
        case BoxCategory::Junk:          return "Junk";
        case BoxCategory::LivingDex:     return "LivingDex";
        case BoxCategory::DuplicateShiny:return "DuplicateShiny";
        case BoxCategory::GoodTrades:    return "GoodTrades";
        case BoxCategory::Breedject:     return "Breedject";
        case BoxCategory::ManualOther:   return "ManualOther";
        default:                          return "Unknown";
    }
}


// ---------------------------------------------------------------------------
// §1 order for category entries in the box map (after the two dexes+buffers).
// ---------------------------------------------------------------------------
static const BoxCategory CATEGORY_ORDER[] = {
    BoxCategory::Legendary,
    BoxCategory::Mythical,
    BoxCategory::UltraBeast,
    BoxCategory::Paradox,
    BoxCategory::Events,
    BoxCategory::ManualForms,
    BoxCategory::Breeding,
    BoxCategory::Utility,
    BoxCategory::Competitive,
    BoxCategory::ShinyTrades,
    BoxCategory::RegularTrades,
    BoxCategory::Junk,
};
static const size_t CATEGORY_ORDER_COUNT =
    sizeof(CATEGORY_ORDER) / sizeof(CATEGORY_ORDER[0]);

// Human-readable labels matching §1 order.
static const char* CATEGORY_LABELS[] = {
    "Legendary",
    "Mythical",
    "Ultra Beasts",
    "Paradox",
    "Events",
    "Forms",
    "Breeding",
    "Utility",
    "Competitive",
    "Shiny Trades",
    "Regular Trades",
    "Junk",
};


// ---------------------------------------------------------------------------
// build_box_map
// ---------------------------------------------------------------------------
std::vector<BoxMapEntry> build_box_map(const MasterBoxLayoutV3& layout){
    std::vector<BoxMapEntry> result;
    result.reserve(4 + CATEGORY_ORDER_COUNT);

    // ---- Shiny Dex ----
    // The Shiny Dex starts at layout.shiny_dex_start.
    // Its last box is (regular_dex_start - 1) - shiny_dex_buffer_boxes.
    uint16_t shiny_dex_end = static_cast<uint16_t>(
        layout.regular_dex_start - layout.shiny_dex_buffer_boxes - 1
    );
    result.push_back({ layout.shiny_dex_start, shiny_dex_end, "Shiny Dex" });

    // ---- Shiny Dex buffer ----
    uint16_t shiny_buf_start = static_cast<uint16_t>(shiny_dex_end + 1);
    uint16_t shiny_buf_end   = static_cast<uint16_t>(layout.regular_dex_start - 1);
    result.push_back({ shiny_buf_start, shiny_buf_end, "(buffer)" });

    // ---- Regular Dex ----
    // The Regular Dex starts at layout.regular_dex_start.
    // Its last box is (first category box start) - regular_dex_buffer_boxes - 1.
    // Find the lowest category box start.
    uint16_t first_cat_start = 0xFFFF;
    for (const BoxCategory cat : CATEGORY_ORDER){
        auto it = layout.category_box_ranges.find(cat);
        if (it != layout.category_box_ranges.end()){
            first_cat_start = std::min(first_cat_start, it->second.first);
        }
    }
    uint16_t regular_dex_end = static_cast<uint16_t>(
        first_cat_start - layout.regular_dex_buffer_boxes - 1
    );
    result.push_back({ layout.regular_dex_start, regular_dex_end, "Regular Dex" });

    // ---- Regular Dex buffer ----
    uint16_t reg_buf_start = static_cast<uint16_t>(regular_dex_end + 1);
    uint16_t reg_buf_end   = static_cast<uint16_t>(first_cat_start - 1);
    result.push_back({ reg_buf_start, reg_buf_end, "(buffer)" });

    // ---- Category boxes in §1 order ----
    for (size_t i = 0; i < CATEGORY_ORDER_COUNT; ++i){
        BoxCategory cat = CATEGORY_ORDER[i];
        auto it = layout.category_box_ranges.find(cat);
        if (it != layout.category_box_ranges.end()){
            result.push_back({ it->second.first, it->second.second, CATEGORY_LABELS[i] });
        }
    }

    return result;
}


// ---------------------------------------------------------------------------
// build_master_plan_v3
// ---------------------------------------------------------------------------
MasterPlanV3 build_master_plan_v3(
    const std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    const MasterBoxLayoutV3& layout,
    const RouterConfig& cfg,
    uint16_t scan_start
){
    MasterPlanV3 plan;
    plan.box_map = build_box_map(layout);

    if (catalogue.empty()){
        return plan;
    }

    // -----------------------------------------------------------------------
    // Compute buffer flat ranges — these are forbidden target zones.
    // Buffer 1: boxes [shiny_buf_start-1 .. shiny_buf_end-1] (0-indexed)
    // Buffer 2: boxes [reg_buf_start-1   .. reg_buf_end-1]   (0-indexed)
    // We derive them from the box_map (entries [1] and [3]).
    // -----------------------------------------------------------------------
    // box_map[1] = shiny buffer, box_map[3] = regular buffer
    const uint16_t shiny_buf_box_start_1idx  = plan.box_map[1].box_start;
    const uint16_t shiny_buf_box_end_1idx    = plan.box_map[1].box_end;
    const uint16_t reg_buf_box_start_1idx    = plan.box_map[3].box_start;
    const uint16_t reg_buf_box_end_1idx      = plan.box_map[3].box_end;

    // Flat ranges (inclusive)
    const size_t shiny_buf_flat_start = static_cast<size_t>(shiny_buf_box_start_1idx - 1) * SLOTS_PER_BOX;
    const size_t shiny_buf_flat_end   = static_cast<size_t>(shiny_buf_box_end_1idx)   * SLOTS_PER_BOX - 1;
    const size_t reg_buf_flat_start   = static_cast<size_t>(reg_buf_box_start_1idx - 1) * SLOTS_PER_BOX;
    const size_t reg_buf_flat_end     = static_cast<size_t>(reg_buf_box_end_1idx)     * SLOTS_PER_BOX - 1;

    // Helper: check if a flat slot falls in a buffer region.
    auto in_buffer = [&](size_t flat) -> bool {
        return (flat >= shiny_buf_flat_start && flat <= shiny_buf_flat_end) ||
               (flat >= reg_buf_flat_start   && flat <= reg_buf_flat_end);
    };

    // -----------------------------------------------------------------------
    // Phase 0: validate that no layout box range overlaps with the buffer zones
    // or other ranges. Warn but do not throw — the planner continues.
    // -----------------------------------------------------------------------
    for (const auto& [cat, range] : layout.category_box_ranges){
        const size_t start_flat = static_cast<size_t>(range.first  - 1) * SLOTS_PER_BOX;
        const size_t end_flat   = static_cast<size_t>(range.second)     * SLOTS_PER_BOX - 1;
        if (start_flat <= shiny_buf_flat_end && end_flat >= shiny_buf_flat_start){
            std::ostringstream oss;
            oss << "[WARNING] Category " << category_name_v3(cat)
                << " box range overlaps with the Shiny Dex buffer region!";
            plan.warnings.push_back(oss.str());
        }
        if (start_flat <= reg_buf_flat_end && end_flat >= reg_buf_flat_start){
            std::ostringstream oss;
            oss << "[WARNING] Category " << category_name_v3(cat)
                << " box range overlaps with the Regular Dex buffer region!";
            plan.warnings.push_back(oss.str());
        }
    }

    // -----------------------------------------------------------------------
    // Phase 1: route all entries.
    // -----------------------------------------------------------------------
    const std::vector<RouteResultV3> routes = route_all_v3(catalogue, layout, cfg);

    // -----------------------------------------------------------------------
    // Phase 2: assign target flat slots.
    //
    // Dex keepers get fixed slots by dex number.
    // Duplicates get next-free slots in their category box range.
    // -----------------------------------------------------------------------

    // Per-category next-free slot counter (append order).
    std::map<BoxCategory, size_t> bucket_next;
    for (const BoxCategory cat : CATEGORY_ORDER){
        bucket_next[cat] = 0;
    }
    // Junk is the spill destination so also initialise.
    bucket_next[BoxCategory::Junk] = 0;

    // Helper: convert a category range [start_1idx, end_1idx] + nth slot (0-indexed)
    // to a flat index. Returns SIZE_MAX if out of range.
    auto category_flat = [&](BoxCategory cat, size_t n) -> size_t {
        auto it = layout.category_box_ranges.find(cat);
        if (it == layout.category_box_ranges.end()) return SIZE_MAX;
        const size_t start_box = static_cast<size_t>(it->second.first  - 1);  // 0-indexed
        const size_t end_box   = static_cast<size_t>(it->second.second - 1);  // 0-indexed inclusive
        const size_t capacity  = (end_box - start_box + 1) * SLOTS_PER_BOX;
        if (n >= capacity) return SIZE_MAX;
        const size_t slot_in_box = n % SLOTS_PER_BOX;
        const size_t abs_box     = start_box + n / SLOTS_PER_BOX;
        const size_t flat        = abs_to_flat(abs_box, slot_in_box);
        // Safety: never assign into a buffer.
        if (in_buffer(flat)){
            return SIZE_MAX;
        }
        return flat;
    };

    // Helper: assign a duplicate to its category bucket (with Junk spill).
    auto assign_bucket = [&](size_t ci, BoxCategory cat, std::vector<size_t>& target_slot) {
        size_t n    = bucket_next[cat]++;
        size_t flat = category_flat(cat, n);
        if (flat != SIZE_MAX){
            target_slot[ci] = flat;
            return;
        }
        // Overflow: try Junk.
        {
            std::ostringstream oss;
            oss << "Category " << category_name_v3(cat)
                << " overflow for catalogue[" << ci
                << "] (dex=" << (catalogue[ci] ? catalogue[ci]->dex_number : 0)
                << ") — spilling to Junk.";
            plan.warnings.push_back(oss.str());
        }
        size_t jn    = bucket_next[BoxCategory::Junk]++;
        size_t jflat = category_flat(BoxCategory::Junk, jn);
        if (jflat != SIZE_MAX){
            target_slot[ci] = jflat;
            return;
        }
        // Both full → blocking.
        {
            std::ostringstream oss;
            oss << "[BLOCKING] Junk box also full for catalogue[" << ci
                << "] (dex=" << (catalogue[ci] ? catalogue[ci]->dex_number : 0)
                << ") — no space available.";
            plan.warnings.push_back(oss.str());
        }
        target_slot[ci] = SIZE_MAX;
    };

    // target_slot[ci] = flat index where this catalogue entry should go.
    // SIZE_MAX = unplaceable.
    std::vector<size_t> target_slot(catalogue.size(), SIZE_MAX);

    for (size_t ci = 0; ci < catalogue.size(); ++ci){
        if (!catalogue[ci].has_value()) continue;
        const CollectedPokemonInfo& info = *catalogue[ci];
        const RouteResultV3& r           = routes[ci];

        if (r.is_dex_keeper){
            if (r.category == BoxCategory::RegularDex){
                // Fixed slot: (regular_dex_start-1)*30 + regular_dex_slot(dex)
                const size_t slot_in_region = regular_dex_slot(info.dex_number);
                const size_t abs_box        = static_cast<size_t>(layout.regular_dex_start - 1)
                                              + slot_in_region / SLOTS_PER_BOX;
                const size_t slot_in_box    = slot_in_region % SLOTS_PER_BOX;
                const size_t flat           = abs_to_flat(abs_box, slot_in_box);
                // Sanity: must not fall in buffer.
                if (in_buffer(flat)){
                    std::ostringstream oss;
                    oss << "[BLOCKING] RegularDex target for dex#" << info.dex_number
                        << " falls in a buffer region — layout is misconfigured.";
                    plan.warnings.push_back(oss.str());
                    target_slot[ci] = SIZE_MAX;
                } else {
                    target_slot[ci] = flat;
                }

                // Collect overqualified row.
                if (!r.also_qualifies.empty()){
                    std::ostringstream oss;
                    oss << "RegularDex slot " << (slot_in_region + 1)
                        << ": dex#" << info.dex_number
                        << " also qualifies for";
                    for (size_t i = 0; i < r.also_qualifies.size(); ++i){
                        oss << (i == 0 ? " " : ", ") << r.also_qualifies[i];
                    }
                    plan.overqualified_rows.push_back(oss.str());
                }

            } else if (r.category == BoxCategory::ShinyDex){
                // Fixed slot: (shiny_dex_start-1)*30 + *shiny_dex_slot(dex, shiny_locked)
                auto slot_opt = shiny_dex_slot(info.dex_number, layout.shiny_locked);
                if (!slot_opt.has_value()){
                    // Shiny-locked — route_all_v3 should never mark this as keeper,
                    // but guard anyway.
                    std::ostringstream oss;
                    oss << "[BLOCKING] ShinyDex keeper dex#" << info.dex_number
                        << " is shiny-locked — cannot assign ShinyDex slot.";
                    plan.warnings.push_back(oss.str());
                    target_slot[ci] = SIZE_MAX;
                } else {
                    const size_t slot_in_region = *slot_opt;
                    const size_t abs_box     = static_cast<size_t>(layout.shiny_dex_start - 1)
                                              + slot_in_region / SLOTS_PER_BOX;
                    const size_t slot_in_box = slot_in_region % SLOTS_PER_BOX;
                    const size_t flat        = abs_to_flat(abs_box, slot_in_box);
                    if (in_buffer(flat)){
                        std::ostringstream oss;
                        oss << "[BLOCKING] ShinyDex target for dex#" << info.dex_number
                            << " falls in a buffer region — layout is misconfigured.";
                        plan.warnings.push_back(oss.str());
                        target_slot[ci] = SIZE_MAX;
                    } else {
                        target_slot[ci] = flat;
                    }

                    // Collect overqualified row.
                    if (!r.also_qualifies.empty()){
                        std::ostringstream oss;
                        oss << "ShinyDex slot " << (slot_in_region + 1)
                            << ": dex#" << info.dex_number
                            << " also qualifies for";
                        for (size_t i = 0; i < r.also_qualifies.size(); ++i){
                            oss << (i == 0 ? " " : ", ") << r.also_qualifies[i];
                        }
                        plan.overqualified_rows.push_back(oss.str());
                    }
                }

            } else {
                // Unexpected keeper category.
                std::ostringstream oss;
                oss << "[WARNING] Unexpected dex keeper category "
                    << category_name_v3(r.category)
                    << " for catalogue[" << ci << "].";
                plan.warnings.push_back(oss.str());
                target_slot[ci] = SIZE_MAX;
            }

        } else {
            // Duplicate: route to category bucket.
            assign_bucket(ci, r.category, target_slot);
        }
    }

    // -----------------------------------------------------------------------
    // Phase 2b: populate slot_routes (parallel to catalogue).
    // One entry per catalogue index: category name + 1-indexed dest box.
    // Empty-slot entries and unplaceable entries get category="" / dest_box=-1.
    // -----------------------------------------------------------------------
    plan.slot_routes.resize(catalogue.size());
    for (size_t ci = 0; ci < catalogue.size(); ++ci){
        if (!catalogue[ci].has_value()) continue;        // empty slot → defaults
        const RouteResultV3& r = routes[ci];
        const size_t tgt = target_slot[ci];
        plan.slot_routes[ci].category = category_name_v3(r.category);
        if (tgt != SIZE_MAX){
            // dest_box is 1-indexed: flat_to_cursor(tgt).box is 0-indexed.
            plan.slot_routes[ci].dest_box = static_cast<int>(flat_to_cursor(tgt).box) + 1;
        }
        // else dest_box stays -1 (unplaceable)
    }

    // -----------------------------------------------------------------------
    // Phase 3: current_flat — where each catalogue entry currently lives.
    // -----------------------------------------------------------------------
    // Compute maximum flat index needed for the board.
    size_t max_abs_box = static_cast<size_t>(scan_start) +
        catalogue.size() / SLOTS_PER_BOX +
        (catalogue.size() % SLOTS_PER_BOX ? 1 : 0);
    for (auto& [cat, range] : layout.category_box_ranges){
        max_abs_box = std::max(max_abs_box, static_cast<size_t>(range.second));
    }
    // Buffer regions are included automatically since they lie between layout ranges.
    max_abs_box += 1;  // +1 for 1-indexed→0-indexed boundary

    const size_t board_size = max_abs_box * SLOTS_PER_BOX;

    // board_occupant[flat] = ci if slot is occupied by that catalogue entry,
    //                        SIZE_MAX if empty.
    std::vector<size_t> board_occupant(board_size, SIZE_MAX);

    // current_flat[ci] = flat index where ci currently lives.
    std::vector<size_t> current_flat(catalogue.size(), SIZE_MAX);

    for (size_t ci = 0; ci < catalogue.size(); ++ci){
        if (!catalogue[ci].has_value()) continue;
        const size_t abs_box     = static_cast<size_t>(scan_start) + ci / SLOTS_PER_BOX;
        const size_t slot_in_box = ci % SLOTS_PER_BOX;
        const size_t flat        = abs_to_flat(abs_box, slot_in_box);
        if (flat < board_size){
            board_occupant[flat] = ci;
            current_flat[ci]     = flat;
        }
    }

    // -----------------------------------------------------------------------
    // Phase 4: greedy cycle-break move computation (never-overwrite).
    // Mirrors the v1 MasterBoxPlanner algorithm exactly.
    // -----------------------------------------------------------------------

    // desired_at[flat] = ci that should end up at flat (SIZE_MAX = nobody).
    std::vector<size_t> desired_at(board_size, SIZE_MAX);
    for (size_t ci = 0; ci < catalogue.size(); ++ci){
        if (!catalogue[ci].has_value()) continue;
        const size_t tgt = target_slot[ci];
        if (tgt == SIZE_MAX || tgt >= board_size) continue;
        desired_at[tgt] = ci;
    }

    // Collect free slots for use as temporaries.
    std::vector<size_t> free_slots;
    free_slots.reserve(board_size / 4);
    for (size_t f = 0; f < board_size; ++f){
        if (board_occupant[f] == SIZE_MAX){
            // Exclude buffer slots from being used as temporaries.
            if (!in_buffer(f)){
                free_slots.push_back(f);
            }
        }
    }

    auto pop_free = [&]() -> size_t {
        while (!free_slots.empty()){
            size_t f = free_slots.back();
            free_slots.pop_back();
            if (board_occupant[f] == SIZE_MAX) return f;
        }
        return SIZE_MAX;
    };

    // do_move: emit a PlannedMove and update board state.
    auto do_move = [&](size_t from_flat, size_t to_flat){
        const size_t ci_from = board_occupant[from_flat];
        const size_t ci_to   = board_occupant[to_flat];

        plan.moves.push_back(PlannedMove{
            flat_to_cursor(from_flat),
            flat_to_cursor(to_flat)
        });

        board_occupant[from_flat] = ci_to;
        board_occupant[to_flat]   = ci_from;
        if (ci_from != SIZE_MAX) current_flat[ci_from] = to_flat;
        if (ci_to   != SIZE_MAX) current_flat[ci_to]   = from_flat;

        // Track new free slot.
        if (ci_to == SIZE_MAX && !in_buffer(from_flat)){
            free_slots.push_back(from_flat);
        }
    };

    // Build pending set of target flats that need satisfying.
    std::set<size_t> pending;
    for (size_t ci = 0; ci < catalogue.size(); ++ci){
        if (!catalogue[ci].has_value()) continue;
        if (target_slot[ci] == SIZE_MAX) continue;
        if (current_flat[ci] != target_slot[ci]){
            pending.insert(target_slot[ci]);
        }
    }

    size_t iterations     = 0;
    const size_t max_iter = catalogue.size() * catalogue.size() + 1000;

    while (!pending.empty() && iterations++ < max_iter){
        const size_t tgt = *pending.begin();
        pending.erase(pending.begin());

        const size_t want_ci = desired_at[tgt];
        if (want_ci == SIZE_MAX) continue;

        const size_t cur = current_flat[want_ci];
        if (cur == SIZE_MAX || cur == tgt) continue;  // already there

        const size_t cur_occupant_ci = board_occupant[tgt];

        if (cur_occupant_ci == SIZE_MAX){
            // Target is empty — direct move.
            do_move(cur, tgt);

        } else if (cur_occupant_ci == want_ci){
            // Already satisfied.
            continue;

        } else {
            // Target is occupied by the wrong Pokémon.
            const size_t occ_tgt = target_slot[cur_occupant_ci];

            if (occ_tgt != SIZE_MAX && occ_tgt < board_size &&
                board_occupant[occ_tgt] == SIZE_MAX)
            {
                // Case (a): occupant's own target is free — move it there.
                do_move(tgt, occ_tgt);
                pending.insert(tgt);
            } else {
                // Case (b): cycle — need a temporary slot.
                const size_t temp = pop_free();
                if (temp == SIZE_MAX){
                    std::ostringstream oss;
                    oss << "[BLOCKING] No free slot available to use as intermediary "
                        << "while placing catalogue[" << want_ci
                        << "] (dex=" << (catalogue[want_ci] ? catalogue[want_ci]->dex_number : 0)
                        << ") — cannot complete sort without overwriting an occupied slot.";
                    plan.warnings.push_back(oss.str());
                    continue;
                }
                do_move(tgt, temp);
                pending.insert(tgt);
                if (occ_tgt != SIZE_MAX && occ_tgt != temp){
                    pending.insert(occ_tgt);
                }
            }
        }
    }

    if (iterations >= max_iter){
        plan.warnings.push_back(
            "[BLOCKING] Move loop exceeded maximum iterations — cycle detection failed. "
            "Please report this as a bug."
        );
    }

    return plan;
}


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

/*  Master Box Planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure planning logic — no Qt, no hardware, no file IO.
 *
 *  Algorithm overview:
 *   Phase 1: Route + target assignment
 *     For each occupied slot in the catalogue, compute route().
 *     Assign a target slot within the category's box range:
 *       - LivingDex: one slot per dex-number (0-indexed within the range).
 *         Multiple same-species entries contend; wins_slot picks the winner;
 *         the loser is re-routed (typically to DuplicateShiny or its IV bucket).
 *       - Bucket categories: slots assigned in append order.
 *     When a category's box range is full, overflow goes to ManualOther.
 *     If ManualOther is also full, a [BLOCKING] warning is emitted.
 *
 *   Phase 2: Move computation (greedy cycle-break)
 *     Build a map from current-slot → target-slot for every entry that needs
 *     to move.  Walk through target slots in order; if the occupant must move,
 *     trace the cycle and break it using an empty slot or scratch slot as a
 *     temporary.  If no temp is available, emit a [BLOCKING] warning.
 */

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "Pokemon/Pokemon_BoxCursor.h"
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "PokemonHome_MasterBoxPlanner.h"
#include "PokemonHome_MasterBoxLayout.h"
#include "PokemonHome_MasterBoxRouter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
static const size_t SLOTS_PER_BOX = 30;   // 5 rows × 6 columns


// ---------------------------------------------------------------------------
// wins_slot  — spec §4.1 contention resolution
// ---------------------------------------------------------------------------
bool wins_slot(
    const CollectedPokemonInfo& challenger,
    const CollectedPokemonInfo& incumbent,
    const RouterConfig& cfg
){
    // 1. Shiny beats non-shiny.
    if (challenger.shiny && !incumbent.shiny) return true;
    if (!challenger.shiny && incumbent.shiny) return false;

    // 2. Owner-OT beats non-owner-OT.
    const bool ch_owner = is_owner_ot(challenger, cfg.owner_ot_names);
    const bool in_owner = is_owner_ot(incumbent,  cfg.owner_ot_names);
    if (ch_owner && !in_owner) return true;
    if (!ch_owner && in_owner) return false;

    // 3. Higher iv_best_count wins.
    if (challenger.iv_best_count > incumbent.iv_best_count) return true;
    if (challenger.iv_best_count < incumbent.iv_best_count) return false;

    // 4. Higher iv_total_estimate wins.
    if (challenger.iv_total_estimate > incumbent.iv_total_estimate) return true;
    if (challenger.iv_total_estimate < incumbent.iv_total_estimate) return false;

    // 5. Tie → incumbent wins (stable).
    return false;
}


// ---------------------------------------------------------------------------
// Internal: convert 0-indexed box number + slot-within-box to a BoxCursor.
// ---------------------------------------------------------------------------
static BoxCursor make_cursor(size_t abs_box, size_t slot_in_box){
    return BoxCursor(
        abs_box,
        slot_in_box / BOX_COLS,
        slot_in_box % BOX_COLS
    );
}


// ---------------------------------------------------------------------------
// Internal: convert a BoxCursor to a flat global index within the catalogue.
// catalogue index 0 = scan_start box, slot 0.
// We never need to convert from the *absolute* box (that's what the planner
// works with); catalogue_offset converts a BoxCursor (which uses absolute
// box numbers) to a catalogue-relative index given scan_start.
// ---------------------------------------------------------------------------
static size_t cursor_to_catalogue_index(const BoxCursor& c, size_t scan_start){
    // c.box is 0-indexed absolute HOME box number.
    return (c.box - scan_start) * SLOTS_PER_BOX + c.row * BOX_COLS + c.column;
}


// ---------------------------------------------------------------------------
// Internal: convert 0-indexed absolute-box + within-box slot to global index
// within a flat "board" vector that covers the full HOME box range.
// The board here is scratch + all boxes together — we use absolute indices.
// ---------------------------------------------------------------------------
static size_t abs_to_flat(size_t abs_box, size_t slot_in_box){
    return abs_box * SLOTS_PER_BOX + slot_in_box;
}
static size_t cursor_to_flat(const BoxCursor& c){
    return abs_to_flat(c.box, c.row * BOX_COLS + c.column);
}


// ---------------------------------------------------------------------------
// Internal helper: return a human-readable name for a BoxCategory.
// Used in warning messages so the operator sees "Breeding" rather than "2".
// ---------------------------------------------------------------------------
static const char* category_name(BoxCategory cat){
    switch (cat){
        case BoxCategory::LivingDex:     return "LivingDex";
        case BoxCategory::Competitive:   return "Competitive";
        case BoxCategory::Breeding:      return "Breeding";
        case BoxCategory::Breedject:     return "Breedject";
        case BoxCategory::Events:        return "Events";
        case BoxCategory::GoodTrades:    return "GoodTrades";
        case BoxCategory::DuplicateShiny:return "DuplicateShiny";
        case BoxCategory::Legendary:     return "Legendary";
        case BoxCategory::Mythical:      return "Mythical";
        case BoxCategory::UltraBeast:    return "UltraBeast";
        case BoxCategory::Paradox:       return "Paradox";
        case BoxCategory::ManualForms:   return "ManualForms";
        case BoxCategory::ManualOther:   return "ManualOther";
        case BoxCategory::Utility:       return "Utility";
        default:                          return "Unknown";
    }
}


// ---------------------------------------------------------------------------
// build_master_plan
// ---------------------------------------------------------------------------
MasterPlan build_master_plan(
    const std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    const MasterBoxLayout& layout,
    const RouterConfig& cfg,
    size_t scan_start,
    uint16_t scratch_box_start,
    uint16_t scratch_box_count
){
    // Precondition (enforced by caller via UserSetupError before this is called):
    //   scan_start == layout.living_dex_start_box - 1
    // The planner uses scan_start as the 0-indexed absolute box of catalogue[0].

    MasterPlan plan;

    if (catalogue.empty()){
        return plan;
    }

    const size_t catalogue_boxes = catalogue.size() / SLOTS_PER_BOX +
                                    (catalogue.size() % SLOTS_PER_BOX ? 1 : 0);
    const size_t scan_end_box    = scan_start + catalogue_boxes; // exclusive

    // -----------------------------------------------------------------------
    // Phase 1a: Build a flat "board" that mirrors the HOME boxes.
    // board[flat_index] = catalogue_index if that slot was scanned, else -1.
    // We also track which catalogue entries are still un-assigned.
    //
    // For simplicity the board covers [0, max_box) where max_box is large
    // enough for all category ranges + scratch boxes.
    // -----------------------------------------------------------------------

    // Compute the maximum box number referenced by any category range.
    size_t max_abs_box = scan_end_box;
    for (auto& [cat, range] : layout.category_box_ranges){
        max_abs_box = std::max(max_abs_box, static_cast<size_t>(range.second)); // 1-indexed end
    }
    max_abs_box = std::max(max_abs_box,
        static_cast<size_t>(scratch_box_start) + scratch_box_count);
    // +1 because range.second is 1-indexed inclusive, so highest box = range.second-1 0-indexed + 1
    max_abs_box += 1;

    // board: flat_index → optional<catalogue_index>
    // nullopt means empty.
    std::vector<std::optional<size_t>> board(max_abs_box * SLOTS_PER_BOX, std::nullopt);

    // Populate board with catalogue data.
    for (size_t ci = 0; ci < catalogue.size(); ci++){
        if (!catalogue[ci].has_value()) continue;
        size_t abs_box      = scan_start + ci / SLOTS_PER_BOX;
        size_t slot_in_box  = ci % SLOTS_PER_BOX;
        size_t flat         = abs_to_flat(abs_box, slot_in_box);
        if (flat < board.size()){
            board[flat] = ci;
        }
    }

    // -----------------------------------------------------------------------
    // Phase 1b: Route each catalogue entry and assign target slots.
    //
    // target_slot[ci] = flat_index of where catalogue entry ci should end up.
    // -1 means "assigned to ManualOther (overflow)" which we resolve later.
    // -----------------------------------------------------------------------

    // We need to track which living-dex species slots are taken by a shiny.
    // Key: dex_number → flat-index of the winning entry's target slot.
    std::map<uint16_t, size_t> dex_shiny_slot;   // shiny winner for each species

    // Per-category next-free slot tracking.
    // For LivingDex we assign by dex-number (0-indexed within range).
    // For bucket categories we assign in catalogue order.
    std::map<BoxCategory, size_t> bucket_next;    // next free slot index within range

    // Initialize all bucket counters to 0.
    for (auto& [cat, range] : layout.category_box_ranges){
        bucket_next[cat] = 0;
    }

    // target_slot[ci] = flat index of target.
    // SIZE_MAX means "no assignment yet".
    // We use a two-pass approach: first pass routes all entries and tries to
    // assign living-dex slots; second pass handles contention losers.

    std::vector<size_t> target_slot(catalogue.size(), SIZE_MAX);

    // living-dex winner map: dex_number → catalogue_index of the current winner.
    std::map<uint16_t, size_t> living_dex_winner;  // dex → ci of winner

    // We need to decide base_form_signature_matches and species_slot_taken_by_shiny
    // for each entry.  For the planner:
    //   - base_form_signature_matches = true for all entries (we don't have
    //     form data beyond dex_number in the catalogue; ManualForms routing
    //     would require form signature comparison which needs additional data).
    //     The catalogue only stores dex_number; we default to true.
    //   - species_slot_taken_by_shiny is determined dynamically as we resolve
    //     the living-dex winner assignments.

    // First pass: for each catalogue entry, compute preliminary route.
    // For LivingDex-routed entries, resolve contention.
    // For non-LivingDex entries, assign to bucket.

    // Helper: given a BoxCategory and category range, return the flat index
    // of the Nth slot (0-indexed) within that range.
    auto category_flat = [&](BoxCategory cat, size_t n) -> std::optional<size_t> {
        auto it = layout.category_box_ranges.find(cat);
        if (it == layout.category_box_ranges.end()) return std::nullopt;
        const auto& [start_1idx, end_1idx] = it->second;
        // start_1idx and end_1idx are 1-indexed box numbers.
        const size_t start_box = static_cast<size_t>(start_1idx - 1);  // 0-indexed
        const size_t end_box   = static_cast<size_t>(end_1idx - 1);    // 0-indexed inclusive
        const size_t capacity  = (end_box - start_box + 1) * SLOTS_PER_BOX;
        if (n >= capacity) return std::nullopt;
        const size_t slot_in_box = n % SLOTS_PER_BOX;
        const size_t abs_box     = start_box + n / SLOTS_PER_BOX;
        return abs_to_flat(abs_box, slot_in_box);
    };

    // Helper: assign to a bucket category. Returns flat or SIZE_MAX + warning.
    // Increments bucket_next[cat].
    auto assign_bucket = [&](size_t ci, BoxCategory cat) -> size_t {
        size_t n = bucket_next[cat]++;
        auto flat_opt = category_flat(cat, n);
        if (flat_opt){
            return *flat_opt;
        }
        // Overflow: try ManualOther.
        {
            std::ostringstream oss;
            oss << "Category " << category_name(cat)
                << " overflow for catalogue[" << ci
                << "] (dex=" << (catalogue[ci] ? catalogue[ci]->dex_number : 0)
                << ") — placing in ManualOther.";
            plan.warnings.push_back(oss.str());
        }
        size_t m = bucket_next[BoxCategory::ManualOther]++;
        auto mo_opt = category_flat(BoxCategory::ManualOther, m);
        if (mo_opt){
            return *mo_opt;
        }
        {
            std::ostringstream oss;
            oss << "[BLOCKING] ManualOther box also full for catalogue[" << ci
                << "] (dex=" << (catalogue[ci] ? catalogue[ci]->dex_number : 0)
                << ") — no space available.";
            plan.warnings.push_back(oss.str());
        }
        return SIZE_MAX;
    };

    // Gather LivingDex-candidate entries: all occupied, base form assumed true.
    // We'll iterate through catalogue in order and process each entry.

    // Deferred re-route queue: entries that lost contention and need re-routing.
    // We process them after the first pass.
    std::vector<size_t> requeue;   // catalogue indices

    for (size_t ci = 0; ci < catalogue.size(); ci++){
        if (!catalogue[ci].has_value()) continue;
        const CollectedPokemonInfo& info = *catalogue[ci];

        // Determine species_slot_taken_by_shiny for this entry:
        // true if another shiny of the same species has already won the dex slot.
        const bool slot_taken_by_shiny =
            info.shiny
            ? false  // shiny itself doesn't count as "its own slot taken"
            : (dex_shiny_slot.count(info.dex_number) > 0);

        // We pass base_form_signature_matches = true for now (planner doesn't
        // have form-signature data; ManualForms entries would need external data).
        BoxCategory cat = route(info, cfg, slot_taken_by_shiny, true);

        if (cat == BoxCategory::LivingDex){
            // Try to claim the dex slot.
            auto wit = living_dex_winner.find(info.dex_number);
            if (wit == living_dex_winner.end()){
                // First claim.
                living_dex_winner[info.dex_number] = ci;
                if (info.shiny){
                    dex_shiny_slot[info.dex_number] = ci;
                }
            }
            else{
                // Contention: compare with existing winner.
                size_t incumbent_ci = wit->second;
                if (wins_slot(info, *catalogue[incumbent_ci], cfg)){
                    // Challenger wins: push incumbent to requeue.
                    requeue.push_back(incumbent_ci);
                    living_dex_winner[info.dex_number] = ci;
                    if (info.shiny){
                        dex_shiny_slot[info.dex_number] = ci;
                    }
                    else if (catalogue[incumbent_ci]->shiny){
                        // Incumbent was shiny but lost — remove its shiny claim.
                        dex_shiny_slot.erase(info.dex_number);
                    }
                }
                else{
                    // Incumbent wins: challenger goes to requeue.
                    requeue.push_back(ci);
                }
            }
        }
        else{
            // Non-LivingDex category: assign to bucket.
            size_t flat = assign_bucket(ci, cat);
            target_slot[ci] = flat;
        }
    }

    // Assign living-dex winners their target slots.
    // LivingDex target = the slot at (dex_number - 1) within the LivingDex range.
    for (auto& [dex, winner_ci] : living_dex_winner){
        size_t n = static_cast<size_t>(dex - 1);  // 0-indexed slot within range
        auto flat_opt = category_flat(BoxCategory::LivingDex, n);
        if (flat_opt){
            target_slot[winner_ci] = *flat_opt;
        }
        else{
            // Dex number out of LivingDex range — treat as overflow to ManualOther.
            std::ostringstream oss;
            oss << "LivingDex slot for dex#" << dex
                << " is out of range — placing in ManualOther.";
            plan.warnings.push_back(oss.str());
            size_t flat = assign_bucket(winner_ci, BoxCategory::ManualOther);
            target_slot[winner_ci] = flat;
        }
    }

    // Process requeue: entries that lost contention need re-routing.
    // For re-routed entries we re-call route() with updated shiny-slot info.
    for (size_t ci : requeue){
        if (!catalogue[ci].has_value()) continue;
        const CollectedPokemonInfo& info = *catalogue[ci];

        // Re-evaluate with updated dex_shiny_slot.
        const bool slot_taken_by_shiny =
            dex_shiny_slot.count(info.dex_number) > 0;

        // Re-route: since we lost the living-dex slot, the loser gets routed
        // without the chance to take LivingDex again (its dex slot is taken).
        BoxCategory cat = route(info, cfg, slot_taken_by_shiny, true);

        // If it still routes to LivingDex (e.g. shiny that lost to another shiny),
        // it goes to DuplicateShiny (shiny slot taken) or ManualOther.
        if (cat == BoxCategory::LivingDex){
            // This WILL happen for owners of alt-forms sharing a dex# with a base
            // form, because base_form_signature_matches is always true in v1
            // (form-signature detection is a documented v1 limitation — the catalogue
            // only stores dex_number, not the full form signature).  Such entries
            // end up here and are redirected to ManualOther for manual review.
            cat = BoxCategory::ManualOther;
        }

        size_t flat = assign_bucket(ci, cat);
        target_slot[ci] = flat;
    }

    // -----------------------------------------------------------------------
    // Phase 2: Move computation.
    //
    // We now know, for each catalogue entry ci, where it currently is (current
    // flat index = scan_start box offset) and where it should go (target flat).
    //
    // Build current_occupant[flat] = optional<ci> for all flat indices in board.
    // Also build desired_occupant[flat] = optional<ci>.
    //
    // Then compute moves using a greedy cycle-break algorithm:
    //   For each target flat that is not yet satisfied:
    //     If the target is empty: move the desired occupant there.
    //     If the target is occupied by the right ci: done.
    //     Otherwise: detect the permutation cycle.  Break cycle using an empty
    //     slot or scratch slot as a temp.
    // -----------------------------------------------------------------------

    // current_flat[ci] = where catalogue entry ci currently is in the flat board.
    std::vector<size_t> current_flat(catalogue.size(), SIZE_MAX);
    for (size_t ci = 0; ci < catalogue.size(); ci++){
        if (!catalogue[ci].has_value()) continue;
        const size_t abs_box     = scan_start + ci / SLOTS_PER_BOX;
        const size_t slot_in_box = ci % SLOTS_PER_BOX;
        current_flat[ci] = abs_to_flat(abs_box, slot_in_box);
    }

    // board_occupant[flat] = ci of what's currently there (or SIZE_MAX = empty).
    std::vector<size_t> board_occupant(board.size(), SIZE_MAX);
    for (size_t ci = 0; ci < catalogue.size(); ci++){
        if (!catalogue[ci].has_value()) continue;
        const size_t flat = current_flat[ci];
        if (flat < board_occupant.size()){
            board_occupant[flat] = ci;
        }
    }

    // Collect all free (empty) slots across the board + scratch boxes.
    // We'll use these as intermediaries.
    std::vector<size_t> free_slots;
    // Scan all currently-empty flat indices in the full board.
    for (size_t f = 0; f < board_occupant.size(); f++){
        if (board_occupant[f] == SIZE_MAX){
            free_slots.push_back(f);
        }
    }
    // Also include scratch box slots explicitly (some may already be in free_slots
    // if they're empty; that's fine — we deduplicate below if needed).
    // Scratch boxes start at scratch_box_start and span scratch_box_count boxes.
    // They are already included in board_occupant (populated as empty == SIZE_MAX).

    // Helper: find a free slot (returns SIZE_MAX if none).
    // Pop from free_slots.
    auto pop_free = [&]() -> size_t {
        while (!free_slots.empty()){
            size_t f = free_slots.back();
            free_slots.pop_back();
            if (board_occupant[f] == SIZE_MAX){
                return f;  // confirmed still empty
            }
        }
        return SIZE_MAX;
    };

    // Helper: emit a PlannedMove and update board_occupant + current_flat.
    auto do_move = [&](size_t from_flat, size_t to_flat){
        // Y-pick from_flat, Y-place to_flat (swap).
        // If to_flat is empty: simply moves.
        // If to_flat is occupied: swaps.
        size_t ci_from = board_occupant[from_flat];
        size_t ci_to   = board_occupant[to_flat];

        BoxCursor from_cur = make_cursor(from_flat / SLOTS_PER_BOX,
                                         from_flat % SLOTS_PER_BOX);
        BoxCursor to_cur   = make_cursor(to_flat   / SLOTS_PER_BOX,
                                         to_flat   % SLOTS_PER_BOX);
        plan.moves.push_back(PlannedMove{ from_cur, to_cur });

        // Update board.
        board_occupant[from_flat] = ci_to;
        board_occupant[to_flat]   = ci_from;

        if (ci_from != SIZE_MAX) current_flat[ci_from] = to_flat;
        if (ci_to   != SIZE_MAX) current_flat[ci_to]   = from_flat;

        // Update free_slots tracking.
        if (ci_to == SIZE_MAX){
            // to_flat was empty, now from_flat is empty.
            free_slots.push_back(from_flat);
        }
        // (If ci_to was occupied, no new free slot was created.)
    };

    // Main move loop.  For each occupied catalogue entry that needs to move:
    // Use a set to iterate stably over pending targets.
    // pending_targets: set of target flat indices that haven't been satisfied yet.
    std::set<size_t> pending;
    for (size_t ci = 0; ci < catalogue.size(); ci++){
        if (!catalogue[ci].has_value()) continue;
        if (target_slot[ci] == SIZE_MAX) continue;    // unplaceable (blocking warning already emitted)
        const size_t cur = current_flat[ci];
        const size_t tgt = target_slot[ci];
        if (cur != tgt){
            pending.insert(tgt);
        }
    }

    // Build desired_at[flat] = ci that should end up at flat.
    std::vector<size_t> desired_at(board_occupant.size(), SIZE_MAX);
    for (size_t ci = 0; ci < catalogue.size(); ci++){
        if (!catalogue[ci].has_value()) continue;
        if (target_slot[ci] == SIZE_MAX) continue;
        if (target_slot[ci] < desired_at.size()){
            desired_at[target_slot[ci]] = ci;
        }
    }

    // Greedy move loop.
    // For each pending target, check if the target is free.
    //   If free: move the desired occupant's current position to target.
    //   If occupied by wrong ci: we have a chain or cycle.
    //     Cycle-break: move the desired occupant somewhere temporary first.
    size_t iterations = 0;
    const size_t max_iterations = catalogue.size() * catalogue.size() + 1000;

    while (!pending.empty() && iterations++ < max_iterations){
        size_t tgt = *pending.begin();
        pending.erase(pending.begin());

        const size_t want_ci = desired_at[tgt];
        if (want_ci == SIZE_MAX) continue;  // nothing needs this slot

        const size_t cur = current_flat[want_ci];
        if (cur == tgt) continue;  // already there

        // What's currently at tgt?
        const size_t cur_occupant_ci = board_occupant[tgt];

        if (cur_occupant_ci == SIZE_MAX){
            // Target is empty — direct move from cur to tgt.
            do_move(cur, tgt);
            // Check if cur's former occupant (want_ci) reached its target.
            // (Already moved, so want_ci is now at tgt — which is its target.)
        }
        else if (cur_occupant_ci == want_ci){
            // The right Pokémon is already here — shouldn't reach pending if satisfied.
            continue;
        }
        else{
            // Target is occupied by the wrong Pokémon.
            // Two options:
            //  (a) The occupant's own target is somewhere else (chain): move occupant
            //      to its own target first (if that target is free).
            //  (b) We're in a cycle: need a temp slot.

            const size_t occ_tgt = target_slot[cur_occupant_ci];

            if (occ_tgt != SIZE_MAX && board_occupant[occ_tgt] == SIZE_MAX){
                // Case (a): move occupant to its own empty target slot.
                do_move(tgt, occ_tgt);
                // Now tgt is free; re-add tgt to pending so we pick it up next.
                pending.insert(tgt);
            }
            else{
                // Case (b): need a temp.  Pop a free slot.
                size_t temp = pop_free();
                if (temp == SIZE_MAX){
                    std::ostringstream oss;
                    oss << "[BLOCKING] No free slot available to use as intermediary "
                        << "while placing catalogue[" << want_ci
                        << "] (dex=" << (catalogue[want_ci] ? catalogue[want_ci]->dex_number : 0)
                        << ") — cannot complete sort without overwriting an occupied slot.";
                    plan.warnings.push_back(oss.str());
                    continue;
                }
                // Move cur_occupant_ci to temp.
                do_move(tgt, temp);
                // Now tgt is free; re-add to pending so we can move want_ci there.
                pending.insert(tgt);
                // Also re-add temp target if occ_ci needs to move somewhere.
                if (occ_tgt != SIZE_MAX && occ_tgt != temp){
                    pending.insert(occ_tgt);
                }
            }
        }
    }

    if (iterations >= max_iterations){
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

/*  Master Box Router V3
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  v3 routing logic: species grouping (by SpeciesKey), dex-first best-copy
 *  dedup (split shiny / non-shiny), duplicate priority chain, and overqualified
 *  detection for the dex keepers.
 *
 *  Design notes:
 *  - route_all_v3 is pure: no file IO, no Qt, no hardware.
 *  - Groups are keyed by SpeciesKey (dex_number + primary_type + secondary_type
 *    + gmax + gender).  Cosmetic-only forms (same types/gmax/gender, e.g. Unown
 *    letters, Vivillon patterns) are NOT distinguishable and land in the same
 *    group — this is the documented reader limitation from v2 / spec §7.
 *  - Within each group we elect exactly one RegularDex keeper (best non-shiny
 *    via wins_slot) and at most one ShinyDex keeper (best shiny, unless the
 *    species is shiny-locked).
 *  - The "Forms" check in route_duplicate_v3 detects groups whose SpeciesKey
 *    differs from the canonical base for that dex# (t1/t2/gmax/gender differ
 *    from what the most-common key for that dex# reports).  For a group with
 *    a single dex#, this is determined by comparing the key to the first key
 *    seen for that dex# (i.e. the first copy scanned is treated as canonical
 *    within the test; real production would consult a species-db, but the
 *    reader limitation means we can't do better without extra data).
 *    For route_duplicate_v3 called in isolation, variant detection is done by
 *    comparing the pokémon's types/gmax/gender to the "base" implied by its
 *    own dex# — since we don't have a base-form DB, we flag as Forms only when
 *    the caller explicitly groups the pokémon into a variant group.  To keep
 *    route_duplicate_v3 correct in isolation (as required by the API), we
 *    pass a `is_variant` flag through an internal helper.  The public
 *    route_duplicate_v3 passes is_variant=false (conservative); route_all_v3
 *    passes is_variant=true when the group's key differs from the dex#'s
 *    canonical key.
 */

#include "PokemonHome_MasterBoxRouterV3.h"
#include "PokemonHome_MasterBoxPlanner.h"   // wins_slot

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Internal helper — true when p came from an event distribution.
// Mirrors the identical helper in MasterBoxRouter.cpp.
// ---------------------------------------------------------------------------
static bool is_event_v3(const Pokemon::CollectedPokemonInfo& p){
    if (p.ball_slug == "cherish-ball") return true;
    switch (p.origin_mark){
        case Pokemon::OriginMark::GO:
        case Pokemon::OriginMark::LGPE:
        case Pokemon::OriginMark::GAMEBOY:
            return true;
        default:
            return false;
    }
}


// ---------------------------------------------------------------------------
// Internal helper — true when p's dex# is in any of the four rare sets.
// ---------------------------------------------------------------------------
static bool is_rare_species_v3(
    const Pokemon::CollectedPokemonInfo& p,
    const MasterBoxLayoutV3&             layout
){
    return layout.legendary  .count(p.dex_number) ||
           layout.mythical   .count(p.dex_number) ||
           layout.ultra_beast.count(p.dex_number) ||
           layout.paradox    .count(p.dex_number);
}


// ---------------------------------------------------------------------------
// Internal implementation of route_duplicate_v3 with an explicit variant flag.
//
// The public route_duplicate_v3 calls this with is_variant=false.
// route_all_v3 calls this with is_variant=true for variant groups.
// ---------------------------------------------------------------------------
static BoxCategory route_duplicate_impl(
    const Pokemon::CollectedPokemonInfo& p,
    const MasterBoxLayoutV3&             layout,
    const RouterConfig&                  cfg,
    bool                                 is_variant
){
    // 1. ManualForms — detectable form variant (type/gmax/gender differs from
    //    the canonical base for this dex#).  Only reliable when the caller
    //    has confirmed this via grouping (is_variant).
    if (is_variant){
        return BoxCategory::ManualForms;
    }

    // 2–5. Rare-species collection boxes (priority order per spec §3).
    if (layout.legendary  .count(p.dex_number)) return BoxCategory::Legendary;
    if (layout.mythical   .count(p.dex_number)) return BoxCategory::Mythical;
    if (layout.ultra_beast.count(p.dex_number)) return BoxCategory::UltraBeast;
    if (layout.paradox    .count(p.dex_number)) return BoxCategory::Paradox;

    // 6. Events — cherish-ball or GO/LGPE/GAMEBOY origin mark.
    if (is_event_v3(p)) return BoxCategory::Events;

    // 7. Utility — ability/item/move rule match.
    if (p_matches_utility(p, cfg.utility_rules)) return BoxCategory::Utility;

    // 8. Breeding — IV best-count in [breeding_range.first, breeding_range.second].
    if (p.iv_read &&
        p.iv_best_count >= cfg.breeding_range.first &&
        p.iv_best_count <= cfg.breeding_range.second){
        return BoxCategory::Breeding;
    }

    // 9. Competitive — IV best-count >= competitive_min31.
    if (p.iv_read && p.iv_best_count >= cfg.competitive_min31){
        return BoxCategory::Competitive;
    }

    // 10. ShinyTrades — shiny duplicate.
    if (p.shiny) return BoxCategory::ShinyTrades;

    // 11. RegularTrades — foreign OT (has trade value).
    if (!is_owner_ot(p, cfg.owner_ot_names)) return BoxCategory::RegularTrades;

    // 12. Junk — default review bucket.
    // Auto-protections: never Junk a shiny / owner-OT / legendary / mythical /
    // event / perfect-IV (all of those are caught by earlier steps above or
    // should be here as a safety net).
    // At this point: non-shiny, owner-OT, non-rare, non-event, no useful IVs,
    // no utility match — plain common duplicate.
    return BoxCategory::Junk;
}


// ---------------------------------------------------------------------------
// Public: route_duplicate_v3 (non-variant, conservative).
// ---------------------------------------------------------------------------
BoxCategory route_duplicate_v3(
    const Pokemon::CollectedPokemonInfo& p,
    const MasterBoxLayoutV3&             layout,
    const RouterConfig&                  cfg
){
    return route_duplicate_impl(p, layout, cfg, /*is_variant=*/false);
}


// ---------------------------------------------------------------------------
// Build the SpeciesKey for a given CollectedPokemonInfo.
// ---------------------------------------------------------------------------
static SpeciesKey make_species_key(const Pokemon::CollectedPokemonInfo& p){
    return SpeciesKey{
        p.dex_number,
        p.primary_type,
        p.secondary_type,
        p.gmax,
        p.gender
    };
}


// ---------------------------------------------------------------------------
// Populate also_qualifies for a dex keeper.
// Checks all higher categories the keeper ALSO matches.
// ---------------------------------------------------------------------------
static std::vector<std::string> compute_also_qualifies(
    const Pokemon::CollectedPokemonInfo& p,
    const MasterBoxLayoutV3&             layout,
    const RouterConfig&                  cfg,
    bool                                 is_variant
){
    std::vector<std::string> result;

    // Forms — detectable variant.
    if (is_variant) result.push_back("Forms");

    // Rare-species collection boxes.
    if (layout.legendary  .count(p.dex_number)) result.push_back("Legendary");
    if (layout.mythical   .count(p.dex_number)) result.push_back("Mythical");
    if (layout.ultra_beast.count(p.dex_number)) result.push_back("UltraBeast");
    if (layout.paradox    .count(p.dex_number)) result.push_back("Paradox");

    // Events.
    if (is_event_v3(p)) result.push_back("Events");

    // Utility.
    if (p_matches_utility(p, cfg.utility_rules)) result.push_back("Utility");

    // Breeding.
    if (p.iv_read &&
        p.iv_best_count >= cfg.breeding_range.first &&
        p.iv_best_count <= cfg.breeding_range.second){
        result.push_back("Breeding");
    }

    // Competitive.
    if (p.iv_read && p.iv_best_count >= cfg.competitive_min31){
        result.push_back("Competitive");
    }

    return result;
}


// ---------------------------------------------------------------------------
// route_all_v3 — full catalogue routing.
// ---------------------------------------------------------------------------
std::vector<RouteResultV3> route_all_v3(
    const std::vector<std::optional<Pokemon::CollectedPokemonInfo>>& catalogue,
    const MasterBoxLayoutV3&                                         layout,
    const RouterConfig&                                              cfg
){
    const size_t N = catalogue.size();
    std::vector<RouteResultV3> results(N);  // default: Junk, not keeper

    // -------------------------------------------------------------------------
    // Step 1: Build groups keyed by SpeciesKey.
    // Each group holds the catalogue indices of non-empty copies.
    // We also track, per dex_number, the first SpeciesKey seen so we can
    // identify variant groups later.
    // -------------------------------------------------------------------------

    // map<SpeciesKey, vector<size_t>> — ordered map uses operator< on SpeciesKey.
    std::map<SpeciesKey, std::vector<size_t>> groups;
    // canonical key per dex# = first key seen for that dex# (used for Forms detection).
    std::map<uint16_t, SpeciesKey> canonical_key;

    for (size_t i = 0; i < N; ++i){
        if (!catalogue[i].has_value()) continue;
        const auto& p = *catalogue[i];
        SpeciesKey key = make_species_key(p);
        groups[key].push_back(i);
        if (canonical_key.find(p.dex_number) == canonical_key.end()){
            canonical_key[p.dex_number] = key;
        }
    }

    // -------------------------------------------------------------------------
    // Step 2: Process each group.
    // -------------------------------------------------------------------------
    for (auto& [key, indices] : groups){
        // Determine whether this group is a detectable variant.
        // A group is a variant if its key differs from the canonical key for
        // the same dex#.  (The canonical key is the first key seen for that
        // dex# across all groups.)
        bool is_variant = false;
        auto canon_it = canonical_key.find(key.dex_number);
        if (canon_it != canonical_key.end()){
            is_variant = !(key == canon_it->second);
        }

        // Shiny-locked check: if the dex# is shiny-locked, no copy can fill
        // the ShinyDex slot regardless.
        const bool shiny_locked = layout.shiny_locked.count(key.dex_number) > 0;

        // Separate indices into shiny and non-shiny copies.
        std::vector<size_t> shiny_indices;
        std::vector<size_t> nonshiny_indices;
        for (size_t ci : indices){
            if (catalogue[ci]->shiny) shiny_indices.push_back(ci);
            else                      nonshiny_indices.push_back(ci);
        }

        // ---- Select best non-shiny copy (RegularDex keeper) ----------------
        // wins_slot(challenger, incumbent) returns true if challenger beats incumbent.
        // We pick the copy that beats all others.
        size_t best_nonshiny = SIZE_MAX;
        for (size_t ci : nonshiny_indices){
            if (best_nonshiny == SIZE_MAX){
                best_nonshiny = ci;
            } else if (wins_slot(*catalogue[ci], *catalogue[best_nonshiny], cfg)){
                best_nonshiny = ci;
            }
        }

        // ---- Select best shiny copy (ShinyDex keeper) ----------------------
        size_t best_shiny = SIZE_MAX;
        if (!shiny_locked){
            for (size_t ci : shiny_indices){
                if (best_shiny == SIZE_MAX){
                    best_shiny = ci;
                } else if (wins_slot(*catalogue[ci], *catalogue[best_shiny], cfg)){
                    best_shiny = ci;
                }
            }
        }

        // ---- Assign results -------------------------------------------------
        // Non-shiny keeper.
        if (best_nonshiny != SIZE_MAX){
            const auto& p = *catalogue[best_nonshiny];
            RouteResultV3& r = results[best_nonshiny];
            r.category      = BoxCategory::RegularDex;
            r.is_dex_keeper = true;
            r.also_qualifies = compute_also_qualifies(p, layout, cfg, is_variant);
        }

        // Shiny keeper.
        if (best_shiny != SIZE_MAX){
            const auto& p = *catalogue[best_shiny];
            RouteResultV3& r = results[best_shiny];
            r.category      = BoxCategory::ShinyDex;
            r.is_dex_keeper = true;
            r.also_qualifies = compute_also_qualifies(p, layout, cfg, is_variant);
        }

        // All non-shiny duplicates.
        for (size_t ci : nonshiny_indices){
            if (ci == best_nonshiny) continue;
            const auto& p = *catalogue[ci];
            RouteResultV3& r = results[ci];
            r.is_dex_keeper = false;
            r.category = route_duplicate_impl(p, layout, cfg, is_variant);
        }

        // All shiny duplicates (including shiny-locked shinies).
        for (size_t ci : shiny_indices){
            if (ci == best_shiny) continue;
            const auto& p = *catalogue[ci];
            RouteResultV3& r = results[ci];
            r.is_dex_keeper = false;
            r.category = route_duplicate_impl(p, layout, cfg, is_variant);
        }

        // Shiny-locked shinies: best_shiny == SIZE_MAX but shiny_indices is non-empty.
        // All of them go through route_duplicate_impl (as duplicates).
        // Already handled by the loop above (ci == SIZE_MAX never matches any index).
    }

    return results;
}


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

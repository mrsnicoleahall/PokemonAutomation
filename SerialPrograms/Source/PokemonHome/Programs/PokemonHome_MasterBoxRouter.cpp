/*  Master Box Router
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure routing logic — no Qt, no hardware, no file IO.
 *  Routing order (first match wins), per spec §4:
 *    1. !base_form_signature_matches         → ManualForms
 *    2. shiny && species_slot_taken_by_shiny → DuplicateShiny
 *    3. OT ∉ owner && !shiny && !legend/myth && !event → GoodTrades
 *    4. iv_read && best_count >= competitive_min31      → Competitive
 *    5. iv_read && best_count in breeding_range         → Breeding
 *    6. iv_read && best_count in breedject_range        → Breedject
 *    6b. matches any UtilityRule (ability/item/move slug) → Utility  [v2]
 *    7. event (cherish ball OR origin_mark ∈ {GO,LGPE,GAMEBOY}) → Events
 *    8. dex ∈ mythical → Mythical;  ∈ legendary → Legendary;
 *       ∈ ultra_beast  → UltraBeast; ∈ paradox  → Paradox
 *    9. else → LivingDex
 */

#include "PokemonHome_MasterBoxRouter.h"

#include <algorithm>
#include <cctype>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Internal helper — canonicalise a slug for format-agnostic comparison.
// Lowercases and strips every non-alphanumeric character so that OCR output
// ("flamebody") matches hyphenated defaults ("flame-body"), and so the move
// branch handles both hyphenated dictionary keys and space-separated forms.
// ---------------------------------------------------------------------------
static std::string canon_slug(const std::string& s){
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s){
        if (std::isalnum(c)){
            out += static_cast<char>(std::tolower(c));
        }
    }
    return out;
}


// ---------------------------------------------------------------------------
// Internal helper — true when the Pokémon came from an event distribution.
// ---------------------------------------------------------------------------
static bool is_event(const Pokemon::CollectedPokemonInfo& p){
    if (p.ball_slug == "cherish-ball"){
        return true;
    }
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
// Public helpers
// ---------------------------------------------------------------------------

bool is_owner_ot(const Pokemon::CollectedPokemonInfo& p, const std::set<std::string>& owners){
    return owners.find(p.ot_name) != owners.end();
}

bool p_matches_utility(const Pokemon::CollectedPokemonInfo& p, const std::vector<UtilityRule>& rules){  // v2
    // Use canon_slug on both sides so OCR-normalised slugs (hyphens stripped,
    // e.g. "flamebody") match hyphenated defaults ("flame-body"), and vice-versa.
    for (const UtilityRule& r : rules){
        const std::string canon_target = canon_slug(r.target_slug);
        switch (r.kind){
        case UtilityRule::Ability:
            if (canon_slug(p.ability_slug) == canon_target) return true;
            break;
        case UtilityRule::Item:
            if (canon_slug(p.held_item_slug) == canon_target) return true;
            break;
        case UtilityRule::Move:
            for (const std::string& m : p.moves){
                if (canon_slug(m) == canon_target) return true;
            }
            break;
        }
    }
    return false;
}


// ---------------------------------------------------------------------------
// Primary routing function — first matching rule wins.
// ---------------------------------------------------------------------------

BoxCategory route(
    const Pokemon::CollectedPokemonInfo& p,
    const RouterConfig& cfg,
    bool species_slot_taken_by_shiny,
    bool base_form_signature_matches
){
    // 1. Non-standard form → needs manual assignment.
    if (!base_form_signature_matches){
        return BoxCategory::ManualForms;
    }

    // 2. Already have this shiny species → store as duplicate.
    if (p.shiny && species_slot_taken_by_shiny){
        return BoxCategory::DuplicateShiny;
    }

    // Pre-compute flags used in step 3 and later.
    const bool rare_species =
        (cfg.legendary   && cfg.legendary  ->count(p.dex_number)) ||
        (cfg.mythical    && cfg.mythical   ->count(p.dex_number)) ||
        (cfg.ultra_beast && cfg.ultra_beast->count(p.dex_number)) ||
        (cfg.paradox     && cfg.paradox    ->count(p.dex_number));

    // 3. Foreign OT with nothing special going on → trade fodder.
    if (!is_owner_ot(p, cfg.owner_ot_names) && !p.shiny && !rare_species && !is_event(p)){
        return BoxCategory::GoodTrades;
    }

    // 4–6. IV-based routing (only when IVs were successfully read).
    if (p.iv_read){
        if (p.iv_best_count >= cfg.competitive_min31){
            return BoxCategory::Competitive;
        }
        if (p.iv_best_count >= cfg.breeding_range.first &&
            p.iv_best_count <= cfg.breeding_range.second){
            return BoxCategory::Breeding;
        }
        if (p.iv_best_count >= cfg.breedject_range.first &&
            p.iv_best_count <= cfg.breedject_range.second){
            return BoxCategory::Breedject;
        }
    }

    // 6b. Utility — ability/item/move match (v2).
    if (p_matches_utility(p, cfg.utility_rules)){
        return BoxCategory::Utility;
    }

    // 7. Event distribution Pokémon.
    if (is_event(p)){
        return BoxCategory::Events;
    }

    // 8. Special-species boxes (checked in priority order from brief).
    if (cfg.mythical    && cfg.mythical   ->count(p.dex_number)) return BoxCategory::Mythical;
    if (cfg.legendary   && cfg.legendary ->count(p.dex_number)) return BoxCategory::Legendary;
    if (cfg.ultra_beast && cfg.ultra_beast->count(p.dex_number)) return BoxCategory::UltraBeast;
    if (cfg.paradox     && cfg.paradox   ->count(p.dex_number)) return BoxCategory::Paradox;

    // 9. Default — living dex slot.
    return BoxCategory::LivingDex;
}


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

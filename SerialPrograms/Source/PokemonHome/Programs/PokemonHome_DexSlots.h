/*  Dex Slot Math
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure math functions mapping a species' National Dex number to its
 *  0-indexed slot in the Regular Dex region and the Shiny Dex region.
 *  No Qt, no hardware — safe to call from tests and planners alike.
 */

#ifndef PokemonAutomation_PokemonHome_DexSlots_H
#define PokemonAutomation_PokemonHome_DexSlots_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// Returns the 0-indexed slot for dex_number in the Regular Dex region.
// Species N always maps to slot N-1, preserving gaps for unowned species.
size_t regular_dex_slot(uint16_t dex_number);

// Returns the count of non-shiny-locked species in [1, max_dex].
// Used by shiny_dex_slot() and by the planner to size the Shiny Dex region.
size_t shiny_dex_species_count(uint16_t max_dex, const std::set<uint16_t>& shiny_locked);

// Returns the 0-indexed slot for dex_number in the Shiny Dex region.
// Returns nullopt if dex_number is shiny-locked.
// Shiny-locked species are compacted out, leaving no gap for them.
std::optional<size_t> shiny_dex_slot(uint16_t dex_number, const std::set<uint16_t>& shiny_locked);


}
}
}
#endif

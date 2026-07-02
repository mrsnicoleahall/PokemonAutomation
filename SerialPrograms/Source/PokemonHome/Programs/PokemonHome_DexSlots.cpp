/*  Dex Slot Math
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonHome_DexSlots.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


size_t regular_dex_slot(uint16_t dex_number){
    return static_cast<size_t>(dex_number - 1);
}

size_t shiny_dex_species_count(uint16_t max_dex, const std::set<uint16_t>& shiny_locked){
    // Count locked entries in [1, max_dex].
    auto it_begin = shiny_locked.lower_bound(1);
    auto it_end   = shiny_locked.upper_bound(max_dex);
    size_t locked_count = static_cast<size_t>(std::distance(it_begin, it_end));
    return static_cast<size_t>(max_dex) - locked_count;
}

std::optional<size_t> shiny_dex_slot(uint16_t dex_number, const std::set<uint16_t>& shiny_locked){
    if (shiny_locked.count(dex_number)){
        return std::nullopt;
    }
    return shiny_dex_species_count(dex_number, shiny_locked) - 1;
}


}
}
}

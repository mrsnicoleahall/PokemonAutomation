/*  Master Box Layout
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Loads master_box_layout.json and exposes the MasterBoxLayout struct.
 *  Provides the box-range assignments and the four special-dex number sets
 *  that are wired into RouterConfig (Task 3) at program startup.
 */

#ifndef PokemonAutomation_PokemonHome_MasterBoxLayout_H
#define PokemonAutomation_PokemonHome_MasterBoxLayout_H

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include "PokemonHome_MasterBoxRouter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


// ---------------------------------------------------------------------------
// Full layout configuration loaded from master_box_layout.json.
// ---------------------------------------------------------------------------
struct MasterBoxLayout{
    // 1-indexed box number where the Living Dex starts.
    uint16_t living_dex_start_box = 1;

    // Per-category [start, end] inclusive box ranges (1-indexed).
    std::map<BoxCategory, std::pair<uint16_t, uint16_t>> category_box_ranges;

    // Special dex-number sets (national dex numbers).
    std::set<uint16_t> legendary;
    std::set<uint16_t> mythical;
    std::set<uint16_t> ultra_beast;
    std::set<uint16_t> paradox;
};


// ---------------------------------------------------------------------------
// Load master_box_layout.json from the given path.
// Throws on any parse error or missing required key.
// ---------------------------------------------------------------------------
MasterBoxLayout load_master_box_layout(const std::string& path);


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

#endif

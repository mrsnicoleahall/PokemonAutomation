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


// ---------------------------------------------------------------------------
// v3 layout configuration.
// Loads master_box_layout_v3.json (layout) + shiny_locked.json (shiny-lock
// data) and exposes the dual-dex ranges, per-category box ranges, and the
// shiny-locked species set.
// ---------------------------------------------------------------------------
struct MasterBoxLayoutV3{
    // 1-indexed box number where the Shiny Dex starts (first physical box).
    uint16_t shiny_dex_start = 1;
    // 1-indexed box number where the Regular Dex starts.
    uint16_t regular_dex_start = 41;
    // Number of empty buffer boxes after the Shiny Dex (before Regular Dex).
    uint16_t shiny_dex_buffer_boxes = 5;
    // Number of empty buffer boxes after the Regular Dex (before collection boxes).
    uint16_t regular_dex_buffer_boxes = 5;

    // Per-category [start, end] inclusive box ranges (1-indexed) for all
    // sections other than the two dexes (Legendary, Mythical, UltraBeast,
    // Paradox, Events, Forms/ManualForms, Breeding, Utility, Competitive,
    // ShinyTrades, RegularTrades, Junk).
    std::map<BoxCategory, std::pair<uint16_t, uint16_t>> category_box_ranges;

    // Special dex-number sets (national dex numbers) — used for routing
    // duplicates into collection boxes.
    std::set<uint16_t> legendary;
    std::set<uint16_t> mythical;
    std::set<uint16_t> ultra_beast;
    std::set<uint16_t> paradox;

    // Species that are shiny-locked (can never legally be shiny in HOME).
    // These species are omitted entirely from the Shiny Dex (no gap allocated).
    std::set<uint16_t> shiny_locked;
};


// ---------------------------------------------------------------------------
// Load v3 layout from two JSON files:
//   layout_path      — master_box_layout_v3.json
//   shiny_locked_path — shiny_locked.json
// Throws on any parse error or missing required key.
// ---------------------------------------------------------------------------
MasterBoxLayoutV3 load_master_box_layout_v3(
    const std::string& layout_path,
    const std::string& shiny_locked_path
);


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

#endif

/*  Home Box Navigation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonHome_BoxNavigation_H
#define PokemonAutomation_PokemonHome_BoxNavigation_H

#include <array>
#include <cstddef>
#include <optional>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Pokemon_BoxCursor.h"
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "Pokemon/Options/Pokemon_BoxSortingTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

// ---------------------------------------------------------------------------
// Shared per-slot occupancy test.
//
// Returns true when the slot at (row, col) within the box-grid region of
// `screen` appears to contain a Pokémon icon.
//
// Uses stddev >= 10 on the icon sub-rectangle — the same coordinates and
// threshold used by both find_occupied_slots_in_box and
// BoxSorterMaster::count_occupied_slots_in_box.  Both functions call this
// helper so the two notions of "occupied" cannot diverge.
// ---------------------------------------------------------------------------
bool slot_is_occupied(const ImageViewRGB32& screen, size_t row, size_t col);


// Move the red cursor to the first slot of the box
bool go_to_first_slot(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    Milliseconds VIDEO_DELAY
);

// Move the cursor to the given coordinates, knowing current pos via the cursor struct
[[nodiscard]] Pokemon::BoxCursor move_cursor_to(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const Pokemon::BoxCursor& cur_cursor,
    const Pokemon::BoxCursor& dest_cursor,
    Milliseconds GAME_DELAY
);

// Read current screen to find occupied and empty slots in the box.
// Returns the (row, col) index of the first pokemon (aka non-empty) slot in the box.
std::array<size_t, 2> find_occupied_slots_in_box(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    std::vector<std::optional<Pokemon::CollectedPokemonInfo>>& boxes_data,
    const std::vector<Pokemon::SortingRule>& sort_preferences
);

// Read the current summary screen and assign various pokemon info into cur_pokemon_info
void read_summary_screen(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    Pokemon::CollectedPokemonInfo& cur_pokemon_info,
    Language ot_name_language = Language::None
);

void print_boxes_data(
    const std::vector<std::optional<Pokemon::CollectedPokemonInfo>>& boxes_data, 
    SingleSwitchProgramEnvironment& env
);

void exit_menus(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::chrono::milliseconds video_delay);

}
}
}
#endif

/*  Pokemon Home — Rename Boxes (v3)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Standalone program (NOT part of the sorter) that renames HOME boxes to
 *  match the v3 dual-dex box-map labels.  Run AFTER the sorter has placed
 *  Pokémon into their final positions.
 *
 *  The on-screen-keyboard navigation and per-key timing are LOG-ONLY in
 *  this scaffold.  Real OSK input will be calibrated interactively with
 *  Nicole on the hardware rig (see type_box_name comment in the .cpp).
 *
 *  Options:
 *    START_BOX  — 1-indexed first box to rename (default 1).
 *    BOX_COUNT  — how many consecutive boxes to rename (default 1,
 *                 so we can rename ONE box first when calibrating).
 */

#ifndef PokemonAutomation_PokemonHome_RenameBoxes_H
#define PokemonAutomation_PokemonHome_RenameBoxes_H

#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/TimeDurationOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


class RenameBoxes_Descriptor : public SingleSwitchProgramDescriptor{
public:
    RenameBoxes_Descriptor();
};


class RenameBoxes : public SingleSwitchProgramInstance{
public:
    RenameBoxes();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    // -----------------------------------------------------------------------
    // Stub: log what WOULD be typed for a given box.
    //
    // INTENDED REAL FLOW (to be implemented + calibrated on the rig with
    // Nicole once the hardware is set up):
    //
    //   1. Navigate to the target box:
    //        - From the HOME main grid, open the box-rename editor by holding
    //          ZL on the box header / tapping the box name in the UI (exact
    //          button TBD from live observation).
    //   2. Clear the existing name:
    //        - Press the Clear / Delete softkey on the HOME on-screen keyboard
    //          enough times to blank the field, or use a "Select All + Delete"
    //          sequence (to be confirmed on device).
    //   3. Type the label character by character:
    //        - Navigate the on-screen keyboard with the d-pad / left-stick to
    //          reach each character and press A to select it.
    //        - The coordinate offsets and timing between key-presses (ms/frame)
    //          are calibrated interactively: rename one box → verify → refine
    //          → batch.  They are NOT hardcoded here.
    //   4. Confirm the new name (press + / OK softkey on the keyboard).
    //   5. Verify by reading back the box-header text via the existing OCR
    //        infrastructure (optional, can be added once step 3 is calibrated).
    //
    // CURRENT BEHAVIOR (scaffold / log-only):
    //   Only logs what label WOULD be written.  No buttons are pressed.
    //   Completely safe to run on hardware — it will not change any box name.
    // -----------------------------------------------------------------------
    void type_box_name(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        uint16_t box_index,         // 1-indexed HOME box number
        const std::string& label    // e.g. "Shiny Dex 01", "Junk"
    );

    // -----------------------------------------------------------------------
    // Options
    // -----------------------------------------------------------------------
    SimpleIntegerOption<uint16_t> START_BOX;
    SimpleIntegerOption<uint16_t> BOX_COUNT;
    MillisecondsOption VIDEO_DELAY;
    MillisecondsOption GAME_DELAY;
    EventNotificationsOption NOTIFICATIONS;
};


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif  // PokemonAutomation_PokemonHome_RenameBoxes_H

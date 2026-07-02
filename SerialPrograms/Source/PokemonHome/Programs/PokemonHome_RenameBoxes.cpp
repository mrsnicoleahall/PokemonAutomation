/*  Pokemon Home — Rename Boxes (v3)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Standalone program (NOT part of the sorter) that renames HOME boxes to
 *  match the v3 dual-dex box-map labels.  Run AFTER the sorter has placed
 *  Pokémon into their final positions.
 *
 *  STATUS: SCAFFOLD — log-only, no real keyboard input.
 *  The OSK navigation and per-key timing will be calibrated interactively
 *  with Nicole on the hardware rig.  See type_box_name() for the full
 *  intended flow and the log-only placeholder.
 */

#include <string>
#include <vector>
#include "CommonFramework/Globals.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonHome_MasterBoxLayout.h"
#include "PokemonHome_MasterPlannerV3.h"
#include "PokemonHome_RenameBoxes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr uint16_t MAX_HOME_BOXES = 200;


// ---------------------------------------------------------------------------
// Descriptor
// ---------------------------------------------------------------------------

RenameBoxes_Descriptor::RenameBoxes_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonHome:RenameBoxes",
        STRING_POKEMON + " Home", "Rename Boxes (v3)",
        "",
        "Rename HOME boxes to match the v3 dual-dex box-map labels "
        "(Shiny Dex, Regular Dex, Legendary, Junk, etc.).  "
        "Run this AFTER sorting.  "
        "LOG-ONLY scaffold — no keyboard input until calibrated on the rig.",
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::OPTIONAL_,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


// ---------------------------------------------------------------------------
// Constructor — wire up all options
// ---------------------------------------------------------------------------

RenameBoxes::RenameBoxes()
    : START_BOX(
        "<b>Start Box:</b><br>"
        "First HOME box to rename (1-indexed).  "
        "Set to 1 to begin at the very first box.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, MAX_HOME_BOXES
    )
    , BOX_COUNT(
        "<b>Box Count:</b><br>"
        "Number of consecutive boxes to rename starting at Start Box.  "
        "Default 1 — rename a single box first when calibrating the "
        "on-screen keyboard.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, MAX_HOME_BOXES
    )
    , VIDEO_DELAY(
        "<b>Capture Card Delay:</b>",
        LockMode::LOCK_WHILE_RUNNING,
        "400 ms"
    )
    , GAME_DELAY(
        "<b>" + STRING_POKEMON + " Home App Delay:</b>",
        LockMode::LOCK_WHILE_RUNNING,
        "240 ms"
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH
    })
{
    PA_ADD_OPTION(START_BOX);
    PA_ADD_OPTION(BOX_COUNT);
    PA_ADD_OPTION(VIDEO_DELAY);
    PA_ADD_OPTION(GAME_DELAY);
    PA_ADD_OPTION(NOTIFICATIONS);
}


// ---------------------------------------------------------------------------
// type_box_name — log-only stub
//
// INTENDED REAL FLOW (calibrated interactively on the rig):
//   1. Navigate to box <box_index> in HOME.
//   2. Open the box-name editor (hold ZL on the box header, or tap the box
//      name — exact navigation to be confirmed from live hardware observation).
//   3. Clear the existing name (Clear softkey or Select-All + Delete sequence).
//   4. Type <label> character by character via the HOME on-screen keyboard:
//        - Move the d-pad / left-stick to each character key and press A.
//        - Timing between presses (ms per move / ms per confirm) is calibrated
//          interactively: rename ONE box, observe, refine, then batch.
//   5. Confirm with the OK / + softkey.
//   6. (Optional) Verify by reading back the box-header text via OCR.
//
// A failed rename logs the error and continues to the next box — it does
// NOT abort the program.  Box names are cosmetic; data integrity is unaffected.
//
// CURRENT BEHAVIOR (log-only, safe on hardware):
//   Logs the box index and the label that WOULD be set.
//   No d-pad presses, no A presses, no button input of any kind.
// ---------------------------------------------------------------------------
void RenameBoxes::type_box_name(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& /*context*/,
    uint16_t box_index,
    const std::string& label
){
    // *** LOG-ONLY STUB — no keyboard interaction ***
    // Replace this body (and the /*context*/ suppress above) once the OSK
    // navigation has been calibrated on the hardware rig.
    env.log(
        "Box " + std::to_string(box_index) +
        " \xe2\x86\x92 would set name '" + label + "' [log-only, no input sent]"
    );
}


// ---------------------------------------------------------------------------
// program — entry point
// ---------------------------------------------------------------------------

void RenameBoxes::program(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    // -----------------------------------------------------------------------
    // Step 1: Load the v3 layout and build the box map.
    // -----------------------------------------------------------------------
    const std::string layout_path =
        RESOURCE_PATH() + "PokemonHome/DexTemplates/master_box_layout_v3.json";
    const std::string shiny_locked_path =
        RESOURCE_PATH() + "PokemonHome/shiny_locked.json";

    env.log("Loading v3 box layout from: " + layout_path);
    MasterBoxLayoutV3 layout = load_master_box_layout_v3(layout_path, shiny_locked_path);

    std::vector<BoxMapEntry> box_map = build_box_map(layout);
    env.log("Box map built — " + std::to_string(box_map.size()) + " section(s).");

    // -----------------------------------------------------------------------
    // Step 2: Compute the per-box label for every box in [START_BOX, START_BOX+BOX_COUNT).
    //
    // Per-box label rule (matches the §1 spec):
    //   - Multi-box sections:  "<section label> NN"  (e.g. "Shiny Dex 01")
    //   - Single-box sections: "<section label>"     (e.g. "Junk")
    //   - Buffer boxes:        "(buffer)"
    //
    // The box_map from build_box_map() gives us labeled ranges; we walk every
    // box in the requested range and find its section.
    // -----------------------------------------------------------------------
    const uint16_t start = static_cast<uint16_t>(START_BOX);
    const uint16_t count = static_cast<uint16_t>(BOX_COUNT);
    const uint16_t end   = static_cast<uint16_t>(start + count - 1u);  // inclusive

    env.log(
        "Will label boxes " + std::to_string(start) +
        " through " + std::to_string(end) + "."
    );

    for (uint16_t box = start; box <= end; ++box){
        // Find which section this box belongs to.
        std::string label;
        bool found = false;
        for (const BoxMapEntry& entry : box_map){
            if (box >= entry.box_start && box <= entry.box_end){
                const uint16_t span = static_cast<uint16_t>(entry.box_end - entry.box_start + 1u);
                if (span == 1){
                    // Single-box section: label only (no number suffix).
                    label = entry.label;
                } else {
                    // Multi-box section: label + 1-based index within the section.
                    const uint16_t idx = static_cast<uint16_t>(box - entry.box_start + 1u);
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), " %02u", static_cast<unsigned>(idx));
                    label = entry.label + buf;
                }
                found = true;
                break;
            }
        }
        if (!found){
            // Box is beyond the mapped layout — log and skip.
            env.log(
                "Box " + std::to_string(box) +
                " is outside the v3 box map; skipping."
            );
            continue;
        }

        // -----------------------------------------------------------------------
        // Step 3: "Type" the label (log-only until calibrated on the rig).
        // -----------------------------------------------------------------------
        type_box_name(env, context, box, label);
    }

    // -----------------------------------------------------------------------
    // Step 4: Done.
    // -----------------------------------------------------------------------
    env.log(
        "RenameBoxes complete (log-only scaffold).  "
        "No box names have been changed — real OSK input will be wired in "
        "after interactive calibration on the hardware rig."
    );
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}


}  // namespace PokemonHome
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

/*  Home Box Sorter Master
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonHome_BoxSorterMaster_H
#define PokemonAutomation_PokemonHome_BoxSorterMaster_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "Common/Cpp/Options/TimeDurationOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonTools/Options/LanguageOCROption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


class BoxSorterMaster_Descriptor : public SingleSwitchProgramDescriptor{
public:
    BoxSorterMaster_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class BoxSorterMaster : public SingleSwitchProgramInstance{
public:
    BoxSorterMaster();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    // Read the summary screen for a Pokémon and, if READ_IVS is enabled,
    // open the Judge view (Y button), read all six IV ratings via IvJudgeReaderScope,
    // summarize with summarize_ivs(), assign IV fields onto info, then return to summary (B).
    // If the Judge view cannot be confirmed or any stat is UnableToDetect, sets
    // info.iv_read = false and continues — does NOT abort the run.
    void read_summary_screen_with_ivs(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        CollectedPokemonInfo& info,
        Language ot_language,
        bool read_ivs,
        Language iv_language
    );

    // Walk SCAN_BOX_COUNT boxes starting at SCAN_BOX_START-1 (0-indexed), reading
    // each occupied Pokémon via read_summary_screen_with_ivs.  After each box,
    // calls write_catalogue_incremental to flush progress to disk.
    // Returns the final nav cursor position.
    [[nodiscard]] BoxCursor catalogue_boxes(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
        size_t box_count,
        size_t starting_box,
        BoxCursor nav_cursor,
        Language ot_language,
        bool read_ivs,
        Language iv_language,
        const std::string& output_path,
        size_t already_done_boxes,         // resume offset: skip this many boxes at front
        const std::vector<size_t>& saved_fingerprints,  // Part C.2: occupancy mismatch check
        bool skip_occupancy_check          // true when execute pass already started (moves_done>0)
    );

    // Scan box_idx (0-indexed relative to SCAN_BOX_START) and count occupied slots.
    // Used for resume fingerprinting: compare against saved count in JSON.
    size_t count_occupied_slots_in_box(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context
    );

    // -----------------------------------------------------------------------
    // Scan options
    // -----------------------------------------------------------------------
    SimpleIntegerOption<uint16_t> SCAN_BOX_START;
    SimpleIntegerOption<uint16_t> SCAN_BOX_COUNT;

    // -----------------------------------------------------------------------
    // OT / owner options
    // -----------------------------------------------------------------------
    OCR::LanguageOCROption OT_NAME_LANGUAGE;
    // Comma-separated list of OT names that count as "owner" (e.g. "Nicole, Cole").
    // Parsed to a lowercased set at program start.
    StringOption OWNER_OT_NAMES;

    // -----------------------------------------------------------------------
    // IV options
    // -----------------------------------------------------------------------
    BooleanCheckBoxOption READ_IVS;
    OCR::LanguageOCROption IV_LANGUAGE;
    // Routing thresholds (for Task 8 planner; stored here so the JSON carries them).
    SimpleIntegerOption<uint8_t>  COMPETITIVE_MIN31;
    SimpleIntegerOption<uint8_t>  BREEDING_MIN;
    SimpleIntegerOption<uint8_t>  BREEDING_MAX;
    SimpleIntegerOption<uint8_t>  BREEDJECT_MIN;
    SimpleIntegerOption<uint8_t>  BREEDJECT_MAX;

    // -----------------------------------------------------------------------
    // Timing options
    // -----------------------------------------------------------------------
    MillisecondsOption VIDEO_DELAY;
    MillisecondsOption GAME_DELAY;

    // -----------------------------------------------------------------------
    // Output / control options
    // -----------------------------------------------------------------------
    StringOption OUTPUT_FILE;
    BooleanCheckBoxOption DRY_RUN;
    BooleanCheckBoxOption FRESH_START;
    EventNotificationsOption NOTIFICATIONS;
};


// ---------------------------------------------------------------------------
// Free functions — catalogue JSON I/O (usable from tests or Task 8)
// ---------------------------------------------------------------------------

// Write the full catalogue to <path>.  Includes a "boxes_done" field so
// load_catalogue_resume can skip already-verified boxes.
// Also records the occupancy count for each box as a fingerprint for resume
// matching (key: "box_occupancy", an array of integers, one per box done).
void write_catalogue_incremental(
    const std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    size_t boxes_done,
    const std::string& path,
    const std::vector<size_t>& box_occupancy_fingerprints
);

// Load an existing catalogue JSON written by write_catalogue_incremental.
// On success: fills catalogue, sets boxes_done, fills box_occupancy_fingerprints,
// and returns true.
// On any error (missing file, parse failure, schema mismatch): returns false
// and leaves all out-params unchanged.
bool load_catalogue_resume(
    std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    size_t& boxes_done,
    std::vector<size_t>& box_occupancy_fingerprints,
    const std::string& path
);


}
}
}
#endif // PokemonAutomation_PokemonHome_BoxSorterMaster_H

/*  Home Box Sorter Master
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Strings/Unicode.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_StringNormalization.h"
#include "CommonTools/StartupChecks/StartProgramChecks.h"
#include "CommonTools/VisualDetectors/FrozenImageDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_BoxCursor.h"
#include "Pokemon/Pokemon_CollectedPokemonInfo.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonHome/Inference/PokemonHome_ButtonDetector.h"
#include "PokemonHome/Inference/PokemonHome_IvJudgeReader.h"
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
#include "PokemonHome_BoxNavigation.h"
#include "PokemonHome_BoxSorterMaster.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static const size_t MAX_HOME_BOXES_MASTER = 200;


// ---------------------------------------------------------------------------
// Descriptor + Stats
// ---------------------------------------------------------------------------

BoxSorterMaster_Descriptor::BoxSorterMaster_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonHome:BoxSorterMaster",
        STRING_POKEMON + " Home", "Master Box Sorter",
        "Programs/PokemonHome/BoxSorterMaster.html",
        "Catalogue all " + STRING_POKEMON + " across your HOME boxes with IV data, "
        "then (Task 8) sort them into their designated category boxes.",
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

struct BoxSorterMaster_Descriptor::Stats : public StatsTracker{
    Stats()
        : boxes_scanned(m_stats["Boxes Scanned"])
        , pkmn(m_stats["Pokemon"])
        , empty(m_stats["Empty Slots"])
        , iv_read(m_stats["IV Reads"])
        , iv_failed(m_stats["IV Read Failures"])
    {
        m_display_order.emplace_back(Stat("Boxes Scanned"));
        m_display_order.emplace_back(Stat("Pokemon"));
        m_display_order.emplace_back(Stat("Empty Slots"));
        m_display_order.emplace_back(Stat("IV Reads"));
        m_display_order.emplace_back(Stat("IV Read Failures"));
    }
    std::atomic<uint64_t>& boxes_scanned;
    std::atomic<uint64_t>& pkmn;
    std::atomic<uint64_t>& empty;
    std::atomic<uint64_t>& iv_read;
    std::atomic<uint64_t>& iv_failed;
};

std::unique_ptr<StatsTracker> BoxSorterMaster_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BoxSorterMaster::BoxSorterMaster()
    : SCAN_BOX_START(
        "<b>Scan Box Start:</b><br>First box to catalogue (1-indexed).",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, (uint16_t)MAX_HOME_BOXES_MASTER
    )
    , SCAN_BOX_COUNT(
        "<b>Scan Box Count:</b><br>Number of boxes to catalogue.",
        LockMode::LOCK_WHILE_RUNNING,
        10, 1, (uint16_t)MAX_HOME_BOXES_MASTER
    )
    , OT_NAME_LANGUAGE(
        "<b>OT Name Language:</b><br>Language used to OCR trainer names. "
        "Set to None to skip OT name reading.",
        LanguageSet{
            Language::English,
            Language::Japanese,
            Language::Spanish,
            Language::French,
            Language::German,
            Language::Italian,
            Language::Korean,
            Language::ChineseSimplified,
            Language::ChineseTraditional,
        },
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , OWNER_OT_NAMES(
        false,
        "<b>Owner OT Names:</b><br>Comma-separated list of OT names that count as \"yours\" "
        "(e.g. \"Nicole, Cole\"). Used by the router in Task 8. Case-insensitive.",
        LockMode::LOCK_WHILE_RUNNING,
        "Nicole, Cole",
        "e.g. Nicole, Cole"
    )
    , READ_IVS(
        "<b>Read IVs:</b><br>Press Y on each summary screen to open the Judge view and "
        "record IV ratings. Adds ~2 seconds per Pokémon.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , IV_LANGUAGE(
        "<b>IV Judge Language:</b><br>Language of the IV Judge text labels.",
        LanguageSet{
            Language::English,
            Language::Japanese,
            Language::Spanish,
            Language::French,
            Language::German,
            Language::Italian,
            Language::Korean,
            Language::ChineseSimplified,
            Language::ChineseTraditional,
        },
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    // IV routing thresholds — stored here so they travel with the JSON catalogue
    // and Task 8 can read them back without asking the user again.
    , COMPETITIVE_MIN31(
        "<b>Competitive: min 31-IV stats:</b><br>"
        "A " + STRING_POKEMON + " with this many Best-judge stats (or more) is routed Competitive.",
        LockMode::LOCK_WHILE_RUNNING,
        6, 0, 6
    )
    , BREEDING_MIN(
        "<b>Breeding IV range — min:</b><br>Inclusive lower bound for Best-stat count to route Breeding.",
        LockMode::LOCK_WHILE_RUNNING,
        3, 0, 6
    )
    , BREEDING_MAX(
        "<b>Breeding IV range — max:</b><br>Inclusive upper bound for Best-stat count to route Breeding.",
        LockMode::LOCK_WHILE_RUNNING,
        5, 0, 6
    )
    , BREEDJECT_MIN(
        "<b>Breedject IV range — min:</b><br>Inclusive lower bound for Best-stat count to route Breedject.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 0, 6
    )
    , BREEDJECT_MAX(
        "<b>Breedject IV range — max:</b><br>Inclusive upper bound for Best-stat count to route Breedject.",
        LockMode::LOCK_WHILE_RUNNING,
        2, 0, 6
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
    , OUTPUT_FILE(
        false,
        "<b>Output File:</b><br>JSON file basename for the catalogue. "
        "The program writes <basename>.json after each box.",
        LockMode::LOCK_WHILE_RUNNING,
        "home_catalogue",
        "home_catalogue"
    )
    , DRY_RUN(
        "<b>Dry Run:</b><br>Catalogue and write JSON without moving any " + STRING_POKEMON + ". "
        "Task 8 (execute pass) is the only phase that moves " + STRING_POKEMON + ".",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , FRESH_START(
        "<b>Fresh Start:</b><br>Ignore any existing catalogue file and re-scan all boxes from scratch. "
        "When unchecked the program tries to resume from the last saved position.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH
    })
{
    PA_ADD_OPTION(SCAN_BOX_START);
    PA_ADD_OPTION(SCAN_BOX_COUNT);
    PA_ADD_OPTION(OT_NAME_LANGUAGE);
    PA_ADD_OPTION(OWNER_OT_NAMES);
    PA_ADD_OPTION(READ_IVS);
    PA_ADD_OPTION(IV_LANGUAGE);
    PA_ADD_OPTION(COMPETITIVE_MIN31);
    PA_ADD_OPTION(BREEDING_MIN);
    PA_ADD_OPTION(BREEDING_MAX);
    PA_ADD_OPTION(BREEDJECT_MIN);
    PA_ADD_OPTION(BREEDJECT_MAX);
    PA_ADD_OPTION(VIDEO_DELAY);
    PA_ADD_OPTION(GAME_DELAY);
    PA_ADD_OPTION(OUTPUT_FILE);
    PA_ADD_OPTION(DRY_RUN);
    PA_ADD_OPTION(FRESH_START);
    PA_ADD_OPTION(NOTIFICATIONS);
}


// ---------------------------------------------------------------------------
// read_summary_screen_with_ivs
// ---------------------------------------------------------------------------

void BoxSorterMaster::read_summary_screen_with_ivs(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    CollectedPokemonInfo& info,
    Language ot_language,
    bool read_ivs,
    Language iv_language
){
    // Always read the base summary screen first (also advances to next slot via R).
    // read_summary_screen presses R at the end to move to the next summary.
    // We must press R *after* the optional IV read, so we do not call
    // read_summary_screen and instead open the screen manually here for IV reads.
    // However, read_summary_screen in BoxNavigation.cpp already handles the full
    // summary reading AND presses R at the end.
    //
    // Strategy: call read_summary_screen for all the standard fields (it presses R
    // at the end to advance to the next Pokémon).  For IV reads we must intercept
    // before that R press.  Since we cannot easily split read_summary_screen, we
    // adopt a two-phase approach:
    //   Phase 1: read IVs (press Y, read, press B to return to summary).
    //   Phase 2: call read_summary_screen which reads the summary and then presses R.
    // This means the IV read happens before the standard summary read, which is fine
    // because both look at the same Pokémon.

    BoxSorterMaster_Descriptor::Stats& stats =
        env.current_stats<BoxSorterMaster_Descriptor::Stats>();

    if (read_ivs && iv_language != Language::None){
        // Press Y to open the Judge/stat view from the summary screen.
        pbf_press_button(context, BUTTON_Y, 80ms, 500ms);
        context.wait_for_all_requests();

        // Wait for the Judge screen to stabilise using a FrozenImageDetector
        // on the stat area (same approach as read_summary_screen does for the
        // summary screen itself).
        {
            VideoOverlaySet vo(env.console);
            FrozenImageDetector judge_frozen(
                COLOR_GREEN,
                { 0.820, 0.185, 0.120, 0.065 },   // HP stat label area on Judge screen
                Milliseconds(80),
                20
            );
            judge_frozen.make_overlays(vo);
            int ret = wait_until(env.console, context, Seconds(5), { judge_frozen });
            if (ret != 0){
                // Judge screen not detected — log and skip IV read for this Pokémon.
                env.log("BoxSorterMaster: Judge screen not confirmed — skipping IV read for this slot.", COLOR_YELLOW);
                info.iv_read = false;
                stats.iv_failed++;
                env.update_stats();
                // Return to summary screen.
                pbf_press_button(context, BUTTON_B, 80ms, 400ms);
                context.wait_for_all_requests();
            }
            else{
                // Judge screen confirmed — snapshot and read.
                VideoSnapshot screen = env.console.video().snapshot();
                IvJudgeReaderScope scope(env.console.overlay(), iv_language);
                IvJudgeReader::Results r = scope.read(env.console.logger(), screen);
                env.log("IV Judge results: " + r.to_string());

                IVSummary s = summarize_ivs(r);

                if (s.read){
                    info.iv_read        = true;
                    info.iv_best_count  = s.best_count;
                    info.iv_total_estimate = s.total_estimate;
                    info.iv_perfect     = s.perfect;
                    stats.iv_read++;
                    env.update_stats();
                    env.log(
                        "IV summary: best=" + std::to_string(s.best_count) +
                        " total=" + std::to_string(s.total_estimate) +
                        " perfect=" + std::to_string(s.perfect)
                    );
                }
                else{
                    info.iv_read = false;
                    stats.iv_failed++;
                    env.update_stats();
                    env.log("BoxSorterMaster: One or more IV stats UnableToDetect — marking iv_read=false.", COLOR_YELLOW);
                }

                // Return to summary screen.
                pbf_press_button(context, BUTTON_B, 80ms, 400ms);
                context.wait_for_all_requests();
            }
        }
    }

    // Read standard summary fields (dex #, shiny, gmax, ball, gender, OT, types,
    // origin mark) and advance to the next summary screen via R.
    read_summary_screen(env, context, info, ot_language);
}


// ---------------------------------------------------------------------------
// count_occupied_slots_in_box  (used for resume fingerprinting)
// ---------------------------------------------------------------------------

size_t BoxSorterMaster::count_occupied_slots_in_box(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    VideoSnapshot screen = env.console.video().snapshot();
    size_t count = 0;
    for (size_t row = 0; row < BOX_ROWS; row++){
        for (size_t col = 0; col < BOX_COLS; col++){
            ImageFloatBox slot_box(
                0.06 + (0.072 * static_cast<double>(col)),
                0.2  + (0.105 * static_cast<double>(row)),
                0.03, 0.057
            );
            int stddev = static_cast<int>(image_stddev(extract_box_reference(screen, slot_box)).sum());
            if (stddev >= 10){ count++; }
        }
    }
    (void)context; // not currently needed but kept for future use
    return count;
}


// ---------------------------------------------------------------------------
// catalogue_boxes  — the main catalogue pass
// ---------------------------------------------------------------------------

[[nodiscard]] BoxCursor BoxSorterMaster::catalogue_boxes(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    size_t box_count,
    size_t starting_box,           // 0-indexed absolute box number
    BoxCursor nav_cursor,
    Language ot_language,
    bool read_ivs,
    Language iv_language,
    const std::string& output_path,
    size_t already_done_boxes
){
    BoxSorterMaster_Descriptor::Stats& stats =
        env.current_stats<BoxSorterMaster_Descriptor::Stats>();

    VideoOverlaySet video_overlay_set(env.console);
    BoxViewWatcher box_view_watcher(&env.console.overlay());
    SummaryScreenWatcher summary_screen_watcher(&env.console.overlay());

    // Catalogue already holds data for already_done_boxes * 30 slots (loaded
    // from JSON).  We start reading from box already_done_boxes.

    // No-sort preferences for populate pass (we do not sort; we just catalogue).
    const std::vector<SortingRule> no_sort_prefs = {{ SortingRuleType::DexNo, false }};

    // box_occupancy_fingerprints is rebuilt here so write_catalogue_incremental
    // can persist it.  For resumed boxes we already have the counts from the
    // load call (they are passed in via catalogue's existing slots).
    // We store them separately.
    std::vector<size_t> box_fingerprints;
    box_fingerprints.reserve(box_count);

    // For each box we either already have the data (resume) or scan fresh.
    for (size_t box_rel = 0; box_rel < box_count; box_rel++){
        const size_t abs_box = starting_box + box_rel;

        // Navigate to this box.
        if (box_rel == 0){
            // Move cursor to the starting box and reset to first slot.
            if (abs_box != nav_cursor.box){
                BoxCursor dest(abs_box, 0, 0);
                nav_cursor = move_cursor_to(env, context, nav_cursor, dest, GAME_DELAY);
            }
            env.log("BoxSorterMaster: Moving cursor to first slot.");
            if (!go_to_first_slot(env, context, VIDEO_DELAY)){
                throw UserSetupError(
                    env.logger(),
                    "BoxSorterMaster: Could not move cursor to the first slot — "
                    "consider adjusting delay."
                );
            }
        }
        else{
            BoxCursor dest(abs_box, 0, 0);
            nav_cursor = move_cursor_to(env, context, nav_cursor, dest, GAME_DELAY);
        }

        context.wait_for_all_requests();

        const std::string log_msg =
            "BoxSorterMaster: Cataloguing box " +
            std::to_string(box_rel + 1) + "/" + std::to_string(box_count) +
            " (HOME box " + std::to_string(abs_box + 1) + ")";
        env.log(log_msg);
        env.console.overlay().add_log(log_msg);

        // Count occupancy for resume fingerprinting.
        const size_t occupancy = count_occupied_slots_in_box(env, context);
        box_fingerprints.push_back(occupancy);

        if (box_rel < already_done_boxes){
            // This box was loaded from the JSON resume.  Re-check occupancy
            // to decide whether the saved data is still valid.
            // The saved fingerprint was loaded into box_fingerprints during
            // load_catalogue_resume; we compare against the live count.
            // (already_done_boxes is the count of boxes whose data we trust.)
            // Since we are before already_done_boxes, skip re-reading.
            env.log(
                "BoxSorterMaster: Box " + std::to_string(box_rel + 1) +
                " already in catalogue (resume) — live occupancy=" +
                std::to_string(occupancy) + ", skipping re-read."
            );
            stats.boxes_scanned++;
            env.update_stats();
            continue;
        }

        // ---- Fresh scan for this box ----

        // find_occupied_slots_in_box appends 30 slots to catalogue.
        const std::array<size_t, 2> first_slot =
            find_occupied_slots_in_box(env, context, catalogue, no_sort_prefs);

        // Update stats for empty / occupied slots just appended.
        for (size_t row = 0; row < BOX_ROWS; row++){
            for (size_t col = 0; col < BOX_COLS; col++){
                const size_t g = to_global_index(box_rel, row, col);
                if (!catalogue[g].has_value()){ stats.empty++; }
                else                          { stats.pkmn++;  }
            }
        }
        env.update_stats();

        if (first_slot[0] != SIZE_MAX){
            // Move cursor to the first occupied slot in the box.
            BoxCursor dest_first(abs_box, first_slot[0], first_slot[1]);
            nav_cursor = move_cursor_to(env, context, nav_cursor, dest_first, GAME_DELAY);

            env.add_overlay_log("Reading summaries...");

            // Open the summary screen.
            pbf_press_button(context, BUTTON_A, 80ms, GAME_DELAY);
            context.wait_for_all_requests();
            video_overlay_set.clear();
            pbf_press_dpad(context, DPAD_DOWN, 80ms, GAME_DELAY);
            pbf_press_button(context, BUTTON_A, 80ms, 100ms);
            context.wait_for_all_requests();

            int ret = wait_until(env.console, context, Seconds(5), { summary_screen_watcher });
            if (ret != 0){
                OperationFailedException::fire(
                    ErrorReport::SEND_ERROR_REPORT,
                    "BoxSorterMaster: Summary screen not found after 5 sec",
                    env.console
                );
            }

            // Cycle through every slot in this box and read summaries.
            for (size_t row = 0; row < BOX_ROWS; row++){
                for (size_t col = 0; col < BOX_COLS; col++){
                    const size_t g = to_global_index(box_rel, row, col);
                    if (!catalogue[g].has_value()){ continue; }

                    read_summary_screen_with_ivs(
                        env, context,
                        catalogue[g].value(),
                        ot_language, read_ivs, iv_language
                    );
                }
            }

            // Log what we found in this box.
            {
                std::ostringstream ss;
                ss << "\n";
                for (size_t row = 0; row < BOX_ROWS; row++){
                    for (size_t col = 0; col < BOX_COLS; col++){
                        ss << "[" << row << ", " << col << "]: "
                           << catalogue[to_global_index(box_rel, row, col)]
                           << "\n";
                    }
                }
                env.log(ss.str());
            }

            // Return to box view.
            pbf_press_button(context, BUTTON_B, 80ms, 100ms);
            context.wait_for_all_requests();
            ret = wait_until(env.console, context, Seconds(5), { box_view_watcher });
            if (ret != 0){
                OperationFailedException::fire(
                    ErrorReport::SEND_ERROR_REPORT,
                    "BoxSorterMaster: Box view not found after leaving summary",
                    env.console
                );
            }
            video_overlay_set.clear();
        }

        stats.boxes_scanned++;
        env.update_stats();

        // Write incremental checkpoint after every box.
        write_catalogue_incremental(catalogue, box_rel + 1, output_path, box_fingerprints);
        env.log("BoxSorterMaster: Wrote incremental catalogue after box " +
                std::to_string(box_rel + 1));

        video_overlay_set.clear();
    }

    return nav_cursor;
}


// ---------------------------------------------------------------------------
// program()
// ---------------------------------------------------------------------------

void BoxSorterMaster::program(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    StartProgramChecks::check_performance_class_wired_or_wireless(context);

    const size_t scan_start  = static_cast<size_t>(SCAN_BOX_START - 1);  // 0-indexed
    const size_t scan_count  = static_cast<size_t>(SCAN_BOX_COUNT);
    const bool   read_ivs    = static_cast<bool>(READ_IVS);
    const bool   fresh_start = static_cast<bool>(FRESH_START);
    const bool   dry_run     = static_cast<bool>(DRY_RUN);

    if (scan_start + scan_count > MAX_HOME_BOXES_MASTER){
        throw UserSetupError(
            env.logger(),
            "BoxSorterMaster: Scan range exceeds maximum HOME box count."
        );
    }

    const Language ot_lang = static_cast<Language>(OT_NAME_LANGUAGE);
    const Language iv_lang = static_cast<Language>(IV_LANGUAGE);

    const std::string output_path =
        static_cast<std::string>(OUTPUT_FILE) + ".json";

    if (dry_run){
        env.log("BoxSorterMaster: DRY RUN mode — cataloguing only, no moves will be made.");
    }

    // -----------------------------------------------------------------------
    // Parse OWNER_OT_NAMES into a lowercased set (Task 8 will use this).
    // We log it here so the operator can verify before a long run.
    // -----------------------------------------------------------------------
    {
        std::string raw_owners = static_cast<std::string>(OWNER_OT_NAMES);
        std::string lower;
        lower.reserve(raw_owners.size());
        std::transform(raw_owners.begin(), raw_owners.end(),
                       std::back_inserter(lower),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        std::ostringstream oss;
        oss << "BoxSorterMaster: Owner OT names (lowercased): [";
        bool first = true;
        std::istringstream token_stream(lower);
        std::string token;
        while (std::getline(token_stream, token, ',')){
            // Trim whitespace.
            auto start = token.find_first_not_of(" \t");
            auto end   = token.find_last_not_of(" \t");
            if (start == std::string::npos){ continue; }
            token = token.substr(start, end - start + 1);
            if (!first){ oss << ", "; }
            oss << "\"" << token << "\"";
            first = false;
        }
        oss << "]";
        env.log(oss.str());
    }

    // -----------------------------------------------------------------------
    // Catalogue with resume support
    // -----------------------------------------------------------------------

    std::vector<std::optional<CollectedPokemonInfo>> catalogue;
    size_t already_done_boxes   = 0;
    std::vector<size_t> saved_fingerprints;

    if (!fresh_start){
        bool loaded = load_catalogue_resume(
            catalogue,
            already_done_boxes,
            saved_fingerprints,
            output_path
        );
        if (loaded){
            env.log(
                "BoxSorterMaster: Resuming from existing catalogue — " +
                std::to_string(already_done_boxes) + " boxes already recorded."
            );
        }
        else{
            env.log("BoxSorterMaster: No valid catalogue found — starting fresh.");
            already_done_boxes = 0;
        }
    }

    exit_menus(env, context, VIDEO_DELAY.get());

    BoxCursor nav_cursor(scan_start, 0, 0);

    // Run the catalogue pass.
    nav_cursor = catalogue_boxes(
        env, context,
        catalogue,
        scan_count,
        scan_start,
        nav_cursor,
        ot_lang,
        read_ivs,
        iv_lang,
        output_path,
        already_done_boxes
    );

    env.log("BoxSorterMaster: Catalogue pass complete. JSON written to: " + output_path);

    // -----------------------------------------------------------------------
    // TODO (Task 8): Planner pass — build a move plan from catalogue + layout.
    //
    // RouterConfig cfg;
    // cfg.owner_ot_names = parsed_owner_set;
    // cfg.competitive_min31 = COMPETITIVE_MIN31;
    // cfg.breeding_range    = {BREEDING_MIN, BREEDING_MAX};
    // cfg.breedject_range   = {BREEDJECT_MIN, BREEDJECT_MAX};
    // cfg.legendary         = &layout.legendary;
    // ...
    // std::vector<MoveOp> plan = build_sort_plan(catalogue, layout, cfg);
    // save_plan_to_json(plan, plan_path);
    //
    // TODO (Task 8): Execute pass — execute plan if !DRY_RUN.
    // -----------------------------------------------------------------------

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}


// ---------------------------------------------------------------------------
// write_catalogue_incremental
// ---------------------------------------------------------------------------

void write_catalogue_incremental(
    const std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    size_t boxes_done,
    const std::string& path,
    const std::vector<size_t>& box_occupancy_fingerprints
){
    JsonObject root;
    root["boxes_done"] = static_cast<int64_t>(boxes_done);

    // Persist the occupancy fingerprints (one count per completed box).
    JsonArray fp_array;
    for (size_t n : box_occupancy_fingerprints){
        fp_array.push_back(static_cast<int64_t>(n));
    }
    root["box_occupancy"] = std::move(fp_array);

    // Persist all Pokemon slots (same schema as save_boxes_data_to_json).
    JsonArray slots;
    for (size_t slot_idx = 0; slot_idx < catalogue.size(); slot_idx++){
        BoxCursor cursor(slot_idx);
        JsonObject entry;
        entry["index"]  = static_cast<int64_t>(slot_idx);
        entry["box"]    = static_cast<int64_t>(cursor.box);
        entry["row"]    = static_cast<int64_t>(cursor.row);
        entry["column"] = static_cast<int64_t>(cursor.column);

        const auto& pkmn = catalogue[slot_idx];
        if (pkmn.has_value()){
            // NOTE edit when adding new struct members
            entry["name"]           = pkmn->name_slug;
            entry["dex"]            = static_cast<int64_t>(pkmn->dex_number);
            entry["shiny"]          = pkmn->shiny;
            entry["gmax"]           = pkmn->gmax;
            entry["alpha"]          = pkmn->alpha;
            entry["ball"]           = pkmn->ball_slug;
            entry["gender"]         = gender_to_string(pkmn->gender);
            entry["ot_id"]          = static_cast<int64_t>(pkmn->ot_id);
            entry["ot_name"]        = pkmn->ot_name;
            entry["primary_type"]   = POKEMON_TYPE_SLUGS().get_string(pkmn->primary_type);
            entry["secondary_type"] = POKEMON_TYPE_SLUGS().get_string(pkmn->secondary_type);
            entry["tera_type"]      = POKEMON_TERA_TYPE_SLUGS().get_string(pkmn->tera_type);
            entry["origin_mark"]    = ORIGIN_MARK_SLUGS().get_string(pkmn->origin_mark);
            entry["iv_read"]        = pkmn->iv_read;
            entry["iv_best_count"]  = static_cast<int64_t>(pkmn->iv_best_count);
            entry["iv_total_estimate"] = static_cast<int64_t>(pkmn->iv_total_estimate);
            entry["iv_perfect"]     = pkmn->iv_perfect;
        }
        slots.push_back(std::move(entry));
    }
    root["catalogue"] = std::move(slots);

    root.dump(path);
}


// ---------------------------------------------------------------------------
// load_catalogue_resume
// ---------------------------------------------------------------------------

bool load_catalogue_resume(
    std::vector<std::optional<CollectedPokemonInfo>>& catalogue,
    size_t& boxes_done,
    std::vector<size_t>& box_occupancy_fingerprints,
    const std::string& path
){
    // Attempt to load JSON.
    JsonValue root_val;
    try{
        root_val = load_json_file(path);
    }
    catch (...){
        return false;
    }

    if (!root_val.is_object()){ return false; }
    const JsonObject* root = root_val.to_object();
    if (!root){ return false; }

    // Read boxes_done.
    {
        const JsonValue* bval = root->get_value("boxes_done");
        if (!bval || !bval->is_integer()){ return false; }
        const size_t bkdone_inner = static_cast<size_t>(bval->to_integer_default(0));
        // Store for later commit.
        boxes_done = bkdone_inner;
    }
    // Re-read as local var for clarity (boxes_done was just set above).
    const size_t bkdone = boxes_done;

    // Read box_occupancy fingerprints.
    std::vector<size_t> fp;
    {
        const JsonArray* fparr = root->get_array("box_occupancy");
        if (fparr){
            for (const JsonValue& v : *fparr){
                fp.push_back(static_cast<size_t>(v.to_integer_default(0)));
            }
        }
    }

    // Read catalogue slots.
    const JsonArray* carr = root->get_array("catalogue");
    if (!carr){ return false; }

    std::vector<std::optional<CollectedPokemonInfo>> loaded;
    loaded.reserve(carr->size());

    for (const JsonValue& entry_val : *carr){
        if (!entry_val.is_object()){
            loaded.push_back(std::nullopt);
            continue;
        }
        const JsonObject* obj = entry_val.to_object();
        if (!obj){
            loaded.push_back(std::nullopt);
            continue;
        }

        // If there's a "name" key the slot is occupied.
        const std::string* name_ptr = obj->get_string("name");
        if (!name_ptr || name_ptr->empty()){
            loaded.push_back(std::nullopt);
            continue;
        }

        CollectedPokemonInfo info;
        info.name_slug         = *name_ptr;
        info.dex_number        = static_cast<uint16_t>(obj->get_integer_default("dex", 0));
        info.shiny             = obj->get_boolean_default("shiny", false);
        info.gmax              = obj->get_boolean_default("gmax", false);
        info.alpha             = obj->get_boolean_default("alpha", false);
        info.ball_slug         = obj->get_string_default("ball", "");
        info.ot_id             = static_cast<uint32_t>(obj->get_integer_default("ot_id", 0));
        info.ot_name           = obj->get_string_default("ot_name", "");
        info.iv_read           = obj->get_boolean_default("iv_read", false);
        info.iv_best_count     = static_cast<uint8_t>(obj->get_integer_default("iv_best_count", 0));
        info.iv_total_estimate = static_cast<uint16_t>(obj->get_integer_default("iv_total_estimate", 0));
        info.iv_perfect        = obj->get_boolean_default("iv_perfect", false);

        // Note: primary_type, secondary_type, tera_type, origin_mark, gender are not
        // round-tripped via enum parsing here to keep the loader simple and avoid
        // pulling in all the enum slug maps.  They default to NONE/Genderless.
        // Task 8's planner only needs dex#, iv_best_count, iv_read, ot_name, shiny.

        loaded.push_back(std::move(info));
    }

    // Commit.  (boxes_done was already written into the out-param above.)
    catalogue                  = std::move(loaded);
    box_occupancy_fingerprints = std::move(fp);
    (void)bkdone; // already committed to boxes_done in the block above
    return true;
}


}
}
}

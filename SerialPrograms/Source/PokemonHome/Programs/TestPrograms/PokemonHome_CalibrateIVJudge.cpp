/*  Calibrate IV Judge
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <sstream>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/FrozenImageDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonHome/Inference/PokemonHome_IvJudgeReader.h"
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
#include "PokemonHome/Inference/PokemonHome_MovesReader.h"
#include "PokemonHome/Inference/PokemonHome_SummaryExtrasReader.h"
#include "PokemonHome_CalibrateIVJudge.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;


CalibrateIVJudge_Descriptor::CalibrateIVJudge_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonHome:CalibrateIVJudge",
        STRING_POKEMON + " Home", "Calibrate IV Judge",
        "",
        "Open a " + STRING_POKEMON + "'s summary screen in HOME, then run this program. "
        "It presses Y to enter the Judge/stat view, reads all six IV ratings, logs the "
        "results, and dumps each stat crop for offline coordinate tuning.",
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

CalibrateIVJudge::CalibrateIVJudge()
    : LANGUAGE(
        "<b>Game Language:</b>",
        IV_READER().languages(),
        LockMode::LOCK_WHILE_RUNNING
    )
    , READ_MOVES(
        "<b>Read Moves:</b><br>After the extras snapshot, navigate to the moves screen "
        "(BUTTON_R) and dump the four move-slot crops for coordinate calibration.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
{
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(READ_MOVES);
}

void CalibrateIVJudge::program(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    // Press Y to open the Judge/stat view from the summary screen.
    pbf_press_button(context, BUTTON_Y, 80ms, 500ms);
    context.wait_for_all_requests();

    IvJudgeReaderScope scope(env.console.overlay(), LANGUAGE);
    VideoSnapshot screen = env.console.video().snapshot();

    Pokemon::IvJudgeReader::Results r = scope.read(env.console.logger(), screen);
    env.log("IV Judge results: " + r.to_string());

    IVSummary s = summarize_ivs(r);
    env.log(
        "IV summary: read=" + std::to_string(s.read) +
        " best_count=" + std::to_string(s.best_count) +
        " total_est=" + std::to_string(s.total_estimate) +
        " perfect=" + std::to_string(s.perfect)
    );

    // Dump each stat crop for offline coordinate tuning.
    int i = 0;
    for (const ImageViewRGB32& img : scope.dump_images(screen)){
        dump_image(env.console, ProgramInfo(), "CalibrateIVJudge_stat" + std::to_string(i++), img);
    }

    // ---- Summary extras (ability / nature / held item) ----
    // Still on the summary screen — no navigation needed.
    {
        VideoOverlaySet vo(env.console);
        make_summary_extras_overlays(vo);

        VideoSnapshot extras_screen = env.console.video().snapshot();
        SummaryExtras extras = read_summary_extras(
            env.console, extras_screen, static_cast<Language>(LANGUAGE)
        );

        env.log("CalibrateExtras: ability="   + extras.ability_slug);
        env.log("CalibrateExtras: nature="    + extras.nature);
        env.log("CalibrateExtras: held_item=" + extras.held_item_slug);

        // Dump the three crop images for offline coordinate tuning.
        dump_image(
            env.console, ProgramInfo(), "CalibrateExtras_ability",
            extract_box_reference(extras_screen, ABILITY_BOX)
        );
        dump_image(
            env.console, ProgramInfo(), "CalibrateExtras_nature",
            extract_box_reference(extras_screen, NATURE_BOX)
        );
        dump_image(
            env.console, ProgramInfo(), "CalibrateExtras_held_item",
            extract_box_reference(extras_screen, HELD_ITEM_BOX)
        );
    }

    // ---- Moves screen (guarded by READ_MOVES option) ----
    if (READ_MOVES){
        // Navigate: summary → moves screen.
        // Mirrors BoxSorterMaster Phase 3 exactly — BUTTON_R, same timing.
        pbf_press_button(context, BUTTON_R, 80ms, 500ms);
        context.wait_for_all_requests();

        // Wait for the moves screen to stabilise (same FrozenImageDetector
        // approach and crop area used by BoxSorterMaster).
        {
            VideoOverlaySet vo(env.console);
            FrozenImageDetector moves_frozen(
                COLOR_GREEN,
                { 0.62, 0.22, 0.28, 0.06 },   // Move 1 slot area — calibrate on rig
                Milliseconds(80),
                20
            );
            moves_frozen.make_overlays(vo);
            int ret = wait_until(env.console, context, Seconds(5), { moves_frozen });
            if (ret != 0){
                env.log(
                    "CalibrateIVJudge: Moves screen not confirmed within 5 s — skipping moves dump.",
                    COLOR_YELLOW
                );
            }
            else{
                // Moves screen confirmed — snapshot, read, and dump crops.
                VideoSnapshot moves_screen = env.console.video().snapshot();
                MovesReaderScope moves_scope(env.console.overlay(), static_cast<Language>(LANGUAGE));

                std::vector<std::string> move_slugs =
                    moves_scope.read(env.console.logger(), moves_screen);

                {
                    std::ostringstream oss;
                    oss << "CalibrateMoves: [";
                    for (size_t j = 0; j < move_slugs.size(); j++){
                        if (j > 0){ oss << ", "; }
                        oss << move_slugs[j];
                    }
                    oss << "]";
                    env.log(oss.str());
                }

                // Dump each of the four move-slot crops.
                int m = 0;
                for (const ImageViewRGB32& crop : moves_scope.dump_images(moves_screen)){
                    dump_image(
                        env.console, ProgramInfo(),
                        "CalibrateMoves_stat" + std::to_string(m++),
                        crop
                    );
                }
            }

            // Return to summary screen — mirrors BoxSorterMaster.
            pbf_press_button(context, BUTTON_B, 80ms, 400ms);
            context.wait_for_all_requests();
        }
    }
}


}
}
}

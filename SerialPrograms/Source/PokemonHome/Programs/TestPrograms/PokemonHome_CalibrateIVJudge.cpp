/*  Calibrate IV Judge
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonHome/Inference/PokemonHome_IvJudgeReader.h"
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
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
{
    PA_ADD_OPTION(LANGUAGE);
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
}


}
}
}

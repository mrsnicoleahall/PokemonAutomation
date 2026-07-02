/*  Pokemon Home Panels
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/GlobalSettingsPanel.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonHome_Panels.h"

#include "Programs/PokemonHome_PageSwap.h"
#include "Programs/PokemonHome_BoxSorter.h"
#include "Programs/PokemonHome_BoxSorterLivingDex.h"
#include "Programs/PokemonHome_BoxSorterMaster.h"

#include "Programs/PokemonHome_GenerateNameOCR.h"
#include "Programs/PokemonHome_RenameBoxes.h"
#include "Programs/TestPrograms/PokemonHome_CalibrateIVJudge.h"
#include "Programs/TestPrograms/PokemonHome_ReadSummaryScreen.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{



PanelListFactory::PanelListFactory()
    : PanelListDescriptor(Pokemon::STRING_POKEMON + " Home")
{}

std::vector<PanelEntry> PanelListFactory::make_panels() const{
    std::vector<PanelEntry> ret;

//    ret.emplace_back("---- Settings ----");
//    ret.emplace_back(make_settings<GameSettings_Descriptor, GameSettingsPanel>());
    ret.emplace_back("---- General ----");
    ret.emplace_back(make_single_switch_program<PokemonHome::PageSwap_Descriptor, PokemonHome::PageSwap>());
    ret.emplace_back(make_single_switch_program<PokemonHome::BoxSorter_Descriptor, PokemonHome::BoxSorter>());
    if (IS_BETA_VERSION || PreloadSettings::instance().DEVELOPER_MODE){
        ret.emplace_back(make_single_switch_program<PokemonHome::BoxSorterLivingDex_Descriptor, PokemonHome::BoxSorterLivingDex>());
        ret.emplace_back(make_single_switch_program<PokemonHome::BoxSorterMaster_Descriptor, PokemonHome::BoxSorterMaster>());
    }
//    ret.emplace_back("---- Trading ----");

//    ret.emplace_back("---- Farming ----");

//    ret.emplace_back("---- Shiny Hunting ----");

    if (PreloadSettings::instance().DEVELOPER_MODE){
        ret.emplace_back("---- Developer Tools ----");
        ret.emplace_back(make_single_switch_program<PokemonHome::GenerateNameOCRData_Descriptor, PokemonHome::GenerateNameOCRData>());
        ret.emplace_back(make_single_switch_program<PokemonHome::CalibrateIVJudge_Descriptor, PokemonHome::CalibrateIVJudge>());
        ret.emplace_back(make_single_switch_program<PokemonHome::ReadSummaryScreen_Descriptor, PokemonHome::ReadSummaryScreen>());
        // Scaffold: box-renamer.  Log-only until OSK navigation is calibrated
        // interactively on the hardware rig.  Separate from the sorter.
        ret.emplace_back(make_single_switch_program<PokemonHome::RenameBoxes_Descriptor, PokemonHome::RenameBoxes>());
    }

    return ret;
}




}
}
}

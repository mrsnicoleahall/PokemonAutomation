/*  Calibrate IV Judge
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonHome_CalibrateIVJudge_H
#define PokemonAutomation_PokemonHome_CalibrateIVJudge_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "CommonTools/Options/LanguageOCROption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

class CalibrateIVJudge_Descriptor : public SingleSwitchProgramDescriptor{
public:
    CalibrateIVJudge_Descriptor();
};

class CalibrateIVJudge : public SingleSwitchProgramInstance{
public:
    CalibrateIVJudge();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    OCR::LanguageOCROption LANGUAGE;
    BooleanCheckBoxOption READ_MOVES;
};

}
}
}
#endif

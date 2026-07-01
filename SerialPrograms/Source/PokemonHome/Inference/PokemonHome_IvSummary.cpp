/*  PokemonHome IV Summary
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon/Pokemon_IvJudge.h"
#include "PokemonHome_IvSummary.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

using Pokemon::IvJudgeValue;


uint8_t iv_value_midpoint(IvJudgeValue v){
    switch (v){
        case IvJudgeValue::Best:
        case IvJudgeValue::HyperTrained: return 31;
        case IvJudgeValue::Fantastic:    return 30;
        case IvJudgeValue::VeryGood:     return 27;
        case IvJudgeValue::PrettyGood:   return 20;
        case IvJudgeValue::Decent:       return 8;
        default:                         return 0;  // NoGood, UnableToDetect
    }
}

IVSummary summarize_ivs(const Pokemon::IvJudgeReader::Results& r){
    const IvJudgeValue stats[6] = {r.hp, r.attack, r.defense, r.spatk, r.spdef, r.speed};
    IVSummary s{true, 0, 0, false};
    for (IvJudgeValue v : stats){
        if (v == IvJudgeValue::UnableToDetect){ s.read = false; }
        if (v == IvJudgeValue::Best || v == IvJudgeValue::HyperTrained){ s.best_count++; }
        s.total_estimate = static_cast<uint16_t>(s.total_estimate + iv_value_midpoint(v));
    }
    s.perfect = (s.best_count == 6);
    return s;
}


}
}
}

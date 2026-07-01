/*  PokemonHome IV Summary
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonHome_IvSummary_H
#define PokemonAutomation_PokemonHome_IvSummary_H

#include <cstdint>
#include "Pokemon/Inference/Pokemon_IvJudgeReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


struct IVSummary{
    bool    read;           // false if any stat is UnableToDetect
    uint8_t best_count;     // number of stats that are Best or HyperTrained
    uint16_t total_estimate; // sum of iv_value_midpoint() for all six stats
    bool    perfect;        // true iff best_count == 6
};

// Returns the representative midpoint IV value for a given IvJudgeValue.
// Best/HyperTrained=31, Fantastic=30, VeryGood=27, PrettyGood=20, Decent=8, NoGood/UnableToDetect=0.
uint8_t iv_value_midpoint(Pokemon::IvJudgeValue v);

// Condenses a full set of six IvJudgeValue results into an IVSummary.
// Sets read=false if any stat is UnableToDetect.
// Counts both Best and HyperTrained toward best_count.
IVSummary summarize_ivs(const Pokemon::IvJudgeReader::Results& r);


}
}
}
#endif

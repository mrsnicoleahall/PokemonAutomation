/*  PokemonHome Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */


#include "PokemonHome/Inference/PokemonHome_ButtonDetector.h"
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
#include "Pokemon/Inference/Pokemon_IvJudgeReader.h"
#include "PokemonHome_Tests.h"
#include "TestUtils.h"

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

namespace PokemonAutomation{

using namespace NintendoSwitch;
using namespace NintendoSwitch::PokemonHome;

int test_pokemonHome_BoxView(const ImageViewRGB32& image, const std::vector<std::string>& keywords){
    SummaryScreenDetector summary_screen_detector;
    bool result = summary_screen_detector.detect(image);
    TEST_RESULT_EQUAL(result, false);

    BoxViewDetector box_view_detector;
    result = box_view_detector.detect(image);
    TEST_RESULT_EQUAL(result, true);
    return 0;
}

int test_pokemonHome_SummaryScreen(const ImageViewRGB32& image, const std::vector<std::string>& keywords){
    SummaryScreenDetector summary_screen_detector;
    bool result = summary_screen_detector.detect(image);
    TEST_RESULT_EQUAL(result, true);

    BoxViewDetector box_view_detector;
    result = box_view_detector.detect(image);
    TEST_RESULT_EQUAL(result, false);
    return 0;
}

int test_pokemonHome_IvSummary(const ImageViewRGB32& /*image*/){
    using Pokemon::IvJudgeValue;
    using R = Pokemon::IvJudgeReader::Results;

    // All Best/HyperTrained — flawless set
    R flawless{IvJudgeValue::Best, IvJudgeValue::Best, IvJudgeValue::Best,
               IvJudgeValue::Best, IvJudgeValue::Best, IvJudgeValue::HyperTrained};
    IVSummary s = summarize_ivs(flawless);
    TEST_RESULT_EQUAL(s.best_count, static_cast<uint8_t>(6));   // HyperTrained counts as 31
    TEST_RESULT_EQUAL(s.perfect, true);
    TEST_RESULT_EQUAL(s.read, true);
    TEST_RESULT_EQUAL(s.total_estimate, static_cast<uint16_t>(186));  // 5*31 + 31 = 186

    // Mixed set with UnableToDetect — not fully read
    R mixed{IvJudgeValue::Best, IvJudgeValue::Best, IvJudgeValue::VeryGood,
            IvJudgeValue::Decent, IvJudgeValue::NoGood, IvJudgeValue::UnableToDetect};
    IVSummary m = summarize_ivs(mixed);
    TEST_RESULT_EQUAL(m.best_count, static_cast<uint8_t>(2));
    TEST_RESULT_EQUAL(m.perfect, false);
    TEST_RESULT_EQUAL(m.read, false);   // UnableToDetect -> not fully read

    return 0;
}

}

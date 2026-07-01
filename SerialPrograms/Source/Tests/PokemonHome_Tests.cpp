/*  PokemonHome Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */


#include "PokemonHome/Inference/PokemonHome_ButtonDetector.h"
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
#include "PokemonHome/Programs/PokemonHome_MasterBoxRouter.h"
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

int test_pokemonHome_MasterBoxRouter(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;

    std::set<uint16_t> legend{144, 145, 146};
    std::set<uint16_t> myth{151};
    std::set<uint16_t> ub{793};
    std::set<uint16_t> para{984};
    RouterConfig cfg{
        {"nicole", "cole"},  // owner_ot_names
        6,                   // competitive_min31
        {3, 5},              // breeding_range
        {1, 2},              // breedject_range
        &legend, &myth, &ub, &para
    };

    // 9. LivingDex — owner, normal, iv_best_count=0
    PA base{};
    base.dex_number = 1;
    base.ot_name = "nicole";
    base.iv_read = true;
    base.iv_best_count = 0;
    TEST_RESULT_EQUAL((int)route(base, cfg, false, true), (int)BoxCategory::LivingDex);

    // 1. ManualForms — base_form_signature_matches == false
    PA form = base;
    TEST_RESULT_EQUAL((int)route(form, cfg, false, false), (int)BoxCategory::ManualForms);

    // 2. DuplicateShiny — shiny + species_slot_taken_by_shiny
    PA dupshiny = base;
    dupshiny.shiny = true;
    TEST_RESULT_EQUAL((int)route(dupshiny, cfg, true, true), (int)BoxCategory::DuplicateShiny);

    // 3. GoodTrades — foreign OT, not shiny, not legend/myth/event
    PA trade = base;
    trade.ot_name = "ash";
    TEST_RESULT_EQUAL((int)route(trade, cfg, false, true), (int)BoxCategory::GoodTrades);

    // 4. Competitive — iv_best_count >= competitive_min31 (6)
    PA comp = base;
    comp.iv_best_count = 6;
    TEST_RESULT_EQUAL((int)route(comp, cfg, false, true), (int)BoxCategory::Competitive);

    // 5. Breeding — iv_best_count in [3,5]
    PA breed = base;
    breed.iv_best_count = 4;
    TEST_RESULT_EQUAL((int)route(breed, cfg, false, true), (int)BoxCategory::Breeding);

    // 6. Breedject — iv_best_count in [1,2]
    PA breedj = base;
    breedj.iv_best_count = 1;
    TEST_RESULT_EQUAL((int)route(breedj, cfg, false, true), (int)BoxCategory::Breedject);

    // 8a. Mythical
    PA myth1 = base;
    myth1.dex_number = 151;
    TEST_RESULT_EQUAL((int)route(myth1, cfg, false, true), (int)BoxCategory::Mythical);

    // 8b. Legendary
    PA leg = base;
    leg.dex_number = 144;
    TEST_RESULT_EQUAL((int)route(leg, cfg, false, true), (int)BoxCategory::Legendary);

    // 8c. UltraBeast
    PA ub1 = base;
    ub1.dex_number = 793;
    TEST_RESULT_EQUAL((int)route(ub1, cfg, false, true), (int)BoxCategory::UltraBeast);

    // 8d. Paradox
    PA par = base;
    par.dex_number = 984;
    TEST_RESULT_EQUAL((int)route(par, cfg, false, true), (int)BoxCategory::Paradox);

    // 7. Events — cherish ball
    PA ev_cherish = base;
    ev_cherish.ball_slug = "cherish-ball";
    TEST_RESULT_EQUAL((int)route(ev_cherish, cfg, false, true), (int)BoxCategory::Events);

    // 7. Events — GO origin mark
    PA ev_go = base;
    ev_go.origin_mark = Pokemon::OriginMark::GO;
    TEST_RESULT_EQUAL((int)route(ev_go, cfg, false, true), (int)BoxCategory::Events);

    // GoodTrades — foreign OT shiny does NOT go to GoodTrades (shiny is excluded from step 3)
    PA shiny_ash = base;
    shiny_ash.ot_name = "ash";
    shiny_ash.shiny = true;
    // shiny + foreign + species slot NOT taken → DuplicateShiny check fails (slot not taken) → goes past step 2,
    // step 3 skipped (shiny), no IV boost → falls to LivingDex
    TEST_RESULT_EQUAL((int)route(shiny_ash, cfg, false, true), (int)BoxCategory::LivingDex);

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

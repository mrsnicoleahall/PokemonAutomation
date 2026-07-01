/*  PokemonHome Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */


#include "CommonFramework/Globals.h"
#include "PokemonHome/Inference/PokemonHome_ButtonDetector.h"
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
#include "PokemonHome/Programs/PokemonHome_MasterBoxLayout.h"
#include "PokemonHome/Programs/PokemonHome_MasterBoxPlanner.h"
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

    // Fix 1 regression: foreign-OT non-shiny UltraBeast must NOT land in GoodTrades.
    PA ub_foreign = base;
    ub_foreign.ot_name = "ash";
    ub_foreign.dex_number = 793;  // in the ub set
    TEST_RESULT_EQUAL((int)route(ub_foreign, cfg, false, true), (int)BoxCategory::UltraBeast);

    // Fix 1 regression: foreign-OT non-shiny Paradox must NOT land in GoodTrades.
    PA para_foreign = base;
    para_foreign.ot_name = "ash";
    para_foreign.dex_number = 984;  // in the paradox set
    TEST_RESULT_EQUAL((int)route(para_foreign, cfg, false, true), (int)BoxCategory::Paradox);

    return 0;
}

int test_pokemonHome_MasterBoxLayout(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    MasterBoxLayout L = load_master_box_layout(
        RESOURCE_PATH() + "PokemonHome/DexTemplates/master_box_layout.json"
    );
    TEST_RESULT_EQUAL(L.living_dex_start_box, (uint16_t)1);
    TEST_RESULT_EQUAL(L.mythical.count(151) > 0, true);
    TEST_RESULT_EQUAL(L.legendary.count(144) > 0, true);
    TEST_RESULT_EQUAL(L.category_box_ranges.at(BoxCategory::LivingDex).second, (uint16_t)35);
    return 0;
}

int test_pokemonHome_MasterPlanner(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;

    // ---------------------------------------------------------------------------
    // Build a minimal MasterBoxLayout for testing.
    // LivingDex occupies boxes 1-35 (1-indexed), each box holds 30 slots.
    // DuplicateShiny occupies box 36.
    // ManualOther occupies box 37.
    // We only populate what the planner needs.
    // ---------------------------------------------------------------------------
    MasterBoxLayout layout;
    layout.living_dex_start_box = 1;
    layout.category_box_ranges[BoxCategory::LivingDex]      = {1,  35};
    layout.category_box_ranges[BoxCategory::DuplicateShiny] = {36, 36};
    layout.category_box_ranges[BoxCategory::ManualOther]    = {37, 37};
    layout.category_box_ranges[BoxCategory::Competitive]    = {38, 40};
    layout.category_box_ranges[BoxCategory::Breeding]       = {41, 43};
    layout.category_box_ranges[BoxCategory::Breedject]      = {44, 45};
    layout.category_box_ranges[BoxCategory::Events]         = {46, 46};
    layout.category_box_ranges[BoxCategory::GoodTrades]     = {47, 48};
    layout.category_box_ranges[BoxCategory::Legendary]      = {49, 50};
    layout.category_box_ranges[BoxCategory::Mythical]       = {51, 51};
    layout.category_box_ranges[BoxCategory::UltraBeast]     = {52, 52};
    layout.category_box_ranges[BoxCategory::Paradox]        = {53, 53};
    layout.category_box_ranges[BoxCategory::ManualForms]    = {54, 54};

    // RouterConfig: owner = "nicole"
    RouterConfig cfg;
    cfg.owner_ot_names     = {"nicole"};
    cfg.competitive_min31  = 6;
    cfg.breeding_range     = {3, 5};
    cfg.breedject_range    = {1, 2};
    cfg.legendary          = nullptr;
    cfg.mythical           = nullptr;
    cfg.ultra_beast        = nullptr;
    cfg.paradox            = nullptr;

    // ---------------------------------------------------------------------------
    // Test 1: wins_slot ordering.
    // ---------------------------------------------------------------------------

    // Shiny beats non-shiny.
    PA shiny_bulba{};
    shiny_bulba.dex_number = 1;
    shiny_bulba.shiny      = true;
    shiny_bulba.ot_name    = "ash";   // non-owner

    PA normal_bulba{};
    normal_bulba.dex_number = 1;
    normal_bulba.shiny      = false;
    normal_bulba.ot_name    = "ash";

    TEST_RESULT_EQUAL(wins_slot(shiny_bulba, normal_bulba, cfg), true);
    TEST_RESULT_EQUAL(wins_slot(normal_bulba, shiny_bulba, cfg), false);

    // Among non-shiny: owner beats non-owner.
    PA owner_bulba{};
    owner_bulba.dex_number = 1;
    owner_bulba.shiny      = false;
    owner_bulba.ot_name    = "nicole";  // owner
    owner_bulba.iv_best_count = 2;

    PA foreign_bulba{};
    foreign_bulba.dex_number = 1;
    foreign_bulba.shiny      = false;
    foreign_bulba.ot_name    = "ash";
    foreign_bulba.iv_best_count = 5;   // higher IVs, but foreign

    TEST_RESULT_EQUAL(wins_slot(owner_bulba, foreign_bulba, cfg), true);
    TEST_RESULT_EQUAL(wins_slot(foreign_bulba, owner_bulba, cfg), false);

    // Among same OT status non-shiny: higher iv_best_count wins.
    PA hi_iv{};
    hi_iv.dex_number    = 1;
    hi_iv.shiny         = false;
    hi_iv.ot_name       = "nicole";
    hi_iv.iv_best_count = 5;
    hi_iv.iv_total_estimate = 160;

    PA lo_iv{};
    lo_iv.dex_number    = 1;
    lo_iv.shiny         = false;
    lo_iv.ot_name       = "nicole";
    lo_iv.iv_best_count = 3;
    lo_iv.iv_total_estimate = 120;

    TEST_RESULT_EQUAL(wins_slot(hi_iv, lo_iv, cfg), true);
    TEST_RESULT_EQUAL(wins_slot(lo_iv, hi_iv, cfg), false);

    // Tie on all fields → incumbent wins (challenger does NOT win).
    PA tie1 = lo_iv, tie2 = lo_iv;
    TEST_RESULT_EQUAL(wins_slot(tie1, tie2, cfg), false);

    // ---------------------------------------------------------------------------
    // Test 2: full planner — two Bulbasaurs in the scan region, one shiny.
    // Scan starts at box 0 (layout.living_dex_start_box - 1 = 0).
    // Catalogue index 0 = box 0, slot 0.
    //
    // layout.living_dex_start_box = 1 → scan_start = 0.
    // Bulbasaur dex = 1 → living-dex target flat = category flat for LivingDex slot 0.
    // LivingDex range = boxes 1–35 (0-indexed: 0–34).
    // Wait — living_dex_start_box=1 means boxes 1..35 (1-indexed), i.e. 0..34 (0-indexed).
    // Flat index for LivingDex slot 0 = box 0 (0-indexed) * 30 + 0 = 0.
    //
    // But catalogue starts at box scan_start=0 too, so catalogue[0] is at flat 0,
    // which is the same as LivingDex slot 0.
    //
    // Place two Bulbasaurs in catalogue[0] and catalogue[1]:
    //   catalogue[0] = non-shiny, ot="ash"  (foreign)
    //   catalogue[1] = shiny,     ot="ash"  (shiny always wins dex slot)
    //
    // Expected:
    //   - The shiny wins the dex-slot (flat=0 from LivingDex box 1-indexed=1, 0-indexed=0).
    //   - The non-shiny gets re-routed.  Non-shiny, foreign OT, no IVs → GoodTrades.
    //
    // catalogue[0] is at flat=0 (box 0, slot 0).
    // catalogue[1] is at flat=1 (box 0, slot 1).
    // LivingDex box 1 (1-indexed) = box 0 (0-indexed) → same box as scan_start!
    // LivingDex slot 0 flat = 0.
    //
    // Shiny winner (ci=1) needs to move from flat=1 to flat=0.
    // Non-shiny loser (ci=0) needs to move to GoodTrades (box 47-1=46, slot 0, flat=46*30=1380).
    // catalogue[0] is currently at flat=0.
    //
    // Move plan to place shiny (at flat=1) into its dex slot (flat=0):
    //   Move 1: evict the non-shiny from flat=0 (the dex target) directly to its
    //           GoodTrades target (flat=1380), which is empty — direct move.
    //   Move 2: move the shiny from flat=1 to the now-empty flat=0.
    // Total: 2 moves expected.
    // ---------------------------------------------------------------------------

    std::vector<std::optional<PA>> cat2(2, std::nullopt);
    // catalogue[0] = non-shiny foreign Bulbasaur
    PA bulba_normal{};
    bulba_normal.dex_number = 1;
    bulba_normal.shiny      = false;
    bulba_normal.ot_name    = "ash";
    cat2[0] = bulba_normal;

    // catalogue[1] = shiny Bulbasaur (also foreign OT — shiny should still win dex slot)
    PA bulba_shiny{};
    bulba_shiny.dex_number = 1;
    bulba_shiny.shiny      = true;
    bulba_shiny.ot_name    = "ash";
    cat2[1] = bulba_shiny;

    // scratch: 3 boxes starting after scan range.
    // scan_start=0, catalogue covers box 0 (1 box).  scratch_box_start = 1 (just after scan).
    // scan_start = layout.living_dex_start_box - 1 = 0 (precondition enforced by program()).
    MasterPlan plan2 = build_master_plan(cat2, layout, cfg, /*scan_start=*/0, /*scratch_box_start=*/55, /*scratch_box_count=*/3);

    // No blocking warnings.
    bool has_blocking2 = false;
    for (const auto& w : plan2.warnings){
        if (w.rfind("[BLOCKING]", 0) == 0){ has_blocking2 = true; }
    }
    TEST_RESULT_EQUAL(has_blocking2, false);

    // The shiny must end up at the LivingDex slot for dex#1.
    // Verify by checking that the shiny's target (found in moves) is the LivingDex slot.
    // LivingDex range starts at box 0 (0-indexed), slot 0 → flat index 0.
    // The shiny (ci=1) starts at flat=1, target=flat=0.
    // So one move must be from cursor(0,0,1) to cursor(0,0,0).
    bool found_shiny_move = false;
    for (const auto& mv : plan2.moves){
        // from=(box=0,row=0,col=1), to=(box=0,row=0,col=0)
        if (mv.from.box == 0 && mv.from.row == 0 && mv.from.column == 1 &&
            mv.to.box   == 0 && mv.to.row   == 0 && mv.to.column   == 0){
            found_shiny_move = true;
        }
    }
    TEST_RESULT_EQUAL(found_shiny_move, true);

    // The non-shiny Bulbasaur starts at catalogue[0] = flat=0 (box=0, row=0, col=0).
    // GoodTrades range starts at box 47-1=46 (0-indexed) → flat=46*30=1380.
    // The planner first evicts the non-shiny from flat=0 (the dex slot) to its
    // GoodTrades target (flat=1380) so the target is free, then moves the shiny in.
    // Check that a move goes from (box=0, col=0) to GoodTrades (box=46).
    bool found_normal_move = false;
    for (const auto& mv : plan2.moves){
        if (mv.from.box == 0 && mv.from.row == 0 && mv.from.column == 0 &&
            mv.to.box   == 46){
            found_normal_move = true;
        }
    }
    TEST_RESULT_EQUAL(found_normal_move, true);

    // ---------------------------------------------------------------------------
    // Test 3: single Pokémon already in the correct slot — no moves needed.
    // Put a Bulbasaur at catalogue index 0 (flat=0) where LivingDex wants it.
    // ---------------------------------------------------------------------------
    std::vector<std::optional<PA>> cat3(1, std::nullopt);
    PA bulba3{};
    bulba3.dex_number = 1;
    bulba3.shiny      = false;
    bulba3.ot_name    = "nicole";
    cat3[0] = bulba3;

    MasterPlan plan3 = build_master_plan(cat3, layout, cfg, /*scan_start=*/0, /*scratch_box_start=*/55, /*scratch_box_count=*/3);
    // Bulbasaur is already at flat=0 = LivingDex slot 0 → no moves.
    TEST_RESULT_EQUAL(plan3.moves.empty(), true);

    // ---------------------------------------------------------------------------
    // Test 4: wins_slot with iv_total_estimate as tiebreaker.
    // ---------------------------------------------------------------------------
    PA a{}, b{};
    a.dex_number = 2; a.shiny = false; a.ot_name = "nicole";
    a.iv_best_count = 4; a.iv_total_estimate = 130;
    b.dex_number = 2; b.shiny = false; b.ot_name = "nicole";
    b.iv_best_count = 4; b.iv_total_estimate = 140;
    // b has higher total_estimate → b wins
    TEST_RESULT_EQUAL(wins_slot(b, a, cfg), true);
    TEST_RESULT_EQUAL(wins_slot(a, b, cfg), false);

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

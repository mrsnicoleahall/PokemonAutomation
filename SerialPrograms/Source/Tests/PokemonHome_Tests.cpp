/*  PokemonHome Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */


#include <sstream>
#include <vector>
#include "CommonFramework/Globals.h"
#include "PokemonHome/Inference/PokemonHome_ButtonDetector.h"
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
#include "PokemonHome/Programs/PokemonHome_CatalogueCsv.h"
#include "PokemonHome/Programs/PokemonHome_DexSlots.h"
#include "PokemonHome/Programs/PokemonHome_MasterBoxLayout.h"
#include "PokemonHome/Programs/PokemonHome_MasterBoxPlanner.h"
#include "PokemonHome/Programs/PokemonHome_MasterBoxRouter.h"
#include "PokemonHome/Programs/PokemonHome_MasterBoxRouterV3.h"
#include "PokemonHome/Programs/PokemonHome_MasterPlannerV3.h"
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

int test_pokemonHome_UtilityPlan(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;

    // ---------------------------------------------------------------------------
    // Regression test: build_master_plan treats BoxCategory::Utility as a
    // route-all bucket — multiple Utility mons coexist with NO single-slot
    // contention.  Both mons must land inside the Utility box range; neither
    // should be dropped to ManualOther.
    //
    // Layout (1-indexed box numbers):
    //   LivingDex  1-35   Utility 60-60
    //   DuplicateShiny 36  ManualOther 37
    //
    // Two Flame-Body mons placed in catalogue[0] and catalogue[1].
    // RouterConfig has utility_rules = [{Ability, "flame-body"}].
    // Both should route to BoxCategory::Utility.
    //
    // Utility box range: boxes 60-60 (1-indexed) = box 59 (0-indexed).
    // Utility slot 0 → flat = 59*30 = 1770
    // Utility slot 1 → flat = 59*30 + 1 = 1771
    // Both mons start at flat=0 (catalogue[0]) and flat=1 (catalogue[1]).
    // Both need to move into the Utility box → expect 2 moves, no BLOCKING warnings.
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
    layout.category_box_ranges[BoxCategory::Utility]        = {60, 60};  // 30-slot bucket

    // RouterConfig: owner="nicole", utility_rules = [{Ability, "flame-body"}]
    RouterConfig cfg;
    cfg.owner_ot_names    = {"nicole"};
    cfg.competitive_min31 = 6;
    cfg.breeding_range    = {3, 5};
    cfg.breedject_range   = {1, 2};
    cfg.legendary         = nullptr;
    cfg.mythical          = nullptr;
    cfg.ultra_beast       = nullptr;
    cfg.paradox           = nullptr;
    cfg.utility_rules     = {{UtilityRule::Ability, "flame-body"}};

    // Two Flame-Body mons placed at catalogue[0] and catalogue[1].
    // (scan_start=0, so catalogue[0]=flat 0, catalogue[1]=flat 1)
    std::vector<std::optional<PA>> catalogue(2, std::nullopt);

    PA flamebody_a{};
    flamebody_a.dex_number    = 126;   // Magmar — has Flame Body
    flamebody_a.ot_name       = "nicole";
    flamebody_a.iv_read       = true;
    flamebody_a.iv_best_count = 0;
    flamebody_a.ability_slug  = "flame-body";
    catalogue[0] = flamebody_a;

    PA flamebody_b{};
    flamebody_b.dex_number    = 58;    // Growlithe — can have Flash Fire; we force ability here
    flamebody_b.ot_name       = "nicole";
    flamebody_b.iv_read       = true;
    flamebody_b.iv_best_count = 0;
    flamebody_b.ability_slug  = "flame-body";
    catalogue[1] = flamebody_b;

    // scan_start=0, scratch starts beyond layout boxes.
    MasterPlan plan = build_master_plan(
        catalogue, layout, cfg,
        /*scan_start=*/0,
        /*scratch_box_start=*/65,
        /*scratch_box_count=*/3
    );

    // Assert 1: no BLOCKING warnings — both mons placed successfully.
    bool has_blocking = false;
    for (const auto& w : plan.warnings){
        if (w.rfind("[BLOCKING]", 0) == 0){ has_blocking = true; }
    }
    TEST_RESULT_EQUAL(has_blocking, false);

    // Assert 2: no overflow warnings (neither mon fell back to ManualOther).
    bool has_overflow = false;
    for (const auto& w : plan.warnings){
        if (w.find("ManualOther") != std::string::npos){ has_overflow = true; }
    }
    TEST_RESULT_EQUAL(has_overflow, false);

    // Assert 3: both mons land in the Utility box (0-indexed box 59).
    // Verify via moves: both catalogue entries must move to box 59.
    // catalogue[0] starts at (box=0,row=0,col=0), target=(box=59,slot=0)→(row=0,col=0)
    // catalogue[1] starts at (box=0,row=0,col=1), target=(box=59,slot=1)→(row=0,col=1)
    static const size_t UTILITY_BOX_0IDX = 59;  // 60-1
    bool found_a_in_utility = false;
    bool found_b_in_utility = false;
    for (const auto& mv : plan.moves){
        if (mv.to.box == UTILITY_BOX_0IDX){
            if (mv.to.row == 0 && mv.to.column == 0) found_a_in_utility = true;
            if (mv.to.row == 0 && mv.to.column == 1) found_b_in_utility = true;
        }
    }
    TEST_RESULT_EQUAL(found_a_in_utility, true);
    TEST_RESULT_EQUAL(found_b_in_utility, true);

    return 0;
}

int test_pokemonHome_UtilityRouting(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;
    std::set<uint16_t> legend, myth, ub, para;
    std::vector<UtilityRule> ur = {
        {UtilityRule::Ability, "flame-body"}, {UtilityRule::Ability, "synchronize"},
        {UtilityRule::Item,    "amulet-coin"}, {UtilityRule::Move,   "false-swipe"},
    };
    RouterConfig cfg{ {"nicole","cole"}, 6, {3,5}, {1,2}, &legend,&myth,&ub,&para, ur };

    // Hatcher with flame-body ability → Utility
    PA hatcher{}; hatcher.dex_number=1; hatcher.ot_name="nicole"; hatcher.iv_read=true; hatcher.ability_slug="flame-body";
    TEST_RESULT_EQUAL((int)route(hatcher,cfg,false,true), (int)BoxCategory::Utility);

    // False-swipe user → Utility
    PA catcher{}; catcher.dex_number=286; catcher.ot_name="nicole"; catcher.iv_read=true; catcher.moves={"false-swipe","spore"};
    TEST_RESULT_EQUAL((int)route(catcher,cfg,false,true), (int)BoxCategory::Utility);

    // Amulet-coin holder → Utility
    PA money{}; money.dex_number=52; money.ot_name="nicole"; money.iv_read=true; money.held_item_slug="amulet-coin";
    TEST_RESULT_EQUAL((int)route(money,cfg,false,true), (int)BoxCategory::Utility);

    // 6x31 Synchronize mon → Competitive wins (IV routing fires before Utility)
    PA comp{}; comp.dex_number=280; comp.ot_name="nicole"; comp.iv_read=true; comp.iv_best_count=6; comp.ability_slug="synchronize";
    TEST_RESULT_EQUAL((int)route(comp,cfg,false,true), (int)BoxCategory::Competitive);

    // Plain mon, no utility match → LivingDex
    PA plain{}; plain.dex_number=1; plain.ot_name="nicole"; plain.iv_read=true;
    TEST_RESULT_EQUAL((int)route(plain,cfg,false,true), (int)BoxCategory::LivingDex);

    // -----------------------------------------------------------------------
    // Format-contract regression: OCR strips hyphens ("flamebody") but defaults
    // are hyphenated ("flame-body").  canon_slug on both sides must bridge this.
    // -----------------------------------------------------------------------

    // Ability: OCR-normalised "flamebody" must match default rule "flame-body".
    PA ocr_ability{};
    ocr_ability.dex_number = 126; ocr_ability.ot_name = "nicole"; ocr_ability.iv_read = true;
    ocr_ability.ability_slug = "flamebody";   // hyphen-stripped OCR output
    // utility_rules has {Ability, "flame-body"} (hyphenated default)
    TEST_RESULT_EQUAL((int)route(ocr_ability,cfg,false,true), (int)BoxCategory::Utility);

    // Item: OCR-normalised "amuletcoin" must match default rule "amulet-coin".
    PA ocr_item{};
    ocr_item.dex_number = 52; ocr_item.ot_name = "nicole"; ocr_item.iv_read = true;
    ocr_item.held_item_slug = "amuletcoin";   // hyphen-stripped OCR output
    // utility_rules has {Item, "amulet-coin"} (hyphenated default)
    TEST_RESULT_EQUAL((int)route(ocr_item,cfg,false,true), (int)BoxCategory::Utility);

    // Move: OCR tokens use hyphenated keys ("false-swipe" from PokemonMovesOCR.json);
    // canon_slug must still match against the hyphenated default rule "false-swipe".
    // (Both sides are already hyphenated here — canon_slug is idempotent on alphanumerics.)
    PA ocr_move{};
    ocr_move.dex_number = 286; ocr_move.ot_name = "nicole"; ocr_move.iv_read = true;
    ocr_move.moves = {"false-swipe", "spore"};   // hyphenated keys as stored by MovesReaderScope
    TEST_RESULT_EQUAL((int)route(ocr_move,cfg,false,true), (int)BoxCategory::Utility);

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

int test_pokemonHome_CatalogueCsv(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;

    // Test 1: header has exactly 28 columns.
    {
        std::string hdr = catalogue_csv_header();
        // Remove trailing newline for counting.
        if (!hdr.empty() && hdr.back() == '\n'){ hdr.pop_back(); }
        int commas = 0;
        for (char c : hdr){ if (c == ',') commas++; }
        TEST_RESULT_EQUAL(commas, 27);  // 28 columns = 27 commas

        // Verify exact column order at start.
        const std::string expected_prefix = "box,row,col,dex,species,shiny,";
        TEST_RESULT_COMPONENT_EQUAL(
            hdr.substr(0, expected_prefix.size()),
            expected_prefix,
            "header_prefix"
        );
    }

    // Test 2: row output for a known CollectedPokemonInfo.
    {
        PA info{};
        info.dex_number         = 133;
        info.name_slug          = "eevee";
        info.shiny              = true;
        info.gmax               = false;
        info.alpha              = false;
        info.ball_slug          = "poke-ball";
        info.gender             = Pokemon::StatsHuntGenderFilter::Female;
        info.ot_id              = 12345;
        info.ot_name            = "Nicole";
        info.iv_read            = true;
        info.iv_best_count      = 6;
        info.iv_total_estimate  = 186;
        info.iv_perfect         = true;
        info.ability_slug       = "adaptability";
        info.nature             = "timid";
        info.held_item_slug     = "leftovers";
        info.extras_read        = true;
        info.moves_read         = true;
        info.moves              = {"tackle", "growl", "sand-attack"};

        std::string row = catalogue_csv_row(4, 1, 2, info, "LivingDex", 5);
        // Remove trailing newline.
        if (!row.empty() && row.back() == '\n'){ row.pop_back(); }

        // Split by comma (no embedded commas in this test's fields).
        std::vector<std::string> cells;
        std::stringstream ss(row);
        std::string cell;
        while (std::getline(ss, cell, ',')){ cells.push_back(cell); }

        TEST_RESULT_EQUAL(cells.size(), static_cast<size_t>(28));
        TEST_RESULT_COMPONENT_EQUAL(cells[0],  std::string("5"),      "box (1-indexed)");
        TEST_RESULT_COMPONENT_EQUAL(cells[1],  std::string("2"),      "row (1-indexed)");
        TEST_RESULT_COMPONENT_EQUAL(cells[2],  std::string("3"),      "col (1-indexed)");
        TEST_RESULT_COMPONENT_EQUAL(cells[3],  std::string("133"),    "dex");
        TEST_RESULT_COMPONENT_EQUAL(cells[4],  std::string("eevee"),  "species");
        TEST_RESULT_COMPONENT_EQUAL(cells[5],  std::string("true"),   "shiny");
        TEST_RESULT_COMPONENT_EQUAL(cells[9],  std::string("poke-ball"), "ball");
        // moves joined by | (index 23)
        TEST_RESULT_COMPONENT_EQUAL(cells[23], std::string("tackle|growl|sand-attack"), "moves");
        TEST_RESULT_COMPONENT_EQUAL(cells[26], std::string("LivingDex"), "routed_category");
        // dest_box is the raw 0-indexed int passed in (no +1)
        TEST_RESULT_COMPONENT_EQUAL(cells[27], std::string("5"), "dest_box");
    }

    // Test 3: CSV escaping — OT name containing a comma.
    {
        PA info{};
        info.dex_number = 25;
        info.name_slug  = "pikachu";
        info.ot_name    = "Ash, Jr.";  // contains a comma → must be quoted
        info.gender     = Pokemon::StatsHuntGenderFilter::Any;

        std::string row = catalogue_csv_row(0, 0, 0, info, "LivingDex", 0);
        // The OT name field should appear quoted: "Ash, Jr."
        TEST_RESULT_COMPONENT_EQUAL(
            row.find("\"Ash, Jr.\"") != std::string::npos,
            true,
            "ot_name comma escaping"
        );
    }

    // Test 4: CSV escaping — OT name containing a double-quote.
    {
        PA info{};
        info.dex_number = 25;
        info.name_slug  = "pikachu";
        info.ot_name    = "Say \"hi\"";  // contains quotes → must be quoted+escaped
        info.gender     = Pokemon::StatsHuntGenderFilter::Any;

        std::string row = catalogue_csv_row(0, 0, 0, info, "LivingDex", 0);
        // Should produce: "Say ""hi"""
        TEST_RESULT_COMPONENT_EQUAL(
            row.find("\"Say \"\"hi\"\"\"") != std::string::npos,
            true,
            "ot_name quote escaping"
        );
    }

    // Test 5: spec-required escaping: ot_name "a,b\"c" → "a,b\"\"c"
    {
        PA info{};
        info.dex_number = 25;
        info.name_slug  = "pikachu";
        info.shiny      = true;
        info.moves      = {"false-swipe", "spore"};
        info.gender     = Pokemon::StatsHuntGenderFilter::Any;
        info.ot_name    = "a,b\"c";  // comma + double-quote

        std::string row = catalogue_csv_row(0, 0, 0, info, "LivingDex", 0);

        // Contains dex 25
        TEST_RESULT_COMPONENT_EQUAL(
            row.find(",25,") != std::string::npos,
            true,
            "dex=25 present"
        );
        // Contains shiny=true
        TEST_RESULT_COMPONENT_EQUAL(
            row.find(",true,") != std::string::npos,
            true,
            "shiny=true present"
        );
        // Contains joined moves (may be in a quoted field since | is safe, no escaping needed)
        TEST_RESULT_COMPONENT_EQUAL(
            row.find("false-swipe|spore") != std::string::npos,
            true,
            "moves joined by pipe"
        );
        // OT name with comma+quote must be escaped: "a,b""c"
        TEST_RESULT_COMPONENT_EQUAL(
            row.find("\"a,b\"\"c\"") != std::string::npos,
            true,
            "ot_name comma+quote escaping"
        );
    }

    return 0;
}

int test_pokemonHome_DexSlots(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    TEST_RESULT_EQUAL(regular_dex_slot(1), (size_t)0);
    TEST_RESULT_EQUAL(regular_dex_slot(956), (size_t)955);   // gap-preserving: slot == dex-1
    std::set<uint16_t> locked = {2, 4};                       // pretend 2 and 4 are shiny-locked
    TEST_RESULT_EQUAL(shiny_dex_slot(1, locked).has_value(), true);
    TEST_RESULT_EQUAL(*shiny_dex_slot(1, locked), (size_t)0);
    TEST_RESULT_EQUAL(shiny_dex_slot(2, locked).has_value(), false); // locked → no slot
    TEST_RESULT_EQUAL(*shiny_dex_slot(3, locked), (size_t)1);        // 1 then 3 → rank 1 (2 skipped)
    TEST_RESULT_EQUAL(*shiny_dex_slot(5, locked), (size_t)2);        // 1,3,5 → rank 2 (2,4 skipped)
    return 0;
}

int test_pokemonHome_MasterRouterV3(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;

    // -------------------------------------------------------------------------
    // Build a minimal MasterBoxLayoutV3 and RouterConfig for all sub-tests.
    // Species 144 = Legendary, 151 = Mythical, 793 = UltraBeast, 984 = Paradox.
    // Species 494 (Victini) = shiny-locked.
    // -------------------------------------------------------------------------
    MasterBoxLayoutV3 layout;
    layout.shiny_dex_start         = 1;
    layout.regular_dex_start       = 41;
    layout.shiny_dex_buffer_boxes  = 5;
    layout.regular_dex_buffer_boxes = 5;
    layout.legendary               = {144, 145, 146};
    layout.mythical                = {151};
    layout.ultra_beast             = {793};
    layout.paradox                 = {984};
    layout.shiny_locked            = {494};  // Victini — definitively shiny-locked
    // Category box ranges (1-indexed, minimal set for routing).
    layout.category_box_ranges[BoxCategory::Legendary]    = {81, 81};
    layout.category_box_ranges[BoxCategory::Mythical]     = {82, 82};
    layout.category_box_ranges[BoxCategory::UltraBeast]   = {83, 83};
    layout.category_box_ranges[BoxCategory::Paradox]      = {84, 84};
    layout.category_box_ranges[BoxCategory::Events]       = {85, 85};
    layout.category_box_ranges[BoxCategory::ManualForms]  = {86, 86};
    layout.category_box_ranges[BoxCategory::Utility]      = {87, 87};
    layout.category_box_ranges[BoxCategory::Breeding]     = {88, 88};
    layout.category_box_ranges[BoxCategory::Competitive]  = {89, 89};
    layout.category_box_ranges[BoxCategory::ShinyTrades]  = {90, 90};
    layout.category_box_ranges[BoxCategory::RegularTrades]= {91, 91};
    layout.category_box_ranges[BoxCategory::Junk]         = {92, 92};

    RouterConfig cfg;
    cfg.owner_ot_names    = {"nicole", "cole"};
    cfg.competitive_min31 = 6;
    cfg.breeding_range    = {3, 5};
    cfg.breedject_range   = {1, 2};
    cfg.legendary         = &layout.legendary;
    cfg.mythical          = &layout.mythical;
    cfg.ultra_beast       = &layout.ultra_beast;
    cfg.paradox           = &layout.paradox;
    cfg.utility_rules     = {{UtilityRule::Ability, "flame-body"}};

    // =========================================================================
    // Test (a): two same-species non-shiny Bulbasaur.
    // Higher IV copy → RegularDexSlot keeper; lower IV → Duplicate → RegularTrades.
    // =========================================================================
    {
        PA hi{};
        hi.dex_number       = 1;
        hi.shiny            = false;
        hi.ot_name          = "nicole";
        hi.iv_read          = true;
        hi.iv_best_count    = 4;
        hi.iv_total_estimate = 140;
        hi.primary_type     = Pokemon::PokemonType::GRASS;
        hi.secondary_type   = Pokemon::PokemonType::POISON;

        PA lo{};
        lo.dex_number       = 1;
        lo.shiny            = false;
        lo.ot_name          = "ash";          // foreign OT, lower IVs
        lo.iv_read          = true;
        lo.iv_best_count    = 0;
        lo.iv_total_estimate = 50;
        lo.primary_type     = Pokemon::PokemonType::GRASS;
        lo.secondary_type   = Pokemon::PokemonType::POISON;

        std::vector<std::optional<PA>> cat = {hi, lo};
        auto results = route_all_v3(cat, layout, cfg);

        TEST_RESULT_EQUAL(results.size(), (size_t)2);

        // hi is the better copy — it should be the RegularDexSlot keeper.
        TEST_RESULT_EQUAL(results[0].is_dex_keeper, true);
        TEST_RESULT_EQUAL((int)results[0].category, (int)BoxCategory::RegularDex);

        // lo is the duplicate — foreign OT, no IVs → RegularTrades.
        TEST_RESULT_EQUAL(results[1].is_dex_keeper, false);
        TEST_RESULT_EQUAL((int)results[1].category, (int)BoxCategory::RegularTrades);
    }

    // =========================================================================
    // Test (b): two same-species shiny Bulbasaur.
    // Best shiny → ShinyDexSlot keeper; other shiny → ShinyTrades.
    // =========================================================================
    {
        PA s1{};
        s1.dex_number       = 1;
        s1.shiny            = true;
        s1.ot_name          = "nicole";
        s1.iv_read          = true;
        s1.iv_best_count    = 5;
        s1.iv_total_estimate = 150;
        s1.primary_type     = Pokemon::PokemonType::GRASS;
        s1.secondary_type   = Pokemon::PokemonType::POISON;

        PA s2{};
        s2.dex_number       = 1;
        s2.shiny            = true;
        s2.ot_name          = "ash";
        s2.iv_read          = true;
        s2.iv_best_count    = 0;
        s2.iv_total_estimate = 30;
        s2.primary_type     = Pokemon::PokemonType::GRASS;
        s2.secondary_type   = Pokemon::PokemonType::POISON;

        std::vector<std::optional<PA>> cat = {s1, s2};
        auto results = route_all_v3(cat, layout, cfg);

        // s1 (owner, better IVs) wins the ShinyDex slot.
        TEST_RESULT_EQUAL(results[0].is_dex_keeper, true);
        TEST_RESULT_EQUAL((int)results[0].category, (int)BoxCategory::ShinyDex);

        // s2 is a shiny duplicate → ShinyTrades.
        TEST_RESULT_EQUAL(results[1].is_dex_keeper, false);
        TEST_RESULT_EQUAL((int)results[1].category, (int)BoxCategory::ShinyTrades);
    }

    // =========================================================================
    // Test (c): shiny of a shiny-locked species (Victini, dex 494).
    // Must NOT become a ShinyDex keeper — routes as duplicate instead.
    // =========================================================================
    {
        PA locked_shiny{};
        locked_shiny.dex_number   = 494;   // Victini — shiny-locked
        locked_shiny.shiny        = true;
        locked_shiny.ot_name      = "nicole";
        locked_shiny.iv_read      = true;
        locked_shiny.iv_best_count = 6;
        locked_shiny.iv_perfect   = true;
        locked_shiny.primary_type = Pokemon::PokemonType::PSYCHIC;
        locked_shiny.secondary_type = Pokemon::PokemonType::FIRE;

        std::vector<std::optional<PA>> cat = {locked_shiny};
        auto results = route_all_v3(cat, layout, cfg);

        // Shiny-locked species cannot fill a ShinyDex slot — must not be keeper.
        TEST_RESULT_EQUAL(results[0].is_dex_keeper, false);
        // It should NOT be assigned ShinyDex category.
        TEST_RESULT_EQUAL((int)results[0].category != (int)BoxCategory::ShinyDex, true);
        // Victini is 6×31 (iv_best_count=6, competitive_min31=6) and owner-OT →
        // the correct duplicate route is Competitive.
        TEST_RESULT_EQUAL((int)results[0].category, (int)BoxCategory::Competitive);
    }

    // =========================================================================
    // Test (d): route_duplicate_v3 priority chain.
    // =========================================================================
    {
        // (d1) Duplicate Legendary (dex 144 = Articuno) → BoxCategory::Legendary
        PA leg_dup{};
        leg_dup.dex_number  = 144;
        leg_dup.shiny       = false;
        leg_dup.ot_name     = "nicole";
        leg_dup.iv_read     = true;
        leg_dup.iv_best_count = 0;
        leg_dup.primary_type = Pokemon::PokemonType::ICE;
        leg_dup.secondary_type = Pokemon::PokemonType::FLYING;
        TEST_RESULT_EQUAL(
            (int)route_duplicate_v3(leg_dup, layout, cfg),
            (int)BoxCategory::Legendary
        );

        // (d2) 6×31 duplicate → BoxCategory::Competitive
        PA comp_dup{};
        comp_dup.dex_number   = 1;
        comp_dup.shiny        = false;
        comp_dup.ot_name      = "ash";
        comp_dup.iv_read      = true;
        comp_dup.iv_best_count = 6;
        comp_dup.iv_perfect   = true;
        comp_dup.primary_type = Pokemon::PokemonType::GRASS;
        comp_dup.secondary_type = Pokemon::PokemonType::POISON;
        TEST_RESULT_EQUAL(
            (int)route_duplicate_v3(comp_dup, layout, cfg),
            (int)BoxCategory::Competitive
        );

        // (d3) Shiny duplicate (non-legendary, non-perfect) → BoxCategory::ShinyTrades
        PA shiny_dup{};
        shiny_dup.dex_number   = 1;
        shiny_dup.shiny        = true;
        shiny_dup.ot_name      = "ash";
        shiny_dup.iv_read      = true;
        shiny_dup.iv_best_count = 0;
        shiny_dup.primary_type = Pokemon::PokemonType::GRASS;
        shiny_dup.secondary_type = Pokemon::PokemonType::POISON;
        TEST_RESULT_EQUAL(
            (int)route_duplicate_v3(shiny_dup, layout, cfg),
            (int)BoxCategory::ShinyTrades
        );

        // (d4) Foreign-OT non-shiny duplicate (has trade value) → RegularTrades
        PA junk_dup{};
        junk_dup.dex_number   = 1;
        junk_dup.shiny        = false;
        junk_dup.ot_name      = "ash";         // foreign OT
        junk_dup.iv_read      = true;
        junk_dup.iv_best_count = 0;            // no IVs
        junk_dup.primary_type = Pokemon::PokemonType::GRASS;
        junk_dup.secondary_type = Pokemon::PokemonType::POISON;
        // Not legendary/mythical/event/shiny/utility/competitive/breeding → Junk
        // Note: RegularTrades applies when foreign OT — test that the route picks
        // between RegularTrades and Junk consistently with v3 spec.
        // Foreign OT with no trade value → RegularTrades per spec (foreign OT alone
        // qualifies as trade value).
        TEST_RESULT_EQUAL(
            (int)route_duplicate_v3(junk_dup, layout, cfg),
            (int)BoxCategory::RegularTrades   // foreign OT = trade value → RegularTrades
        );

        // (d5) Owner-OT, zero IVs, no special flags → Junk (protected from Junk only
        // if shiny/legendary/event/perfect — owner-OT plain mon goes to Junk)
        PA owner_junk{};
        owner_junk.dex_number    = 1;
        owner_junk.shiny         = false;
        owner_junk.ot_name       = "nicole";   // owner OT
        owner_junk.iv_read       = true;
        owner_junk.iv_best_count = 0;
        owner_junk.primary_type  = Pokemon::PokemonType::GRASS;
        owner_junk.secondary_type = Pokemon::PokemonType::POISON;
        TEST_RESULT_EQUAL(
            (int)route_duplicate_v3(owner_junk, layout, cfg),
            (int)BoxCategory::Junk
        );

        // (d6) Utility match → BoxCategory::Utility (before Competitive)
        PA util_dup{};
        util_dup.dex_number    = 58;
        util_dup.shiny         = false;
        util_dup.ot_name       = "nicole";
        util_dup.iv_read       = true;
        util_dup.iv_best_count = 0;
        util_dup.ability_slug  = "flame-body";
        util_dup.primary_type  = Pokemon::PokemonType::FIRE;
        TEST_RESULT_EQUAL(
            (int)route_duplicate_v3(util_dup, layout, cfg),
            (int)BoxCategory::Utility
        );
    }

    // =========================================================================
    // Test (e): a dex keeper that is 6×31 has "Competitive" in also_qualifies.
    // =========================================================================
    {
        PA perfect{};
        perfect.dex_number       = 1;
        perfect.shiny            = false;
        perfect.ot_name          = "nicole";
        perfect.iv_read          = true;
        perfect.iv_best_count    = 6;
        perfect.iv_perfect       = true;
        perfect.iv_total_estimate = 186;
        perfect.primary_type     = Pokemon::PokemonType::GRASS;
        perfect.secondary_type   = Pokemon::PokemonType::POISON;

        // Only one copy → sole keeper.
        std::vector<std::optional<PA>> cat = {perfect};
        auto results = route_all_v3(cat, layout, cfg);

        TEST_RESULT_EQUAL(results.size(), (size_t)1);
        TEST_RESULT_EQUAL(results[0].is_dex_keeper, true);
        TEST_RESULT_EQUAL((int)results[0].category, (int)BoxCategory::RegularDex);

        // also_qualifies must contain "Competitive".
        bool has_competitive = false;
        for (const auto& q : results[0].also_qualifies){
            if (q == "Competitive") has_competitive = true;
        }
        TEST_RESULT_EQUAL(has_competitive, true);
    }

    // =========================================================================
    // Test (f): empty slot → default RouteResultV3 (category Junk, not keeper).
    // =========================================================================
    {
        std::vector<std::optional<PA>> cat = {std::nullopt};
        auto results = route_all_v3(cat, layout, cfg);
        TEST_RESULT_EQUAL(results.size(), (size_t)1);
        // Empty slots get a default result — category is Junk and is_dex_keeper is false.
        TEST_RESULT_EQUAL(results[0].is_dex_keeper, false);
        TEST_RESULT_EQUAL((int)results[0].category, (int)BoxCategory::Junk);
    }

    // =========================================================================
    // Test (g): cross-group dex# dedup — two SpeciesKey groups sharing the same
    // dex_number must produce exactly ONE RegularDex keeper (not two).
    //
    // Scenario: dex# 52 (Meowth).  Two detectable variants:
    //   slot 0 — Normal-type  (canonical, higher IVs)    → should become RegularDex keeper
    //   slot 1 — Psychic-type (variant, lower IVs)       → duplicate → ManualForms
    //
    // Before the cross-group dedup fix both would have become RegularDex keepers
    // (one per SpeciesKey group), colliding on the same planner dex slot.
    // =========================================================================
    {
        PA base_meowth{};
        base_meowth.dex_number        = 52;
        base_meowth.shiny             = false;
        base_meowth.ot_name           = "nicole";
        base_meowth.iv_read           = true;
        base_meowth.iv_best_count     = 4;
        base_meowth.iv_total_estimate = 130;
        base_meowth.primary_type      = Pokemon::PokemonType::NORMAL;
        base_meowth.secondary_type    = Pokemon::PokemonType::NONE;

        PA variant_meowth{};
        variant_meowth.dex_number        = 52;
        variant_meowth.shiny             = false;
        variant_meowth.ot_name           = "nicole";
        variant_meowth.iv_read           = true;
        variant_meowth.iv_best_count     = 1;
        variant_meowth.iv_total_estimate = 80;
        variant_meowth.primary_type      = Pokemon::PokemonType::PSYCHIC;  // Galarian form
        variant_meowth.secondary_type    = Pokemon::PokemonType::NONE;

        std::vector<std::optional<PA>> cat = {base_meowth, variant_meowth};
        auto results = route_all_v3(cat, layout, cfg);

        TEST_RESULT_EQUAL(results.size(), (size_t)2);

        // Exactly ONE copy must be a RegularDex keeper across both groups.
        int keeper_count = 0;
        for (const auto& r : results){
            if (r.is_dex_keeper && r.category == BoxCategory::RegularDex){
                keeper_count++;
            }
        }
        TEST_RESULT_EQUAL(keeper_count, 1);

        // Neither copy should get RegularDex AND ShinyDex simultaneously.
        // (sanity — both are non-shiny)
        for (const auto& r : results){
            TEST_RESULT_EQUAL((int)(r.is_dex_keeper && r.category == BoxCategory::ShinyDex), 0);
        }

        // The base (Normal-type, higher IVs, slot 0) wins the dex slot — it is the
        // canonical key (first SpeciesKey seen for dex 52) and has better IVs.
        TEST_RESULT_EQUAL(results[0].is_dex_keeper, true);
        TEST_RESULT_EQUAL((int)results[0].category, (int)BoxCategory::RegularDex);

        // The Psychic-type variant (slot 1) is demoted — it came from a variant group
        // so it routes to ManualForms.
        TEST_RESULT_EQUAL(results[1].is_dex_keeper, false);
        TEST_RESULT_EQUAL((int)results[1].category, (int)BoxCategory::ManualForms);
    }

    return 0;
}

int test_pokemonHome_PlannerV3(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;

    // -------------------------------------------------------------------------
    // Minimal layout mirroring the real v3 layout (§1):
    //   Shiny Dex: boxes 1-35
    //   Buffer:    boxes 36-40
    //   Regular Dex: boxes 41-75
    //   Buffer:    boxes 76-80
    //   Categories: boxes 81-104
    // -------------------------------------------------------------------------
    MasterBoxLayoutV3 layout;
    layout.shiny_dex_start           = 1;
    layout.regular_dex_start         = 41;
    layout.shiny_dex_buffer_boxes    = 5;
    layout.regular_dex_buffer_boxes  = 5;
    // shiny_locked: none for Pikachu (#25)
    layout.shiny_locked              = {};
    layout.legendary                 = {};
    layout.mythical                  = {};
    layout.ultra_beast               = {};
    layout.paradox                   = {};
    layout.category_box_ranges[BoxCategory::Legendary]     = {81, 82};
    layout.category_box_ranges[BoxCategory::Mythical]      = {83, 84};
    layout.category_box_ranges[BoxCategory::UltraBeast]    = {85, 86};
    layout.category_box_ranges[BoxCategory::Paradox]       = {87, 88};
    layout.category_box_ranges[BoxCategory::Events]        = {89, 90};
    layout.category_box_ranges[BoxCategory::ManualForms]   = {91, 92};
    layout.category_box_ranges[BoxCategory::Breeding]      = {93, 94};
    layout.category_box_ranges[BoxCategory::Utility]       = {95, 96};
    layout.category_box_ranges[BoxCategory::Competitive]   = {97, 98};
    layout.category_box_ranges[BoxCategory::ShinyTrades]   = {99, 100};
    layout.category_box_ranges[BoxCategory::RegularTrades] = {101, 102};
    layout.category_box_ranges[BoxCategory::Junk]          = {103, 104};

    RouterConfig cfg;
    cfg.owner_ot_names    = {"nicole", "cole"};
    cfg.competitive_min31 = 6;
    cfg.breeding_range    = {3, 5};
    cfg.breedject_range   = {1, 2};
    cfg.legendary         = &layout.legendary;
    cfg.mythical          = &layout.mythical;
    cfg.ultra_beast       = &layout.ultra_beast;
    cfg.paradox           = &layout.paradox;

    // =========================================================================
    // Test (a): build_box_map yields 16 labeled ranges in §1 order,
    //           Shiny Dex first, two "(buffer)" entries.
    // =========================================================================
    {
        auto bm = build_box_map(layout);
        TEST_RESULT_EQUAL(bm.size(), (size_t)16);

        // §1 order: ShinyDex, (buffer), RegularDex, (buffer), then 12 category boxes
        TEST_RESULT_COMPONENT_EQUAL(bm[0].label, std::string("Shiny Dex"),    "bm[0] label");
        TEST_RESULT_COMPONENT_EQUAL(bm[1].label, std::string("(buffer)"),     "bm[1] label");
        TEST_RESULT_COMPONENT_EQUAL(bm[2].label, std::string("Regular Dex"),  "bm[2] label");
        TEST_RESULT_COMPONENT_EQUAL(bm[3].label, std::string("(buffer)"),     "bm[3] label");

        // Check Shiny Dex span (1-indexed)
        TEST_RESULT_COMPONENT_EQUAL(bm[0].box_start, (uint16_t)1,  "ShinyDex start");
        TEST_RESULT_COMPONENT_EQUAL(bm[0].box_end,   (uint16_t)35, "ShinyDex end");

        // Check shiny buffer span: boxes 36-40
        TEST_RESULT_COMPONENT_EQUAL(bm[1].box_start, (uint16_t)36, "shiny buffer start");
        TEST_RESULT_COMPONENT_EQUAL(bm[1].box_end,   (uint16_t)40, "shiny buffer end");

        // Check Regular Dex span (1-indexed)
        TEST_RESULT_COMPONENT_EQUAL(bm[2].box_start, (uint16_t)41, "RegDex start");
        TEST_RESULT_COMPONENT_EQUAL(bm[2].box_end,   (uint16_t)75, "RegDex end");

        // Check regular buffer span: boxes 76-80
        TEST_RESULT_COMPONENT_EQUAL(bm[3].box_start, (uint16_t)76, "reg buffer start");
        TEST_RESULT_COMPONENT_EQUAL(bm[3].box_end,   (uint16_t)80, "reg buffer end");

        // Check tail categories (indices 4..15)
        const char* expected_labels[] = {
            "Legendary", "Mythical", "Ultra Beasts", "Paradox",
            "Events", "Forms", "Breeding", "Utility", "Competitive",
            "Shiny Trades", "Regular Trades", "Junk"
        };
        for (size_t i = 4; i < 16; ++i){
            TEST_RESULT_COMPONENT_EQUAL(bm[i].label, std::string(expected_labels[i-4]),
                std::string("bm[") + std::to_string(i) + "] label");
        }
    }

    // =========================================================================
    // Test (b): tiny catalogue → shiny Pikachu → ShinyDex, non-shiny → RegularDex,
    //           duplicate shiny → ShinyTrades. No target in buffer boxes.
    // =========================================================================
    {
        // Pikachu dex# = 25.
        PA shiny_pika{};
        shiny_pika.dex_number        = 25;
        shiny_pika.shiny             = true;
        shiny_pika.ot_name           = "nicole";
        shiny_pika.iv_read           = true;
        shiny_pika.iv_best_count     = 5;
        shiny_pika.iv_total_estimate = 150;
        shiny_pika.primary_type      = Pokemon::PokemonType::ELECTRIC;

        PA normal_pika{};
        normal_pika.dex_number        = 25;
        normal_pika.shiny             = false;
        normal_pika.ot_name           = "nicole";
        normal_pika.iv_read           = true;
        normal_pika.iv_best_count     = 4;
        normal_pika.iv_total_estimate = 130;
        normal_pika.primary_type      = Pokemon::PokemonType::ELECTRIC;

        PA dup_shiny_pika{};
        dup_shiny_pika.dex_number        = 25;
        dup_shiny_pika.shiny             = true;
        dup_shiny_pika.ot_name           = "ash";   // lower priority
        dup_shiny_pika.iv_read           = true;
        dup_shiny_pika.iv_best_count     = 0;
        dup_shiny_pika.iv_total_estimate = 30;
        dup_shiny_pika.primary_type      = Pokemon::PokemonType::ELECTRIC;

        // scan_start = shiny_dex_start - 1 = 0 (absolute 0-indexed box)
        std::vector<std::optional<PA>> catalogue = {shiny_pika, normal_pika, dup_shiny_pika};
        MasterPlanV3 plan = build_master_plan_v3(catalogue, layout, cfg, /*scan_start=*/0);

        // No blocking warnings.
        bool has_blocking = false;
        for (const auto& w : plan.warnings){
            if (w.rfind("[BLOCKING]", 0) == 0) has_blocking = true;
        }
        TEST_RESULT_EQUAL(has_blocking, false);

        // Shiny Pikachu (index 0) → should move to ShinyDex region (boxes 1-35 = 0-indexed 0-34).
        // shiny_dex_slot(25, {}) = 24 (0-indexed slot in shiny dex region).
        // target flat = 0*30 + 24 = 24. box (0-indexed) = 0, slot = 24.
        // shiny pika starts at flat=0, target=24 → needs to move.
        bool found_shiny_move_to_shiny_region = false;
        for (const auto& mv : plan.moves){
            // ShinyDex boxes 1-35 → 0-indexed 0-34
            if (mv.to.box >= 0 && mv.to.box <= 34){
                found_shiny_move_to_shiny_region = true;
            }
        }
        TEST_RESULT_EQUAL(found_shiny_move_to_shiny_region, true);

        // Regular pika (index 1) → RegularDex region (boxes 41-75 = 0-indexed 40-74).
        // regular_dex_slot(25) = 24 (0-indexed in regular dex region).
        // target flat = (41-1)*30 + 24 = 40*30 + 24 = 1224. box (0-indexed) = 40.
        bool found_normal_in_regular_dex = false;
        for (const auto& mv : plan.moves){
            // RegularDex 0-indexed 40-74
            if (mv.to.box >= 40 && mv.to.box <= 74){
                found_normal_in_regular_dex = true;
            }
        }
        TEST_RESULT_EQUAL(found_normal_in_regular_dex, true);

        // Dup shiny (index 2) → ShinyTrades (boxes 99-100 = 0-indexed 98-99).
        bool found_dup_in_shinytrades = false;
        for (const auto& mv : plan.moves){
            if (mv.to.box >= 98 && mv.to.box <= 99){
                found_dup_in_shinytrades = true;
            }
        }
        TEST_RESULT_EQUAL(found_dup_in_shinytrades, true);

        // =====================================================================
        // Test (c): NEVER assign a target into the two buffer regions.
        //   Buffer 1: boxes 36-40 (0-indexed 35-39)
        //   Buffer 2: boxes 76-80 (0-indexed 75-79)
        // =====================================================================
        bool target_in_buffer = false;
        for (const auto& mv : plan.moves){
            const size_t b = mv.to.box;
            if ((b >= 35 && b <= 39) || (b >= 75 && b <= 79)){
                target_in_buffer = true;
            }
        }
        TEST_RESULT_EQUAL(target_in_buffer, false);

        // box_map must be present and have 16 entries.
        TEST_RESULT_EQUAL(plan.box_map.size(), (size_t)16);

        // =====================================================================
        // Test (d): slot_routes must be populated for placed Pokémon.
        // catalogue has 3 entries (indices 0, 1, 2) — all placed.
        // slot_routes must be sized == catalogue.size() == 3.
        // For each placed entry (index 0, 1, 2):
        //   - category must be non-empty
        //   - dest_box must be > 0 (1-indexed)
        // (shiny_pika → ShinyDex, normal_pika → RegularDex, dup_shiny_pika → ShinyTrades)
        // =====================================================================
        TEST_RESULT_EQUAL(plan.slot_routes.size(), (size_t)3);

        // shiny_pika (index 0): keeper → ShinyDex, dest_box in shiny dex region (1-35)
        TEST_RESULT_COMPONENT_EQUAL(
            !plan.slot_routes[0].category.empty(), true,
            "slot_routes[0].category non-empty"
        );
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[0].dest_box > 0, true,
            "slot_routes[0].dest_box > 0"
        );
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[0].category, std::string("ShinyDex"),
            "slot_routes[0].category == ShinyDex"
        );
        // ShinyDex target: shiny_dex_slot(25,{})=24, dest_box = 1-indexed box 1 (24/30=0 → box 1)
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[0].dest_box, 1,
            "slot_routes[0].dest_box == 1"
        );

        // normal_pika (index 1): keeper → RegularDex, dest_box in regular dex region (41-75)
        TEST_RESULT_COMPONENT_EQUAL(
            !plan.slot_routes[1].category.empty(), true,
            "slot_routes[1].category non-empty"
        );
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[1].dest_box > 0, true,
            "slot_routes[1].dest_box > 0"
        );
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[1].category, std::string("RegularDex"),
            "slot_routes[1].category == RegularDex"
        );
        // RegularDex target: regular_dex_slot(25)=24, dest_box = 1-indexed box 41 (40*30+24 → abs_box=40 → box 41)
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[1].dest_box, 41,
            "slot_routes[1].dest_box == 41"
        );

        // dup_shiny_pika (index 2): duplicate → ShinyTrades, dest_box in 99-100
        TEST_RESULT_COMPONENT_EQUAL(
            !plan.slot_routes[2].category.empty(), true,
            "slot_routes[2].category non-empty"
        );
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[2].dest_box > 0, true,
            "slot_routes[2].dest_box > 0"
        );
        TEST_RESULT_COMPONENT_EQUAL(
            plan.slot_routes[2].category, std::string("ShinyTrades"),
            "slot_routes[2].category == ShinyTrades"
        );
    }

    return 0;
}

int test_pokemonHome_LayoutV3(const ImageViewRGB32& /*image*/){
    using namespace NintendoSwitch::PokemonHome;

    const std::string layout_path      = RESOURCE_PATH() + "PokemonHome/DexTemplates/master_box_layout_v3.json";
    const std::string shiny_lock_path  = RESOURCE_PATH() + "PokemonHome/shiny_locked.json";

    MasterBoxLayoutV3 L = load_master_box_layout_v3(layout_path, shiny_lock_path);

    // shiny_dex_start must be 1 (first physical box).
    TEST_RESULT_EQUAL(L.shiny_dex_start, static_cast<uint16_t>(1));

    // regular_dex_start must be strictly after shiny_dex_start.
    TEST_RESULT_EQUAL(L.regular_dex_start > L.shiny_dex_start, true);

    // category_box_ranges must contain BoxCategory::Junk.
    TEST_RESULT_EQUAL(L.category_box_ranges.count(BoxCategory::Junk) > 0, true);

    // shiny_locked must be non-empty and contain Victini (494) — definitively locked.
    TEST_RESULT_EQUAL(L.shiny_locked.empty(), false);
    TEST_RESULT_EQUAL(L.shiny_locked.count(494) > 0, true);

    return 0;
}

}

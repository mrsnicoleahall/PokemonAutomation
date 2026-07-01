# Master Box Sorter v2 (ability/item/nature/moves) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Read ability, nature, held item, and moves from HOME so the Utility box (and the nature/move parts of Breeding) auto-sort, extending the completed v1 Master Box Sorter.

**Architecture:** Extend `CollectedPokemonInfo` with the new fields; add a `SummaryExtrasReader` (ability/nature/item, same summary screen) and a `MovesReader` (separate moves screen, toggleable), both reusing `OCR::SmallDictionaryMatcher` + existing dictionaries. Add a `BoxCategory::Utility`, a configurable Utility-rules table on `RouterConfig`, and a route step for it. Catalogue JSON round-trips the new fields.

**Tech Stack:** C++23, Qt 6.8.3, Tesseract OCR (dictionary matcher), CMake. Command-line image-fixture tests via `./run-cli-tests.sh`.

**Spec:** `docs/superpowers/specs/2026-07-01-home-master-box-sorter-v2-attributes-design.md`
**Branch:** `feature/master-box-sorter-v2` (already created; base = `main`).

## Global Constraints

- Build with **Qt 6.8.3**; build dir `build_mac`; `-DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos"`. New source files go in `SerialPrograms/cmake/SourceFiles.cmake` (alphabetical in the matching block); reconfigure to pick them up.
- **Tests run ONLY via `./run-cli-tests.sh`** (headless/offscreen). NEVER launch `SerialPrograms.app` directly, pass `--command-line-test-mode` yourself, or create any `SerialPrograms-Settings.json` (triggers a blocking GUI modal). Pure-logic tests register in `TEST_MAP` via `image_void_detector_helper` (single-arg `const ImageViewRGB32&`, ignore image), with a repo-root fixture `CommandLineTests/PokemonHome/<Object>/dummy.png` (copy an existing dummy.png).
- **Readable-only; never release; never overwrite.** New logic may branch only on ability/nature/item/moves (now readable) + v1's fields. Egg group is NOT readable — do not attempt it.
- Namespace `PokemonAutomation::NintendoSwitch::PokemonHome` (inference/programs) / `PokemonAutomation::Pokemon` (shared data). Prefer `static_cast<>`. Mirror existing sibling files' style.
- Resource files (dictionaries) live in the **Packages repo** (`Packages/Resources/...`, a separate git repo) and must be committed there separately; source lives in the main repo.
- Hardware readers (ability/item/moves crops + moves-screen nav) are **compile-verified only** here (no Switch); coordinates are SV/known-seeded placeholders calibrated on the rig. Pure router/planner logic IS unit-tested.
- Commit messages end with a blank line then `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

---

## Task 1: Extend `CollectedPokemonInfo` with ability/nature/item/moves

**Files:**
- Modify: `SerialPrograms/Source/Pokemon/Pokemon_CollectedPokemonInfo.h`
- Modify: `SerialPrograms/Source/Pokemon/Pokemon_CollectedPokemonInfo.cpp`

**Interfaces:**
- Produces new members: `std::string ability_slug=""; std::string nature=""; std::string held_item_slug=""; std::vector<std::string> moves; bool extras_read=false; bool moves_read=false;`
- These appear in `operator==`, `operator<<`, and `save_boxes_data_to_json`.

- [ ] **Step 1: Add fields.** In the header, after the v1 IV fields, add:
```cpp
    // v2: attributes read from the summary (ability/nature/item) and moves screen.
    std::string ability_slug = "";
    std::string nature = "";           // normalized nature slug
    std::string held_item_slug = "";   // "" = none/unread
    std::vector<std::string> moves;    // up to 4 normalized move slugs
    bool extras_read = false;          // ability/nature/item were read
    bool moves_read = false;           // moves screen was read
```
Add `#include <vector>` and `#include <string>` if not already present.

- [ ] **Step 2: Update helpers** in the `.cpp` (`operator==`, `operator<<`, `save_boxes_data_to_json`), mirroring how v1 added the IV fields. Compare all six in `==`; print them in `<<` (moves as a comma-joined list); write JSON keys `ability_slug`, `nature`, `held_item_slug`, `moves` (a JSON array), `extras_read`, `moves_read`.

- [ ] **Step 3: Build.**
```
cd /Users/nicole/Projects/PokemonAutomator/build_mac && cmake --build . -j 10 --target SerialPrograms 2>&1 | tail -6
```
Expected: builds clean.

- [ ] **Step 4: Commit.**
```
git add SerialPrograms/Source/Pokemon/Pokemon_CollectedPokemonInfo.*
git commit -m "feat(home v2): add ability/nature/item/moves fields to CollectedPokemonInfo

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Add `BoxCategory::Utility` + Utility routing (pure, TDD)

**Files:**
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouter.{h,cpp}`
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxLayout.cpp` (category string↔enum map)
- Modify: `Packages/Resources/PokemonHome/DexTemplates/master_box_layout.json` (add Utility range; commit in Packages repo)
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`

**Interfaces:**
- Consumes: `CollectedPokemonInfo` (Task 1), v1 `route`/`RouterConfig`/`BoxCategory`.
- Produces:
  - `enum class BoxCategory { ... , Utility };` (add `Utility` to the existing enum)
  - `struct UtilityRule { enum Kind { Ability, Item, Move } kind; std::string target_slug; };`
  - add `std::vector<UtilityRule> utility_rules;` to `RouterConfig`
  - `bool p_matches_utility(const CollectedPokemonInfo& p, const std::vector<UtilityRule>& rules);`
  - `route(...)` gains a step **after Breedject and before Events**: `if (p_matches_utility(p, cfg.utility_rules)) return BoxCategory::Utility;`

Default rules the program will populate (Task 6): abilities `flame-body`,`magma-armor`,`synchronize`,`pickup`,`run-away`; items `amulet-coin`,`smoke-ball`; moves `false-swipe`,`pay-day`.

- [ ] **Step 1: Add `Utility` to the `BoxCategory` enum** (end of enum, before any count sentinel if present).

- [ ] **Step 2: Add the layout mapping.** In `PokemonHome_MasterBoxLayout.cpp`, add `{"Utility", BoxCategory::Utility}` to the category-string→enum lookup table (alongside the other 13). In `Packages/Resources/PokemonHome/DexTemplates/master_box_layout.json`, add `"Utility": [60, 60]` to `category_box_ranges`. Commit the JSON in the Packages repo:
```
git -C /Users/nicole/Projects/PokemonAutomator/Packages add Resources/PokemonHome/DexTemplates/master_box_layout.json
git -C /Users/nicole/Projects/PokemonAutomator/Packages commit -m "Add Utility box range to PokemonHome master_box_layout.json"
```
Also copy the updated JSON into the deployed resources so tests load it: `cp Packages/Resources/PokemonHome/DexTemplates/master_box_layout.json build_mac/Resources/PokemonHome/DexTemplates/master_box_layout.json`.

- [ ] **Step 3: Write failing tests** in `PokemonHome_Tests.cpp` (extend the existing router test or add `test_pokemonHome_UtilityRouting`):
```cpp
int test_pokemonHome_UtilityRouting(const ImageViewRGB32&){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;
    std::set<uint16_t> legend, myth, ub, para;
    std::vector<UtilityRule> ur = {
        {UtilityRule::Ability,"flame-body"}, {UtilityRule::Ability,"synchronize"},
        {UtilityRule::Item,"amulet-coin"}, {UtilityRule::Move,"false-swipe"},
    };
    RouterConfig cfg{ {"nicole","cole"}, 6, {3,5}, {1,2}, &legend,&myth,&ub,&para, ur };

    PA hatcher{}; hatcher.dex_number=1; hatcher.ot_name="nicole"; hatcher.iv_read=true; hatcher.ability_slug="flame-body";
    TEST_RESULT_EQUAL((int)route(hatcher,cfg,false,true), (int)BoxCategory::Utility);

    PA catcher{}; catcher.dex_number=286; catcher.ot_name="nicole"; catcher.iv_read=true; catcher.moves={"false-swipe","spore"};
    TEST_RESULT_EQUAL((int)route(catcher,cfg,false,true), (int)BoxCategory::Utility);

    PA money{}; money.dex_number=52; money.ot_name="nicole"; money.iv_read=true; money.held_item_slug="amulet-coin";
    TEST_RESULT_EQUAL((int)route(money,cfg,false,true), (int)BoxCategory::Utility);

    // 6x31 Synchronize mon → Competitive wins (Utility is after IV boxes)
    PA comp{}; comp.dex_number=280; comp.ot_name="nicole"; comp.iv_read=true; comp.iv_best_count=6; comp.ability_slug="synchronize";
    TEST_RESULT_EQUAL((int)route(comp,cfg,false,true), (int)BoxCategory::Competitive);

    // plain mon, no utility match → LivingDex
    PA plain{}; plain.dex_number=1; plain.ot_name="nicole"; plain.iv_read=true;
    TEST_RESULT_EQUAL((int)route(plain,cfg,false,true), (int)BoxCategory::LivingDex);
    return 0;
}
```
Register `"PokemonHome_UtilityRouting"` in `TEST_MAP`; add `CommandLineTests/PokemonHome/UtilityRouting/dummy.png`.

- [ ] **Step 4: Run to verify it fails** (link error `p_matches_utility` / no `Utility`). `./run-cli-tests.sh` → FAIL.

- [ ] **Step 5: Implement** in `PokemonHome_MasterBoxRouter.cpp`:
```cpp
bool p_matches_utility(const CollectedPokemonInfo& p, const std::vector<UtilityRule>& rules){
    for (const UtilityRule& r : rules){
        switch (r.kind){
        case UtilityRule::Ability: if (p.ability_slug == r.target_slug) return true; break;
        case UtilityRule::Item:    if (p.held_item_slug == r.target_slug) return true; break;
        case UtilityRule::Move:
            for (const std::string& m : p.moves){ if (m == r.target_slug) return true; }
            break;
        }
    }
    return false;
}
```
In `route(...)`, insert after the Breedject check and before the Events check:
```cpp
    if (p_matches_utility(p, cfg.utility_rules)) return BoxCategory::Utility;
```
Add `utility_rules` as the last field of `RouterConfig` in the header (with a `// v2` comment). Update the header include for `<vector>`/`<string>` if needed.

- [ ] **Step 6: Run to verify it passes.** `./run-cli-tests.sh` → `PokemonHome_UtilityRouting` PASS (and existing router test still passes).

- [ ] **Step 7: Commit** (main repo). Note in the message that the Packages JSON was committed separately.

---

## Task 3: Planner treats Utility as a route-all bucket (pure, TDD)

**Files:**
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.cpp`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`

**Interfaces:**
- Consumes: `BoxCategory::Utility` (Task 2), v1 `build_master_plan`, `RouterConfig.utility_rules`.
- Produces: Utility slots assigned in read order within the Utility box range, with NO single-slot contention (multiple utility mons coexist), exactly like the other bucket categories (Competitive/Breeding/Events/…).

- [ ] **Step 1: Inspect** how `build_master_plan` assigns bucket-category targets (the append-order path used by Competitive/Breeding). Confirm whether Utility is automatically handled by the generic per-category range logic. If the planner enumerates categories generically from `layout.category_box_ranges`, Utility already works once the range exists (Task 2) — in that case this task only ADDS a regression test. If the planner hardcodes a set of "bucket" categories, add `Utility` to that set.

- [ ] **Step 2: Write a failing/゙regression test** `test_pokemonHome_UtilityPlan`: build a small catalogue with two Utility-matching mons (e.g. two Flame Body mons) + the layout (Utility range `{60,60}`, i.e. slots 60*30-ish in the plan's index space — use the same index convention the existing planner test uses) and assert `build_master_plan` produces target slots for BOTH inside the Utility box range (neither dropped, no contention). Mirror the existing `test_pokemonHome_MasterPlanner` setup for layout/config construction. Register `"PokemonHome_UtilityPlan"` + fixture `CommandLineTests/PokemonHome/UtilityPlan/dummy.png`.

- [ ] **Step 3: Run to verify** (FAIL if Utility isn't a recognized bucket; PASS if the generic path already covers it). `./run-cli-tests.sh`.

- [ ] **Step 4: Implement** the minimal change if needed (add `Utility` to the planner's bucket handling / ensure its range is enumerated). If Step 3 already passed, no code change — keep the test as a regression guard.

- [ ] **Step 5: Run to verify it passes.** `./run-cli-tests.sh` → `PokemonHome_UtilityPlan` PASS.

- [ ] **Step 6: Commit.**

---

## Task 4: `SummaryExtrasReader` — ability / nature / held item (inference)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Inference/PokemonHome_SummaryExtrasReader.{h,cpp}`
- Modify: `SourceFiles.cmake`

**Interfaces:**
- Consumes: `OCR::SmallDictionaryMatcher` (see `Pokemon/Inference/Pokemon_IvJudgeReader.*` for the pattern), `extract_box_reference`, `OCR::ocr_read`, `OCR::normalize_utf32`, `utf32_to_str`, `Language`. Dictionary `Packages/Resources/Pokemon/NatureCheckerOCR.json`.
- Produces:
  - `struct SummaryExtras { std::string ability_slug; std::string nature; std::string held_item_slug; };`
  - `SummaryExtras read_summary_extras(VideoStream& stream, const VideoSnapshot& screen, Language language);`
  - `void make_summary_extras_overlays(VideoOverlaySet& set);`
  - `extern const ImageFloatBox ABILITY_BOX, NATURE_BOX, HELD_ITEM_BOX;`

- [ ] **Step 1: Header** with the struct + declarations, doc comment noting the item crop + all coords are calibrated on the rig (Task 7).

- [ ] **Step 2: Implement.** Seed `ABILITY_BOX = {0.158,0.838,0.213,0.042}` and `NATURE_BOX = {0.157,0.783,0.212,0.042}` (from `PokemonHome_BoxNavigation.cpp`'s existing overlay boxes); `HELD_ITEM_BOX = {0.157,0.728,0.212,0.042}` as a **placeholder to calibrate**. Read ability + item via `OCR::ocr_read`(English) → `normalize_utf32` → store slug (targeted-match happens in the router; store the normalized text). Read nature via a `SmallDictionaryMatcher` bound to `"Pokemon/NatureCheckerOCR.json"` (mirror `IvJudgeReader`'s `read_substring` + `WHITE_TEXT_FILTERS` usage), storing the matched nature slug. `make_summary_extras_overlays` adds the three boxes to the set. Add sources to CMake.

- [ ] **Step 3: Build.** `cmake --build . -j 10 --target SerialPrograms 2>&1 | tail -6` → clean. (No runtime test; calibrated in Task 7.)

- [ ] **Step 4: Commit.**

---

## Task 5: `MovesReader` — moves screen (inference, toggleable)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Inference/PokemonHome_MovesReader.{h,cpp}`
- Modify: `SourceFiles.cmake`
- Create (Packages): `Packages/Resources/PokemonHome/PokemonMovesOCR.json` (copy of `Packages/Resources/PokemonSV/PokemonMovesOCR.json`)

**Interfaces:**
- Consumes: `OCR::SmallDictionaryMatcher`, `extract_box_reference`, `pbf_press_button`, a moves-screen frozen-image/watcher (mirror the Judge nav in `PokemonHome_BoxSorterMaster.cpp`).
- Produces:
  - `const OCR::SmallDictionaryMatcher& MOVES_READER();` (bound to `"PokemonHome/PokemonMovesOCR.json"`)
  - `class MovesReaderScope { MovesReaderScope(VideoOverlay&, Language); std::vector<std::string> read(Logger&, const ImageViewRGB32&); std::vector<ImageViewRGB32> dump_images(const ImageViewRGB32&); };` with four move crops (placeholders to calibrate).
  - Free helper `bool open_moves_screen(...)` / `bool close_moves_screen(...)` OR document that the program (Task 6) performs the nav and passes a moves-screen snapshot to `read`.

- [ ] **Step 1: Copy the dictionary** to the HOME resources and commit in Packages:
```
cp Packages/Resources/PokemonSV/PokemonMovesOCR.json Packages/Resources/PokemonHome/PokemonMovesOCR.json
git -C /Users/nicole/Projects/PokemonAutomator/Packages add Resources/PokemonHome/PokemonMovesOCR.json
git -C /Users/nicole/Projects/PokemonAutomator/Packages commit -m "Add PokemonHome PokemonMovesOCR.json (copy of SV) for Master Box Sorter v2"
cp Packages/Resources/PokemonHome/PokemonMovesOCR.json build_mac/Resources/PokemonHome/PokemonMovesOCR.json
```

- [ ] **Step 2: Header + impl.** Model `MOVES_READER()` on `IvJudgeReader`'s `IV_READER()` (a `static SmallDictionaryMatcher reader("PokemonHome/PokemonMovesOCR.json")`). Four move crops as placeholders, e.g. a vertical stack `{0.62, 0.22+0.12*i, 0.28, 0.06}` for i in 0..3 — **calibrate in Task 7**. `read(...)` OCR-matches each crop against the dictionary and returns up to 4 normalized move slugs (skip blanks/unmatched). `dump_images` returns the four crops. Add sources to CMake.

- [ ] **Step 3: Build** → clean. (No runtime test; calibrated in Task 7.)

- [ ] **Step 4: Commit** (main repo; note Packages JSON committed separately).

---

## Task 6: Program integration — read extras/moves, options, JSON round-trip

**Files:**
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.{h,cpp}`

**Interfaces:**
- Consumes: Tasks 1,2,4,5 (`read_summary_extras`, `MovesReaderScope`, `p_matches_utility`, new fields, `RouterConfig.utility_rules`).
- Produces: extras/moves populated during catalogue; `RouterConfig` built with the default `utility_rules`; new fields round-tripped in the catalogue JSON.

- [ ] **Step 1: Rename/extend** the v1 `read_summary_screen_with_ivs` to `read_summary_screen_with_extras`. After the v1 summary read: if `READ_EXTRAS`, call `read_summary_extras(...)` and assign `ability_slug/nature/held_item_slug` + `extras_read=true` (all on the current summary screen — no navigation). Keep the v1 Judge/IV read. Then if `READ_MOVES`, navigate to the moves screen (button press + watcher, mirroring the Judge nav + the v1 return-to-summary confirmation), build a `MovesReaderScope`, read moves, set `moves`/`moves_read=true`, and return to the summary (confirmed by a `SummaryScreenWatcher`). On any screen-not-found, set the relevant `*_read=false` and continue (never abort the run).

- [ ] **Step 2: Options.** Add `READ_EXTRAS` (bool, default true) and `READ_MOVES` (bool, default true). Add editable Utility target lists (three string-list options: abilities, items, moves) defaulted to the §4 defaults; parse them (normalized) into `RouterConfig.utility_rules` when building the config. Add a startup `env.log` warning (yellow) if a read flag is on but its relevant language/target list is empty.

- [ ] **Step 3: JSON round-trip.** Extend `write_catalogue_incremental` to write `ability_slug`, `nature`, `held_item_slug`, `moves` (array), `extras_read`, `moves_read` (Task 1 already added them to `save_boxes_data_to_json`, but confirm the program's own writer path includes them). Extend `load_catalogue_resume` to deserialize all six symmetrically (mirror the v1 enum-field round-trip the review required).

- [ ] **Step 4: Build** → clean.
```
cd build_mac && cmake ../SerialPrograms -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos" >/dev/null 2>&1; cmake --build . -j 10 2>&1 | tail -8
```

- [ ] **Step 5: Run tests** `./run-cli-tests.sh` — all pure-logic tests still pass (no new hardware test here).

- [ ] **Step 6: Commit.**

---

## Task 7: Extend calibration program for extras + moves

**Files:**
- Modify: `SerialPrograms/Source/PokemonHome/Programs/TestPrograms/PokemonHome_CalibrateIVJudge.{h,cpp}` (extend it; keep the name or add a sibling `CalibrateSummaryExtras` — extending is simpler and one panel)

**Interfaces:**
- Consumes: `SummaryExtrasReader`, `MovesReaderScope`.
- Produces: the calibration program also dumps the ability/nature/item crops (from the summary) and, in a moves step, navigates to the moves screen and dumps the four move crops + logs the read values.

- [ ] **Step 1: Extend `program()`** to, after the existing Judge dump: draw `make_summary_extras_overlays`, snapshot the summary, call `read_summary_extras` + log, and `dump_image` the ability/nature/item crops. Then (guarded by a `READ_MOVES` bool option) press to open the moves screen, build `MovesReaderScope`, log the read moves, and dump its four crops. Return to summary.

- [ ] **Step 2: Build** → clean; confirm the panel still registers.

- [ ] **Step 3: Commit.**

---

## Task 8: Final build, tests, docs

**Files:**
- Modify: `docs/BoxSorterMaster-usage.md` (Utility box is now auto-sorted; note READ_EXTRAS/READ_MOVES options + calibration of the new crops/moves screen; egg group still manual)
- Modify: `docs/hardware-bringup.md` (Phase 4 now also calibrates ability/nature/item + moves crops)

- [ ] **Step 1: Full clean rebuild.** `rm -rf build_mac && mkdir build_mac && cd build_mac && cmake ../SerialPrograms -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos" && cmake --build . -j 10 2>&1 | tail -5` → `[100%] Built target SerialPrograms`. (Validates the Packages dictionaries auto-deploy.)

- [ ] **Step 2: Run the full test group** via `./run-cli-tests.sh` — IvSummary, MasterBoxLayout, MasterBoxRouter, MasterPlanner, UtilityRouting, UtilityPlan all pass.

- [ ] **Step 3: Update the two docs** to reflect that the Utility box auto-sorts (Hatcher/Synchronizer/Farmers/Evader/Money-Maker/Catcher), the new options, and that ability/item/moves crops + the moves screen need calibration. Note Breloom + Smeargle both match the Catcher move rule.

- [ ] **Step 4: Commit.**

---

## Self-Review notes (author)

- **Spec coverage:** §2 data model → Task 1; §3 readers (extras) → Task 4, (moves) → Task 5; §4 router Utility + rules → Task 2, planner route-all → Task 3; §5 program integration/options/JSON → Task 6; §6 calibration → Task 7, pure tests → Tasks 2–3, final build/tests → Task 8; §7 limits (egg group) → documented in Task 8.
- **Naming consistency:** `SummaryExtras`/`read_summary_extras`/`SummaryExtrasReader`, `MovesReaderScope`/`MOVES_READER`, `UtilityRule`/`p_matches_utility`/`utility_rules`, `BoxCategory::Utility`, `ability_slug`/`nature`/`held_item_slug`/`moves`/`extras_read`/`moves_read` used identically across tasks.
- **Hardware placeholders (calibrated Task 7):** `HELD_ITEM_BOX`, the four move crops, and the moves-screen open/close nav + timing. Ability/nature boxes are seeded from the existing reader overlays.
- **Ordering trap avoided:** Utility routes AFTER the IV boxes (spec §4) so a flawless Synchronize mon stays Competitive — asserted in Task 2's test.

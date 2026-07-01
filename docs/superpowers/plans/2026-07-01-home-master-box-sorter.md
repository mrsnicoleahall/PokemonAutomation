# HOME Master Box Sorter — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a resumable, space-safe "Master Box Sorter" program to SerialPrograms that sorts a large Pokémon HOME collection into Nicole's Master Box Layout using only data readable from the HOME summary + Judge screens.

**Architecture:** New program `PokemonHome:BoxSorterMaster`, a sibling of the existing `BoxSorterLivingDex`, reusing its box-navigation/summary-reading/swap machinery. Two passes with JSON checkpoints: (1) Catalogue every occupied box → `home_catalogue.json`; (2) Plan target layout → `home_plan.json`, then Execute swaps → `home_progress.json`. A new `IVJudgeReader` inference reads per-stat IV quality from the Judge screen. A pure `MasterBoxRouter` maps each read Pokémon to a target box category. Unresolvable Pokémon go to Manual-Sort staging boxes; nothing is ever released.

**Tech Stack:** C++23, Qt 6.8.3, Tesseract OCR, OpenCV, CMake. Command-line image-fixture test framework in `SerialPrograms/Source/Tests/`.

**Spec:** `docs/superpowers/specs/2026-07-01-home-master-box-sorter-design.md`

## Global Constraints

- **Qt version:** build with **Qt 6.8.3 exactly**. Qt ≥ 6.9 breaks the build (upstream issue #570). Homebrew `qt6` is 6.9+, so 6.8.3 must be pinned deliberately.
- **Packages data:** the separate `Packages` data folder must sit inside the source root for the app to run and for `RESOURCE_PATH()` to resolve.
- **Readable-only:** the sorter may branch ONLY on: dex #, shiny, gender, ball, OT name, OT ID, primary/secondary type, gmax, alpha, tera type, origin mark, and IV tiers from the Judge screen. Never branch on nature/EV/ribbon/mark/favorite/egg-moves (unreadable).
- **Never destructive:** no auto-release; when out of space, stop with a `UserSetupError` — never overwrite an occupied slot.
- **OT names are a list:** owner OTs are `Nicole` and `cole` (lowercase c). Compare after `OCR::normalize_utf32` (lowercases + strips non-alphanumerics), exact full-string match against any list entry.
- **Thresholds (defaults, runtime-tunable):** Competitive = 6×31; Breeding = 3–5×31; Breedjects = 1–2×31; perfect-IV (6×31) = never-junk + wins dex tie-break.
- **No end-to-end runtime test in this environment** (needs Switch + microcontroller + capture card). Verify by: compile, pure-logic unit tests, and Dry-Run/calibration on Nicole's rig.
- **Namespaces:** all new code in `PokemonAutomation::NintendoSwitch::PokemonHome` (inference/programs) or `PokemonAutomation::Pokemon` (shared data), matching neighbors.
- **Commit style:** small commits per task; end messages with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

---

## Phase 0 — Build baseline

### Task 0: Get a clean baseline build

**Files:** none created; establishes the build works before changes.

**Interfaces:**
- Produces: a working `build_mac/SerialPrograms` app and the ability to run `COMMAND_LINE_TESTS`.

- [ ] **Step 1: Install Qt 6.8.3 (pinned).** Homebrew's default is 6.9+. Install 6.8.3 via the Qt online installer or `aqtinstall`:

```bash
python3 -m pip install --user aqtinstall
python3 -m aqt install-qt mac desktop 6.8.3 clang_64 -O "$HOME/Qt"
```

Note the path `$HOME/Qt/6.8.3/macos` (has `clang_64` on some layouts) — this is `CMAKE_PREFIX_PATH`.

- [ ] **Step 2: Confirm the other deps are present.**

Run: `brew list | grep -iE 'cmake|tesseract|opencv'`
Expected: `cmake`, `opencv`, `tesseract` all listed. (Installed already.)

- [ ] **Step 3: Ensure the `Packages` data folder is present.** Clone/download `PokemonAutomation/Packages` and place it in the source root:

```bash
ls /Users/nicole/Projects/PokemonAutomator/Packages >/dev/null 2>&1 && echo "present" || echo "MISSING - download from https://github.com/PokemonAutomation/Packages and place at repo root"
```
Expected: `present` (fetch it if missing — required to run).

- [ ] **Step 4: Configure + build.**

```bash
cd /Users/nicole/Projects/PokemonAutomator
mkdir -p build_mac && cd build_mac
cmake ../SerialPrograms -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos"
cmake --build . -j 10
```
Expected: ends with `[100%] Built target SerialPrograms`.

- [ ] **Step 5: Confirm the test harness runs.** The framework is toggled in `SerialPrograms-Settings.json` (`"20-GlobalSettings" → "COMMAND_LINE_TESTS" → "RUN": true`) with a `FOLDER` pointing at a `CommandLineTests/` tree (see `Tests/CommandLineTests.h`). Launch once, enable it, confirm the existing `PokemonHome_SummaryScreen` test is discovered. No new assertion yet — this just proves the harness path.

- [ ] **Step 6: Commit nothing** (baseline only). Record the working Qt path in a scratch note if helpful.

---

## Phase 1 — Data model & pure logic (unit-testable, no hardware)

### Task 1: Extend `CollectedPokemonInfo` with IV fields

**Files:**
- Modify: `SerialPrograms/Source/Pokemon/Pokemon_CollectedPokemonInfo.h`
- Modify: `SerialPrograms/Source/Pokemon/Pokemon_CollectedPokemonInfo.cpp`

**Interfaces:**
- Produces: new members on `CollectedPokemonInfo`:
  - `bool iv_read = false;`
  - `uint8_t iv_best_count = 0;`   // # of stats judged "Best" (=31)
  - `uint16_t iv_total_estimate = 0;` // sum of tier midpoints, 0..186
  - `bool iv_perfect = false;`     // iv_best_count == 6
- These appear in `operator==`, `operator<<`, and `save_boxes_data_to_json`.

- [ ] **Step 1: Add the fields.** In `Pokemon_CollectedPokemonInfo.h`, after `OriginMark origin_mark = OriginMark::NONE;` (line ~40) add:

```cpp
    // IV data from the HOME Judge screen (see PokemonHome_IVJudgeReader).
    bool iv_read = false;
    uint8_t iv_best_count = 0;       // number of stats judged "Best" (31)
    uint16_t iv_total_estimate = 0;  // sum of per-stat tier midpoints (0..186)
    bool iv_perfect = false;         // iv_best_count == 6
```

- [ ] **Step 2: Update the serialization/compare helpers.** In `Pokemon_CollectedPokemonInfo.cpp`, find `operator==`, `operator<<`, and `save_boxes_data_to_json` (each has a "new struct members" comment per the header). Add the four fields to each: compare them in `==`, print them in `<<` (e.g. ` iv31:` + count + ` ivperfect:` + bool), and write them as JSON keys `iv_read`, `iv_best_count`, `iv_total_estimate`, `iv_perfect` in the saver.

- [ ] **Step 3: Build to confirm it compiles.**

```bash
cd /Users/nicole/Projects/PokemonAutomator/build_mac && cmake --build . -j 10 --target SerialPrograms 2>&1 | tail -5
```
Expected: builds clean (no errors referencing `CollectedPokemonInfo`).

- [ ] **Step 4: Commit.**

```bash
git add SerialPrograms/Source/Pokemon/Pokemon_CollectedPokemonInfo.*
git commit -m "feat(home): add IV fields to CollectedPokemonInfo

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: `IVSummary` over shared `IvJudgeValue` (pure, TDD)

**REUSE, don't reinvent:** the codebase already has `Pokemon::IvJudgeValue`
(`Pokemon/Pokemon_IvJudge.h`) and the `Pokemon::IvJudgeReader::Results` struct
(`Pokemon/Inference/Pokemon_IvJudgeReader.h`). This task only adds the pure helper
that condenses six `IvJudgeValue`s into the summary fields from Task 1.

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Inference/PokemonHome_IvSummary.h`
- Create: `SerialPrograms/Source/PokemonHome/Inference/PokemonHome_IvSummary.cpp`
- Modify: `SerialPrograms/Source/Tests/PokemonHome_Tests.h` / `.cpp`, `SerialPrograms/Source/Tests/TestMap.cpp`
- Modify: `SerialPrograms/CMakeLists.txt` (add the two new source files)

**Interfaces:**
- Consumes: `Pokemon::IvJudgeValue`, `Pokemon::IvJudgeReader::Results`.
- Produces:
  - `struct IVSummary { bool read; uint8_t best_count; uint16_t total_estimate; bool perfect; };`
  - `uint8_t iv_value_midpoint(Pokemon::IvJudgeValue v);` — Best/HyperTrained=31, Fantastic=30, VeryGood=27, PrettyGood=20, Decent=8, NoGood/UnableToDetect=0.
  - `IVSummary summarize_ivs(const Pokemon::IvJudgeReader::Results& r);` — counts Best OR HyperTrained toward `best_count`; if any stat is `UnableToDetect`, sets `read=false`.

- [ ] **Step 1: Write the header** `PokemonHome_IvSummary.h` with `IVSummary`, `iv_value_midpoint`, `summarize_ivs`, including `Pokemon/Inference/Pokemon_IvJudgeReader.h`, in `namespace PokemonAutomation { namespace NintendoSwitch { namespace PokemonHome {`.

- [ ] **Step 2: Write the failing test.** In `Tests/PokemonHome_Tests.h` declare:

```cpp
int test_pokemonHome_IvSummary(const ImageViewRGB32& image, const std::vector<std::string>& keywords);
```

In `Tests/PokemonHome_Tests.cpp` add (image ignored — pure logic):

```cpp
#include "PokemonHome/Inference/PokemonHome_IvSummary.h"
#include "Pokemon/Inference/Pokemon_IvJudgeReader.h"
// ...
int test_pokemonHome_IvSummary(const ImageViewRGB32&, const std::vector<std::string>&){
    using namespace NintendoSwitch::PokemonHome;
    using Pokemon::IvJudgeValue;
    using R = Pokemon::IvJudgeReader::Results;

    R flawless{IvJudgeValue::Best,IvJudgeValue::Best,IvJudgeValue::Best,
               IvJudgeValue::Best,IvJudgeValue::Best,IvJudgeValue::HyperTrained};
    IVSummary s = summarize_ivs(flawless);
    TEST_RESULT_EQUAL(s.best_count, (uint8_t)6);   // HyperTrained counts as 31
    TEST_RESULT_EQUAL(s.perfect, true);
    TEST_RESULT_EQUAL(s.read, true);
    TEST_RESULT_EQUAL(s.total_estimate, (uint16_t)186);

    R mixed{IvJudgeValue::Best,IvJudgeValue::Best,IvJudgeValue::VeryGood,
            IvJudgeValue::Decent,IvJudgeValue::NoGood,IvJudgeValue::UnableToDetect};
    IVSummary m = summarize_ivs(mixed);
    TEST_RESULT_EQUAL(m.best_count, (uint8_t)2);
    TEST_RESULT_EQUAL(m.perfect, false);
    TEST_RESULT_EQUAL(m.read, false);   // UnableToDetect -> not fully read
    return 0;
}
```

Register in `Tests/TestMap.cpp` inside `TEST_MAP`:

```cpp
    {"PokemonHome_IvSummary", std::bind(image_void_detector_helper, test_pokemonHome_IvSummary, _1)},
```

Create a 1×1 dummy fixture so the folder is non-empty: `CommandLineTests/PokemonHome_IvSummary/dummy.png`.

- [ ] **Step 3: Run to verify it fails.** Build; run the app with `COMMAND_LINE_TESTS.RUN=true`, `TEST_LIST=["PokemonHome_IvSummary"]`.
Expected: FAIL / link error (`summarize_ivs` undefined).

- [ ] **Step 4: Implement** `PokemonHome_IvSummary.cpp`:

```cpp
#include "Pokemon/Pokemon_IvJudge.h"
#include "PokemonHome_IvSummary.h"
namespace PokemonAutomation{ namespace NintendoSwitch{ namespace PokemonHome{
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
        s.total_estimate += iv_value_midpoint(v);
    }
    s.perfect = (s.best_count == 6);
    return s;
}
}}}
```

Add both source files to `SerialPrograms/CMakeLists.txt` (search for `PokemonHome_ButtonDetector.cpp` and add the new paths beside it in the same source list).

- [ ] **Step 5: Run to verify it passes.** Rebuild + rerun the test.
Expected: `PokemonHome_IvSummary` PASS.

- [ ] **Step 6: Commit.**

```bash
git add SerialPrograms/Source/PokemonHome/Inference/PokemonHome_IvSummary.* SerialPrograms/Source/Tests/* SerialPrograms/CMakeLists.txt CommandLineTests/PokemonHome_IvSummary/
git commit -m "feat(home): IVSummary over shared IvJudgeValue with tests

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: `MasterBoxRouter` — pure routing logic (TDD, the heart)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouter.h`
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouter.cpp`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`, `SerialPrograms/CMakeLists.txt`

**Interfaces:**
- Consumes: `CollectedPokemonInfo` (Task 1), the dex-number sets from the layout config (Task 4 provides the loaded `MasterBoxLayout`; for this task the router takes the sets as parameters so it stays pure and testable without file IO).
- Produces:
  - `enum class BoxCategory { LivingDex, Competitive, Breeding, Breedject, Events, GoodTrades, DuplicateShiny, Legendary, Mythical, UltraBeast, Paradox, ManualForms, ManualOther };`
  - `struct RouterConfig { std::set<std::string> owner_ot_names; uint8_t competitive_min31; std::pair<uint8_t,uint8_t> breeding_range; std::pair<uint8_t,uint8_t> breedject_range; const std::set<uint16_t>* legendary; const std::set<uint16_t>* mythical; const std::set<uint16_t>* ultra_beast; const std::set<uint16_t>* paradox; };`
  - `BoxCategory route(const CollectedPokemonInfo& p, const RouterConfig& cfg, bool species_slot_taken_by_shiny, bool base_form_signature_matches);`

Routing order (first match wins), per spec §4:
1. `!base_form_signature_matches` → `ManualForms`
2. shiny && `species_slot_taken_by_shiny` → `DuplicateShiny`
3. OT ∉ owner && not shiny/legendary/mythical/ultra-beast/paradox/event → `GoodTrades`
   (rare collectible species fall through to their collection box in step 8)
4. `iv_read && best_count >= competitive_min31` → `Competitive`
5. `iv_read && best_count in breeding_range` → `Breeding`
6. `iv_read && best_count in breedject_range` → `Breedject`
7. event (cherish ball OR origin_mark ∈ {GO, LGPE, GAMEBOY}) → `Events`
8. dex ∈ mythical → `Mythical`; ∈ legendary → `Legendary`; ∈ ultra_beast → `UltraBeast`; ∈ paradox → `Paradox`
9. else → `LivingDex`

(Helper `bool is_owner_ot(const CollectedPokemonInfo&, const std::set<std::string>&)` compares `p.ot_name` — already normalized at read time — against the set.)

- [ ] **Step 1: Write the header** with the enum, `RouterConfig`, `route`, and `is_owner_ot` declarations.

- [ ] **Step 2: Write failing tests.** In `Tests/PokemonHome_Tests.*` add `test_pokemonHome_MasterBoxRouter`. Build the config inline and assert each branch:

```cpp
int test_pokemonHome_MasterBoxRouter(const ImageViewRGB32&, const std::vector<std::string>&){
    using namespace NintendoSwitch::PokemonHome;
    using PA = Pokemon::CollectedPokemonInfo;
    std::set<uint16_t> legend{144,145,146}, myth{151}, ub{793}, para{984};
    RouterConfig cfg{ {"nicole","cole"}, 6, {3,5}, {1,2}, &legend, &myth, &ub, &para };

    PA base{}; base.dex_number=1; base.ot_name="nicole"; base.iv_read=true; base.iv_best_count=0;
    TEST_RESULT_EQUAL((int)route(base,cfg,false,true), (int)BoxCategory::LivingDex);

    PA form=base; // wrong form signature
    TEST_RESULT_EQUAL((int)route(form,cfg,false,false), (int)BoxCategory::ManualForms);

    PA dupshiny=base; dupshiny.shiny=true;
    TEST_RESULT_EQUAL((int)route(dupshiny,cfg,true,true), (int)BoxCategory::DuplicateShiny);

    PA trade=base; trade.ot_name="ash";
    TEST_RESULT_EQUAL((int)route(trade,cfg,false,true), (int)BoxCategory::GoodTrades);

    PA comp=base; comp.iv_best_count=6;
    TEST_RESULT_EQUAL((int)route(comp,cfg,false,true), (int)BoxCategory::Competitive);

    PA breed=base; breed.iv_best_count=4;
    TEST_RESULT_EQUAL((int)route(breed,cfg,false,true), (int)BoxCategory::Breeding);

    PA breedj=base; breedj.iv_best_count=1;
    TEST_RESULT_EQUAL((int)route(breedj,cfg,false,true), (int)BoxCategory::Breedject);

    PA myth1=base; myth1.dex_number=151;
    TEST_RESULT_EQUAL((int)route(myth1,cfg,false,true), (int)BoxCategory::Mythical);

    PA leg=base; leg.dex_number=144;
    TEST_RESULT_EQUAL((int)route(leg,cfg,false,true), (int)BoxCategory::Legendary);
    return 0;
}
```
Register `PokemonHome_MasterBoxRouter` in `TEST_MAP` + add `CommandLineTests/PokemonHome_MasterBoxRouter/dummy.png`.

- [ ] **Step 3: Run to verify it fails** (link error: `route` undefined). Expected: FAIL.

- [ ] **Step 4: Implement `PokemonHome_MasterBoxRouter.cpp`** following the routing order above. Event helper:

```cpp
static bool is_event(const CollectedPokemonInfo& p){
    if (p.ball_slug == "cherish-ball") return true;
    switch (p.origin_mark){
        case Pokemon::OriginMark::GO:
        case Pokemon::OriginMark::LGPE:
        case Pokemon::OriginMark::GAMEBOY: return true;
        default: return false;
    }
}
bool is_owner_ot(const CollectedPokemonInfo& p, const std::set<std::string>& owners){
    return owners.find(p.ot_name) != owners.end();
}
BoxCategory route(const CollectedPokemonInfo& p, const RouterConfig& cfg,
                  bool species_slot_taken_by_shiny, bool base_form_signature_matches){
    if (!base_form_signature_matches) return BoxCategory::ManualForms;
    if (p.shiny && species_slot_taken_by_shiny) return BoxCategory::DuplicateShiny;
    bool rare_species = cfg.legendary->count(p.dex_number) || cfg.mythical->count(p.dex_number)
                     || cfg.ultra_beast->count(p.dex_number) || cfg.paradox->count(p.dex_number);
    if (!is_owner_ot(p, cfg.owner_ot_names) && !p.shiny && !rare_species && !is_event(p))
        return BoxCategory::GoodTrades;
    if (p.iv_read && p.iv_best_count >= cfg.competitive_min31) return BoxCategory::Competitive;
    if (p.iv_read && p.iv_best_count >= cfg.breeding_range.first && p.iv_best_count <= cfg.breeding_range.second) return BoxCategory::Breeding;
    if (p.iv_read && p.iv_best_count >= cfg.breedject_range.first && p.iv_best_count <= cfg.breedject_range.second) return BoxCategory::Breedject;
    if (is_event(p)) return BoxCategory::Events;
    if (cfg.mythical->count(p.dex_number)) return BoxCategory::Mythical;
    if (cfg.legendary->count(p.dex_number)) return BoxCategory::Legendary;
    if (cfg.ultra_beast->count(p.dex_number)) return BoxCategory::UltraBeast;
    if (cfg.paradox->count(p.dex_number)) return BoxCategory::Paradox;
    return BoxCategory::LivingDex;
}
```
Add sources to `CMakeLists.txt`.

- [ ] **Step 5: Run to verify it passes.** Expected: `PokemonHome_MasterBoxRouter` PASS.

- [ ] **Step 6: Commit.**

```bash
git add SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouter.* SerialPrograms/Source/Tests/* SerialPrograms/CMakeLists.txt CommandLineTests/PokemonHome_MasterBoxRouter/
git commit -m "feat(home): MasterBoxRouter pure routing logic with tests

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Master layout config + loader

**Files:**
- Create: `Packages/Resources/PokemonHome/DexTemplates/master_box_layout.json`
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxLayout.h`
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxLayout.cpp`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `load_json_file` + `JsonObject/JsonArray` (see usage in `BoxSorterLivingDex.cpp:342`).
- Produces:
  - `struct MasterBoxLayout { uint16_t living_dex_start_box; std::map<BoxCategory,std::pair<uint16_t,uint16_t>> category_box_ranges; std::set<uint16_t> legendary, mythical, ultra_beast, paradox; };`
  - `MasterBoxLayout load_master_box_layout(const std::string& path);`

JSON shape (box numbers = spec defaults; `[start,end]` inclusive, 1-indexed):

```json
{
  "living_dex_start_box": 1,
  "category_box_ranges": {
    "LivingDex": [1, 35], "ManualForms": [36, 52],
    "Legendary": [53, 53], "Mythical": [54, 54], "UltraBeast": [55, 55], "Paradox": [56, 56],
    "Events": [60, 60], "Competitive": [61, 61], "Breeding": [62, 62], "Breedject": [63, 63],
    "GoodTrades": [64, 64], "DuplicateShiny": [65, 65], "ManualOther": [66, 66]
  },
  "legendary": [144,145,146,150,243,244,245,249,250,377,378,379,380,381,382,383,384,480,481,482,483,484,485,486,487,488,638,639,640,641,642,643,644,645,646,716,717,718,772,773,785,786,787,788,789,790,791,792,800,888,889,890,891,892,894,895,896,897,898,905,1001,1002,1003,1004,1007,1008,1014,1015,1016,1017],
  "mythical": [151,251,385,386,489,490,491,492,493,494,647,648,649,719,720,721,801,802,807,808,809,893,1025],
  "ultra_beast": [793,794,795,796,797,798,799,803,804,805,806],
  "paradox": [984,985,986,987,988,989,990,991,992,993,994,995,1005,1006,1009,1010,1020,1021,1022,1023]
}
```

- [ ] **Step 1: Create the JSON file** exactly as above in the `Packages` resources path. (These dex-number sets are the initial best-effort; refine later — they are data, not logic.)

- [ ] **Step 2: Write the header** with `MasterBoxLayout` + `load_master_box_layout`.

- [ ] **Step 3: Write a failing test** that loads the file and checks a few invariants:

```cpp
int test_pokemonHome_MasterBoxLayout(const ImageViewRGB32&, const std::vector<std::string>&){
    using namespace NintendoSwitch::PokemonHome;
    MasterBoxLayout L = load_master_box_layout(RESOURCE_PATH() + "PokemonHome/DexTemplates/master_box_layout.json");
    TEST_RESULT_EQUAL(L.living_dex_start_box, (uint16_t)1);
    TEST_RESULT_EQUAL(L.mythical.count(151) > 0, true);
    TEST_RESULT_EQUAL(L.legendary.count(144) > 0, true);
    TEST_RESULT_EQUAL(L.category_box_ranges.at(BoxCategory::LivingDex).second, (uint16_t)35);
    return 0;
}
```
Register + add dummy fixture folder.

- [ ] **Step 4: Run to verify it fails** (undefined loader). Expected: FAIL.

- [ ] **Step 5: Implement the loader** using the `BoxSorterLivingDex.cpp:342-358` JSON pattern (`load_json_file`, `to_object_throw`, `get_integer_throw`, iterate arrays). Map category strings→`BoxCategory` with a small `EnumStringMap`-style lookup. Add sources to CMake.

- [ ] **Step 6: Run to verify it passes.** Expected: PASS.

- [ ] **Step 7: Commit** (source in Arduino-Source; the JSON belongs in the `Packages` repo — note in the commit that the resource must be submitted to `Packages` separately).

---

## Phase 2 — Hardware inference (calibrate on rig; not unit-testable here)

### Task 5: `PokemonHome_IvJudgeReader` — thin scope over the shared engine

**REUSE:** model this file **exactly** on `PokemonSV/Inference/Boxes/PokemonSV_IvJudgeReader.{h,cpp}`.
The shared `Pokemon::IvJudgeReader` does the multi-language dictionary OCR; this task
only supplies HOME's six crop boxes + an OCR dictionary JSON.

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Inference/PokemonHome_IvJudgeReader.h`
- Create: `SerialPrograms/Source/PokemonHome/Inference/PokemonHome_IvJudgeReader.cpp`
- Create: `Packages/Resources/PokemonHome/IVCheckerOCR.json` (copy of `Packages/Resources/PokemonSV/IVCheckerOCR.json`)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Pokemon::IvJudgeReader`, `Pokemon::IvJudgeValue`, `OverlayBoxScope`, `extract_box_reference`, `OCR::WHITE_TEXT_FILTERS` (see `PokemonSV_IvJudgeReader.cpp`).
- Produces:
  - `const Pokemon::IvJudgeReader& IV_READER();` — bound to `"PokemonHome/IVCheckerOCR.json"`.
  - `class IvJudgeReaderScope` with ctor `(VideoOverlay&, Language)`, `Results read(Logger&, const ImageViewRGB32&)`, and `std::vector<ImageViewRGB32> dump_images(const ImageViewRGB32&)` — identical shape to SV's.

- [ ] **Step 1: Copy `PokemonSV_IvJudgeReader.{h,cpp}`** to the HOME Inference folder; rename namespace `PokemonSV`→`PokemonHome`, the include guard, and the JSON path in `IV_READER()` to `"PokemonHome/IVCheckerOCR.json"`.

- [ ] **Step 2: Copy the OCR dictionary** `Packages/Resources/PokemonSV/IVCheckerOCR.json` → `Packages/Resources/PokemonHome/IVCheckerOCR.json` (same rating tokens across all languages; identical for HOME). Commit note: this resource is submitted to the `Packages` repo separately.

- [ ] **Step 3: Seed the six crop boxes from SV** (HOME's hexagonal stat chart is nearly identical; fine-tuned in Task 6):

```cpp
IvJudgeReaderScope::IvJudgeReaderScope(VideoOverlay& overlay, Language language)
    : m_language(language)
    , m_box_hp      (overlay, {0.825, 0.192, 0.110, 0.052})
    , m_box_attack  (overlay, {0.886, 0.302, 0.110, 0.052})
    , m_box_defense (overlay, {0.886, 0.406, 0.110, 0.052})
    , m_box_spatk   (overlay, {0.660, 0.302, 0.110, 0.052})
    , m_box_spdef   (overlay, {0.660, 0.406, 0.110, 0.052})
    , m_box_speed   (overlay, {0.825, 0.470, 0.110, 0.052})
{}
```
(The `read(...)` bodies are copied verbatim from SV — no logic change.) Add sources to CMake.

- [ ] **Step 4: Build to confirm it compiles.**

```bash
cd build_mac && cmake --build . -j 10 --target SerialPrograms 2>&1 | tail -5
```
Expected: builds clean. (Coordinates fine-tuned on hardware in Task 6.)

- [ ] **Step 5: Commit.**

```bash
git add SerialPrograms/Source/PokemonHome/Inference/PokemonHome_IvJudgeReader.* SerialPrograms/CMakeLists.txt
git commit -m "feat(home): IvJudgeReader scope reusing shared engine (SV-seeded crops)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 6: `CalibrateIVJudge` test program (hardware calibration)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Programs/TestPrograms/PokemonHome_CalibrateIVJudge.h`
- Create: `SerialPrograms/Source/PokemonHome/Programs/TestPrograms/PokemonHome_CalibrateIVJudge.cpp`
- Modify: `SerialPrograms/Source/PokemonHome/PokemonHome_Panels.cpp` (register), `CMakeLists.txt`

**Interfaces:**
- Consumes: `SingleSwitchProgramDescriptor/Instance` pattern (see `TestPrograms/PokemonHome_ReadSummaryScreen.*`), `PokemonHome::IvJudgeReaderScope` (Task 5), `summarize_ivs` (Task 2).
- Produces: a runnable panel program `PokemonHome:CalibrateIVJudge` that ALSO verifies the summary→Judge button press.

- [ ] **Step 1: Copy `ReadSummaryScreen.{h,cpp}`** to `CalibrateIVJudge.{h,cpp}`; rename the descriptor/class and its ID string to `PokemonHome:CalibrateIVJudge` / "Calibrate IV Judge". Add a `Language` option (default English) for the reader.

- [ ] **Step 2: Implement `program()`:** assume the user has a Pokémon's **summary** screen open. Press `Y` to open the Judge/stat view, then draw the scope's overlay boxes, snapshot, read, and log — plus dump the crops for offline tuning:

```cpp
void CalibrateIVJudge::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // Verify the summary -> Judge navigation (press Y).
    pbf_press_button(context, BUTTON_Y, 80ms, 500ms);
    context.wait_for_all_requests();

    IvJudgeReaderScope scope(env.console.overlay(), LANGUAGE);
    VideoSnapshot screen = env.console.video().snapshot();
    Pokemon::IvJudgeReader::Results r = scope.read(env.console.logger(), screen);
    env.log("IV Judge results: " + r.to_string());

    IVSummary s = summarize_ivs(r);
    env.log("IV summary: read=" + std::to_string(s.read) + " best_count=" + std::to_string(s.best_count)
            + " total_est=" + std::to_string(s.total_estimate) + " perfect=" + std::to_string(s.perfect));

    // Dump each stat crop for offline coordinate tuning.
    int i = 0;
    for (const ImageViewRGB32& img : scope.dump_images(screen)){
        dump_image(env.console, ProgramInfo(), "CalibrateIVJudge_stat" + std::to_string(i++), img);
    }
}
```

- [ ] **Step 3: Register** in `PokemonHome_Panels.cpp` (add include + `ret.emplace_back(make_single_switch_program<CalibrateIVJudge_Descriptor, CalibrateIVJudge>());`). Add sources to CMake.

- [ ] **Step 4: Build.** Expected: `[100%] Built target SerialPrograms`.

- [ ] **Step 5 (Nicole, on hardware): Calibrate.** Open a Pokémon's **summary** screen in HOME, run `CalibrateIVJudge`. Confirm the `Y` press lands on the Judge/stat view and all six ratings read correctly (check the log + dumped stat crops) for Pokémon whose IVs you know. Adjust the six crop boxes in `PokemonHome_IvJudgeReader.cpp` (and the `Y`-press timing here) until stable.

- [ ] **Step 6: Commit** the calibrated coordinates.

```bash
git add SerialPrograms/Source/PokemonHome/Programs/TestPrograms/PokemonHome_CalibrateIVJudge.* SerialPrograms/Source/PokemonHome/PokemonHome_Panels.cpp SerialPrograms/Source/PokemonHome/Inference/PokemonHome_IvJudgeReader.cpp SerialPrograms/CMakeLists.txt
git commit -m "feat(home): CalibrateIVJudge program + calibrated Judge crops

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Phase 3 — The sorter program

### Task 7: `BoxSorterMaster` — Catalogue pass with resume

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.h`
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.cpp`
- Modify: `PokemonHome_Panels.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `BoxSorterLivingDex`'s helpers (`populate_box_data`, `find_occupied_slots_in_box`, `read_summary_screen`, `move_cursor_to`, `go_to_first_slot`, `exit_menus`, `save_boxes_data_to_json` — all in `PokemonHome_BoxNavigation.*` / `Pokemon_CollectedPokemonInfo.*`), plus Tasks 4–5.
- Produces:
  - The program class + descriptor (`PokemonHome:BoxSorterMaster`).
  - Extended `read_summary_screen_with_ivs(...)` — wraps `read_summary_screen`, then presses `Y` to open the Judge view, constructs an `IvJudgeReaderScope`, calls `scope.read(...)`, passes the `Results` through `summarize_ivs`, and fills the IV fields (`iv_read/iv_best_count/iv_total_estimate/iv_perfect`), then presses `B` to return. If `READ_IVS` option is off, skip.
  - `void write_catalogue_incremental(const std::vector<std::optional<CollectedPokemonInfo>>&, size_t boxes_done, const std::string& path);`
  - `bool load_catalogue_resume(std::vector<std::optional<CollectedPokemonInfo>>&, size_t& boxes_done, const std::string& path);`

- [ ] **Step 1: Scaffold** the descriptor + instance by copying the structure of `BoxSorterLivingDex.{h,cpp}` (constructor, options, `make_stats`). Options to include: `SCAN_BOX_START`, `SCAN_BOX_COUNT`, per-category box-range overrides (default from layout), `OWNER_OT_NAMES` (a `StringListOption` or comma field → set), `READ_IVS` (bool), `COMPETITIVE_MIN31` (default 6), `BREEDING_MIN/MAX` (3/5), `BREEDJECT_MIN/MAX` (1/2), `VIDEO_DELAY`, `GAME_DELAY`, `OUTPUT_FILE`, `DRY_RUN`, `FRESH_START` (bool), `NOTIFICATIONS`.

- [ ] **Step 2: Implement `read_summary_screen_with_ivs`.** Call `read_summary_screen` first. Then, if `READ_IVS`: `pbf_press_button(context, BUTTON_Y, ...)` to open the Judge view (timing verified by `CalibrateIVJudge` in Task 6), `context.wait_for_all_requests()`, wait for a Judge/stat-screen frozen-image, snapshot, build an `IvJudgeReaderScope` and call `scope.read(...)`, run the `Results` through `summarize_ivs`, then press `B` to return to the summary. Assign `iv_read/iv_best_count/iv_total_estimate/iv_perfect` onto the info struct.

- [ ] **Step 3: Implement the Catalogue pass with resume.** Reuse `populate_box_data` structure but swap the per-Pokémon read to `read_summary_screen_with_ivs`, and after each box call `write_catalogue_incremental(..., box_idx+1, path)`. On program start, if `!FRESH_START` and the catalogue file exists, call `load_catalogue_resume` and re-scan each already-recorded box's grid fingerprint (`find_occupied_slots_in_box` occupancy) — if occupancy matches the saved count, skip re-reading; else re-read. Begin cataloguing at `boxes_done`.

- [ ] **Step 4: Build.** Expected: builds clean.

- [ ] **Step 5: Dry-Run verification path (documented, Nicole runs on rig).** With `DRY_RUN=true` the program catalogues only and writes `home_catalogue.json`. Confirm on a 1–2 box test that the JSON contains sane values (dex #s, IV counts). No moves happen.

- [ ] **Step 6: Commit.**

```bash
git add SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.* SerialPrograms/Source/PokemonHome/PokemonHome_Panels.cpp SerialPrograms/CMakeLists.txt
git commit -m "feat(home): BoxSorterMaster catalogue pass with resumable JSON

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 8: Plan + Execute pass with progress/resume + space safety

**Files:**
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.cpp` / `.h`
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.h`
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.cpp`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `MasterBoxRouter::route` (Task 3), `MasterBoxLayout` (Task 4), the catalogue vector (Task 7).
- Produces (pure, testable):
  - `struct PlannedMove { BoxCursor from; BoxCursor to; };`
  - `struct MasterPlan { std::vector<PlannedMove> moves; std::vector<std::string> warnings; };`
  - `MasterPlan build_master_plan(const std::vector<std::optional<CollectedPokemonInfo>>& catalogue, const MasterBoxLayout& layout, const RouterConfig& cfg, uint16_t scratch_box_start, uint16_t scratch_box_count);`
  - Dex-slot contention resolved by `bool wins_slot(const CollectedPokemonInfo& challenger, const CollectedPokemonInfo& incumbent, const RouterConfig&)` implementing spec §4.1 (shiny > owner-OT > best_count > total_estimate > form/event/ball).

- [ ] **Step 1: Write failing tests for the planner** (`test_pokemonHome_MasterPlanner`): feed a small hand-built catalogue (e.g. two Bulbasaurs, one shiny) and assert the shiny wins slot 0 and the other routes to `DuplicateShiny`'s box; assert `wins_slot` ordering (shiny beats non-shiny; among non-shiny, higher best_count wins; owner-OT beats non-owner). Register + dummy fixture.

- [ ] **Step 2: Run to verify it fails.** Expected: FAIL (undefined `build_master_plan`).

- [ ] **Step 3: Implement the planner** (pure): for each catalogue entry compute `route(...)`; assign target slots within the category's box range in dex order for LivingDex, append-order for the bucket boxes; resolve contention with `wins_slot`; when a category box overflows, push a `warning` and place overflow in `ManualOther`. Compute a minimal move list toward targets (greedy: for each target slot not already holding its intended occupant, plan a move using an empty slot or the scratch buffer as intermediary). Return `MasterPlan`. Space rule: if no empty/scratch slot is available for an intermediary, add a blocking warning that Execute turns into a `UserSetupError`.

- [ ] **Step 4: Run to verify it passes.** Expected: `PokemonHome_MasterPlanner` PASS.

- [ ] **Step 5: Wire Execute into `BoxSorterMaster::program`.** After cataloguing: compute scratch buffer = the `scratch_box_count` (default 3) boxes immediately after the used range; call `build_master_plan`; write `home_plan.json` (always) and, if `DRY_RUN`, stop here. Otherwise execute `moves` one at a time using the existing Y-pick/Y-place swap pattern (`BoxSorterLivingDex.cpp:453-459`), appending each to `home_progress.json`. If a `warnings` entry is blocking, `UserSetupError`. On resume (`!FRESH_START` + progress file exists), reload plan+progress, re-read affected boxes, and skip already-`done` moves.

- [ ] **Step 6: Build + run planner tests.** Expected: builds clean; planner tests PASS.

- [ ] **Step 7: Commit.**

```bash
git add SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.* SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.* SerialPrograms/Source/Tests/* SerialPrograms/CMakeLists.txt CommandLineTests/PokemonHome_MasterPlanner/
git commit -m "feat(home): master plan/execute pass with resume + space safety

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Phase 4 — Finish

### Task 9: Full build, docs, and wiki stub

**Files:**
- Create: `SerialPrograms/BuildInstructions/` note is not needed; instead update `docs/superpowers/` usage notes.
- Create: `docs/BoxSorterMaster-usage.md` (local usage guide: options, Dry-Run-first, resume behavior, which boxes are manual).

**Interfaces:** none (documentation + final verification).

- [ ] **Step 1: Full clean build.**

```bash
cd /Users/nicole/Projects/PokemonAutomator/build_mac && cmake --build . -j 10 2>&1 | tail -5
```
Expected: `[100%] Built target SerialPrograms`.

- [ ] **Step 2: Run the whole PokemonHome test group.** Set `TEST_LIST` to the four new tests (`PokemonHome_IVTierMapping`, `PokemonHome_MasterBoxRouter`, `PokemonHome_MasterBoxLayout`, `PokemonHome_MasterPlanner`).
Expected: all PASS.

- [ ] **Step 3: Write `docs/BoxSorterMaster-usage.md`** documenting: run order (Calibrate → Dry Run → Full Run), every option + default, the resume/Fresh-Start behavior, and the explicit list of boxes that remain manual (ManualForms, ManualOther, plus Competitive/Breeding caveats). State that IVs come from the Judge screen and cosmetic same-type forms are routed to ManualForms.

- [ ] **Step 4: Commit.**

```bash
git add docs/BoxSorterMaster-usage.md
git commit -m "docs(home): BoxSorterMaster usage guide

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

- [ ] **Step 5 (optional, later): Wiki page** for the public `ComputerControl` wiki repo, per the tutorial. Out of scope for a working program; do only if publishing.

---

## Self-Review notes (author check)

- **Spec coverage:** §2 readable set → Tasks 1,5; §3 two-pass/resume → Tasks 7,8; §4 routing → Task 3; §4.1 contention → Task 8 `wins_slot`; §4.3 OT list → Task 3 config + Task 7 option; §5.1 IvJudgeReader (shared-engine reuse) → Tasks 2,5; §5.2 struct ext → Task 1; §5.3 program → Tasks 7,8; §5.4 calibration → Task 6; §5.5 layout resource → Task 4; §6 space mgmt → Task 8; §7 error handling → Tasks 7,8; §8 testing → Tasks 2,3,4,8 + Task 6/7 hardware; §9 build → Task 0.
- **Naming consistency:** `route`, `wins_slot`, `summarize_ivs`, `iv_value_midpoint`, `IVSummary`, `IvJudgeReaderScope`, `IV_READER`, `build_master_plan`, `MasterBoxLayout`, `RouterConfig`, `BoxCategory` used identically across tasks. IV data types reuse `Pokemon::IvJudgeValue` / `Pokemon::IvJudgeReader::Results` (not a bespoke tier enum).
- **Known hardware placeholders (intentional, resolved on rig):** the summary→Judge `Y`-press timing + the six crop-box coordinates in `PokemonHome_IvJudgeReader.cpp` (SV-seeded) — both calibrated in Task 6; the summary→Judge navigation in Task 7 depends on that same calibration.

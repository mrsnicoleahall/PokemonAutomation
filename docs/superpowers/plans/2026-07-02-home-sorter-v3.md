# Master Box Sorter v3 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Rework the HOME sorter into v3: a Shiny Dex + Regular Dex (full National Dex order, gaps for unowned, 5-box buffers), dex-first de-duplication, a new duplicate-routing priority, kept collection boxes, a Forms box, split Shiny/Regular Trades, Junk, extra CSV/box-map outputs, and an interactive release prompt. Box auto-renaming is scaffolded only (finished interactively later).

**Architecture:** Keep v1/v2's catalogue pass, readers, and execute/swap machinery unchanged. Replace the **routing + planning + layout** with v3 logic: a new `MasterBoxRouterV3` that groups the catalogue by species, fills each dex slot with the best copy (shiny/non-shiny split), and routes duplicates by priority; deterministic dex-slot math (regular = dex#−1; shiny = rank among shiny-able); a v3 layout config; and new outputs. Pure logic is unit-tested; hardware paths compile-verified + rig-calibrated.

**Tech Stack:** C++23, Qt 6.8.3, CMake, the command-line image-fixture test harness (`./run-cli-tests.sh`).

**Spec:** `docs/superpowers/specs/2026-07-02-home-sorter-v3-design.md`
**Branch:** `feature/master-box-sorter-v3` (created; base = `main`).

## Global Constraints

- Build Qt 6.8.3 (`-DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos"`, dir `build_mac`). New sources → `SerialPrograms/cmake/SourceFiles.cmake` (alphabetical); reconfigure to pick up.
- **Tests only via `./run-cli-tests.sh`** (headless/offscreen). NEVER launch `SerialPrograms.app` or create `SerialPrograms-Settings.json`. Pure-logic tests: `image_void_detector_helper` + `TEST_MAP` key + repo-root `CommandLineTests/PokemonHome/<Object>/dummy.png`.
- **Readable-only; never release; never overwrite.** Never place into the reserved buffer boxes. Out of space → interactive prompt (if enabled) then stop with `UserSetupError`.
- Namespace `PokemonAutomation::NintendoSwitch::PokemonHome` (progs/inference) / `Pokemon` (shared). Prefer `static_cast<>`. Mirror existing sibling style. Reuse enum→string helpers (`POKEMON_TYPE_SLUGS`, `ORIGIN_MARK_SLUGS`, `gender_to_string`, `NATIONAL_DEX_SLUGS`).
- Resource JSON lives in the **PokePackages** repo (`Packages/Resources/...`, remote `nicole`); commit there separately AND `cp` into `build_mac/Resources/...` for tests.
- **"Best / most powerful"** = higher `iv_best_count`, tie-broken by higher `iv_total_estimate`, then arbitrary (reuse v1 `wins_slot`).
- **Duplicate priority (first match):** Forms → Legendary → Mythical → UltraBeast → Paradox → Events → Utility → Breeding → Competitive → ShinyTrades(if shiny) → RegularTrades → Junk. Dexes are filled BEFORE duplicate routing (dex-first).
- Commit messages end with a blank line + `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

---

## Task 1: v3 `BoxCategory` + shiny-locked data + v3 layout config

**Files:**
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouter.h` (enum)
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxLayout.{h,cpp}` (category map + shiny-locked + v3 fields)
- Create: `Packages/Resources/PokemonHome/DexTemplates/master_box_layout_v3.json`, `Packages/Resources/PokemonHome/shiny_locked.json`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`

**Interfaces:**
- Produces: `BoxCategory` gains `ShinyDex, RegularDex, ShinyTrades, RegularTrades, Junk` (keep `Legendary, Mythical, UltraBeast, Paradox, Events, Utility, Breeding, Competitive`; the old `LivingDex, DuplicateShiny, GoodTrades, ManualForms, ManualOther` may remain but are unused by v3).
- `struct MasterBoxLayoutV3 { uint16_t shiny_dex_start; uint16_t regular_dex_start; uint16_t shiny_dex_buffer_boxes; uint16_t regular_dex_buffer_boxes; std::map<BoxCategory,std::pair<uint16_t,uint16_t>> category_box_ranges; std::set<uint16_t> legendary, mythical, ultra_beast, paradox; std::set<uint16_t> shiny_locked; };`
- `MasterBoxLayoutV3 load_master_box_layout_v3(const std::string& layout_path, const std::string& shiny_locked_path);`

- [ ] **Step 1: Enum.** Add the five new `BoxCategory` values (end of enum). Add their `{"ShinyDex",…}` entries to the layout loader's category-string map and to the planner `category_name()`.
- [ ] **Step 2: JSON.** Create `master_box_layout_v3.json`: `shiny_dex_start`, `regular_dex_start` (computed: Shiny Dex first at box 1; Regular Dex after Shiny Dex span + 5 buffer), buffer counts (5/5), `category_box_ranges` for Legendary/Mythical/UltraBeast/Paradox/Events/Forms/Breeding/Utility/Competitive/ShinyTrades/RegularTrades/Junk (default 2 boxes each after the dexes+buffers, in the §1 order), and the legendary/mythical/ultra_beast/paradox dex-number sets (reuse the v1 sets from `master_box_layout.json`). Create `shiny_locked.json`: `{"shiny_locked":[<dex numbers>]}` seeded with well-known shiny-locked species (e.g. many Mythicals: 151?no—Mew is shiny-capable; use the established shiny-locked list: e.g. 494 Victini, 648 Meloetta, 649 Genesect?… ) — **seed best-effort from a reputable shiny-lock reference; it is refinable data, list what you're confident about and leave a comment that Nicole will refine on the rig.** Commit both to PokePackages (`git -C Packages ... commit`) and `cp` into `build_mac/Resources/PokemonHome/...`.
- [ ] **Step 3: Failing test** `test_pokemonHome_LayoutV3`: load both files; assert `shiny_dex_start==1`, `regular_dex_start > shiny_dex_start`, `category_box_ranges` contains `Junk`, `shiny_locked` is non-empty and contains a known-locked dex #. Register + fixture.
- [ ] **Step 4: RED** via `./run-cli-tests.sh`.
- [ ] **Step 5: Implement** `MasterBoxLayoutV3` + `load_master_box_layout_v3` (mirror the v1 `load_master_box_layout` JSON idiom; add shiny-locked array parse). Add to CMake.
- [ ] **Step 6: GREEN.**
- [ ] **Step 7: Commit** (main repo; note PokePackages committed separately).

---

## Task 2: Dex-slot math (pure, TDD)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_DexSlots.{h,cpp}`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`, `SourceFiles.cmake`

**Interfaces:**
- Consumes: `MasterBoxLayoutV3` (Task 1), `NATIONAL_DEX_SLUGS()` (for count = 1025).
- Produces:
  - `size_t regular_dex_slot(uint16_t dex_number);` → `dex_number - 1` (0-indexed slot within the Regular Dex region; species N always maps to slot N−1, leaving gaps for unowned).
  - `std::optional<size_t> shiny_dex_slot(uint16_t dex_number, const std::set<uint16_t>& shiny_locked);` → `nullopt` if `dex_number` is shiny-locked; else the **rank** of `dex_number` among non-shiny-locked species ≤ dex_number, 0-indexed (compacted so shiny-locked species leave no gap).
  - `size_t shiny_dex_species_count(uint16_t max_dex, const std::set<uint16_t>& shiny_locked);` helper.

- [ ] **Step 1: Failing test** `test_pokemonHome_DexSlots`:
```cpp
int test_pokemonHome_DexSlots(const ImageViewRGB32&){
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
```
Register `PokemonHome_DexSlots` + fixture.
- [ ] **Step 2: RED.**
- [ ] **Step 3: Implement.** `regular_dex_slot` = `dex_number - 1`. `shiny_dex_slot`: if locked → nullopt; else count non-locked species in `[1, dex_number]` minus 1. Add to CMake.
- [ ] **Step 4: GREEN.**
- [ ] **Step 5: Commit.**

---

## Task 3: `MasterBoxRouterV3` — dex-first dedup + duplicate priority (pure, TDD — the heart)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouterV3.{h,cpp}`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`, `SourceFiles.cmake`

**Interfaces:**
- Consumes: `CollectedPokemonInfo`, `wins_slot` (v1 contention — best copy), `p_matches_utility` (v2), `MasterBoxLayoutV3`, dex-slot fns (Task 2), `RouterConfig` (owner OTs, thresholds, utility rules).
- Produces:
  - `enum class DexAssign { RegularDexSlot, ShinyDexSlot, Duplicate };`
  - `struct SpeciesKey { uint16_t dex_number; PokemonType t1, t2; bool gmax; StatsHuntGenderFilter gender; };` + equality/hash so copies of the "same species/form" group together (mirror v1's base-form signature comparison).
  - `struct RouteResultV3 { BoxCategory category; bool is_dex_keeper; std::vector<std::string> also_qualifies; };` (`also_qualifies` = the higher categories a dex-keeper ALSO matched → overqualified report).
  - `std::vector<RouteResultV3> route_all_v3(const std::vector<std::optional<CollectedPokemonInfo>>& catalogue, const MasterBoxLayoutV3& layout, const RouterConfig& cfg);` — one result per catalogue slot (nullopt slots → skipped/`category` unused).
  - `BoxCategory route_duplicate_v3(const CollectedPokemonInfo& p, const MasterBoxLayoutV3& layout, const RouterConfig& cfg);` (the priority chain in Global Constraints).

Algorithm (`route_all_v3`): group non-empty catalogue entries by species/form key; within each group, pick the best non-shiny (→ RegularDex keeper) and best shiny (→ ShinyDex keeper, unless shiny-locked → then that shiny is a duplicate) via `wins_slot`; every other copy → `route_duplicate_v3`. For each dex keeper, compute `also_qualifies` by checking whether it would match Forms/collection/Utility/Breeding/Competitive (for the overqualified report). Return per-slot results.

- [ ] **Step 1: Failing tests** covering: (a) two same-species non-shiny → best is RegularDexSlot keeper, other → duplicate routed (RegularTrades/Junk); (b) two same-species shiny → best is ShinyDexSlot keeper, other → ShinyTrades; (c) a shiny of a shiny-locked species → NOT a shiny-dex keeper (routes as duplicate); (d) `route_duplicate_v3` priority: a duplicate Flame-Body → Forms? no—Forms is for form variants; a duplicate with ability flame-body → Utility (not Forms unless it's a detected variant); a legendary duplicate (dex in legendary set) → Legendary; a 6×31 duplicate → Competitive; a shiny duplicate → ShinyTrades; a plain low-value duplicate → Junk (unless protected); (e) a dex keeper that is 6×31 records `also_qualifies` containing "Competitive". Use hand-built `CollectedPokemonInfo` + a small `MasterBoxLayoutV3`/`RouterConfig`. Register `PokemonHome_MasterRouterV3` + fixture.
- [ ] **Step 2: RED.**
- [ ] **Step 3: Implement** grouping + best-copy selection + duplicate priority + overqualified detection. Add to CMake.
- [ ] **Step 4: GREEN.**
- [ ] **Step 5: Commit.**

---

## Task 4: v3 planner — assemble layout, assign slots, box-map, reserve buffers (pure, TDD)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterPlannerV3.{h,cpp}`
- Modify: `Tests/PokemonHome_Tests.*`, `Tests/TestMap.cpp`, `SourceFiles.cmake`

**Interfaces:**
- Consumes: `route_all_v3` (Task 3), dex-slot fns (Task 2), `MasterBoxLayoutV3`.
- Produces:
  - `struct BoxMapEntry { uint16_t box_start; uint16_t box_end; std::string label; };`
  - `std::vector<BoxMapEntry> build_box_map(const MasterBoxLayoutV3&);` (the labeled legend: "Shiny Dex", "(buffer)", "Regular Dex", "(buffer)", "Legendary", …, "Junk").
  - `struct MasterPlanV3 { std::vector<PlannedMove> moves; std::vector<std::string> warnings; std::vector<std::string> overqualified_rows; std::vector<BoxMapEntry> box_map; };`
  - `MasterPlanV3 build_master_plan_v3(const std::vector<std::optional<CollectedPokemonInfo>>& catalogue, const MasterBoxLayoutV3& layout, const RouterConfig& cfg, uint16_t scan_start);`

Planner: run `route_all_v3`; map each keeper to its absolute slot (ShinyDex region + `shiny_dex_slot`, RegularDex region + `regular_dex_slot`); map duplicates into their category box range (route-all, append order); NEVER assign into the two buffer regions; compute moves toward targets (reuse v1 cycle-break/never-overwrite move logic); collect overqualified rows; build the box map. Overflow of a category range → warning + spill to Junk (or a blocking warning if truly out of room).

- [ ] **Step 1: Failing tests** `test_pokemonHome_PlannerV3`: (a) `build_box_map` yields ranges in the §1 order with Shiny Dex first and two buffer ranges; (b) a small catalogue (one shiny Pikachu + one non-shiny Pikachu + a duplicate shiny Pikachu) plans the shiny→ShinyDex slot, non-shiny→RegularDex slot, dup-shiny→ShinyTrades, none into buffers; (c) buffers never receive a target. Register `PokemonHome_PlannerV3` + fixture.
- [ ] **Step 2: RED.**
- [ ] **Step 3: Implement.** Add to CMake.
- [ ] **Step 4: GREEN.**
- [ ] **Step 5: Commit.**

---

## Task 5: Program integration — wire v3, outputs, interactive release

**Files:**
- Modify: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.{h,cpp}`

**Interfaces:**
- Consumes: `load_master_box_layout_v3`, `build_master_plan_v3`, existing catalogue pass + execute/swap + CSV (`PokemonHome_CatalogueCsv`).
- Produces: v3 sort behavior end-to-end; `<output>_dex_overqualified.csv`, `<output>_boxmap.txt`, plus existing catalogue CSV + `_plan.json`.

- [ ] **Step 1:** Add options: `USE_V3_LAYOUT` (bool, default **true** — routes through the v3 planner; false keeps v1/v2 path for fallback), `ALLOW_RELEASE_PROMPT` (bool default true), per-category box-range overrides defaulted from `master_box_layout_v3.json`, and reuse existing OWNER_OT_NAMES / thresholds / READ_* options. Load `master_box_layout_v3.json` + `shiny_locked.json`.
- [ ] **Step 2:** After cataloguing, call `build_master_plan_v3(...)`; write `_plan.json`, the **overqualified CSV** (from `plan.overqualified_rows`), the **box-map** (`build_box_map` / `plan.box_map`), and the full catalogue CSV — all at the existing plan-write point (dry-run too).
- [ ] **Step 3:** Execute the plan's moves with the existing never-overwrite swap+verify loop and execute-resume (unchanged). Reserve buffer boxes (planner already avoids them). 
- [ ] **Step 4: Interactive release** — when `ALLOW_RELEASE_PROMPT` and free space is insufficient to continue (define: no empty non-buffer slot available for a needed intermediary), send a status notification + `env.log` a clear instruction to release the Junk box, then WAIT (poll for freed space up to a timeout / until the user resumes). On continued shortage → `UserSetupError`. Never release automatically.
- [ ] **Step 5: Build** clean + `./run-cli-tests.sh` all pass (existing + Tasks 1–4 tests). Compile-only for the hardware paths.
- [ ] **Step 6: Commit.**

---

## Task 6: Box-renamer scaffold (standalone, calibrated interactively later)

**Files:**
- Create: `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_RenameBoxes.{h,cpp}` (+ register in `PokemonHome_Panels.cpp`, add to CMake)

**Interfaces:**
- Consumes: `build_box_map` (Task 4) for labels; standard `SingleSwitchProgram` pattern.
- Produces: a runnable `PokemonHome:RenameBoxes` program (its own panel), SEPARATE from the sorter.

- [ ] **Step 1:** Scaffold the descriptor/instance (mirror an existing single-switch program). Options: `START_BOX`, `BOX_COUNT` (so we can rename ONE box first), `OUTPUT_FILE` (to read the same box-map labels), and a `DRY_RUN` that just logs the label it WOULD type per box (no keyboard input).
- [ ] **Step 2:** `program()`: compute labels from the box map; for each box in range, log `"Box N → '<label>'"`. Include a clearly-commented **stub** `type_box_name(context, label)` that documents the intended OSK navigation (open box-name editor → clear → type via on-screen keyboard → confirm) but is left minimal/guarded — **the real OSK navigation + timing are calibrated interactively with Nicole on the rig** (per spec §9a). Default behavior with no calibration = log-only (safe).
- [ ] **Step 3: Build** clean; confirm the panel registers. No hardware run here.
- [ ] **Step 4: Commit.**

---

## Task 7: Final build, tests, docs

**Files:**
- Modify: `docs/BoxSorterMaster-usage.md`, `docs/hardware-bringup.md`

- [ ] **Step 1: Full clean rebuild** (`rm -rf build_mac && …`) → `[100%] Built target SerialPrograms`; validates v3 resources auto-deploy.
- [ ] **Step 2:** `./run-cli-tests.sh` — all tests pass (existing + LayoutV3, DexSlots, MasterRouterV3, PlannerV3).
- [ ] **Step 3:** Update usage doc (new HOME order: Shiny Dex → Regular Dex → collection → Forms → Breeding → Utility → Competitive → Shiny/Regular Trades → Junk; dex-first + dedup; shiny-lock skip; 5-box buffers; overqualified CSV + box-map; release prompt; box-rename is a separate optional post-sort program calibrated together). Update hardware-bringup Phase order to mention the v3 Dry Run + that box-rename is a later joint step.
- [ ] **Step 4: Commit.**

---

## Self-Review notes (author)

- **Spec coverage:** §1 layout → Tasks 1,4; §2 dex-first dedup → Task 3; §3 duplicate priority → Task 3; §3a overqualified CSV → Tasks 3,5; §4 two dexes/gaps/shiny-lock/buffers → Tasks 1,2,4; §5 trades split → Task 3; §6 interactive release → Task 5; §7 readability (inherited) → n/a; §9 outputs/box-map → Tasks 4,5; §9a box-rename standalone scaffold → Task 6; §10 code map → all; §11 sequencing → Task 7 docs.
- **Naming consistency:** `MasterBoxLayoutV3`/`load_master_box_layout_v3`, `regular_dex_slot`/`shiny_dex_slot`, `route_all_v3`/`route_duplicate_v3`/`RouteResultV3`, `build_master_plan_v3`/`MasterPlanV3`/`BoxMapEntry`/`build_box_map`, new `BoxCategory` values `ShinyDex/RegularDex/ShinyTrades/RegularTrades/Junk` used identically across tasks.
- **Hardware/interactive deferrals (not built blind):** interactive release prompt timing (Task 5) and the box-rename OSK navigation (Task 6) are calibrated on the rig; Task 6 defaults to safe log-only.
- **Reuse:** catalogue pass, readers (IV/extras/moves), execute/swap+verify, execute-resume, and CSV writer are unchanged from v1/v2; v3 only replaces routing/planning/layout + adds outputs.

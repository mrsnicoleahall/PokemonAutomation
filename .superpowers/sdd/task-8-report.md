# Task 8 Report — Master Box Planner + Execute Pass

**Date:** 2026-07-01  
**Branch:** feature/master-box-sorter  

---

## Status: COMPLETE — Build clean, all 4 tests GREEN

---

## Part A — Planner (Pure, TDD)

### Files created:
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.h`
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.cpp`

### Structs/functions delivered:
- `struct PlannedMove { BoxCursor from; BoxCursor to; }`
- `struct MasterPlan { std::vector<PlannedMove> moves; std::vector<std::string> warnings; }`
- `bool wins_slot(challenger, incumbent, cfg)` — spec §4.1: shiny > owner-OT > iv_best_count > iv_total_estimate > tie (incumbent wins)
- `MasterPlan build_master_plan(catalogue, layout, cfg, scratch_box_start, scratch_box_count)`

### Planner algorithm:
1. **Route + target assignment:** For each catalogue entry, call `route()`. LivingDex entries contend per dex_number using `wins_slot`. Losers are requeued for re-routing.
2. **Requeue processing:** Losing entries re-route with updated `species_slot_taken_by_shiny`. If still LivingDex, forced to ManualOther.
3. **Overflow handling:** When a category range fills, excess goes to ManualOther with a non-blocking warning. If ManualOther also fills, a `[BLOCKING]` warning is emitted.
4. **Move computation (greedy cycle-break):**
   - Build `desired_at[flat]` and `current_flat[ci]` maps.
   - For each pending target slot:
     - If target empty: direct move.
     - If target occupied by wrong entry: if that entry's own target is free, chain it out first (case a). Otherwise use a free/scratch slot as temp (case b).
     - If no temp available: `[BLOCKING]` warning.

### Space-safety:
- Never overwrites an occupied slot — all swaps go through the `do_move` function which tracks `board_occupant`.
- `[BLOCKING]` warning → Execute will throw `UserSetupError`.

### TDD (RED → GREEN):
- Test written first: `test_pokemonHome_MasterPlanner` in `PokemonHome_Tests.cpp`.
- RED: link error before `.cpp` existed (confirmed by the task flow).
- GREEN: all 4 tests pass including MasterPlanner.

### Test coverage:
- `wins_slot`: shiny beats non-shiny, owner beats non-owner (even with lower IVs), higher iv_best_count wins, iv_total_estimate as tiebreaker, tie → incumbent.
- Contention: two Bulbasaurs (non-shiny + shiny), shiny wins dex slot, non-shiny routes to GoodTrades, correct 2-move plan verified.
- No-move case: single Pokémon already in the correct slot.

### Fixture:
- `CommandLineTests/PokemonHome/MasterPlanner/dummy.png` (copied from MasterBoxRouter).
- TEST_MAP key: `"PokemonHome_MasterPlanner"` registered via `image_void_detector_helper`.

---

## Part B — Execute pass wired into BoxSorterMaster::program()

Replaced the `// TODO (Task 8)` seam with:

1. **Layout loading:** `load_master_box_layout()` from `Resources/PokemonHome/DexTemplates/master_box_layout.json`. Throws `UserSetupError` on failure.
2. **RouterConfig construction:** Wires live threshold options (COMPETITIVE_MIN31, BREEDING_{MIN,MAX}, BREEDJECT_{MIN,MAX}) and layout's dex sets. Owner OT set now captured into `owner_ot_set` in the parsing block.
3. **Scratch buffer:** 3 boxes immediately after the scan range.
4. **Plan building:** `build_master_plan()` called. All warnings logged yellow.
5. **Blocking check:** Any `[BLOCKING]` warning → `UserSetupError` before any hardware interaction.
6. **Plan JSON:** Always written to `<OUTPUT_FILE>_plan.json` (even on DRY_RUN).
7. **DRY_RUN stop:** Returns after writing plan.
8. **Resume for execute:** Loads `<OUTPUT_FILE>_progress.json`; skips `moves_done` completed moves.
9. **Execute loop:** For each remaining move: `move_cursor_to` + Y-pick, `move_cursor_to` + Y-place, `wait_for_all_requests`, write progress after each move.

---

## Part C — Carried fixes from Task 7 review

### C.1 — load_catalogue_resume round-trips all fields

Previously left `primary_type`, `secondary_type`, `tera_type`, `origin_mark`, `gender` at zero/default on resume. Fixed by deserializing using the same slug maps the saver uses:
- `POKEMON_TYPE_SLUGS().get_enum(slug, NONE)` for primary/secondary types.
- `POKEMON_TERA_TYPE_SLUGS().get_enum(slug, NONE)` for tera type.
- `ORIGIN_MARK_SLUGS().get_enum(slug, NONE)` for origin mark.
- Manual reverse lookup for gender (`"Male"/"Female"/"Any"/"Genderless"` strings).

Added includes: `Pokemon_OriginMarks.h`, `Pokemon_Types.h`, `Pokemon_StatsHuntFilter.h`.

### C.2 — Occupancy mismatch check on resume

Added `saved_fingerprints` parameter to `catalogue_boxes()` (both header and implementation). For each resume box (`box_rel < already_done_boxes`), if a saved fingerprint exists, compare live occupancy count against saved count. On mismatch: throws `UserSetupError` naming the box and telling the user to enable Fresh Start.

### C.3 — Startup warning for READ_IVS=true + IV_LANGUAGE=None

Added immediately after `iv_lang` is resolved: if `read_ivs && iv_lang == Language::None`, logs a yellow `COLOR_YELLOW` warning explaining that IVs will not be read and routing will degrade.

---

## Files modified:
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.h` (NEW)
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxPlanner.cpp` (NEW)
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.h` (signature fix C.2)
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_BoxSorterMaster.cpp` (B + C.1 + C.2 + C.3)
- `SerialPrograms/Source/Tests/PokemonHome_Tests.h` (added declaration)
- `SerialPrograms/Source/Tests/PokemonHome_Tests.cpp` (added test body)
- `SerialPrograms/Source/Tests/TestMap.cpp` (registered TEST_MAP entry)
- `SerialPrograms/cmake/SourceFiles.cmake` (added planner .h/.cpp)
- `CommandLineTests/PokemonHome/MasterPlanner/dummy.png` (test fixture)

---

## Self-review checklist:
- [x] Planner is pure (no Qt, no hardware, no file IO)
- [x] Unit test covers wins_slot ordering and contention scenarios
- [x] Execute never overwrites an occupied slot (always checks blocking warning first; board_occupant tracks all swaps)
- [x] Resume skips already-done moves (progress JSON)
- [x] load_catalogue_resume round-trips all router/planner fields (C.1)
- [x] Occupancy mismatch check present and throws UserSetupError (C.2)
- [x] IV warning logged when READ_IVS=true && IV_LANGUAGE=None (C.3)
- [x] Build clean (0 errors, 0 warnings)
- [x] All 4 CLI tests pass: IvSummary, MasterBoxLayout, MasterBoxRouter, MasterPlanner

---

## Known design choices / caveats:
- **scan_start derivation:** The planner derives scan_start from `living_dex_start_box - 1`, which means the scan must always start at the living-dex start box. If the user scans a different region, the planner will mis-map catalogue indices to absolute box numbers. This is expected behavior given the current spec.
- **base_form_signature_matches:** The planner passes `true` for all entries (no form-signature data in the catalogue). ManualForms routing requires external form data not available at planning time.
- **scratch_box_count=3:** Hard-coded to 3 as specified. Could be a program option in a future task.

---

## Review Fixes — 2026-07-01

### Fix 1 (CRITICAL) — occupancy-mismatch check gated on execute-resume

**Problem:** `catalogue_boxes` fired the C.2 occupancy check on every resume, including execute-resume, where moves have legitimately changed box occupancy.

**Change:**
- Added `bool skip_occupancy_check` parameter to `catalogue_boxes` (header + implementation).
- In `program()`, before calling `catalogue_boxes`, read `<OUTPUT_FILE>_progress.json` and check `moves_done > 0`. If so, set `execute_already_started = true`.
- Inside `catalogue_boxes`, gate the mismatch check with `!skip_occupancy_check`.
- Added a brief comment explaining why (moves shift mons, so occupancy differs from catalogue-phase fingerprints).

**Files:** `PokemonHome_BoxSorterMaster.h`, `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Build clean. MasterPlanner test still passes (check is a runtime guard only).

---

### Fix 2 (Important) — scan-start alignment guard

**Problem:** If `SCAN_BOX_START != layout.living_dex_start_box`, the planner maps catalogue indices to the wrong absolute boxes — silently producing a destructive plan.

**Change:** After loading the layout in `program()`, added:
```cpp
if (static_cast<uint16_t>(SCAN_BOX_START) != layout.living_dex_start_box){
    throw UserSetupError(...);
}
```
Error message names both the required box number and the current setting value.

**Files:** `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Build clean; test layout has `living_dex_start_box=1` and tests don't exercise the guard path (guard is in `program()`, tests call `build_master_plan` directly).

---

### Fix 3 (Minor) — always write _plan.json before blocking error

**Problem:** `plan_root.dump(plan_path)` was after the blocking-warning `throw`, so users could not inspect the plan that caused the block.

**Change:** Moved the plan-JSON dump block to execute before the blocking-warning check loop. Both the dump and the check remain in the same code path; only their order changed.

**Files:** `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Build clean. Logic order confirmed by reading the updated source.

---

### Fix 4 (Minor) — comment corrections

**4a — Test comment in `PokemonHome_Tests.cpp`:**
The two-Bulbasaur test comment said move 1 was a "swap" of the two at flats 0 and 1. The actual algorithm evicts the non-shiny from flat=0 directly to its GoodTrades target (flat=1380, empty → direct move), then moves the shiny from flat=1 to the now-empty flat=0. Updated comment to describe both moves accurately. Assertions unchanged.

**4b — Planner comment in `PokemonHome_MasterBoxPlanner.cpp`:**
The ManualOther fallback branch in the requeue loop was commented "Shouldn't normally happen." Updated to note it WILL happen for owners of alt-forms sharing a dex# with a base form, because `base_form_signature_matches` is always true in v1 (documented limitation — the catalogue only stores dex_number, not a form signature).

**Files:** `PokemonHome_Tests.cpp`, `PokemonHome_MasterBoxPlanner.cpp`

**Verified:** Build clean. All 4 CLI tests pass: IvSummary, MasterBoxLayout, MasterBoxRouter, MasterPlanner (4 tests passed, exit code 0).

---

## Final Pass Review Fixes — 2026-07-01

### CRITICAL 1 — Execute-resume re-derives plan from live state (never replays stale suffix)

**Problem:** On execute-resume, `program()` loaded the stale catalogue JSON (pre-move board state), recomputed the plan from it, then replayed `moves[moves_done..end]` by index. The planner's Phase-2 coordinates are only valid relative to the board at planning time; after real moves, the board has changed. Replaying the suffix can swap into slots assumed empty → overwrite/misplacement.

**Fix:**
- Added `stored_moves_done` / `stored_total_moves` detection before the catalogue block.
- When `stored_moves_done > 0`: set `execute_already_started = true` AND set `already_done_boxes = 0` so the catalogue-skip optimization is bypassed entirely — the full live board is re-catalogued.
- Removed the `moves[moves_done..]`-by-index replay. The execute loop now always starts at index 0 of the freshly computed plan.
- The "skip already-recorded box" optimization (C.2 fingerprint check) is gated to `execute_already_started == false`, so it only fires during pure catalogue-phase resumes where no move has touched the board.
- `FRESH_START` continues to ignore all checkpoints and start clean.
- `_progress.json` is still written per-move for audit/telemetry.

**Files:** `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Clean compile. Execute-resume correctness is by construction: any resume with `moves_done > 0` now re-reads all boxes live before planning, so the plan is always grounded in the actual current board state.

---

### CRITICAL 2 — Per-move post-swap verification

**Problem:** No runtime check that the Y-swap actually landed — a missed button press or cursor drift would silently misplace Pokémon.

**Fix:** After each `Y`-pick + `Y`-place in the execute loop, snapshot the screen and call `slot_is_occupied()` on the `to` slot. If the `to` slot is empty after the swap, throw `UserSetupError` naming the boxes/slots before any further move. This enforces "never overwrite" at runtime, not just in the planner's model.

**Files:** `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Clean compile. Logic is correct-by-construction: the check fires immediately after the move, before `write_progress`, so no further moves execute on a bad state.

---

### IMPORTANT 3 — Log plan-length mismatch on execute-resume

**Problem:** If options/layout changed between the original run and a resume, the stored `total_moves` would differ from the fresh plan size. Silently proceeding could mislead the operator.

**Fix:** After computing the fresh plan on execute-resume, if `stored_total_moves != master_plan.moves.size()`, log a yellow warning noting both values and that the fresh plan is the source of truth. The fresh plan (computed from live state) is always executed in full.

**Files:** `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Clean compile. Consistent with the CRITICAL 1 re-derive approach.

---

### IMPORTANT 4 — Unified occupancy detection via shared helper

**Problem:** `count_occupied_slots_in_box` duplicated the grid coordinates and threshold independently from `find_occupied_slots_in_box`. A discrepancy (intentional or accidental) would cause disagreement on borderline slots → wrong resume abort or wrong trust.

**Fix:**
- Added `bool slot_is_occupied(const ImageViewRGB32& screen, size_t row, size_t col)` as a free function in `PokemonHome_BoxNavigation.cpp` (declared in `PokemonHome_BoxNavigation.h`). Single source of truth for coordinates (`0.06 + 0.072*col`, `0.2 + 0.105*row`, `0.03`, `0.057`) and threshold (`stddev >= 10`).
- `find_occupied_slots_in_box` updated to call `slot_is_occupied` instead of inline computation.
- `count_occupied_slots_in_box` updated to call `slot_is_occupied`. The unused `ProControllerContext& context` parameter was removed (it was `(void)`'d — see Minor fixes).

**Files:** `PokemonHome_BoxNavigation.h`, `PokemonHome_BoxNavigation.cpp`, `PokemonHome_BoxSorterMaster.h`, `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Clean compile + all 4 tests pass.

---

### IMPORTANT 5 — Pass `scan_start` explicitly to planner

**Problem:** `build_master_plan` self-derived `scan_start` from `layout.living_dex_start_box - 1`, creating a hidden coupling. If the layout format ever changes or someone calls the planner with a different layout, the derivation silently produces wrong catalogue↔absolute-box mapping.

**Fix:**
- Added `size_t scan_start` as an explicit parameter to `build_master_plan` (before `scratch_box_start`).
- Added a comment in the planner and header documenting the precondition (`scan_start == layout.living_dex_start_box - 1`), which `program()` already enforces with a `UserSetupError`.
- Removed the `scan_start` self-derivation block in the planner.
- Updated the call site in `program()` to pass `scan_start` explicitly.
- Updated both test calls in `PokemonHome_Tests.cpp` to pass `/*scan_start=*/0`.

**Files:** `PokemonHome_MasterBoxPlanner.h`, `PokemonHome_MasterBoxPlanner.cpp`, `PokemonHome_BoxSorterMaster.cpp`, `PokemonHome_Tests.cpp`

**Verified:** Clean compile + all 4 tests pass.

---

### IMPORTANT 6 — Confirm return to summary after Judge `B`

**Problem:** After pressing `B` to leave the Judge screen, if the press was missed or slow, subsequent reads would read the Judge screen (wrong data) for every remaining Pokémon in the box, with no error.

**Fix:** After the `BUTTON_B` press + `wait_for_all_requests()` (in both the "Judge not found" and "Judge found" branches), added a `SummaryScreenWatcher` + `wait_until` (5-second timeout). On failure, fires `OperationFailedException` with error report — symmetric to the existing Judge-entry watcher.

**Files:** `PokemonHome_BoxSorterMaster.cpp`

**Verified:** Clean compile. Logic mirrors the existing Judge-entry and box-view confirmations already in the codebase.

---

### MINOR fixes

**Overflow warning category names:** Changed `static_cast<int>(cat)` to `category_name(cat)` in the planner overflow warning. Added a `category_name(BoxCategory)` internal helper function that returns the slug string for each enum value.

**`.gitignore` de-duplication:** Removed duplicate `Packages/` and `/Packages/` entries, keeping a single `Packages/` entry. Removed duplicate `/build_mac/` (already covered by `build_*/`).

**`count_occupied_slots_in_box` — removed unused `context` parameter:** The parameter was `(void)`'d. Removed from the method signature, declaration in the header, and the call site in `catalogue_boxes`.

**Dead `bkdone` scaffolding:** Removed the `bkdone_inner` local, the `const size_t bkdone = boxes_done` re-read, and the `(void)bkdone` at the end of `load_catalogue_resume`. The value was already written into `boxes_done` directly.

**Duplicate `Breeding` layout line in test:** Removed the second `layout.category_box_ranges[BoxCategory::Breeding] = {41, 43}` assignment (identical duplicate of the first).

**Descriptor blurb:** Replaced `"then (Task 8) sort them into their designated category boxes."` with `"then sort them into their designated category boxes."` — task numbers are internal scaffolding, not user-facing language.

**Files:** `PokemonHome_MasterBoxPlanner.cpp`, `.gitignore`, `PokemonHome_BoxSorterMaster.h`, `PokemonHome_BoxSorterMaster.cpp`, `PokemonHome_Tests.cpp`

**Verified:** Clean compile + all 4 tests pass.

---

## Final Build + Test Result

```
cmake --build . -j 10: CLEAN (0 errors, 0 warnings)
./run-cli-tests.sh: 4 tests passed, exit code 0
  - IvSummary
  - MasterBoxLayout
  - MasterBoxRouter
  - MasterPlanner
```

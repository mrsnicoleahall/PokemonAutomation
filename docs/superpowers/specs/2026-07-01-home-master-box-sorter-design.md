# Pokémon HOME — Master Box Sorter (design)

**Date:** 2026-07-01
**Author:** Nicole Hall (with Claude)
**Repo:** `mrsnicoleahall/PokemonAutomation` (fork of PokemonAutomation/Arduino-Source)
**Program name:** `PokemonHome:BoxSorterMaster` — "Master Box Sorter"

---

## 1. Goal

Sort a large HOME collection (~4,800 Pokémon across up to 6,000 slots / 200 boxes)
into Nicole's Master Box Layout automatically, using only data the SerialPrograms
automation can read from the HOME summary + Judge screens. The run must be
**stoppable at any time and resumable**, re-checking prior progress on resume, and
must be **space-safe** (never get stuck with nowhere to move a Pokémon).

Anything the tool cannot resolve is routed to **Manual Sort** staging boxes — never
guessed at, never released.

This program is a new sibling to the existing
`PokemonHome/Programs/PokemonHome_BoxSorterLivingDex.*`, reusing its box-navigation,
summary-reading, and swap machinery.

---

## 2. What is readable (hard constraint)

Everything in the sorter is driven by these signals and nothing else.

**Readable from the summary screen** (already implemented in `read_summary_screen`,
`PokemonHome_BoxNavigation.cpp`):
National Dex #, Shiny, Gender, Ball (incl. Cherish), OT name, OT ID,
primary/secondary type, Gigantamax, Alpha, Tera type, Origin mark
(GO, Galar, BDSP, LA, SV, Kalos, LGPE, VC/GameBoy, LZA).

**Readable from the Judge / IV screen** (see §5.1). Navigation: from the summary
screen, **press `Y`** to bring up the stat chart with the Judge ratings (requires a
HOME **Premium Plan** — the Judge function is unavailable otherwise). The ratings are
read by **reusing the existing shared engine** `Pokemon::IvJudgeReader` +
`Pokemon::IvJudgeValue` (already used by SV/SwSh/BDSP/LZA) — a multi-language,
dictionary-backed OCR that returns a per-stat `IvJudgeValue` ∈ {UnableToDetect,
NoGood, Decent, PrettyGood, VeryGood, Fantastic, Best, **HyperTrained**}.
From the six values we derive:
- `iv_best_count` — number of stats judged **Best** OR **HyperTrained** (both = 31).
- `iv_total_estimate` — sum of tier midpoints (Best/HyperTrained=31, Fantastic=30,
  VeryGood=27, PrettyGood=20, Decent=8, NoGood/UnableToDetect=0). Tie-breaks only.
- `iv_perfect` — `iv_best_count == 6`.
- If any stat is `UnableToDetect`, `iv_read = false` (placed by non-IV rules only).

**NOT readable — stays Manual Sort:** exact Nature, EVs, ribbons, marks, favorite
flag, egg moves, and any cosmetic form that shares its base form's type
(Unown letters, Vivillon patterns, Furfrou trims, Alcremie creams, Floette colors,
Cap Pikachu, most Rotom/Ogerpon variants). Regional/gmax/gendered forms with a
distinct type ARE distinguishable and are placed automatically.

---

## 3. Architecture — two passes, JSON checkpoints

### Pass 1 — Catalogue (slow, stoppable)
Walk every occupied box in the scan range. For each Pokémon: open summary, read all
readable attributes, open the Judge screen, read IV tiers. Write results to
`home_catalogue.json` **incrementally, one box at a time**.

- Each catalogue entry records: box index, row, col, all read attributes, and a
  `slot_signature` (color-stddev fingerprint of the box grid used to detect change).
- **Resume:** on start, if `home_catalogue.json` exists, re-scan each box's grid
  fingerprint. If a box's fingerprint matches the saved one, skip re-reading it;
  otherwise re-read that box. Cataloguing resumes at the first box not yet recorded.

### Pass 2 — Plan + Execute (stoppable)
1. **Plan:** from the catalogue, run the Router (§4) to assign every Pokémon a
   target `(box, row, col)`. Resolve dex-slot contention by the priority in §4.1.
   Emit `home_plan.json` (full target layout) — this is also the Dry-Run output.
2. **Execute:** perform the moves as a cycle-sort (each move sends a Pokémon toward
   its final slot; empty slots and the scratch buffer absorb displaced mons). Append
   each completed move to `home_progress.json` as `{from, to, done}`.
   - **Resume:** on start, re-read the boxes touched by any not-yet-`done` move,
     diff live state against `home_plan.json`, and execute only the moves still
     outstanding. This is the "check against previous progress" step.

### Modes
- **Dry Run:** Pass 1 + Plan only. Writes `home_catalogue.json` + `home_plan.json`,
  moves nothing. Always run this first on new hardware.
- **Full Run:** Pass 1 + Plan + Execute.
- **Resume:** auto-detected from presence of the JSON files; a `Fresh Start` toggle
  ignores/overwrites them.

---

## 4. Box routing (Nicole's Master Layout, readable-only)

The target layout is encoded as a resource config
`Packages/.../PokemonHome/DexTemplates/master_box_layout.json` describing, in order:
Living Dex ranges (boxes 1–35), Form boxes (36–52), Collection boxes (53–59),
Utility boxes (60–66), plus Manual-Sort staging boxes. Box numbers are the defaults
from the layout doc and are overridable via program options.

Each catalogued Pokémon is routed by the first rule it matches:

1. **Manual-only categories** → Manual Sort staging (never auto-placed elsewhere):
   nothing here today beyond cosmetic-form ambiguity, because IV data is now
   readable. A Pokémon whose dex # matches a Living-Dex base entry but whose
   **type/gender/gmax signature does NOT match the base form** → Manual-Sort-Forms
   (it's an undetermined alt form).
2. **Duplicate Shiny** → box 65 if a shiny already claims this species/form's dex slot.
3. **Good Trades** → box 64 if OT ∉ {my OT names} and not shiny / legendary / mythical /
   ultra-beast / paradox / event. (Rare collectible species are never auto-filed as trade
   fodder — they fall through to their collection box in step 8.)
4. **Competitive** (box 61): `iv_perfect` (6×31).
5. **Breeding** (box 62): `iv_best_count` in 3–5.
6. **Breedjects** (box 63): `iv_best_count` in 1–2.
7. **Events** (box 60): Cherish ball OR origin mark ∈ {GO, …stamp marks}.
8. **Legendary / Mythical / Ultra Beast / Paradox** (boxes 53–56): by dex-number set.
9. **Living Dex** (boxes 1–35): base-form match on dex # + type + gender-diff + gmax.
10. Everything else → general overflow within Manual Sort.

(Thresholds in 4–6 and the OT names in 3 are **runtime options**, defaults shown.)

### 4.1 Dex-slot contention — best specimen wins
When multiple Pokémon qualify for the same Living-Dex/Form/Collection slot, the
keeper is chosen by this readable priority (losers drop to the matching box above):

1. Shiny
2. OT is one of mine (see §4.3)
3. Higher `iv_best_count`, then higher `iv_total_estimate`
4. type-distinct rare form → event/stamp (origin mark / Cherish) → rarer ball

### 4.2 Auto-protection (never routed to Junk, never overwritten)
Shiny · OT is one of mine (§4.3) · Legendary · Mythical · Event (Cherish/GO/stamp) ·
`iv_perfect`. Junk is **never auto-released** — the "Junk / Release Candidates" box
is a manual review bucket only.

### 4.3 My OT names
The two owner OTs are **`Nicole`** and **`cole`** (lowercase c). Matching is
case- and punctuation-insensitive: `normalize_utf32` lowercases and strips
non-alphanumerics before an **exact full-string** compare, so `Cole`/`cole` match
and `cole` never collides with `nicole`. The program exposes OT names as a
**list/set option** (not the single-name field used by `BoxSorterLivingDex`), and a
Pokémon counts as "mine" if its read OT exactly matches any entry after
normalization.

---

## 5. New components

### 5.1 `PokemonHome_IvJudgeReader` (Inference/) — reuses shared engine
- **Reuses** `Pokemon::IvJudgeReader` (dictionary OCR) — do NOT hand-roll OCR. Add a
  thin `IvJudgeReaderScope` modeled exactly on `PokemonSV_IvJudgeReader.cpp`: six crop
  boxes + `IV_READER()` pointing at an OCR dictionary JSON (copy SV's `IVCheckerOCR.json`).
- Navigation: from the summary screen, press `Y` (via `pbf_press_button` + a
  Judge/stat-screen watcher, then B to return). Exact press timing calibrated on rig.
- Crop coordinates **seed from SV's** (stat chart layout is nearly identical):
  HP `{0.825,0.192,...}`, Atk `{0.886,0.302,...}`, Def `{0.886,0.406,...}`,
  SpA `{0.660,0.302,...}`, SpD `{0.660,0.406,...}`, Spe `{0.825,0.470,...}` —
  then fine-tuned via the calibration program (§5.4). Lower risk than a from-scratch reader.
- A pure helper `IVSummary summarize_ivs(const IvJudgeReader::Results&)` converts the
  six `IvJudgeValue`s into `{iv_read, iv_best_count, iv_total_estimate, iv_perfect}`.

### 5.2 `CollectedPokemonInfo` extension
Add optional IV fields: `uint8_t iv_best_count`, `uint16_t iv_total_estimate`,
`bool iv_perfect`, and an `iv_read` flag. Update the `<<`, `==`, and JSON
serialization helpers (the "new struct members" spots called out in
`Pokemon_CollectedPokemonInfo.h`).

### 5.3 `PokemonHome_BoxSorterMaster` (Programs/) — the program
Descriptor + instance modeled on `BoxSorterLivingDex`. Options: scan box range,
Living-Dex start box, per-category box numbers (defaulted from layout), OT names,
IV thresholds, capture/game delays, output-file basename, Dry Run, Fresh Start,
Read IVs (on/off — off skips the slow Judge pass), notifications. Reuses
`populate_box_data`, `move_cursor_to`, `find_occupied_slots_in_box`, swap logic.
Registered in `PokemonHome_Panels.cpp`.

### 5.4 `PokemonHome_CalibrateIVJudge` (Programs/TestPrograms/) — NEW
Sibling to `ReadSummaryScreen`: opens one Pokémon, navigates to Judge, draws the six
crop overlays, OCRs and logs the phrases + derived values. Lets Nicole verify/adjust
crop coordinates on her rig before a real run.

### 5.5 `master_box_layout.json` (Packages resource)
Ordered box definitions + dex-number sets for Legendary/Mythical/UB/Paradox.
Lives in the `Packages` repo; submitted separately per the project's resource-file
workflow.

---

## 6. Space management
~1,200 free slots exist. The program auto-reserves the **last 2–3 boxes after the
used range** as a scratch buffer (confirmed choice). Cycle-sort moves each Pokémon
directly toward its target; displaced mons use empty slots or the buffer. If the
buffer + empties are ever exhausted mid-plan, the program stops with a clear
`UserSetupError` asking to free slots — it never overwrites.

---

## 7. Error handling
- Unreadable dex # / summary → `OperationFailedException` with error report + image
  dump (matches existing code), the slot is marked "unknown" and left untouched.
- Judge screen not found within timeout → mark `iv_read = false`; the Pokémon is
  still placed by non-IV rules and never routed to an IV-only box.
- Any move that can't find a destination → stop with actionable `UserSetupError`.
- All JSON writes are incremental so a crash/stop loses at most the in-flight box/move.

---

## 8. Testing / verification
- **Compile check:** full build via CMake on macOS (Qt 6.8.3 — see §9).
- **Unit-testable pure logic:** the Router (§4) and Planner are pure functions over
  `CollectedPokemonInfo` → can be exercised with hand-written catalogue JSON without
  hardware. Add a small fixture-driven test if the repo's test harness allows.
- **Hardware calibration:** `CalibrateIVJudge` + Dry Run on Nicole's rig confirm
  crop coords and the plan before any live move.
- **No end-to-end runtime test is possible in this environment** (needs Switch +
  microcontroller + capture card; no emulator). Nicole runs the live sort.

---

## 9. Build / toolchain notes
- Deps installed: cmake, tesseract, opencv (done). **Qt 6.8.3 required** — Qt ≥ 6.9
  breaks the build (upstream issue #570); Homebrew's `qt6` is 6.9+, so Qt 6.8.3 must
  be pinned/installed deliberately.
- Needs the separate `Packages` data folder placed inside the source root to run.
- Build: `cmake ../SerialPrograms -DCMAKE_BUILD_TYPE=Release && cmake --build . -j`.

---

## 10. Out of scope (v1)
Nature/EV/ribbon/mark/favorite reading; auto-release of Junk; distinguishing cosmetic
forms that share a type; non-English game/Judge language. All can be added later.

**Form routing (v1 limitation):** the catalogue does not yet compute a per-Pokémon
"base-form signature," so `base_form_signature_matches` is always treated as true and
the dedicated **ManualForms** box is not auto-populated. When a species has both a base
form and an alt form owned, they contend for the single Living-Dex slot; the loser is
routed to **ManualOther** (a manual-review box) rather than ManualForms. Nothing is lost
or misfiled destructively — extra/alt copies land in a manual box for hand-sorting,
consistent with the "route unresolvable → Manual Sort" principle. A future version can
close this by loading the living-dex form template and comparing each read
type/gmax/gender against the base entry to set `base_form_signature_matches` properly.

**Scan alignment:** the plan indexes catalogue slots relative to the living-dex start
box, so `SCAN_BOX_START` must equal the layout's `living_dex_start_box` (the program
enforces this with a `UserSetupError`).

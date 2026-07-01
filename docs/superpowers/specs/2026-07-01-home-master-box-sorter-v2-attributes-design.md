# Master Box Sorter v2 — Ability / Item / Nature / Moves reading (design)

**Date:** 2026-07-01
**Builds on:** `2026-07-01-home-master-box-sorter-design.md` (v1). Same repo/branch conventions.
**Goal:** Read the attributes HOME actually shows but v1 skipped — **ability, nature, held
item, and moves** — so the **Utility box** and the nature/move parts of the **Breeding box**
auto-sort. Everything v1 guarantees still holds (readable-only, never release, never overwrite).

---

## 1. What's readable (grounded in the code)

- **Ability** — on the summary; the reader already defines `ability_box` (0.158,0.838,0.213,0.042).
- **Nature** — on the summary; `nature_box` (0.157,0.783,0.212,0.042) + ready dictionary
  `Packages/Resources/Pokemon/NatureCheckerOCR.json`.
- **Held item** — on the summary; crop coordinates NOT yet in the reader (calibrate).
- **Moves** — on a **separate moves screen** (navigated per Pokémon); dictionary
  `Packages/Resources/PokemonSV/PokemonMovesOCR.json` (moves are cross-game). Gated by a toggle.
- **Egg group** — NOT shown anywhere in HOME → the Breeding "Egg Group Masters" grouping stays
  manual. Nature- and move-based Breeding improves; egg group does not.

Readers reuse `OCR::SmallDictionaryMatcher` (the engine behind `Pokemon::IvJudgeReader`).

---

## 2. Data model

Extend `CollectedPokemonInfo` (v1 already added IV fields):
- `std::string ability_slug = "";`   // normalized; "" if unread
- `std::string nature = "";`         // normalized nature slug (via NatureCheckerOCR)
- `std::string held_item_slug = "";` // "" = none/unread
- `std::vector<std::string> moves;`  // up to 4 normalized move slugs (empty if moves not read)
- `bool extras_read = false;`        // ability/item/nature were read
- `bool moves_read = false;`         // moves screen was read

All new fields are serialized by `write_catalogue_incremental` and deserialized symmetrically by
`load_catalogue_resume` (same requirement v1 review enforced for the enum fields).

---

## 3. Readers (new inference, hardware-calibrated)

`PokemonHome_SummaryExtrasReader` (Inference/):
- **Ability:** OCR `ability_box`. We do NOT need a full ability dictionary — v2 only needs to
  recognize the *target* abilities for Utility roles. OCR the text, normalize via
  `OCR::normalize_utf32`, and store the normalized slug; matching against targets happens in the
  router. (A future full ability dictionary can improve robustness; targeted normalized-compare is
  enough for the fixed Utility set.)
- **Nature:** `NatureCheckerOCR.json` + `SmallDictionaryMatcher` at `nature_box`.
- **Held item:** OCR a held-item crop (coords added + calibrated). Store normalized slug; match
  against target items in the router. (Optionally validate against `ItemNameDisplay.json`.)

`PokemonHome_MovesReader` (Inference/), only when `READ_MOVES`:
- Navigate summary → moves screen (button sequence calibrated, like the Judge nav). Read up to 4
  move crops with `PokemonMovesOCR.json` + `SmallDictionaryMatcher`. Return to summary (confirmed
  by a watcher, per the v1 review fix). Coordinates + nav calibrated on hardware.

All crop coordinates + the moves-screen nav are **placeholders calibrated on the rig** via the
calibration program (§6), exactly like the v1 IV Judge reader.

---

## 4. Router — Utility (box 60) + Breeding refinement

Add a configurable **Utility rules** table to `RouterConfig`:
```
struct UtilityRule { enum Kind { Ability, Item, Move } kind; std::string target_slug; };
std::vector<UtilityRule> utility_rules;   // defaults below
bool p_matches_utility(const CollectedPokemonInfo&, const std::vector<UtilityRule>&);
```
Default rules (normalized slugs): abilities `flame-body`, `magma-armor`, `synchronize`, `pickup`,
`run-away`; items `amulet-coin`, `smoke-ball`; moves `false-swipe`, `pay-day` (Catcher's status
move and the rest are bonus, not required to match).

**Routing order** (extends v1 §4; first match wins). Utility sits **after** the IV boxes so a
battle-ready/breeding mon keeps its IV box, and a low-value tool goes to Utility:
1 ManualForms · 2 DuplicateShiny · 3 GoodTrades · 4 Competitive(6×31) · 5 Breeding(3–5×31) ·
6 Breedject(1–2×31) · **7 Utility (matches a utility rule)** · 8 Events · 9 Legendary/Mythical/UB/Paradox · 10 LivingDex.

**No single-slot contention for Utility/Breeding:** these are collection-style boxes — route *all*
matches into the category's box range in read order (a Synchronizer set is up to 25 natures; a
Ditto collection is many). This mirrors how v1 fills bucket boxes. `wins_slot` still governs the
single-occupant Living-Dex/Form/Legendary slots only.

Breeding refinement: nature is now available for the Synchronizer role and for labeling breeding
parents; egg group remains unreadable so egg-group grouping stays manual (documented).

---

## 5. Program integration

- Rename/extend `read_summary_screen_with_ivs` → `read_summary_screen_with_extras`: after the v1
  summary read, also OCR ability + nature + held item (same screen, no nav, sets `extras_read`);
  then if `READ_MOVES`, do the Judge read (v1) and the moves-screen read, restoring the summary.
- New options on `BoxSorterMaster`: `READ_EXTRAS` (ability/item/nature, default **on**),
  `READ_MOVES` (default **on**), and editable Utility target lists (defaulted from §4).
- Catalogue JSON carries the new fields; resume round-trips them (§2).
- A startup warning if a target list is empty while its read flag is on (mirrors the v1 IV-language
  warning).

Everything else (two-pass, resumable, space-safe, never-overwrite, DRY_RUN) is unchanged from v1.

---

## 6. Testing / verification

- **Pure, unit-tested:** `p_matches_utility` and the extended router order — real assertions
  (e.g. a Flame Body low-IV mon → Utility; a Synchronize 6×31 mon → Competitive not Utility; an
  Amulet Coin mon → Utility; a plain mon → LivingDex). Via `./run-cli-tests.sh`.
- **Hardware-calibrated (not runtime-testable here):** the ability/item/nature crops, the moves
  crops, and the moves-screen navigation — verified with the calibration program on the rig.
- **Calibration:** extend `CalibrateIVJudge` (or add `CalibrateSummaryExtras`) to also dump the
  ability/nature/item crops and, in a moves mode, the moves-screen crops + confirm the nav.
- Full clean build green; all CLI tests pass.

---

## 7. Out of scope (v2)
Egg group (never shown in HOME); EV/ribbon/mark/favorite (v1); a *complete* ability/item OCR
dictionary (v2 does targeted matching against the configured Utility target lists, which is
sufficient — a full dictionary is a later polish); non-English game/OCR language for the new
readers (English default, like v1).

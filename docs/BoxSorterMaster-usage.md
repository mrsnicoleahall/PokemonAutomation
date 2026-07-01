# Master Box Sorter — Usage Guide

`PokemonHome:BoxSorterMaster` sorts a large Pokémon HOME collection into Nicole's
Master Box Layout using only data the automation can read from HOME's summary and
Judge screens. It is resumable, space-safe, and never releases or overwrites a Pokémon.

See the design + implementation docs under `docs/superpowers/` for the full rationale.

---

## Prerequisites

- **Console:** Nintendo **Switch 2** with **Pokémon HOME** installed and open.
- **HOME Premium** — required for the Judge (IV) screen. Without it, IV-based routing
  (Competitive/Breeding/Breedjects and the stat tie-breaks) silently does nothing.
- **Controller device:** ESP32-S3 flashed with `PABotBase2-ESP32-S3` firmware
  (`Packages/Firmware/`), paired to the Switch 2 over Bluetooth.
- **Capture card** feeding the Switch 2's HDMI into the Mac (grant Camera permission
  on first launch — that's the capture feed, not your webcam).
- App built at `build_mac/SerialPrograms.app` (Qt 6.8.3; see `SerialPrograms/BuildInstructions/CompilingForMac.md` and the AGL note in the top-level CMake).

---

## Run order (always, on new hardware)

1. **`CalibrateIVJudge`** (Developer-mode panel). Open a Pokémon's **summary** screen in
   HOME, run it. It presses `Y` to open the Judge view, reads the six stat ratings, logs
   them + the derived IV summary, and dumps the six stat crops. Adjust the crop
   coordinates in `PokemonHome/Inference/PokemonHome_IvJudgeReader.cpp` and the `Y`-press
   timing until all six read correctly for Pokémon whose IVs you know.
2. **Dry Run** of the Master Box Sorter (`DRY_RUN = true`). It catalogues boxes and writes
   `<output>_plan.json` **without moving anything**. Inspect the catalogue + plan to
   confirm reads are correct.
3. **Full Run** (`DRY_RUN = false`) once the Dry Run looks right.

---

## How it works (two passes)

- **Catalogue pass:** walks each scanned box, opens every Pokémon's summary (and Judge
  screen if `READ_IVS`), and writes `<output>.json` **after each box**. Stop any time.
- **Plan + Execute pass:** computes each Pokémon's target box/slot, writes
  `<output>_plan.json` (always), then performs moves one at a time — writing
  `<output>_progress.json` after each move.

### Resume
- With **Fresh Start off** (default), restarting re-uses the JSON checkpoints: the
  catalogue pass skips boxes whose live occupancy still matches, and the execute pass
  re-reads affected boxes and performs only the moves not yet marked done.
- **Fresh Start on** ignores/overwrites the checkpoints and starts clean.
- If a box's occupancy changed since the last catalogue run (and no moves have executed
  yet), the program stops with a clear error naming the box — re-run with Fresh Start or
  re-catalogue.

### Space safety
The program auto-reserves the last few boxes after your used range as a scratch buffer
and only ever moves a Pokémon into an empty/scratch slot. If it can't find room, it
**stops with a `UserSetupError`** — it never overwrites an occupied slot. Free up slots
(or widen the scratch buffer) and resume.

---

## Options (defaults in **bold**)

| Option | Default | Notes |
|---|---|---|
| `SCAN_BOX_START` | **1** | Must equal the layout's living-dex start box (enforced). |
| `SCAN_BOX_COUNT` | — | How many boxes to scan/sort. |
| `OWNER_OT_NAMES` | **nicole, cole** | Your OTs (case-insensitive). "Mine vs Good Trades." |
| `READ_IVS` | **true** | Reads the Judge screen. Turn off to skip IV routing (faster). |
| `IV_LANGUAGE` | — | Must be set (e.g. English) when `READ_IVS` is on, or IVs read as blank. |
| `COMPETITIVE_MIN31` | **6** | Stats at 31 to count as Competitive (flawless). |
| `BREEDING_MIN`/`MAX` | **3 / 5** | 31-count range for the Breeding box. |
| `BREEDJECT_MIN`/`MAX` | **1 / 2** | 31-count range for Breedjects. |
| `READ_EXTRAS` | **true** | (v2) Reads ability + nature + held item off the summary (no extra screen). Drives the Utility box. |
| `READ_MOVES` | **true** | (v2) Opens the moves screen per Pokémon to read moves (Catcher/Pay-Day). Slower + one more calibration surface; turn off to skip. |
| `EXTRAS_LANGUAGE` / `MOVES_LANGUAGE` | — | Set (e.g. English) when the matching read flag is on, or those reads come back blank. |
| Utility target lists | abilities **flame-body, magma-armor, synchronize, pickup, run-away** · items **amulet-coin, smoke-ball** · moves **false-swipe, pay-day** | Editable; a Pokémon matching any target routes to the Utility box. |
| `VIDEO_DELAY` / `GAME_DELAY` | — | Capture-card / HOME-app timing. |
| `OUTPUT_FILE` | **home_catalogue** | Basename for the `.json`, `_plan.json`, `_progress.json`. |
| `DRY_RUN` | **false** | Catalogue + plan only; moves nothing. **Run this first.** |
| `FRESH_START` | **false** | Ignore existing checkpoints and start clean. |

---

## What it sorts automatically vs. by hand

**Automated** (readable): Living Dex by National Dex, best-specimen contention
(Shiny → your OT → most 31-IVs → …), Duplicate Shinies, Good Trades (foreign OT),
Events (Cherish ball / GO / origin marks), Legendary / Mythical / Ultra Beast / Paradox
(by dex #), Competitive / Breeding / Breedjects (by Judge IV counts), and — **as of v2** —
the **Utility box** (Hatcher/Synchronizer/Farmers/Evader/Money-Maker/Catcher) via reading
ability + held item + moves. Utility is checked *after* the IV boxes, so a flawless
Synchronize mon still files as Competitive. Your Breloom **and** Smeargles both match the
Catcher rule (False Swipe) and land in Utility together.

**Left for you** (truly unreadable) — routed to Manual Sort boxes, never released:
**EV / ribbon / mark / favorite**-dependent choices, **egg group** (never shown in HOME),
and **alternate forms**. Note the form limitation (see design §10): the sorter does not
yet distinguish an alt form from the base species, so when you own both, the extra copy
lands in the **ManualOther** box for hand-sorting rather than a dedicated Forms box.
*(Nature, ability, held item, and moves ARE read as of v2.)*

**Never auto-released / never overwritten:** Shiny · your OT · Legendary · Mythical ·
Event · perfect-IV (6×31). The "Junk" box is a manual review bucket only.

---

## Utility box (Box 60) — now AUTO-sorted (v2)

v2 reads ability + held item + moves, so the sorter fills the Utility box automatically.
A Pokémon matching any Utility target (editable lists) routes here:

| Role | Auto-detected by | Default target |
|---|---|---|
| Hatcher | ability | Flame Body / Magma Armor |
| Synchronizer | ability (+ nature is also read) | Synchronize |
| Farmers | ability | Pickup |
| Evader / Flee Master | ability / held item | Run Away / Smoke Ball |
| Money Maker | held item (+ move) | Amulet Coin / Pay Day |
| Catcher | move | False Swipe (Breloom **and** Smeargle both match) |

Utility is a **route-all** box (no single-slot contention) — a full Synchronizer set of
natures all land here. It's checked *after* the IV boxes, so a flawless Synchronize mon
files as Competitive instead. Requires `READ_EXTRAS` (and `READ_MOVES` for the Catcher/
Pay-Day roles). The ability/item/move crops + the moves-screen navigation are calibrated
on the rig (`CalibrateIVJudge` dumps them).

## Breeding box (62–63) — partial auto (v2)

- **Ditto base** — a Ditto with high Judge IVs auto-files to **Competitive** (6×31) or
  **Breeding** (3–5×31) by the normal IV rules. (A flawless Ditto lands in Competitive;
  move to Breeding by hand if you prefer.) Foreign-language Ditto is only fuzzily inferable
  and is **not** auto-filed.
- **Egg Group Masters** — egg group is **never shown in HOME**, so grouping by egg group
  stays **manual**. Nature and moves are now read, which helps you identify parents.
- **W.I.P. rows** — leave a section empty for sorting freshly hatched eggs (manual).

**Still never readable** (stays manual): EV/ribbon/mark/favorite; egg group. Alternate
forms sharing a Dex # still aren't separated (extra copies → ManualOther).

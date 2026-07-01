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
| `VIDEO_DELAY` / `GAME_DELAY` | — | Capture-card / HOME-app timing. |
| `OUTPUT_FILE` | **home_catalogue** | Basename for the `.json`, `_plan.json`, `_progress.json`. |
| `DRY_RUN` | **false** | Catalogue + plan only; moves nothing. **Run this first.** |
| `FRESH_START` | **false** | Ignore existing checkpoints and start clean. |

---

## What it sorts automatically vs. by hand

**Automated** (readable): Living Dex by National Dex, best-specimen contention
(Shiny → your OT → most 31-IVs → …), Duplicate Shinies, Good Trades (foreign OT),
Events (Cherish ball / GO / origin marks), Legendary / Mythical / Ultra Beast / Paradox
(by dex #), and Competitive / Breeding / Breedjects (by Judge IV counts).

**Left for you** (unreadable or v1-limited) — routed to Manual Sort boxes, never
released: Nature/EV/ribbon/mark/favorite-dependent choices, and **alternate forms**.
Note the v1 form limitation (see design §10): the sorter does not yet distinguish an
alt form from the base species, so when you own both, the extra copy lands in the
**ManualOther** box for hand-sorting rather than a dedicated Forms box.

**Never auto-released / never overwritten:** Shiny · your OT · Legendary · Mythical ·
Event · perfect-IV (6×31). The "Junk" box is a manual review bucket only.

---

## Manual-curated boxes (reference checklist)

These boxes are defined by attributes the automation **cannot read from HOME**
(ability, nature, moves, held item, egg group, language). The sorter never touches
them by content — keep them by hand. Recorded here as your reference:

### Box 60 — Utility ("expedition toolkit", 30 slots)
- **Hatcher** — Flame Body / Magma Armor (halves egg steps).
- **Catcher** — high level, False Swipe + a status move (Spore / Thunder Wave).
- **Synchronizer** — Synchronize in every nature (e.g. a box of Ralts) to force wild natures.
- **Farmers** — Pickup, to passively gather items.
- **Money Maker** — holds Amulet Coin, ideally knows Pay Day.
- **Evader / Flee Master** — Run Away, or holds Smoke Ball, for escaping encounters.

### Boxes 62 (–63) — Breeding (Masuda + competitive IV)
- **Ditto base** — foreign-language Dittos (Masuda shiny odds) + 5–6×31 IV Dittos in
  various natures (Modest / Adamant / Timid / Jolly …).
- **Egg Group Masters** — perfect-IV male parents by egg group (Field / Monster / Bug …)
  to pass egg moves + stats.
- **W.I.P. rows** — leave a section empty for sorting freshly hatched eggs.

**What the sorter *can* still do here:** a Ditto with high Judge IVs is detectable by
species + 31-count, so it will land in **Competitive** (6×31) or **Breeding** (3–5×31)
by the normal IV rules — a flawless Ditto currently files under Competitive (it's a
breeding tool; you may prefer to move it to Breeding by hand). Foreign-language Ditto is
only fuzzily inferable (OT-name language ≠ your game) and is **not** auto-filed.
Everything else in these two boxes is manual. (A future version could special-case
Ditto → Breeding and read held items if HOME's UI exposes them.)

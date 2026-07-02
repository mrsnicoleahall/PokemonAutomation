# Master Box Sorter — Usage Guide

`PokemonHome:BoxSorterMaster` sorts a large Pokémon HOME collection into Nicole's
Master Box Layout using only data the automation can read from HOME's summary and
Judge screens. It is resumable, space-safe, and never releases or overwrites a Pokémon.

See the design + implementation docs under `docs/superpowers/` for the full rationale.

---

## Prerequisites

- **Console:** Nintendo **Switch 2** with **Pokémon HOME** installed and open.
- **HOME Premium** — required for the Judge (IV) screen. Without it, IV-based routing
  (Competitive/Breeding and the stat tie-breaks) silently does nothing.
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
2. **Dry Run** of the Master Box Sorter (`DRY_RUN = true`). It catalogues boxes, computes
   the v3 plan, and writes `<output>_plan.json`, `<output>_boxmap.txt`, and
   `<output>_dex_overqualified.csv` **without moving anything**. Inspect the catalogue +
   plan + box-map to confirm reads and routing are correct.
3. **Full Run** (`DRY_RUN = false`) once the Dry Run looks right.

---

## HOME layout (physical box order) — v3

All boxes are enumerated in the `<output>_boxmap.txt` legend emitted at plan time.
In order (30 slots/box):

| # | Section | Notes |
|---|---|---|
| 1 | **Shiny Dex** | Best shiny of each species, National Dex order. Shiny-locked species are **omitted** (no gap). Unowned but shiny-able species → gap. |
| 2 | **+5 empty buffer** | Reserved after the Shiny Dex — sorter places nothing here. |
| 3 | **Regular Dex** | Best non-shiny of each species, all 1025 base species. Gap for unowned (annotated missing vs. owned-as-shiny-only). |
| 4 | **+5 empty buffer** | Reserved after the Regular Dex. |
| 5 | **Legendary** | |
| 6 | **Mythical** | |
| 7 | **Ultra Beasts** | |
| 8 | **Paradox** | |
| 9 | **Events** | Cherish Ball / GO / origin-mark signal. |
| 10 | **Forms / Variants / Regional Spins** | Detectable alt forms only (see §Limits). |
| 11 | **Breeding** | |
| 12 | **Utility** | Hatcher / Synchronizer / Farmer / Evader / Catcher / Money Maker. |
| 13 | **Competitive** | |
| 14 | **Shiny Trades** | Every shiny beyond the one in the Shiny Dex slot. |
| 15 | **Regular Trades** | Non-shiny duplicates with trade value (foreign OT / decent IVs / rare ball). |
| 16 | **Junk / Release** | Low-value duplicates; manual review only — nothing here is ever auto-released. |

---

## How it works (two passes)

- **Catalogue pass:** walks each scanned box, opens every Pokémon's summary (and Judge
  screen if `READ_IVS`), and writes `<output>.json` **after each box**. Stop any time.
- **Plan + Execute pass:** computes each Pokémon's target box/slot (v3 logic below),
  writes `<output>_plan.json`, `<output>_boxmap.txt`, and `<output>_dex_overqualified.csv`
  (always, including dry-run), then performs moves one at a time — writing
  `<output>_progress.json` after each move.

### Dex-first deduplication (v3 core rule)

For each species the sorter fills the dex slot **first**, then routes everything else:

- **Shiny Dex slot**: the single best shiny copy (most 31-IVs; tie → highest IV total) fills
  it. All remaining shinies → **Shiny Trades**.
- **Regular Dex slot**: the single best non-shiny copy fills it. Extra non-shinies → duplicate
  routing (see §Priority below).
- A species you own **only as shiny** leaves the Regular Dex slot empty and marks it
  "owned-as-shiny-only" in the catalogue CSV (not treated as missing).
- Legendaries, mythicals, UBs, paradox, and event Pokémon also fill their dex slot first;
  the collection boxes (Legendary, Mythical, UB, Paradox, Events) hold their **duplicates**.

### Duplicate priority (extras after dex slot is claimed)

Duplicates route to the **first** category they qualify for:

1. **Forms / Variants / Regional Spins** — detectable alt form (type/gmax/gender-distinct).
2. **Legendary / Mythical / Ultra Beast / Paradox / Events** — by dex-# set or event signal.
3. **Utility** — matches a Utility rule (ability / held item / move).
4. **Breeding** — IV best-count in the Breeding range (default 3–5 × 31).
5. **Competitive** — IV best-count ≥ Competitive threshold (default 6 = flawless).
6. **Shiny Trades** — duplicate is shiny.
7. **Regular Trades** — trade value: foreign OT, decent IVs, or rare ball.
8. **Junk / Release** — common low-value duplicate (subject to auto-protections below).

**Auto-protected, never junked:** shiny · your OT (nicole/cole) · legendary · mythical ·
event · perfect-IV (6×31) · rare ball.

### Overqualified-dex report

When the best copy of a species fills a dex slot, if it *also* qualifies for Forms,
a collection box, Utility, Breeding, or Competitive, it is recorded in
`<output>_dex_overqualified.csv` (dex position, species, which categories it matched) so
you can promote it manually if you prefer.

### Resume

- With **Fresh Start off** (default), restarting re-uses the JSON checkpoints: the
  catalogue pass skips boxes whose live occupancy still matches, and the execute pass
  re-reads affected boxes and performs only the moves not yet marked done.
- **Fresh Start on** ignores/overwrites the checkpoints and starts clean.
- If a box's occupancy changed since the last catalogue run (and no moves have executed
  yet), the program stops with a clear error naming the box — re-run with Fresh Start or
  re-catalogue.

### Space safety

Buffer boxes are reserved in the layout and the sorter will not place anything in them.
The program auto-reserves a scratch buffer at the end of your scanned range and only ever
moves a Pokémon into an empty/scratch slot. If it can't find room and `ALLOW_RELEASE_PROMPT`
is on (default), it **pauses and prompts you** to release the Junk/Release box to free space,
then continues once you confirm. The program itself **never releases** — you do the release
step; it only asks and waits. If you decline or it can't proceed, it stops with a clear
`UserSetupError` (never overwrites). If `ALLOW_RELEASE_PROMPT` is off, it stops immediately.

---

## Options (defaults in **bold**)

| Option | Default | Notes |
|---|---|---|
| `SCAN_BOX_START` | **1** | Must equal the layout's Shiny Dex start box (enforced). |
| `SCAN_BOX_COUNT` | — | How many boxes to scan/sort. |
| `OWNER_OT_NAMES` | **nicole, cole** | Your OTs (case-insensitive). "Mine vs Good Trades." |
| `READ_IVS` | **true** | Reads the Judge screen. Turn off to skip IV routing (faster). |
| `IV_LANGUAGE` | — | Must be set (e.g. English) when `READ_IVS` is on, or IVs read as blank. |
| `COMPETITIVE_MIN31` | **6** | Stats at 31 to count as Competitive (flawless). |
| `BREEDING_MIN`/`MAX` | **3 / 5** | 31-count range for the Breeding box. |
| `READ_EXTRAS` | **true** | Reads ability + nature + held item off the summary (no extra screen). Drives the Utility box. |
| `READ_MOVES` | **true** | Opens the moves screen per Pokémon to read moves (Catcher/Pay-Day). Slower + one more calibration surface; turn off to skip. |
| `EXTRAS_LANGUAGE` | — | Set (e.g. English) when `READ_EXTRAS` or `READ_MOVES` is on. Moves reuse this same setting — no separate `MOVES_LANGUAGE`. |
| `USE_V3_LAYOUT` | **true** | Use the v3 dual-dex layout (Shiny Dex first, then Regular Dex, buffers, new categories). Turn off to fall back to the v1/v2 layout — not recommended for new sorts. |
| `ALLOW_RELEASE_PROMPT` | **true** | When free space runs low during execution, pause and prompt you to release the Junk box before continuing. The program never releases anything itself. Turn off to stop immediately on low-space instead. |
| Utility target lists | abilities **flame-body, magma-armor, synchronize, pickup, run-away** · items **amulet-coin, smoke-ball** · moves **false-swipe, pay-day** | Editable; a Pokémon matching any target routes to Utility. |
| `VIDEO_DELAY` / `GAME_DELAY` | — | Capture-card / HOME-app timing. |
| `OUTPUT_FILE` | **home_catalogue** | Basename for all output files. |
| `DRY_RUN` | **false** | Catalogue + plan only; moves nothing. **Run this first.** |
| `FRESH_START` | **false** | Ignore existing checkpoints and start clean. |

---

## Outputs

| File | When produced | Contents |
|---|---|---|
| `<output>.json` | After each catalogued box | Full catalogue (species, shiny, OT, IVs, ability, moves, ball, …). |
| `<output>_plan.json` | After catalogue, before moves | Per-Pokémon routing: target box, slot, category. |
| `<output>_boxmap.txt` | At plan time (always, incl. dry-run) | Box-range legend: "Boxes 1–35: Shiny Dex", "Boxes 36–40: buffer", "Boxes 41–75: Regular Dex", "Boxes 76–80: buffer", … — every box labeled. |
| `<output>_dex_overqualified.csv` | At plan time (always, incl. dry-run) | Pokémon placed in a dex slot that also qualified for a higher category — for manual promotion review. |
| `<output>.csv` | At plan time (when EXPORT_CSV is on) | Full catalogue CSV — one row per Pokémon, including `category` and `dest_box` columns for per-Pokémon routing. |
| `<output>_progress.json` | After each move | Resume checkpoint. |

---

## What it sorts automatically vs. by hand

**Automated** (readable in v3): Shiny Dex (best shiny per species, shiny-locked skipped),
Regular Dex (best non-shiny per species, gaps annotated), dex-first deduplication, extra
shinies → Shiny Trades, collection boxes (Legendary / Mythical / Ultra Beast / Paradox /
Events), Competitive / Breeding (by Judge IV counts), Utility box (ability + held item +
moves), Regular Trades (foreign OT / decent IVs / rare ball), Junk.

**Left for you** (truly unreadable) — routed to Forms or Junk, never released:
**EV / ribbon / mark / favorite**-dependent choices, **egg group** (never shown in HOME),
and **cosmetic alt forms** (Unown letters, Vivillon patterns — undistinguishable; type/gmax/
gender-distinct forms ARE auto-detected). *(Nature, ability, held item, and moves are read.)*

**Never auto-released / never overwritten:** Shiny · your OT · Legendary · Mythical ·
Event · perfect-IV (6×31). The "Junk" box is a manual review bucket only.

---

## Utility box — AUTO-sorted

Reads ability + held item + moves; fills the Utility box automatically. A Pokémon matching
any target routes here (editable lists):

| Role | Auto-detected by | Default target |
|---|---|---|
| Hatcher | ability | Flame Body / Magma Armor |
| Synchronizer | ability (+ nature also read) | Synchronize |
| Farmers | ability | Pickup |
| Evader / Flee Master | ability / held item | Run Away / Smoke Ball |
| Money Maker | held item (+ move) | Amulet Coin / Pay Day |
| Catcher | move | False Swipe (Breloom and Smeargle both match) |

Utility is a **route-all** box (no single-slot contention) — a full Synchronizer set of
natures all land here. Utility is checked at **duplicate-routing** time (§Priority above),
so if a Pokémon claims the dex slot it stays in the dex even if it also matches Utility
(it will appear in `_dex_overqualified.csv`). Requires `READ_EXTRAS` (and `READ_MOVES`
for Catcher/Pay-Day roles).

## Breeding box — partial auto

- **Ditto base** — a Ditto with high Judge IVs auto-files to **Competitive** (6×31) or
  **Breeding** (3–5×31) by the normal IV rules. (A flawless Ditto lands in Competitive;
  move to Breeding by hand if you prefer.)
- **Egg Group Masters** — egg group is **never shown in HOME**, so grouping by egg group
  stays **manual**.

---

## Box renaming — SEPARATE, optional, post-sort

Box renaming is **not part of the sort run**. After sorting is complete, if you want
HOME's box-name fields labeled (e.g. "Shiny Dex 01", "Regular Dex 12", "Legendary",
"Junk"), run the standalone **`PokemonHome:RenameBoxes`** program. It reads the same
box-map the sort produced and types each label via the on-screen keyboard.

The OSK navigation timing is calibrated **interactively on the rig** — we rename one box
first, verify it, then batch the rest. It never touches Pokémon; a failed rename logs
and continues. We do this step **together** after a successful sort, not before.

---

## Limits (so nothing surprises you)

- **Shiny-locked list** (`shiny_locked.json`) is best-effort data, refinable — a mis-listed
  species will either get a gap it shouldn't, or fill a Shiny Dex slot it shouldn't.
- **Forms detection** is limited to what HOME exposes: type/gmax/gender-distinct forms are
  auto-detected; cosmetic same-type forms (Unown letters, Vivillon patterns) are not
  distinguishable and fall through per the duplicate priority.
- **EV / ribbon / mark / favorite** — never shown in HOME, always manual.
- **Egg group** — never shown in HOME, always manual.
- HOME Premium required for IV reads (`READ_IVS`).

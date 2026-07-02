# RESUME — live state (2026-07-02, hardware bring-up in progress)

## Where we are
- **v3 code: all 7 tasks DONE.** Final whole-branch review = "merge WITH FIXES". A fix subagent is applying:
  (#1) dedup dex keepers by dex_number in `route_all_v3` (two detectable forms sharing a dex# both claimed the dex slot);
  (#2) DRY_RUN must return BEFORE the release-prompt loop (so dry run never moves the cursor);
  (#3 minor) docs: drop phantom `_slot_routes.csv`, fix box-map example ranges.
  **After fix lands → re-verify build+tests → merge `feature/master-box-sorter-v3` → `main` → push to fork → rebuild.**
- `build_mac/SerialPrograms.app` = full v3 build. GUI opens now (fixed: settings COMMAND_LINE_TESTS.RUN was true → set false).

## Hardware (live)
- Board: **ESP32-S3-N16R8 (Dorhea)**. FLASHED with `Packages/Firmware/PABotBase2-ESP32-S3-2026063001.bin` (esptool via `.aqtvenv`), hash verified. LED **solid red = firmware running, not yet connected to a Switch**.
- Board ports (silkscreen): **"COM" = CH343 UART → Mac (control)**; **"USB" = native ESP32-S3 → flashed through it; goes to Switch 2 dock USB-A (wired controller)**.
- Cables: COM↔Mac = USB-C↔USB-C data; USB↔Switch-dock = USB-A↔USB-C data. (Micro-USB cable she bought is unused.)
- Mac currently sees serial device: **`/dev/cu.usbmodem5C4C1833451`** (native-USB CDC). No separate CH343/WCH device has appeared yet — port identity still being sorted; plan is to functionally test in-app.
- App is OPEN on her screen (settings panel showing).

## NEXT hardware steps (interactive with Nicole)
1. In app: **Controller dropdown → "Serial: PABotBase2"** → device `usbmodem5C4C1833451` → **"Wired Pro Controller"** (NS2 variant if offered). Test it connects to the board.
2. Connect board **USB/native port → Switch 2 dock USB-A**; Switch: **Controllers → Change Grip/Order** to attach the emulated controller.
3. Capture card: dock HDMI-OUT → capture card → Mac; app **Virtual Console → Camera** = capture card (Allow Camera/Mic).
4. **Calibrate**: run `CalibrateIVJudge` (dumps IV + ability/nature/item + moves crops) — tune crop coords + button timing (esp. HELD_ITEM_BOX, moves crops, moves-screen open button; and the Judge Y-press).
5. **Dry Run**: Master Box Sorter, `USE_V3_LAYOUT=on`, `DRY_RUN=on`, `SCAN_BOX_START=1`, scan all boxes, `OWNER_OT_NAMES=nicole,cole`, `READ_IVS/READ_EXTRAS/READ_MOVES=on`, IV/EXTRAS language=English. **Tip: set `ALLOW_RELEASE_PROMPT=off` for the dry run.** Produces `<output>.json` catalogue + `<output>.csv` + `<output>_dex_overqualified.csv` + `<output>_boxmap.txt` + `<output>_plan.json`. Review reads before any real sort.
6. Real sort only after dry-run reads look right. HOME **Premium** required for Judge/IVs.

## v3 behavior (final)
- HOME order: Shiny Dex → 5buf → Regular Dex → 5buf → Legendary → Mythical → Ultra Beasts → Paradox → Events → Forms → Breeding → Utility → Competitive → Shiny Trades → Regular Trades → Junk.
- Dex-first: best copy per species → dex (regular=best non-shiny, shiny=best shiny; shiny dex SKIPS shiny-locked, no gap; regular leaves gaps for missing). Extras route by priority: Forms > Legendary/Mythical/UB/Paradox/Events > Utility > Breeding > Competitive > ShinyTrades(if shiny) > RegularTrades > Junk. Never junk shiny/OT-yours/legendary/mythical/event/perfect-IV.
- Shiny dupes (incl. OT cole) → Shiny Trades; most powerful (IVs) keeps dex slot.
- Interactive: pause + ask to release Junk when out of room (never auto-releases). Never overwrites; never touches buffers.
- **Box rename = SEPARATE `PokemonHome:RenameBoxes` program, calibrated interactively LATER (log-only scaffold for now).**

## Known limitations
- Canonical base-form = first form seen per dex# in catalogue order (order-dependent; non-destructive). shiny_locked.json conservative — refine on rig. Cosmetic same-type forms not distinguishable. OSK box-rename + release-poll timing = rig-calibrated.

## Key paths / commands
- Repo `~/Projects/PokemonAutomator`; build `cd build_mac && cmake --build . -j 10` (Qt `-DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos"`).
- Tests: `./run-cli-tests.sh` (headless/offscreen). NEVER create `build_mac/SerialPrograms-Settings.json` (dual-settings modal).
- Flash: `.aqtvenv/bin/python -m esptool --chip esp32s3 -p /dev/cu.usbmodem<...> --baud 460800 write-flash 0x0 Packages/Firmware/PABotBase2-ESP32-S3-2026063001.bin`.
- GUI vs test mode: `~/Library/Application Support/SerialPrograms/UserSettings/SerialPrograms-Settings.json` (UTF-8 BOM) → `20-GlobalSettings.COMMAND_LINE_TESTS.RUN` (false=GUI).
- Ledgers: `.superpowers/sdd/progress.md` (v1), `progress-v2.md`, `progress-v3.md`.
- Published: code → `mrsnicoleahall/PokemonAutomation`; resources → `mrsnicoleahall/PokePackages` (remote `nicole`). v3 branch not yet merged/pushed (pending fix).

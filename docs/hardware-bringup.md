# Hardware Bring-Up — Step by Step (Switch 2 + ESP32-S3)

A beginner-friendly, do-it-in-order checklist for getting the Master Box Sorter running
on real hardware. **Tomorrow, just tell Claude "I'm ready" and it will drive each step
with you, checking the output as you go.** This page is the map; Claude is the co-pilot.

Nothing here moves a Pokémon until Phase 5, and even then only in Dry Run (no moves)
until we've confirmed everything reads correctly.

Sources: Pokémon Automation Setup Guide —
[ESP32-S3 controller](https://pokemonautomation.github.io/SetupGuide/Controllers/Controller-ESP32-S3.html),
[flash via terminal](https://pokemonautomation.github.io/SetupGuide/Controllers/ESP32-Flash-Terminal.html),
[general/video setup](https://pokemonautomation.github.io/SetupGuide/GeneralSetup.html).

---

## Phase 0 — Parts check (do this first, ideally TODAY)

- [ ] **ESP32-S3 dev board** (Dorhea).
- [ ] **Capture card** (4K USB3.0 HDMI).
- [ ] **HDMI cable** from the Switch 2 **dock's HDMI-OUT** into the capture card's HDMI-IN.
      *(The Switch 2 must be docked — handheld mode has no video out.)*
- [ ] **Cable 1 — ESP32-S3 ➜ Mac:** your USB-C→Micro-USB cable (Mac is USB-C). This goes
      to the board's **"COM"/"UART"** port.
- [ ] ⚠️ **Cable 2 — ESP32-S3 (board "USB"/"OTG" USB-C) ➜ Switch 2 dock FRONT USB-A:** the S3
      is a **wired** controller. Because we must be **docked** (the capture card feeds off the
      dock's HDMI-out on the back), the controller plugs into the **USB-A ("USB 2.0") port on the
      FRONT of the dock** (per Nintendo's dock diagram) — **NOT** the console's USB-C and **NOT**
      the dock's back (back = AC adapter / HDMI OUT / LAN only). You need a **USB-A ↔ USB-C data
      cable** (USB-A into the dock front, USB-C into the board's OTG port).
      *(The console's top USB-C only hosts a wired controller in tabletop mode, which we can't
      use here — no HDMI to the capture card.)*

> The board's two USB ports are **not interchangeable**. One is labeled COM/UART (to the
> Mac), the other USB/OTG (to the Switch). We'll identify them before plugging anything in.

---

## Phase 1 — Flash the ESP32-S3 firmware

Goal: put the `PABotBase2-ESP32-S3` firmware onto the board. `esptool` is already
installed (in `.aqtvenv`). We do this with the board connected to the **Mac only**
(COM/UART port).

- [ ] Plug the ESP32-S3 into the Mac (COM/UART port → Cable 1 → Mac).
- [ ] Find its serial port. Claude will run:
      `ls /dev/tty.usbserial-* /dev/tty.usbmodem*` — we note the exact name.
      *(If nothing shows up, the board may need a USB-serial driver — Claude checks the
      chip and installs the CH34x/CP210x driver if needed.)*
- [ ] Flash it (Claude runs, filling in the real port):
      ```
      cd /Users/nicole/Projects/PokemonAutomator/Packages/Firmware
      /Users/nicole/Projects/PokemonAutomator/.aqtvenv/bin/python -m esptool \
        -p /dev/tty.usbserial-XXXX write-flash 0x0 PABotBase2-ESP32-S3-2026063001.bin
      ```
      *(Some boards need holding BOOT while tapping RESET to enter flash mode — Claude
      will tell you if the flash doesn't start.)*
- [ ] Confirm it prints "Hash of data verified" / "Leaving..." = success.

---

## Phase 2 — Connect the controller to the Switch 2

- [ ] Leave Cable 1 (board COM/UART → Mac) connected.
- [ ] Connect **Cable 2**: board **USB/OTG** port → **Switch 2** (dock USB-A, or console USB-C).
- [ ] On the Switch 2: go to **Controllers → Change Grip/Order** (this is the only screen
      where a controller can pair/attach).
- [ ] In **SerialPrograms** (we launch it in Phase 3), the Controller dropdowns will be:
  - 1st: **"Serial: PABotBase2"**  *(not the older "Serial: PABotBase")*
  - 2nd: your serial device (the `/dev/tty.usbserial-XXXX` from Phase 1)
  - 3rd: **"NS1: Wired Pro Controller"** — *on Switch 2 there may be an "NS2" option; Claude
    will pick the right one with you.*
- [ ] Press the connect action; a controller should appear in the Grip menu. If not:
  press the board's **EN/RESET**, click **"Reset Ctrl"**, wait ~5s. (Reconnect works
  reliably on Switch 2.)

---

## Phase 3 — Capture card + video

- [ ] HDMI: Switch 2 dock HDMI-OUT → capture card HDMI-IN. Capture card USB → Mac.
- [ ] Launch the app: `open /Users/nicole/Projects/PokemonAutomator/build_mac/SerialPrograms.app`
      *(First launch: macOS asks for **Camera** and **Microphone** permission — say **Allow**.
      That's the capture-card feed, not your real webcam.)*
- [ ] In SerialPrograms, open the **Virtual Console** panel.
- [ ] **Camera** dropdown → select the capture card. If missing, click **"Reset Camera"**.
      The black bar becomes your Switch 2 screen when it connects.
- [ ] **Audio Input** → the capture card's audio (optional; nice for confirmation).

At this point the app can **see** the Switch 2 and **control** it. We test with a couple of
manual button presses before automating anything.

---

## Phase 4 — Calibrate the IV Judge reader (one-time)

- [ ] Confirm **HOME Premium** is active (needed for the Judge screen). ✅ (you have it)
- [ ] In HOME, open any Pokémon's **summary** screen.
- [ ] Run the **`CalibrateIVJudge`** program (Developer-mode panel). It presses Y to open
      the Judge view, reads the six stat ratings, logs them, and dumps the six crop images.
- [ ] Claude checks the log + dumped crops against a Pokémon whose IVs you know, and nudges
      the crop coordinates / Y-press timing in `PokemonHome_IvJudgeReader.cpp` until all six
      read correctly. Rebuild, re-run, repeat until stable.

---

## Phase 5 — Dry Run (reads only, moves NOTHING)

- [ ] Open the **Master Box Sorter** program.
- [ ] Set options with Claude: `SCAN_BOX_START` = your living-dex start box (must match the
      layout's start), `SCAN_BOX_COUNT` = start with **1 box** for the first test,
      `OWNER_OT_NAMES` = `nicole, cole`, `READ_IVS` = on, `IV_LANGUAGE` = English,
      **`DRY_RUN` = ON**.
- [ ] Run it. It catalogues that box and writes `home_catalogue.json` + `home_catalogue_plan.json`.
- [ ] Claude reviews the JSON with you: are dex numbers, shininess, OT, and IV counts correct?
      Does the plan route things sensibly? Fix any reader issues before proceeding.

---

## Phase 6 — First real sort (small, supervised)

- [ ] Still on a **small range** (1–2 boxes), set **`DRY_RUN` = OFF** and run.
- [ ] Watch the first few moves. The program verifies each move and **stops with an error
      rather than ever overwriting** a slot. If it stops, we read the message together.
- [ ] Once a small run looks right, scale `SCAN_BOX_COUNT` up. You can **stop any time**;
      restarting resumes safely (it re-reads the live board and re-plans).

---

## Golden rules
- **Dry Run before every new range.** Confirm reads before moves.
- The sorter **never releases** a Pokémon and **never overwrites** a slot — worst case it
  stops and asks for help. That's by design.
- Stuck or unsure? Stop and ask Claude. We go one step at a time.

## Known v1 limits (so nothing surprises you)
- Ability / moves / held item aren't read **yet** (they're on-screen; a planned v2 will read
  them to auto-fill the Utility/Breeding boxes). For now those boxes are hand-curated —
  see `docs/BoxSorterMaster-usage.md`.
- Alternate forms sharing a Dex # aren't separated; extra copies land in a manual box.

# Master Box Sorter v3 — Dual Dex, new priority, trades split, dex gaps (design)

**Date:** 2026-07-02
**Builds on:** v1 (`2026-07-01-home-master-box-sorter-design.md`) + v2 (`…-v2-attributes-design.md`).
**Goal:** Rework the sort into Nicole's finalized scheme: a **Regular (non-shiny) Dex** and a
**Shiny Dex** (both full National Dex order with gaps for what she doesn't own), the collection
boxes kept, a Forms box, functional boxes, split **Shiny/Regular Trades**, and Junk — with a new
priority, shiny-locked skipping in the shiny dex, per-dex buffer boxes, extra reporting, and an
optional interactive release step. Everything v1/v2 guarantees still holds (readable-only, never
release/overwrite without asking, resumable, IV/ability/moves reading).

---

## 1. HOME layout (physical box order) — the target arrangement

In order, 30 slots/box. All boxes are enumerated in a **box-map legend** the program emits
(see §9) so every box is clearly labeled.

1. **Shiny Dex** (FIRST) — shiny copy of each species in National Dex order, **skipping
   shiny-locked species** (NO gap for a species that can't be shiny), gaps left for shiny-able
   species not yet owned.
2. **+5 empty boxes** (buffer after the Shiny Dex — 5 minimum).
3. **Regular Dex** — every species (base forms) in National Dex order, **gaps left for species
   not owned** (e.g. missing 955 → its slot stays empty between 954 and 956). ~35 boxes (1025 species).
4. **+5 empty boxes** (buffer after the Regular Dex — 5 minimum).
5. **Legendary** · 6. **Mythical** · 7. **Ultra Beasts** · 8. **Paradox** · 9. **Events**
   (collection boxes, right after the dexes, before Forms).
10. **Forms / Variants / Regional Spins**
11. **Breeding** · 12. **Utility** · 13. **Competitive**
14. **Shiny Trades** · 15. **Regular Trades**
16. **Junk / Release**

Box counts per section are computed from the layout config; each section is a labeled range in
the box-map. (Legendary/Mythical/UB/Paradox/Events/Forms/functional/trades sizes are configurable,
default 1–3 boxes each, growable.)

---

## 2. Dex-first fill + de-duplication (the core rule)

For each **species/form slot** (Regular Dex, and Shiny Dex for shiny copies):
- **The single best copy Nicole owns fills the dex slot.** "Best / most powerful" =
  highest **IV best-count** (# of 31s via the Judge screen), tie-broken by higher
  **IV total estimate**; if still equal, either copy (arbitrary/first).
- **All remaining copies are duplicates** and route to the special/trade boxes by §3 priority.
- Legendaries, mythicals, UBs, paradox, event mons ALSO fill the dex first (they are Dex
  entries); the collection boxes (§1.5–9) hold their **duplicates**.

Shiny handling:
- The best **shiny** of a species fills the **Shiny Dex** slot; extra shinies → **Shiny Trades**
  (this covers the "several OT-`cole` shiny duplicates" case — the best stays, extras trade).
- The best **non-shiny** fills the **Regular Dex** slot; extra non-shinies → duplicate routing
  (§3), ending in **Regular Trades** or **Junk**.

---

## 3. Priority for DUPLICATES / extras (dex already filled)

A duplicate (any copy beyond the one that claimed its dex slot) routes to the **first** box it
qualifies for, in this order:

1. **Forms / Variants / Regional Spins** — the copy is a detectable alt form (type/gmax/gender-
   distinct; see readability note §7).
2. **Legendary / Mythical / Ultra Beast / Paradox / Events** — by dex-number set / event signal
   (Cherish ball, GO/origin mark). *(Placement of the collection boxes above the functional boxes
   is a deliberate default so spare rare species stay grouped; tunable — flag for review.)*
3. **Utility** — matches a Utility rule (ability/item/move; v2).
4. **Breeding** — IV best-count in the Breeding range (default 3–5).
5. **Competitive** — IV best-count ≥ the Competitive threshold (default 6 = flawless).
6. **Shiny Trades** — the duplicate is **shiny**.
7. **Regular Trades** — tradable value: OT ∉ {nicole, cole}, or decent IVs / rare ball, etc.
8. **Junk / Release** — common low-value duplicate. **Auto-protected, never junked:** shiny,
   OT nicole/cole, legendary, mythical, event, perfect-IV (6×31), rare ball. (Nothing is ever
   auto-released — Junk is a review bucket; see §8.)

Note the difference from v2: **Utility/Breeding/Competitive now outrank the dex only for
DUPLICATES** — the dex still gets one of each species first (§2). The single-Pokémon "special box
vs dex" fight is resolved dex-first.

### 3a. Overqualified-dex report
While filling the dex, any copy placed in a dex slot that ALSO qualifies for Forms / a collection
box / Utility / Breeding / Competitive is recorded to **`<output>_dex_overqualified.csv`**
(dex position, species, and which higher categories it matched) so Nicole can manually promote it.

---

## 4. The two dexes in detail

- **Shiny Dex (physically first):** slots only for **shiny-able** species (shiny-locked species
  omitted entirely — no gap). Uses a new data resource `shiny_locked.json` (species that can never
  be legit shiny / not shiny-obtainable in HOME). Placed by best shiny copy; shiny-able but unowned → gap.
- **Regular Dex:** slots for all 1025 base species in Dex order; placed by **best non-shiny copy**.
  Gap handling — a Regular-Dex empty slot is annotated in the report as one of:
  - **"missing"** — Nicole owns no copy of this species at all, or
  - **"owned as shiny only"** — she owns only a shiny (which lives in the Shiny Dex), so the
    species IS collected; the Regular-Dex slot is left empty but flagged as satisfied-by-shiny (per
    Nicole's "a lone shiny counts toward the Regular Dex" decision).
  The physical rule stays simple: best non-shiny → Regular Dex; best shiny → Shiny Dex; each
  independent. A single physical Pokémon occupies exactly one slot.
- **+5 empty boxes** after each dex (reserved; the sorter must not place anything in them).

---

## 5. Trades split
- **Shiny Trades:** every shiny beyond the one in the Shiny Dex slot.
- **Regular Trades:** non-shiny duplicates with trade value (foreign OT / decent IV / rare ball).
- Everything below trade value → **Junk/Release** (subject to §3.8 protections).

---

## 6. Interactive release-to-free-space (optional, opt-in)
New option `ALLOW_RELEASE_PROMPT` (default **on**). When free space runs low during execution
(configurable threshold, e.g. < 1 box of free slots and the plan still needs room), the program
**pauses and prompts Nicole** (via a notification / on-screen log + a wait) to release the
Junk/Release box to free space, then continues once she confirms. The program itself **never
releases** — Nicole does the release; the program only asks and waits. If she declines / it can't
proceed, it stops with a clear `UserSetupError` (never overwrites).

---

## 7. Readability constraints (unchanged from v2)
Forms detection is limited to what HOME shows the reader (type/gmax/gender-distinct forms;
cosmetic same-type forms like Unown letters / Vivillon patterns are NOT distinguishable, and
`base_form_signature` handling is still imperfect). The Forms box therefore auto-captures only
detectable variants; undetermined ones fall through per priority. IVs need HOME Premium (Judge).
Shiny-lock list is best-effort data, refinable.

---

## 8. Never-destructive (unchanged)
No auto-release; no overwrite. Out of space → prompt (if §6 enabled) then stop with a clear error.
Two-pass, resumable via JSON checkpoints; execute-resume re-derives from live state (v1 fix).

---

## 9. Outputs / labeling
- **Box-map legend** (`<output>_boxmap.txt`/`.csv`): every box range labeled — e.g.
  "Boxes 1–35: Shiny Dex", "Boxes 36–40: (buffer)", "Box 71: Shiny Trades".
- **Full catalogue CSV** (existing) + **overqualified-dex CSV** (§3a).

### 9a. Automated in-HOME box renaming — SEPARATE, OPTIONAL, POST-SORT (built interactively)
This is a **standalone program** (`PokemonHome:RenameBoxes`), **not part of the sort run** and not
a sort option. Nicole runs it **after** sorting is complete, only if she wants it. It reads the
same box-map the legend uses and types each box's label into HOME's box-name field via the
on-screen keyboard (e.g. `Shiny Dex 01`, `Regular Dex 12`, `Legendary`, `Junk`).

**Development approach: interactive / collaborative on the rig, NOT built blind.** OSK text entry
through the microcontroller is the riskiest, untestable-in-this-environment piece. So v3 only
**scaffolds** it — the program structure + label generation from the box-map + a
`start_box`/`count` range so we can rename ONE box first — and the actual on-screen-keyboard
navigation + per-key timing are calibrated **together with Nicole, live**, once the hardware is
set up (rename one box → verify → refine → batch). It runs as its own pass so a rename hiccup
never affects the sort; it never touches Pokémon; a failed rename logs and continues.
Sequencing: the core v3 sort is built and validated FIRST; box-renaming is a follow-up we do
together after a successful sort.

---

## 10. What changes in code (high level)
- **`master_box_layout.json`** → new v3 layout: two dex ranges + two 10-box buffers + collection
  boxes + Forms + Breeding/Utility/Competitive + ShinyTrades + RegularTrades + Junk, each a labeled
  range. New `BoxCategory` values: `RegularDex, ShinyDex, ShinyTrades, RegularTrades, Junk`
  (rename/replace the old `LivingDex/DuplicateShiny/GoodTrades/ManualForms/ManualOther` set as
  needed; keep Legendary/Mythical/UltraBeast/Paradox/Events/Competitive/Breeding/Utility).
- **Router/planner** → dex-first per-species dedup (best copy → dex, split shiny/non-shiny),
  duplicate routing by §3, gaps for unowned, shiny-locked skip in Shiny Dex, reserve buffer boxes,
  emit the overqualified list.
- **New data** `shiny_locked.json` (Packages/PokePackages).
- **Program** → `ALLOW_RELEASE_PROMPT` + the pause/prompt/resume; box-map + overqualified CSV
  writes at the plan point (dry-run too).
- Pure logic (dedup, priority, dex slotting, shiny-lock skip, overqualified detection) is
  **unit-tested**; hardware paths compile-verified + calibrated on the rig.

---

## 11. Sequencing note
Hardware bring-up (flash/connect/**calibrate**) and a **Dry-Run catalogue + full CSV** are
routing-independent and can be done as soon as the hardware arrives, on the current build. v3
changes the **plan/execute**; after v3 is built, a v3 Dry Run validates the new plan before any
real moves.

---

## 12. Out of scope (v3)
Perfect cosmetic-form discrimination; EV/ribbon/mark/favorite reading; non-English OCR.
Shiny-locked list is seeded best-effort and refinable. (Automated box renaming is now IN scope — §9a.)

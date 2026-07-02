# Task 3 Report — MasterBoxRouterV3

**Status:** COMPLETE — all tests GREEN.

---

## What was built

### New files
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouterV3.h`
- `SerialPrograms/Source/PokemonHome/Programs/PokemonHome_MasterBoxRouterV3.cpp`

### Modified files
- `SerialPrograms/cmake/SourceFiles.cmake` — added the two new files
- `SerialPrograms/Source/Tests/PokemonHome_Tests.h` — declared `test_pokemonHome_MasterRouterV3`
- `SerialPrograms/Source/Tests/PokemonHome_Tests.cpp` — implemented the test (cases a–f)
- `SerialPrograms/Source/Tests/TestMap.cpp` — registered `"PokemonHome_MasterRouterV3"`
- `CommandLineTests/PokemonHome/MasterRouterV3/dummy.png` — test fixture

---

## Structures and signatures (exact, for T4)

```cpp
enum class DexAssign { RegularDexSlot, ShinyDexSlot, Duplicate };

struct SpeciesKey {
    uint16_t                       dex_number;
    Pokemon::PokemonType           t1, t2;
    bool                           gmax;
    Pokemon::StatsHuntGenderFilter gender;
    bool operator==(const SpeciesKey&) const noexcept;
    bool operator<(const SpeciesKey&) const noexcept;
};
// + std::hash<SpeciesKey> specialisation in namespace std

struct RouteResultV3 {
    BoxCategory              category      = BoxCategory::Junk;
    bool                     is_dex_keeper = false;
    std::vector<std::string> also_qualifies;
};

BoxCategory route_duplicate_v3(
    const Pokemon::CollectedPokemonInfo& p,
    const MasterBoxLayoutV3& layout,
    const RouterConfig& cfg
);

std::vector<RouteResultV3> route_all_v3(
    const std::vector<std::optional<Pokemon::CollectedPokemonInfo>>& catalogue,
    const MasterBoxLayoutV3& layout,
    const RouterConfig& cfg
);
```

---

## Algorithm summary

### route_all_v3
1. Index all non-empty catalogue entries by `SpeciesKey` (dex_number + t1 + t2 + gmax + gender) using `std::map<SpeciesKey, vector<size_t>>`.
2. Track first `SpeciesKey` seen per `dex_number` as canonical — groups whose key differs are variant groups (Forms).
3. In each group:
   - Best non-shiny copy (by `wins_slot`) → `RegularDex` keeper.
   - Best shiny copy (by `wins_slot`) → `ShinyDex` keeper, unless `dex_number ∈ layout.shiny_locked`.
   - All other copies → `route_duplicate_impl(p, layout, cfg, is_variant)`.
4. For each dex keeper, populate `also_qualifies` (Forms / Legendary / Mythical / UltraBeast / Paradox / Events / Utility / Breeding / Competitive).

### route_duplicate_v3 priority chain
ManualForms → Legendary → Mythical → UltraBeast → Paradox → Events → Utility → Breeding → Competitive → ShinyTrades → RegularTrades → Junk

Empty slots: default `RouteResultV3{Junk, false, {}}`.

---

## Tests (all GREEN)

| Case | What is tested |
|------|----------------|
| (a) | Two non-shiny same species: best → RegularDex keeper, other → Duplicate/RegularTrades |
| (b) | Two shiny same species: best → ShinyDex keeper, other → ShinyTrades |
| (c) | Shiny of shiny-locked species (Victini/494): NOT ShinyDex keeper |
| (d1) | Duplicate legendary (dex 144) → Legendary |
| (d2) | 6×31 duplicate → Competitive |
| (d3) | Shiny duplicate → ShinyTrades |
| (d4) | Foreign OT, zero IV duplicate → RegularTrades |
| (d5) | Owner-OT, zero IV, no flags → Junk |
| (d6) | Utility ability match → Utility |
| (e) | 6×31 dex keeper has "Competitive" in also_qualifies |
| (f) | Empty (nullopt) slot → default (Junk, not keeper) |

Total suite: **10 tests passed**, 0 failed.

---

## Concerns / known limitations

### Forms detection
`route_duplicate_v3` called in isolation always passes `is_variant=false` — Forms detection never fires without `route_all_v3`'s grouping context. T4 must call `route_all_v3` for the variant flag to propagate. This matches v2 documented limitation (spec §7) and the brief's explicit statement.

### Canonical base-form heuristic
Variant detection uses "first SpeciesKey seen for a dex#" as canonical. Catalogue order determines canonicality, not a species DB. A species DB fix is out of scope per spec.

### Cosmetic-only forms
Unown letters, Vivillon patterns — same types/gmax/gender — land in the same SpeciesKey group (indistinguishable by reader). Matches v2 behavior.

---

## Reuse
- `wins_slot` (from `PokemonHome_MasterBoxPlanner.h`) — used as-is.
- `p_matches_utility`, `is_owner_ot` (from `PokemonHome_MasterBoxRouter.h`) — used as-is.
- `MasterBoxLayoutV3`, `RouterConfig`, `BoxCategory` — consumed, not changed.

### How the planner works (relevant excerpt)

`build_master_plan` initializes per-category slot counters by iterating `layout.category_box_ranges`:

```cpp
for (auto& [cat, range] : layout.category_box_ranges){
    bucket_next[cat] = 0;
}
```

Non-LivingDex entries are dispatched via `assign_bucket(ci, cat)`, which calls `category_flat(cat, n)` — a helper that looks up the range in the same map. There is no hardcoded set of bucket categories anywhere in the planner. Any `BoxCategory` value present in `layout.category_box_ranges` is automatically treated as a bucket with append-order assignment and no single-slot contention.

`BoxCategory::Utility` was added in Task 1 and its range was added to `master_box_layout.json` in Task 2. The planner therefore handles it correctly with zero additional logic.

## What was changed

### 1. `category_name()` fix (minor, `PokemonHome_MasterBoxPlanner.cpp`)
Added `case BoxCategory::Utility: return "Utility";` so overflow warning messages print "Utility" instead of "Unknown". This was the only planner file change.

### 2. Regression test `test_pokemonHome_UtilityPlan` (`PokemonHome_Tests.cpp`)
Mirrors the layout/config construction style of `test_pokemonHome_MasterPlanner`.

Setup:
- Layout includes `BoxCategory::Utility = {60, 60}` (1-indexed box range, 30 slots).
- `RouterConfig.utility_rules = [{UtilityRule::Ability, "flame-body"}]`.
- Catalogue: two mons with `ability_slug="flame-body"` at catalogue[0] and catalogue[1].
- `scan_start=0`, scratch at box 65.

Assertions:
1. No `[BLOCKING]` warnings.
2. No `ManualOther` overflow warnings (neither mon dropped).
3. Both mons moved into Utility box (0-indexed box 59 = 1-indexed box 60), landing at slots 0 and 1 respectively.

### 3. Header declaration (`PokemonHome_Tests.h`)
Added `int test_pokemonHome_UtilityPlan(const ImageViewRGB32& image);`.

### 4. TestMap registration (`TestMap.cpp`)
Registered `"PokemonHome_UtilityPlan"` between `PokemonHome_MasterPlanner` and `PokemonHome_UtilityRouting`.

### 5. Fixture (`CommandLineTests/PokemonHome/UtilityPlan/dummy.png`)
Copied from `MasterPlanner/dummy.png`.

## Test result

```
6 tests passed
---- exit code: 0 ----
```

`PokemonHome_UtilityPlan` passed on first run — confirming the planner was already generic and the test serves as a regression guard going forward.

## Commit

`d5c75132a` — `test(planner): add regression test for Utility bucket routing`

## Concerns

None. The planner's generic design means any future `BoxCategory` added to `category_box_ranges` will automatically behave as a bucket without planner changes.

---

## Test-quality fixes (test-only, no production code changed)

**Commit:** `9cf5576dd` — `test(PokemonHome): tighten MasterRouterV3 case-c assertion + fix misleading (d4) comment`

### Fix 1 — case (c): tightened category assertion

Added a concrete `TEST_RESULT_EQUAL` asserting `results[0].category == BoxCategory::Competitive`
for the shiny-locked Victini (6×31, owner-OT "nicole"). The existing
`is_dex_keeper == false` and `category != ShinyDex` assertions were preserved.
The new assertion passed immediately — production routing was already correct.

### Fix 2 — case (d4): corrected misleading comment

Changed `// (d4) Plain low-value duplicate → BoxCategory::Junk` to
`// (d4) Foreign-OT non-shiny duplicate (has trade value) → RegularTrades`
to match the assertion (`BoxCategory::RegularTrades`).

### Test result after fixes

```
10 tests passed
---- exit code: 0 ----
```

`MasterRouterV3` passes with the tightened case-(c) assertion.
Case-(c) observed category: **BoxCategory::Competitive** (correct).

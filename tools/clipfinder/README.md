# clipfinder

The wall push clip scan for OoT and MM, native and multithreaded. It reads a
scene from `models/`, builds the same collision model the viewer does, and
runs the search in the game's f32 arithmetic. It writes the clip points as
JSON, which you can load with the viewer's **Import results** button or run in
game with `tools/clipfinder/wall_clip_tester.lua`.

It also has tools for studying a single clip: the lowest speed that does it,
the angles that work, and a step-by-step single-frame simulation.

## Building

```bash
sh tools/clipfinder/build.sh
```

This needs MSYS2's mingw64 g++ (`C:\msys64\mingw64\bin`). The exe is static,
so it runs without the MSYS2 DLLs. `-ffp-contract=off` keeps the f32 maths
from being fused into multiply-adds, which would change the results.

A rebuild fails at the link step (`ld returned 1 exit status`) while
`clipfinder.exe` is running. Wait for the run to finish first, or build a
copy somewhere else with `OUT=path/to/other.exe sh tools/clipfinder/build.sh`.

The source is in `src/`, split by layer; `src/main.cpp`'s header comment
lists what each file holds. `-flto=auto` lets the hot collision checks inline across files, so
the split costs no speed.

## Examples

```bash
# One map, one form
tools/clipfinder/clipfinder.exe --game MM --map "Laundry Pool" --form Human -o tools/clipfinder/results/laundry.json

# Every map, adult and child, falling clips too, one file per map
tools/clipfinder/clipfinder.exe --game OOT --all --form Adult,Child --falling --out-dir tools/clipfinder/results

# Resume an --all run after the map where it stopped
tools/clipfinder/clipfinder.exe --game MM --all --form All --after "Laundry Pool" --out-dir tools/clipfinder/results

# The lowest speed for one clip, exactly, and the angles that work
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --pair 50,90 --refine -o tools/clipfinder/results/tcs_50_90.json
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --pair 50,90 --angles --max-speed 11 --out-dir tools/clipfinder/results

# That clip at one yaw, at speed 15 or less
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --pair 50,90 --yaw 0xF000 --max-speed 15 -o tools/clipfinder/results/tcs_50_90_f000.json

# A start for each yaw from 0xFF80 to 0x0000, at speed 10.5 or less
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Deku --pair 50,90 --yaw 0xFF80-0x0000 --max-speed 10.5 -o tools/clipfinder/results/tcs_50_90_range.json

# One frame, step by step
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --sim "-239.859,0,824.246,0xFF9D,11" --out-dir tools/clipfinder/results

# One map's dynapoly actors in setup 2 (from the viewer's export), only the wall pairs with a dynapoly wall in them
tools/clipfinder/clipfinder.exe --game OOT --map "Spot 01 - Kakariko Village" --form All --falling --dyna OOT_dyna_all.json --setup 2 --dyna-only -o tools/clipfinder/results/kak_dyna.json

# Every map's dynapolys, every setup ("Export all dynapolys"), one file per map and set of setups
tools/clipfinder/clipfinder.exe --game OOT --all --form All --falling --dyna OOT_dyna_all.json --dyna-only --out-dir tools/clipfinder/results
```

Progress and summaries print to the terminal (stderr). The JSON goes to the
output file.

## Options

### What to scan

| Option | Meaning |
|---|---|
| `--game OOT\|MM` | **Required.** OoT US 1.0 or MM US scenes. |
| `--map "<name>"` | One map, named exactly as in the viewer's map list (`js/model_list.js`), e.g. `"Spot 01 - Kakariko Village"`, `"Laundry Pool"`. |
| `--all` | Every map of the game, one JSON per map (see `--out-dir`). Use this or `--map`. |
| `--after "<name>"` | With `--all`: skip the maps up to and including this one, to resume a run that stopped. |
| `--form <forms>` | Link's form, which sets the wall check radius and height. OoT: `Adult` (radius 18), `Child` (14), `Crawlspace` (10, check height 15: the crawling state, child only). MM: `Human` (14), `Deku` (14), `Zora` (18), `Goron` (19.5), `FierceDeity` (27). `All` means every form of the game. A comma-separated list, like `Adult,Child`, means just those. Default: `Adult` / `Human`. |
| `--radius R` | Use radius R instead of the form's. Single form only. |

Several forms go into **one** JSON, with each clip marked with its form.
Forms with the same radius and check height share one scan and are labelled
together, e.g. `"Human/Deku"`.

A smaller radius's clips are **not** a subset of a bigger radius's. Resting
spots, what fits between walls, and which walls are in reach all change with
the radius, so scan each form you care about.

### What to look for

| Option | Meaning |
|---|---|
| `--falling` | Also look for clips while falling ("low" clips, `drop` > 0). Link's post-move position (posNext) is 2–30 below the floor, so the wall check runs lower than when walking. Slower. Falling crossings are only generated for drops where `checkHeight + dy >= 5`, and each move is checked again from its own start: the drop is measured from the floor at the clip point, and downhill Link starts higher, so he can fall further than that (tested in game: OoT Kakariko Village child, 20 such moves through TRI 673 / 21 / 26 / 20 / 28 falling 21-26 from the start, none clipped). Beyond that, the game's line test runs from Link's feet with floors included and stops him on his own floor (MM Human: drop over 21.8, OoT: over 21, Crawlspace: over 10). |
| `--extended-only` | Keep only the wall pairs (pusher, clipped wall) that clip **only** thanks to the walls' extended planes: the 1-unit / `detMax 300` tolerance of the game's triangle checks, or the pusher reaching past its own edge. (The game's wall check projects Link onto a wall along the Z or X axis, not the wall's normal, so a diagonal wall also pushes Link standing beside it, past its end: at 45 degrees as far past as he is in front of its plane.) See **Acute or extended** below for how a wall pair is categorised; this leaves out every acute pair, all its points included. The terminal says how many pairs and points were left out. With `--first-per-pair`, a pair whose first point found was extended isn't checked for acute points afterwards. |
| `--first-per-pair` | Keep the first clip found for each wall pair (pushing wall, clipped wall), like the tester's one-per-pair recording mode. The output is much smaller, but the scan isn't much faster: most of the time goes on points that never clip. |
| `--max-per-pair N` | Keep at most N points of each wall pair, spread out evenly: the first chosen, then over and over the point farthest from all the ones chosen so far (so they cover the pair end to end, e.g. 10 points 120 apart along a 1300 long wall). Each row the viewer shows a pair in (crossing / standing, walking / falling, and each kind) is thinned separately, so no row goes missing. Always kept: the lowest `--min-speed` reach (thinning runs after it), and a point that makes an acute pair acute. Smaller files: OoT Kakariko Village, child, `--falling`: 16178 points / 6.8 MB, with `--max-per-pair 10` 1403 points / 0.59 MB, all 184 rows still there. Written to the JSON as `"maxPerPair"`. Not applied with `--refine`, `--yaw` or `--angles`. |
| `--pair P,C` | Keep only the clips where TRI P pushes Link through TRI C (polygon ids, as the viewer and tester show them; for a slope or ground clip P is the floor, C the wall). Needed for `--refine` / `--angles`. The scan only looks near the two triangles: the wall pairs and slope walls within a frame's move (`--max-move`) plus two radii and 10 of them. Every triangle still collides as usual, and the pair's clips come out the same as a whole-map scan's (OoT Death Mountain Trail setup 2, falling, TRI 90 → 25: 168 s → 14 s). |
| `--dyna FILE` | Add the dynapoly actors, from the viewer's **Export all dynapolys** (every map's, every setup; see **Dynapolys** below). A single map's `dynapoly-1` export (the old **Export dynapolys**) still works. Each map is scanned once per set of setups with the same dynapolys, and once without dynapolys if the file has none for it. Output files named by `--out-dir` get `_setup<N>[-<N>...]_dyna` added; with `-o` and several sets, `_setup...` goes before `.json`. |
| `--no-slope` | Leave out the slope clips (see **Slope clips** below). |
| `--slope-only` | Only the slope clips: the wall push and ground clip scans are skipped. `--out-dir` names the file `..._slope.json`. |
| `--no-ground` | Leave out the ground clips (see **Ground clips** below). |
| `--ground-only` | Only the ground clips: the wall push and slope clip scans are skipped. `--out-dir` names the file `..._ground.json`, so it doesn't overwrite the full scan's. |
| `--slope-step 1\|2\|3` | The slope clip scan's widest step along a wall's bottom edge (default 3; see **Slope clips** below). `1` searches every unit. |
| `--ground-step 1\|2\|3` | The ground clip scan's widest step along a wall's bottom edge (default 3; see **Ground clips** below). `1` searches every unit: the most (floor, wall) pairs, about 2.5x as long. |
| `--keep-load-void` | Keep the clips whose start is on a loading zone (a floor with an exit, `SurfaceType_GetExitIndex`) or a void plane (floor property 5 / 12, MM 13 too). Left out by default: standing there takes Link out of the scene before any clip matters. Only the floor under the start counts; dynapolys never do (the export has no surface types). E.g. OoT Death Mountain Trail, adult: 374 of 1818 points, nearly all of TRI 348 → 346 / 345, start on the summit's exit to the crater (TRI 453, exit 5). `--sim` prints the start's floor, and `--tri` a poly's exit and floor property. |
| `--slope-starts` | Also search the crossing points whose surroundings are only in bounds when the rays that go into a slope first are ignored (see **In bounds** below). Finds some more crossing clips starting on slopes, but about 3x slower on a mountain: OoT Death Mountain Trail setup 2, adult, falling: 21333 points (4 more wall pairs) in 110 s instead of 21039 in 38 s. |
| `--dyna-only` | With `--dyna`: only scan the wall pairs that have a dynapoly wall in them (pusher or clipped wall), and skip maps without dynapolys. Much faster; the static-only pairs are what a scan without `--dyna` finds, give or take the dynapolys' effect on them. |
| `--setup N` | With `--dyna`: only the dynapolys of setup N, for every form. Without it, OoT pairs each form with the setups it plays in (see **OoT forms and setups** below). |

### Speed and angle analysis

| Option | Meaning |
|---|---|
| `--min-speed` | For every clip, the lowest speed that does it: starts in 32 directions every 1 unit up to 45 away (`--max-move`), each where Link comes to rest there, with the real frame run. Written to each clip as `"reach": {speed, yaw, start}` (`null` if none). The terminal lists the lowest per wall pair. The viewer's "Reachable only" filter uses these values directly. |
| `--refine` | With `--pair`: the exact lowest speed for that clip (of the kind `--clip-kind` picks: walking, falling, slope or ground). It searches every standable in-bounds start within 24 of the best coarse one (0.25 grid), every yaw toward the clip points (then single steps), and speeds every 0.02, bisected down to the exact f32 boundary. A speed only counts if the clip also works at +0.0025 … +0.01, which rules out single-value flukes (like posNext landing exactly on a wall's plane). The JSON's `clips` then holds **only the refined clip** (its `prev` is the start, its `yaw` and `speed` the move), so the tester runs exactly that move. If the refine finds nothing, the ordinary clips are written. Implies `--min-speed`. |
| `--angles --max-speed S` | With `--pair`: every yaw that does that clip at speed S or less, each from its own start, as `--yaw` finds them (and the same table: a start per yaw, without the CSVs). It starts from the refined yaw (`--refine`'s lowest speed, about 1.5 s) and the scan's clip moves already at speed S or less, and walks out from each that clips, `0x10` at a time both ways, until 16 yaws in a row don't clip (`--angle-gap N` for another number; bigger is slower, about 2N failing yaws for the two ends of each run): the yaws that work come in runs, sometimes with a gap (Treasure Chest Shop Human 50 → 90 at 11: `0xFF40`–`0xFFEF`, then `0x0010` on). Prints the runs on one line (`Yaws that clip at speed up to S: ...`). A run more than that many yaws away from the others isn't found; `--yaw FROM-TO` covers a range for sure. About 4 s a yaw at speed 10 (no CSV grid to make): Treasure Chest Shop Human 50 → 90 at 10, 106 yaws in 7 minutes, clipping at `0xFE30`–`0xFFEF`, `0x0010`–`0x007F`, `0x00A0`–`0x017F` and a few single yaws up to `0x03CF`. |
| `--from X,Y,Z[,SPEED]` | With `--pair`: after refining, try all 4096 directions from this one start (feet position): the yaws that clip at SPEED (the refined speed if not given), the yaws that clip at any speed up to `--max-move` / 1.5, and each direction's lowest speed. The game's sine table ignores the yaw's low 4 bits, so e.g. `0xFFC0`–`0xFFCF` move Link the same way. Only that start: a clip that needs Link pressed against a wall to a few thousandths works at other yaws from other starts (`--angles` finds those). Warns if Link wouldn't stand still there or if it's out of bounds. Implies `--refine`. |
| `--yaw YAW --max-speed S` | With `--pair`: the lowest speed (of the kind `--clip-kind` picks) up to S that does that clip moving at exactly YAW (`0x1234` or decimal), from any standable in-bounds start. `--yaw FROM-TO` (e.g. `0xFF80-0x0040`, going up through `0xFFFF` → `0` when TO is below FROM) does every yaw from FROM to TO in steps of `0x10` (the low 4 bits don't change the move), prints each yaw's answer as it goes (with the slowest start's exact position), then a table on stdout, one row per yaw: its minimum speed and a start that clips at it, as exact f32s to set Link at (or `none`). Every start is tried, not just until a slower one turns up. Starts within 0.75 of each other are grouped into regions; a yaw with several separate regions gets a `region` row for each under its own, with that region's lowest speed and its start. Each yaw that clips also gets a CSV next to the JSON, `<output>_<YAW>.csv` (e.g. `tcs.json` → `tcs_FFC0.csv`; with several forms the form is in the name too): a grid of round x values (columns) and z values (rows), about 20 × 40, stepped 1, 2 or 5 × a power of ten, and each cell `Yes` if Link standing exactly there (the nearest f32 to that number) clips at some speed up to S, else `No`. Each cell is tested at its own coordinates, so the grid shows the shape of where the clip works. It starts over the starts found and grows until its edge rows and columns are all `No`, so it covers the whole shape even without `--exact`. (The starts that work are usually thin strips, e.g. along the edge of a wall Link is pressed against: the CSV shows their shape.) The JSON then holds one clip per yaw that works. Starts are every resting spot behind the scan's clip points of the pair along YAW (up to S × 1.5 back and 3 either side, every 0.002 across YAW (`--side-step D` to change it: smaller finds more positions but takes longer, e.g. 0.0005 about 4x) and 0.5 along it: a start pressed against a wall can have to be right to a few thousandths); speeds as for `--refine` (every 0.02, bisected, robust to +0.01). Prints the speed and start, or that none works. The JSON's `clips` holds only those clips, or nothing if none works at those yaws (unlike `--refine`, the scan's clips are never written instead). Slower at high S (Treasure Chest Shop at 30: 35–90 s; at 10: about 5 s). Can't be combined with `--min-speed` / `--refine` / `--angles` / `--from`. |
| `--exact` | With `--yaw`: after the sampled search, try **every f32 x and z** around each region it found: the region's box, one side-step bigger each way, growing until nothing that works is within a side-step of its edge (only the new strip is tried each time). A point counts if Link stands still there (his resting spot is that exact point), it's in bounds, and it clips at some speed up to S. The table and the minimum speeds then come from these (the CSVs test their own grid either way). It only fills in around what the sampling found: a separate spot narrower than `--side-step` that no sampled start landed in is still missed. Treasure Chest Shop Deku 50 → 90 at 0xFFD0, speed 9.9: 12 sampled positions became 28,238 (min speed 9.8214 → 9.8047), about 12 s. A region that would grow past 50 million points is left as sampled. |
| `--speed S` | With `--yaw` or `--angles`: also find, per yaw, a start that clips at **exactly** speed S (printed under the yaw's row, `at exactly S: start ...`; the JSON's clip for the yaw is that move, and a yaw without one gets no clip; with `--angles` the runs line lists the yaws that clip at exactly S). Each start the search found (lowest speed v <= S) is tried as it is, then moved (S - v) x 1.5 back along the yaw so posNext lands where v put it, and a few steps of 0.00005 either way; it has to be where Link rests, in bounds, and clip at exactly S. E.g. Treasure Chest Shop Human 50 → 90 at 9.94054: `0x0120` from (-240.411819, 0, 824.492676), `0x0130` from (-240.434586, 0, 824.502808). With `--yaw` it also makes the CSV grids at exactly speed S: a cell is `Yes` if Link standing exactly there clips moving at the yaw at speed S, and its `_speeds.csv` is S everywhere, so the tester tries every cell at S. Stands in for `--max-speed` if that's not given (the search for starts goes up to S). |
| `--clip-kind walking\|falling\|slope\|ground` | Which of the pair's clips `--refine`, `--yaw`, `--angles`, `--from` and `--speed` work on, and so which frame they run from each start: **walking** wall push (posNext 7.5 below the floor), **falling** wall push (posNext `--drop` below it), a **slope** clip (the frame, then the slowest second frame that works: `speed2`; the speed found is the first frame's) or a **ground** clip (at the pair's `vy`, -20). Default: the first of walking, slope, ground and falling the pair has; the terminal says which, and when the pair has other kinds too (a sloped floor's crossing clips and a ground clip can share a (floor, wall) pair). The clips written are of that kind, `drop` / `speed2` / `vy` included, so the tester runs them as such. |
| `--drop D` | Falling clips: posNext D below the start (y velocity -D / 1.5). This is the fall from where Link starts, not the scan's `drop`, which is measured from the floor under the clip point; down a slope he falls up to about 10 more than that. It has to be at most checkHeight - 5 (21 for Link in OoT); a bigger fall runs the line test at his feet, which isn't a wall push. Default: `--refine` uses the fall of the pair's slowest move that works (the lowest speed is the point), and `--yaw` / `--angles` the smallest valid fall of the pair's clips, or `--refine`'s when `--angles` refines first. If none of the pair's falling clips has a valid fall, nothing is refined and the terminal says so. Falling refines can be slow: OoT Kakariko Village, child, TRI 142 -> 673 ran past 25 minutes. |
| `--max-move N` | How far Link can move in one frame, in units. Default 45 (speed 30), which glitches can beat (55+). Crossing points are tried from starts up to 32 back by default, and every 4 past that out to N when N is over 45. `--min-speed`, `--refine` and `--from` look for starts up to N away (speed N / 1.5). Written to the JSON as `"maxMove"` when it isn't 45; the viewer's max move box picks it up on import. Scans take longer the further out it goes. |

### Debugging one frame

| Option | Meaning |
|---|---|
| `--sim X,Y,Z,YAW,SPEED[,DROP]` | Run one frame and print each step (a slope clip is reported too). DROP can be given as a y velocity instead, `vVY`: `v-20` is a drop of 30. A drop of more than checkHeight - 5 runs the ground clip frame (see **Ground clips**): the start floor's line test plane distance, the line test at the feet, the pushes, the floor check and the verdict. SPEED as `15/7`: one frame of walking per speed at the same yaw, each ending with the floor check, then two frames standing still, printing where each one leaves Link and which wall he's behind; for slope clips. Link stands at (X, Y, Z) (feet) and moves at YAW (`0x1234` or decimal) with speedXZ SPEED. posNext is 7.5 below his feet (walking), or DROP below if given (falling). Prints: whether the start is in bounds and a resting spot, the line test and what it hits, every wall push, where he ends up, and whether that's a clip and out of bounds. Use it when a clip works in game but the scan disagrees, or the other way round. Nothing is written. |

### Output and running

| Option | Meaning |
|---|---|
| `-o FILE`, `--out FILE` | Write the JSON here (single map). |
| `--out-dir DIR` | Write each map's JSON into DIR as `<GAME>_<map>_<form>[_falling][_extended][_first][_setup<N>_dyna][_pair<P>-<C>].json` (`_first`: `--first-per-pair`; `--pair` and `--first-per-pair` get their own files, so they don't overwrite the whole map's full scan). Used with `--all`, or with `--map` instead of `-o`. The directory must exist. A path clipfinder can't write stops the run straight away. Note that clipfinder is a Windows program: from WSL, `/tools/...` means `C:\tools\...`, so use relative paths like `tools/clipfinder/results`. |
| `--root DIR` | The viewer's folder (with `models/` and `js/model_list.js`). Default: the current folder if it has `js/model_list.js`, else two levels up from the exe. |
| `--threads N` | Worker threads. Default: all cores. |

## Output format

```jsonc
{
  "format": "wall-push-clips-2",
  "game": "MM", "map": "Treasure Chest Shop", "falling": false, "extendedOnly": false, "numPolygons": 97,  // , "maxMove": N with --max-move, "setups": [...] with --dyna
  "forms": [
    {"form": "Human", "radius": 14, "checkHeight": 26.8000011}
  ],
  "clips": [
    {"form": "Human",
     "kind": "acute" | "extended" | "slope" | "ground", // the wall pair's category (below), the same for all its points
                                           // (files from before 2026-09-26: per point, and "low" for falling ones)
     "cross": true,                        // crossing (moving through the pusher's plane) vs standing point
     "drop": 0,                            // falling: how far below the floor posNext is (0 = walking)
     "pusher": 50, "crossed": 90,          // TRI ids: the wall that pushes, the wall Link ends up behind
     "from": [x, y, z],                    // the clip point (posNext, or where the line test hits)
     "prev": [x, y, z],                    // where Link stands before the frame
     "next": [x, y, z],                    // posNext after the move
     "res": [x, y, z],                     // after this frame's pushes
     "end": [x, y, z],                     // after 2 more frames (out of bounds)
     "floorY": 0,
     "yaw": 65473, "speed": 9.8252573,     // the move from prev to next (s16 yaw, f32 speedXZ)
     "speed2": 4,                          // slope clips: the next frame's speed (same yaw), if it needs one
     "vy": -20,                            // ground clips: velocity.y for the frame (next.y = prev.y + vy x 1.5)
     "yaws": [...],                        // crossings: the yaws that worked, of the 32 start directions tried for this point
     "reach": {"speed": ..., "yaw": ..., "start": [...]}  // --min-speed
    }
  ]
}
```

All numbers are the exact f32 values, printed so they read back unchanged.
Older single-form files (`wall-push-clips-1`) still import into the viewer
and the tester.

## Using the results

- **Viewer:** load the map. With **Auto-import** on (the default), the
  viewer imports every results file for it from `tools/clipfinder/results`
  (or the folder in the box): the files `--out-dir` named for the map
  (`<GAME>_<map>_...json`) whose `map` and poly count match, that are static
  scans or scanned with the loaded setup's dynapolys (`setups`). They're
  merged: a marker row per form and kind, each point once. So the usual
  setup is one `--all` scan and one `--all --dyna <GAME>_dyna_all.json
  --dyna-only` scan into that folder. Dynapoly scans of other setups, and ones
  made from an export without `setups` (before 2026-09-27), are left out.
  This lists the folder through the server's directory pages, which
  `python -m http.server` has. **Import results** loads one file by hand.
  Clicking a point describes it.
- **In game:** set `TESTS_FILE` in `tools/clipfinder/wall_clip_tester.lua` to the JSON.
  It reads the walls from RAM, runs the tests for the form Link is in, and
  writes `wall_clip_results.txt`. See the settings at the top of that script
  (`SKIP_FALLING`, `MAX_PER_GROUP`, `FORM`, …).
- **Checking a `--yaw` run's CSVs in game:** each `<output>_<YAW>.csv` comes
  with `<output>_<YAW>_speeds.csv`, the same grid with the speed to try each
  cell at (a Yes cell's lowest speed that clips, a No cell's max speed). Set
  `TESTS_FILE` to the run's JSON and `CSV_TESTS = true`: every cell is tried
  in "move" mode (Link moved by the game from exactly that x, z at the yaw),
  and each grid's result is written to `<output>_<YAW>_ingame.csv`, with
  `No (expected Yes)` / `Yes (expected No)` where the game disagrees. The
  summary counts the cells that match and lists the rest. `CSV_CELLS =
  "border"` tries only the cells next to one with the other answer (about a
  quarter of them). Turn `RECORD` off for this: it adds 3 s a cell.

## Dynapolys

Without `--dyna` the scan sees only the scene's static collision. With it, the
dynapoly actors the viewer had loaded join in, the way `z_bgcheck.c` handles
them:

- **Every map at once.** **Export all dynapolys** in the wall clip panel
  writes `<GAME>_dyna_all.json` (format `dynapoly-set-1`): for every map,
  every setup, the dynapoly actors that setup spawns, built the same way as
  the Actors rows but without drawing anything (a few seconds). Setups with
  identical dynapolys share one entry (`setups: [0, 2]`), and so one scan;
  maps without any are left out. Every actor is in, in its default state.
- **Order.** Each map's actors are written in the order they take bg actor
  slots, which is the order the game checks them in and can decide a clip:
  spawn order,
  except that the actors that register their collision from Update once
  their own object has loaded (Bg_Spot01_Objects2, Door_Shutter, ...:
  `LATE_BG_ACTORS` in `js/render_actors.js`) come after all the others. (In
  Kakariko setup 2 a crate pushes Link through the shooting gallery's wall
  only because the gallery's walls are checked after the crate's.) Each actor is its tangible polys in world
  space as `DynaPoly_ExpandSRT` builds them (s16 vertices, normals and plane
  distances recomputed from those), plus the bounding sphere and Y range the
  game culls it with. The viewer draws each actor in its default state (switch
  flags unset and so on), and that state is the one exported.
- **Wall pushes** (`BgCheck_CheckWallImpl`): the dynapoly walls push *before*
  the static ones: every bg actor whose Y range and bounding sphere (grown by
  the radius) take Link, all its walls' Z pushes, then all their X pushes. The
  lists are in reverse poly order, unsorted, with no early out. Then the
  static walls push, in the subdivision of where the dynapolys left him.
- **The dynapoly line check.** After a dynapoly collision (a dynapoly push, or
  the frame's line test stopping him on a dynapoly with no static push after
  it), the game checks the line from posPrev to the result against the
  **static** walls, one face only, and puts Link the radius in front of the
  first one crossed. That stops most "a dynapoly pushes Link through a static
  wall" clips, but not where the line misses the wall: it runs at Link's feet,
  and walking it ends 7.5 below the floor, under a wall whose bottom is at
  floor level. The other way round, a static wall pushing him through a
  dynapoly, isn't checked at all.
- **Line tests and floors.** The frame's line test and the floor check include
  the dynapolys (`BgCheck_CheckLineAgainstDyna`, `BgCheck_RaycastFloorDyna`:
  dynapoly floors have the `detMax 300` tolerance, and their walls count as
  floors only when nothing else was found). Standable spots, in bounds and
  "behind a wall" all see them too.
- **What counts.** A clip through a static wall still has to leave Link out
  of bounds. A clip through a **dynapoly** wall counts wherever he ends up,
  as long as he's still behind it two frames later, and passing clean through
  a thin one is allowed: getting past a gate, a fence or a door is the point,
  and that usually lands in bounds. This covers dynapolys pushed through by
  other dynapolys (the same actor's or another's), and static walls pushing
  Link into a dynapoly, like a crate against a wall.
- **Poly ids.** Dynapolys get the ids after the scene's own, in the file's
  order (`numPolygons` on). The results carry the export as `"dyna"`, so the
  viewer rebuilds the same ids on import, and labels them, e.g.
  `TRI 971 (Obj_Kibako2 dynapoly 4)`. `--sim` does the same.

Things the export can't know: actors that move (they're exported where they
spawn), ones that only appear later, and which dynapoly actors are loaded
together in-game. The export has each actor its spawn list has for the chosen
setup.

## OoT forms and setups

An OoT scene's setups (layers) 0 and 1 are child day and night, 2 and 3 adult
day and night, 4 on cutscenes. A scene without one of them loads another
(`Scene_CommandAlternateHeaderList`): adult night falls back to adult day,
anything else to setup 0. Most scenes only have setup 0, which both ages use.

With `--dyna` (and no `--setup`), each form is scanned only with the
dynapolys of the setups it plays in: Child and Crawlspace setups 0 / 1, Adult
2 / 3, each resolved that way from the setups the scene has (the viewer's
setup list, `models/OOT/actors/OOT_actors_by_scene.json`). The terminal prints
the pairing, e.g. Death Mountain Trail (setups 0, 2, 4-8): `Adult setup 2,
Child setup 0, Crawlspace setup 0`. A form whose setups have no dynapolys is
scanned without them (skipped with `--dyna-only`); cutscene setups are only
scanned with `--setup`, which takes every form.

The viewer does the same on auto-import: loading setup 0-3 of an OoT map
shows only the forms that play in it (the status says which were left out);
a cutscene setup shows every form.

## Holding the stick, and floor snaps

Two kinds of clip beyond one push from a standing start:

- **Hold clips** (`"hold": true`). After the clip frame the scan normally has
  Link stand still for two frames; a wall he's less than 4 behind pushes him
  back out, so that isn't a clip. If he keeps holding the stick instead, the
  next frame's move (the same yaw and speed) can take him further behind it
  before its check runs, and then it can't. When standing still fails, the
  scan tries that one extra frame, then two standing still, and marks the
  point `hold`. The viewer says so when you click it. `wall_clip_tester.lua`
  doesn't hold the stick yet, so it won't reproduce these. (OoT Ice Cavern:
  the rock TRI 721 puts Link 1.5 behind the red ice, and holding on takes him
  through.)
- **Floor snaps.** Moving more than his radius in a frame, the game's line
  test includes floors, and it puts Link the radius out along a sloped
  floor's horizontal direction just as for a wall. That can put him behind
  the wall the slope runs up to, so sloped floors are pushers too, for
  crossings only (floors never push in the wall check). (OoT Bottom of the
  Well: the slopes TRI 863 and 860 through the walls TRI 876 / 884 / 885.)
  The line only meets a slope where it has risen checkHeight - 7.5 above
  Link's floor, often most of a frame's move away, so for slope pushers the
  scan looks for his floor and starts out to the full `--max-move`. Many
  need speeds near or over 30: OoT Shadow Temple TRI 1182 through TRI 1160
  gives 17 points at the default, 233 with `--max-move 55`.

Not modelled: a running start. Every frame starts where Link stands still. A
clip whose frame has to start where only a previous frame's move could have
put him (e.g. still sliding along a wall that would push him away if he
stopped) isn't found. The Ice Cavern red ice clip is one of these.

## Slope clips

`"kind": "slope"`. Walking, posNext is 7.5 below the floor Link starts on,
wherever the move takes him, so the frame's wall check and line test run at
checkHeight - 7.5 above his *start*. Walking up a steep slope into a wall
whose bottom edge is higher than that, nothing stops him: he moves past the
wall's plane, and the floor check (from prevPos.y + 50) puts him on the
higher floor there: the slope's own 1 unit tolerance past its edge, or a
floor behind the wall. Now he's behind the wall at his check height.

He doesn't even have to get past the wall's bottom edge when the wall leans
out over the slope (an overhang, its normal pointing down): at his new check
height its plane is further out than at its bottom, so the slope lifts him
behind it while he's still on the slope. OoT Death Mountain Trail, adult:
stand at (-112.4565, 1273.895, -1522.771), yaw 0x4100, speed 12, then 4. The
51 degree slope TRI 675 runs up to the overhang TRI 642.

The next frame the wall pushes him back out if he's at most 4 behind it
(`wallPush`). So standing still after that frame doesn't always work, and a
second frame's move at the same yaw (`speed2`, the slowest of 1, 2, 3, ...
that works) takes him further behind first. OoT Inside Jabu-Jabu's Belly,
adult: stand at (-722, -338.6496, -4797.859), yaw 0xA617, speed 15, then
speed 7 (4 is the slowest). The slope TRI 2632 runs up to TRI 2630, whose
bottom is at y -320: the wall check runs at -320.15. Where there's a floor
behind the wall more than 4 back, one frame does it on its own (OoT Hyrule
Field: TRI 762 behind TRI 758).

A clip's `pusher` is the floor that lifts Link and `crossed` the wall. The
frame's own pushes and line test mustn't put him behind any wall (then it's
an ordinary wall push clip). The scan goes along every wall's bottom edge,
split into stretches with the same floors behind the wall: every unit along
a stretch up to 60 long, every 2 up to 120, every 3 past that (at most
`--slope-step`; OoT Hyrule Field, child: 12 pairs in 6 s, `--slope-step 1`
16 s; Death Mountain Trail, adult: 5 of 6 pairs in 1.7 s, 3.7 s). At each point it needs a floor that would put Link's check
height on the wall, behind its plane, and a lower floor in front. It aims
moves from standing starts in front to land from 24 in front of the bottom
edge to 24 past it.

**In bounds.** Link is out of bounds if he's behind a wall, or if one of 8
level rays at his check height meets the back of a wall first. For where he
starts or stands (every clip kind, and the viewer's reachability), a ray that
goes into a floor first doesn't count: up a slope the rays pass under the
ground, and under the walls on it, to the back of some wall far off. (The
Death Mountain Trail start above was out of bounds without this, by a wall
240 away.) Whether a clip *ends* out of bounds still uses every ray. One
check keeps every ray unless `--slope-starts`: the crossing search's quick
"is anywhere around this point in bounds" test. Its sample spots are at a
nearby floor's height, often in the air or the ground, and ignoring the rays
into slopes there lets through far more points than it finds clips for.
`reach` is the move itself (the faster of the two speeds); `--min-speed`
leaves it as it is.

Not modelled: Link's own slope handling (sliding down a slope too steep to
stand on, or slowing on one). `wall_clip_tester.lua` runs slope clips in
"move" mode, writing `speed2` just after the first frame, and judges him
after the second.

## Ground clips

`"kind": "ground"`. Falling fast, the game's wall check changes its line
test: when checkHeight + dy < 5 (dy is posNext.y - prevPos.y, so a y
velocity below (5 - checkHeight) / 1.5, -14 for Link in OoT) it tests the
line from prevPos to posNext themselves, Link's feet, with floors, instead
of at his check height. If he starts the frame on the floor, that line
starts on the floor's plane, and whether the floor counts as crossed comes
down to the f32 rounding of its plane distance there (`planeDistA` in
`CollisionPoly_LineVsPoly`): just under 0 and the line goes on into the
ground. (The viewer's yellow "ground-clippable" bands on standable surfaces
are where it does.) Then it passes under the bottom of a wall rising out of
that floor, and the wall push, at posNext.y + checkHeight, is under the
wall's bottom too. The floor check (from prevPos.y + 50) finds whatever is
behind the wall, or nothing, and he falls out of bounds.

OoT Kakariko Village, child: on the slope TRI 491 at (435.5778, 35.55124,
626), pressed against TRI 507, y velocity -20, yaw 0x0000, speed 18. The
line test's plane distance at the start is -0.0000153, and there's no
floor behind TRI 507:

```bash
tools/clipfinder/clipfinder.exe --game OOT --map "Spot 01 - Kakariko Village" --form Child --sim "435.5778,35.55124,626,0,18,v-20"
```

The scan goes along every wall's bottom edge where a floor meets it, split
into stretches with the same floors in front: every unit along a stretch up
to 60 long, every 2 up to 120, every 3 past that (at most `--ground-step`).
At each point it keeps one clip per floor Link starts on (a pair's floor,
which can be a small one further back). OoT Hyrule Field, child: 384
(floor, wall) pairs in about 20 s; `--ground-step 1`, 394 in about 55 s.
It tries starts on that floor (resting spots) moving at the wall
from pressed against it to 24 further back, aimed to end 1 to 24 past it,
at y velocity -20 (the fastest, `minVelocityY`: it goes deepest, soonest).
A clip's `pusher` is the floor Link starts on, `crossed` the wall he goes
under, `vy` the y velocity; `reach` is the move itself. It must end out of
bounds (or past a dynapoly wall), and a wall push or line snap putting him
through the wall is an ordinary wall push clip, not this.

The y velocity is the hard part: Link standing on the floor has -4. The
tester writes `vy` (before gravity) as for falling clips, and runs ground
clips in "move" mode. Not modelled: the ceiling check (from prevPos.y + 10,
up to ceilingCheckHeight + dy - 10, which is small at this dy).

## Acute or extended

Every wall pair (pushing wall, clipped wall) gets one category, per form, and
all its points carry it (`kind`), falling ones included:

- **acute** if at least one of its points is acute on its own: it still clips
  with the extended planes removed (no 1-unit / `detMax 300` tolerance), *and*
  the push that does it starts with Link in front of the pusher's actual face
  (his check point projected along the wall's normal lands on the triangle,
  0.1 of slack for rounding).
- **extended** otherwise: every point needs the pusher's extended plane.

The face test is there because the game's wall check projects Link onto a
wall along the Z or X axis, not the wall's normal, so a diagonal wall also
pushes Link standing beside it, past its end (at 45 degrees as far past as he
is in front of its plane), with no tolerance at all. `--sim` prints both
triangles and the verdict for that one frame.

The viewer shows walking and falling clips of each category as separate rows.
Older result files are made per pair when imported: a pair with any acute
point is acute.

## Keeping it in sync

The search lives only here. `js/wall_push_clips.js` still has the collision
model, `clipFromFrame`, `standSpot`, `landing` and `reachability`, for the
viewer's "Reachable only" on files without `--min-speed` data, dynapolys
included (`src/collision.cpp` / `CollisionModel`). A change to those belongs
in both. One difference: `--min-speed` also runs the frame for
standing points, which the viewer's "Reachable only" doesn't.

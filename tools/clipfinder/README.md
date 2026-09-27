# clipfinder

The wall push clip scan for OoT and MM, native and multithreaded. (It began as
a port of an in-browser scan in the viewer, since removed as too slow.) It
reads a scene from `models/`, builds the same collision model the viewer does,
and runs the search in the game's f32 arithmetic. It writes the clip points as
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
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --pair 50,90 --angles --out-dir tools/clipfinder/results

# That clip at one yaw, at speed 15 or less
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --pair 50,90 --yaw 0xF000 --max-speed 15 -o tools/clipfinder/results/tcs_50_90_f000.json

# A start for each yaw from 0xFF80 to 0x0000, at speed 10.5 or less
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Deku --pair 50,90 --yaw 0xFF80-0x0000 --max-speed 10.5 -o tools/clipfinder/results/tcs_50_90_range.json

# One frame, step by step
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --sim "-239.859,0,824.246,0xFF9D,11" --out-dir tools/clipfinder/results

# With the map's dynapoly actors (exported from the viewer), only the wall pairs with a dynapoly wall in them
tools/clipfinder/clipfinder.exe --game OOT --map "Spot 01 - Kakariko Village" --form All --falling --dyna OOT_Spot_01_-_Kakariko_Village_dyna.json --dyna-only -o tools/clipfinder/results/kak_dyna.json
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
| `--falling` | Also look for clips while falling ("low" clips, `drop` > 0). Link's post-move position (posNext) is 2–30 below the floor, so the wall check runs lower than when walking. Slower. Falling crossings are only generated for drops where `checkHeight + dy >= 5`. Beyond that, the game's line test runs from Link's feet with floors included and stops him on his own floor (MM Human: drop over 21.8, OoT: over 21, Crawlspace: over 10). |
| `--extended-only` | Keep only the wall pairs (pusher, clipped wall) that clip **only** thanks to the walls' extended planes: the 1-unit / `detMax 300` tolerance of the game's triangle checks, or the pusher reaching past its own edge. (The game's wall check projects Link onto a wall along the Z or X axis, not the wall's normal, so a diagonal wall also pushes Link standing beside it, past its end: at 45 degrees as far past as he is in front of its plane.) See **Acute or extended** below for how a wall pair is categorised; this leaves out every acute pair, all its points included. The terminal says how many pairs and points were left out. With `--first-per-pair`, a pair whose first point found was extended isn't checked for acute points afterwards. |
| `--first-per-pair` | Keep the first clip found for each wall pair (pushing wall, clipped wall), like the tester's one-per-pair recording mode. The output is much smaller, but the scan isn't much faster: most of the time goes on points that never clip. |
| `--pair P,C` | Keep only the clips where TRI P pushes Link through TRI C (polygon ids, as the viewer and tester show them). Needed for `--refine` / `--angles`. |
| `--dyna FILE` | Add the map's dynapoly actors, from the viewer's **Export dynapolys** (see **Dynapolys** below). One map's export: use `--map`, not `--all`. Output files named by `--out-dir` get `_dyna` added. |
| `--dyna-only` | With `--dyna`: only scan the wall pairs that have a dynapoly wall in them (pusher or clipped wall). Much faster; the static-only pairs are what a scan without `--dyna` finds, give or take the dynapolys' effect on them. |

### Speed and angle analysis

| Option | Meaning |
|---|---|
| `--min-speed` | For every clip, the lowest speed that does it: starts in 32 directions every 1 unit up to 45 away (`--max-move`), each where Link comes to rest there, with the real frame run. Written to each clip as `"reach": {speed, yaw, start}` (`null` if none). The terminal lists the lowest per wall pair. The viewer's "Reachable only" filter uses these values directly. |
| `--refine` | With `--pair`: the exact lowest **walking** speed for that clip. It searches every standable in-bounds start within 24 of the best coarse one (0.25 grid), every yaw toward the clip points (then single steps), and speeds every 0.02, bisected down to the exact f32 boundary. A speed only counts if the clip also works at +0.0025 … +0.01, which rules out single-value flukes (like posNext landing exactly on a wall's plane). The JSON's `clips` then holds **only the refined clip** (its `prev` is the start, its `yaw` and `speed` the move), so the tester runs exactly that move. If the refine finds nothing, the ordinary clips are written. Implies `--min-speed`. |
| `--angles` | With `--pair`: after refining, try all 4096 directions from the refined start. The game's sine table ignores the yaw's low 4 bits, so e.g. `0xFFC0`–`0xFFCF` move Link the same way. Prints the yaws that clip at the refined speed, the yaws that clip at any speed up to 30 (`--max-move` / 1.5), and each direction's lowest speed. Implies `--refine`. |
| `--from X,Y,Z[,SPEED]` | With `--pair`: `--angles` from this start (feet position) instead of the refined one, and at SPEED if given. Warns if Link wouldn't stand still there or if it's out of bounds. |
| `--yaw YAW --max-speed S` | With `--pair`: the lowest **walking** speed up to S that does that clip moving at exactly YAW (`0x1234` or decimal), from any standable in-bounds start. `--yaw FROM-TO` (e.g. `0xFF80-0x0040`, going up through `0xFFFF` → `0` when TO is below FROM) does every yaw from FROM to TO in steps of `0x10` (the low 4 bits don't change the move), prints each yaw's answer as it goes (with the slowest start's exact position), then a table on stdout, one row per yaw: its minimum speed and the x and z range of the starts that clip at some speed up to S (or `none`). Every start is tried, not just until a slower one turns up. Starts within 0.75 of each other are grouped into regions; a yaw with several separate regions gets a `region` row for each under its own. Each yaw that clips also gets a CSV next to the JSON, `<output>_<YAW>.csv` (e.g. `tcs.json` → `tcs_FFC0.csv`; with several forms the form is in the name too): a grid of round x values (columns) and z values (rows), about 20 × 40, stepped 1, 2 or 5 × a power of ten, and each cell `Yes` if Link standing exactly there (the nearest f32 to that number) clips at some speed up to S, else `No`. Each cell is tested at its own coordinates, so the grid shows the shape of where the clip works. It starts over the starts found and grows until its edge rows and columns are all `No`, so it covers the whole shape even without `--exact`. The ranges are the bounding box of the sampled starts that work, so they're only as fine as the sampling, and not every point inside a box works: the regions are usually thin strips, e.g. along the edge of a wall Link is pressed against. The JSON then holds one clip per yaw that works. Starts are every resting spot behind the scan's clip points of the pair along YAW (up to S × 1.5 back and 3 either side, every 0.002 across YAW (`--side-step D` to change it: smaller finds more positions but takes longer, e.g. 0.0005 about 4x) and 0.5 along it: a start pressed against a wall can have to be right to a few thousandths); speeds as for `--refine` (every 0.02, bisected, robust to +0.01). Prints the speed and start, or that none works. The JSON's `clips` holds only those clips, or nothing if none works at those yaws (unlike `--refine`, the scan's clips are never written instead). Slower at high S (Treasure Chest Shop at 30: 35–90 s; at 10: about 5 s). Can't be combined with `--min-speed` / `--refine` / `--angles`. |
| `--exact` | With `--yaw`: after the sampled search, try **every f32 x and z** around each region it found: the region's box, one side-step bigger each way, growing until nothing that works is within a side-step of its edge (only the new strip is tried each time). A point counts if Link stands still there (his resting spot is that exact point), it's in bounds, and it clips at some speed up to S. The table and the minimum speeds then come from these (the CSVs test their own grid either way). It only fills in around what the sampling found: a separate spot narrower than `--side-step` that no sampled start landed in is still missed. Treasure Chest Shop Deku 50 → 90 at 0xFFD0, speed 9.9: 12 sampled positions became 28,238 (min speed 9.8214 → 9.8047), about 12 s. A region that would grow past 50 million points is left as sampled. |
| `--speed S` | With `--yaw`: make the CSV grids at **exactly** speed S: a cell is `Yes` if Link standing exactly there clips moving at the yaw at speed S, and its `_speeds.csv` is S everywhere, so the tester tries every cell at S. Stands in for `--max-speed` if that's not given (the search for starts goes up to S). |
| `--max-move N` | How far Link can move in one frame, in units. Default 45 (speed 30), which glitches can beat (55+). Crossing points are tried from starts up to 32 back by default, and every 4 past that out to N when N is over 45. `--min-speed`, `--refine` and `--angles` look for starts up to N away (speed N / 1.5). Written to the JSON as `"maxMove"` when it isn't 45; the viewer's max move box picks it up on import. Scans take longer the further out it goes. |

### Debugging one frame

| Option | Meaning |
|---|---|
| `--sim X,Y,Z,YAW,SPEED[,DROP]` | Run one frame and print each step. Link stands at (X, Y, Z) (feet) and moves at YAW (`0x1234` or decimal) with speedXZ SPEED. posNext is 7.5 below his feet (walking), or DROP below if given (falling). Prints: whether the start is in bounds and a resting spot, the line test and what it hits, every wall push, where he ends up, and whether that's a clip and out of bounds. Use it when a clip works in game but the scan disagrees, or the other way round. Nothing is written. |

### Output and running

| Option | Meaning |
|---|---|
| `-o FILE`, `--out FILE` | Write the JSON here (single map). |
| `--out-dir DIR` | Write each map's JSON into DIR as `<GAME>_<map>_<form>[_falling][_extended].json`. Used with `--all`, or with `--map` instead of `-o`. The directory must exist. A path clipfinder can't write stops the run straight away. Note that clipfinder is a Windows program: from WSL, `/tools/...` means `C:\tools\...`, so use relative paths like `tools/clipfinder/results`. |
| `--root DIR` | The viewer's folder (with `models/` and `js/model_list.js`). Default: the current folder if it has `js/model_list.js`, else two levels up from the exe. |
| `--threads N` | Worker threads. Default: all cores. |

## Output format

```jsonc
{
  "format": "wall-push-clips-2",
  "game": "MM", "map": "Treasure Chest Shop", "falling": false, "extendedOnly": false, "numPolygons": 97,  // , "maxMove": N with --max-move
  "forms": [
    {"form": "Human", "radius": 14, "checkHeight": 26.8000011}
  ],
  "clips": [
    {"form": "Human",
     "kind": "acute" | "extended",         // the wall pair's category (below), the same for all its points
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

- **Viewer:** load the map, then **Import results** in the wall clip panel.
  With several forms there's a marker row per form and kind. Clicking a
  point describes it.
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

- **Getting the file.** Load the map in the viewer (with **Render Actors** on),
  then **Export dynapolys** in the wall clip panel. It writes each dynapoly
  actor under the **Actors** rows in the order they take bg actor slots, which
  is the order the game checks them in and can decide a clip: spawn order,
  except that the actors that register their collision from Update once
  their own object has loaded (Bg_Spot01_Objects2, Door_Shutter, ...:
  `LATE_BG_ACTORS` in `js/render_actors.js`) come after all the others. (In
  Kakariko setup 2 a crate pushes Link through the shooting gallery's wall
  only because the gallery's walls are checked after the crate's.) Hide an actor's row to
  leave it out, e.g. a door you'll have opened; the **Actor display** menu
  doesn't count, only the rows. Each actor is its tangible polys in world
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

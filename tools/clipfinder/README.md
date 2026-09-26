# clipfinder

A native, multithreaded version of the viewer's wall push clip scan
(`js/wall_push_clips.js`) for OoT and MM. It reads a scene from `models/`,
builds the same collision model the viewer does, and runs the same search in
the same f32 arithmetic. It writes the clip points as JSON, which you can load
with the viewer's **Import results** button or run in game with
`tools/clipfinder/wall_clip_tester.lua`.

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
`clipfinder.exe` is running. Wait for the run to finish first.

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

# One frame, step by step
tools/clipfinder/clipfinder.exe --game MM --map "Treasure Chest Shop" --form Human --sim "-239.859,0,824.246,0xFF9D,11" --out-dir tools/clipfinder/results
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
| `--falling` | Also look for clips while falling ("low" clips). Link's post-move position (posNext) is 2–30 below the floor, so the wall check runs lower than when walking. Slower. Falling crossings are only generated for drops where `checkHeight + dy >= 5`. Beyond that, the game's line test runs from Link's feet with floors included and stops him on his own floor (MM Human: drop over 21.8, OoT: over 21, Crawlspace: over 10). |
| `--extended-only` | Keep only the wall pairs (pusher, clipped wall) that clip **only** thanks to the walls' extended planes (the 1-unit / `detMax 300` tolerance of the game's triangle checks). A pair with even one point that also clips without that tolerance is an acute angle clip, so all its points are left out, extended ones included. Falling points that still clip without the tolerance count as acute too. The terminal says how many pairs and points were left out. With `--first-per-pair`, a pair whose first point found was extended isn't checked for acute points afterwards. |
| `--first-per-pair` | Keep the first clip found for each wall pair (pushing wall, clipped wall), like the tester's one-per-pair recording mode. The output is much smaller, but the scan isn't much faster: most of the time goes on points that never clip. |
| `--pair P,C` | Keep only the clips where TRI P pushes Link through TRI C (polygon ids, as the viewer and tester show them). Needed for `--refine` / `--angles`. |

### Speed and angle analysis

| Option | Meaning |
|---|---|
| `--min-speed` | For every clip, the lowest speed that does it: starts in 32 directions every 1 unit up to 45 away, each where Link comes to rest there, with the real frame run. Written to each clip as `"reach": {speed, yaw, start}` (`null` if none). The terminal lists the lowest per wall pair. The viewer's "Reachable only" filter uses these values directly. |
| `--refine` | With `--pair`: the exact lowest **walking** speed for that clip. It searches every standable in-bounds start within 24 of the best coarse one (0.25 grid), every yaw toward the clip points (then single steps), and speeds every 0.02, bisected down to the exact f32 boundary. A speed only counts if the clip also works at +0.0025 … +0.01, which rules out single-value flukes (like posNext landing exactly on a wall's plane). The JSON's `clips` then holds **only the refined clip** (its `prev` is the start, its `yaw` and `speed` the move), so the tester runs exactly that move. If the refine finds nothing, the ordinary clips are written. Implies `--min-speed`. |
| `--angles` | With `--pair`: after refining, try all 4096 directions from the refined start. The game's sine table ignores the yaw's low 4 bits, so e.g. `0xFFC0`–`0xFFCF` move Link the same way. Prints the yaws that clip at the refined speed, the yaws that clip at any speed up to 30, and each direction's lowest speed. Implies `--refine`. |
| `--from X,Y,Z[,SPEED]` | With `--pair`: `--angles` from this start (feet position) instead of the refined one, and at SPEED if given. Warns if Link wouldn't stand still there or if it's out of bounds. |

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
  "game": "MM", "map": "Treasure Chest Shop", "falling": false, "extendedOnly": false, "numPolygons": 97,
  "forms": [
    {"form": "Human", "radius": 14, "checkHeight": 26.8000011}
  ],
  "clips": [
    {"form": "Human",
     "kind": "acute" | "extended" | "low", // low = falling
     "cross": true,                        // crossing (moving through the pusher's plane) vs standing point
     "drop": 0,                            // falling: how far below the floor posNext is
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

## Keeping it in sync

clipfinder and `js/wall_push_clips.js` should produce the same points for the
same map and form. A change to the search in one belongs in the other too.
Two exceptions:

- `--min-speed` also runs the frame for standing points, which the viewer's
  "Reachable only" doesn't.
- `--refine`, `--angles` and `--sim` exist only here.

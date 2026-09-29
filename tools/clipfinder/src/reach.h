// clipfinder: the lowest speed a clip works at (--min-speed, --refine) and the angles that work (--angles).
#pragma once

#include "search.h"

// --yaw: a connected group of starts that clip (starts within REGION_LINK of
// each other), its x / z range, and its slowest start
// (pts: every start in it and its own lowest speed, in order along it)
struct StartRegion { double x0, x1, z0, z1; int n; double speed; V3 start; vector<std::pair<V3, double>> pts; };
// --yaw: a grid over the starts that clip, for the per-yaw CSV: round x / z
// values (xs, zs, with that many decimals) and whether Link standing exactly
// at each clips at up to maxSpeed (ok[zi * xs.size() + xi]), and the lowest
// speed it does at (speed, 0 where it doesn't)
struct YawGrid { vector<double> xs, zs; int xDecimals = 0, zDecimals = 0; vector<char> ok; vector<double> speed; };
struct Refined { bool found = false; double speed = 0; int yaw = 0; V3 start, end; int starts = 0; vector<StartRegion> regions; YawGrid grid; };

// Which of a wall pair's clips --refine / --yaw / --angles / --from / --speed
// work on (a pair can have several kinds: a sloped floor pusher's crossing
// clips and a ground clip share (floor, wall)), and so which frame they run
// from each start: 0 walking wall push, 1 falling wall push (posNext `drop`
// below the START, i.e. y velocity -drop / 1.5 - not the scan's Clip::drop,
// which is below the floor at the clip point, and on a slope can be ~10
// less), 2 slope clip (slopeFrame), 3 ground clip (groundFrame at
// velocity.y vy). Set once per pair (chooseFrameSpec) before they run.
// autoDrop (falling, no --drop): --refine picks the fall of the pair's slowest
// move that works (and sets drop to it); --yaw / --angles keep `drop`.
struct FrameSpec { int type = 0; double drop = 0; double vy = -20; bool autoDrop = false; };
extern FrameSpec FRAME_SPEC;
static const char* const FRAME_TYPE_NAMES[] = { "walking", "falling", "slope", "ground" };
// Whether clip c is of FRAME_SPEC's kind (falling: any drop)
bool inFrameSpec(const Clip& c);
// A falling clip's real fall in its frame: prev.y - next.y
double fallOf(const Clip& c);
// Picks FRAME_SPEC from the pair's clips: `want` (-1: the first of walking,
// slope, ground, falling the pair has), `drop` (falling, 0: the smallest real
// fall of the pair's falling clips that's still a wall push: checkHeight -
// drop >= 5, else the game's line test runs at the feet). 0 ok, 1 the pair
// has no clip of that kind, 2 falling with no such drop (or `drop` isn't one).
int chooseFrameSpec(const vector<Clip>& clips, int want, double drop, double checkHeight);

// wall_push_clips.js reachability: the lowest speed Link can do clip `c` at,
// from a standable in-bounds start one frame's move away (32 directions, every
// REACH_STEP up to REACH_DIST). Crossings: the game's move at that yaw and
// speed (0.5 past the plane) has to hit the pusher and clip through the same
// wall. Standing points: unlike the JS (which only checks the line), the frame
// is run too - the move at that yaw and speed has to clip through the same
// wall and end out of bounds - so the speed is one that works exactly.
void reachability(const Model& m, Scratch& s, Clip& c);

// --angles: from the refined start, every one of the 4096 directions the sine
// table tells apart (yaw >> 4): its lowest robust speed up to REACH_DIST / 1.5, and whether
// the refined speed works there. Printed as runs of neighbouring yaws.
void angleRanges(const Model& m, const Refined& r, int pusher, int crossed, int threads);

// --refine: the lowest walking speed for one wall pair, searched finer than
// the scan's grid. Starts: every standable in-bounds resting spot on a 0.25
// grid within 24 of the scan's best start, nearest to the clip points first
// (a start can't do better than its distance to them / 1.5). Yaws: toward
// the clip points, every 8, then every 1 around the best. Speeds: every 0.02
// up to the best so far, and at the first that clips, bisected down to the
// f32 boundary. Returns the best found.
Refined refineMinSpeed(const Model& m, const vector<Clip>& clips, int pusher, int crossed, int threads);

// --min-speed: reachability for every clip (on `threads` threads), then the
// lowest speed per wall pair, crossing / standing and kind, printed.
void findMinSpeeds(const Model& m, vector<Clip>& found, int threads);

// --refine's result as an ordinary clip (the frame run again for its fields),
// so the viewer and wall_clip_tester.lua use its exact start, yaw and speed.
// Its kind is left to the caller (the pair's, from the scan).
std::optional<Clip> refinedClip(const Model& m, const Refined& r, int pusher, int crossed);

// --yaw / --max-speed: the lowest speed up to maxSpeed that does this wall
// pair's walking clip moving at exactly `yaw`, from any standable in-bounds
// start behind the scan's clip points along that yaw. found = false if none.
// Every start is tried up to maxSpeed (not just until a slower one is found),
// and the ones that clip are grouped into regions (slowest first). sideStep:
// how far apart the starts are across the yaw (--side-step). exact (--exact):
// then every f32 x, z in each region's box (a sideStep bigger each way) is
// tried too, and the regions are made from those. gridSpeed (--speed, 0
// none): the CSV grid's cells are tried at exactly that speed instead.
// grid: make the CSV grid (r.grid) - --angles doesn't need it.
Refined clipAtYaw(const Model& m, const vector<Clip>& clips, int pusher, int crossed, int yaw, double maxSpeed, double sideStep, bool exact, double gridSpeed, int threads, bool grid = true);

// --speed S with --yaw / --angles: a start that clips moving at `yaw` at
// exactly speed S, from r's starts (r: clipAtYaw up to S), or none.
std::optional<V3> startAtSpeed(const Model& m, const Refined& r, int yaw, double speed, int pusher, int crossed);

// clipfinder: the lowest speed a clip works at (--min-speed, --refine) and the angles that work (--angles).
#pragma once

#include "search.h"

struct Refined { bool found = false; double speed = 0; int yaw = 0; V3 start, end; int starts = 0; };

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

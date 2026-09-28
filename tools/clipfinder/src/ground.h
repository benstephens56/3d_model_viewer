// clipfinder: ground clips - falling fast from the floor, the frame's line
// test runs at Link's feet and misses the floor he starts on, so he goes
// through the ground and under the bottom of the wall beside him.
#pragma once

#include "search.h"

// BgCheck_CheckWallImpl: with checkHeight + dy < 5 (dy = posNext.y -
// posPrev.y) the line test runs from posPrev to posNext themselves, Link's
// feet, floors and walls (one face), instead of at posNext.y + checkHeight.
// Starting on the floor, that line starts on the floor's plane: when the
// f32 plane distance there comes out < 0 (just under it, rounding), the floor
// doesn't count as crossed, and the line goes on into the ground and under
// the bottom of a wall rising from it. The wall push then runs at
// posNext.y + checkHeight, under the wall's bottom too. The floor check
// (from prevPos.y + 50) finds whatever is behind the wall. Fast falls: at
// velocity.y -20 (the fastest, minVelocityY) dy is -30 (x1.5), so it takes
// velocity.y below (5 - checkHeight) / 1.5 (OoT -14).
//
// E.g. OoT Kakariko Village, child: standing at (435.5778, 35.55124, 626)
// on the slope TRI 491 against TRI 507, velocity.y -20, yaw 0, speed 18.

// velocity.y values tried, fastest first
extern const double GROUND_VYS[];
extern const int GROUND_NVY;

// The frame from `start` (on the floor, as the floor check left him), moving
// at yaw / speed with velocity.y vy, and the two frames after it. A ground
// clip through `wall` (-1: any wall), or none. Clip::vy holds vy.
std::optional<Clip> groundFrame(const Model& m, Scratch& s, const V3& start, int yaw, double speed, double vy, int wall = -1);

// The frame's line test at the feet: where it leaves Link and what it hit
// (poly -1: nothing).
struct GroundLine { V3 res; int poly = -1; };
GroundLine groundLine(const Model& m, Scratch& s, const V3& prev, const V3& next);

// For every wall with a floor along its bottom edge: starts on that floor in
// front of it, whose line test misses the floor, and moves under the wall.
// Calls yield for each clip (kind 3), one per point along the wall.
// pairDone(floor, wall): skip what's already found (--first-per-pair).
void groundClipsForWall(const Model& m, Scratch& s, const Poly& W,
	const std::function<bool(int, int)>& pairDone, const std::function<void(const Clip&)>& yield);

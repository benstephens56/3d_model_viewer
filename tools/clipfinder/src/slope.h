// clipfinder: slope clips - walking up a floor under a wall's bottom edge, and
// the floor check lifting Link up behind it.
#pragma once

#include "search.h"

// Walking, posNext is GROUND_DROP below the floor Link starts on, wherever
// the move takes him, so the frame's wall check (and line test) runs at
// checkHeight - GROUND_DROP above his start, not above the floor he walks
// onto. Up a steep slope that can be under the bottom of the wall the slope
// runs into, and nothing stops him. The floor check (from prevPos.y + 50)
// then puts him on a higher floor past the wall's plane (the slope's own
// 1 unit tolerance past its edge, or a floor behind the wall), and now he's
// behind the wall at his check height. The next frame the wall pushes him
// back out if he's at most 4 behind it (wallPush), so a second frame's move
// (speed2) can be needed to take him further (OoT Jabu-Jabu adult: the slope
// TRI 2632 up to TRI 2630, speed 15 then 7).
//
// For every wall along its bottom edge: floors just behind it that would lift
// Link to its height, and starts in front whose check height is under its
// bottom. Calls yield for each clip (kind 2), one per point along the wall.
// pairDone(floor, wall): skip what's already found (--first-per-pair).
void slopeClipsForWall(const Model& m, Scratch& s, const Poly& W,
	const std::function<bool(int, int)>& pairDone, const std::function<void(const Clip&)>& yield);

// The frame from standing still at `start`, moving at yaw / speed, and what
// follows: standing still, or failing that one more frame at the lowest of
// speeds 1, 2, ... that works (Clip::speed2). A slope clip through `wall`
// (-1: any wall), or none.
std::optional<Clip> slopeFrame(const Model& m, Scratch& s, const V3& start, int yaw, double speed, int wall = -1);

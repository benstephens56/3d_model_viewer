// clipfinder: the scan for wall push clips over a whole map.
#pragma once

#include "frame.h"

struct Clip {
	// The wall pair's category, the same for all its points (walking and
	// falling): 0 acute if any of the pair's points is an acutePoint, else 1
	// extended. Set at the end of the scan. 2: a slope clip (slope.h), whose
	// pusher is the floor that lifts Link behind the wall. 3: a ground clip
	// (ground.h), whose pusher is the floor Link falls through.
	int kind = 1;
	// This point on its own clips without the extended planes, pushed from in
	// front of the pusher's face (pushOnFace)
	bool acutePoint = false;
	bool cross = false;
	bool hold = false;    // only with the stick held one more frame (ClipResult::hold)
	int drop = 0;         // falling: posNext this far below the floor (0 walking)
	int pusher = -1, crossed = -1;
	V3 from, prev, next, res, end;
	bool hasNext = false, hasFloorY = false, endNoFloor = false;
	double floorY = 0;
	vector<int> yaws;
	int yaw = 0;          // crossings and standing points: the exact move (s16 yaw, f32 speed)
	bool hasMove = false;
	double speed = 0;
	double speed2 = 0;    // slope clips: a second frame's speed (same yaw) before standing still, 0 none
	double vy = 0;        // ground clips: velocity.y for the frame (posNext.y = prev.y + vy x 1.5)
	// --min-speed: the slowest move from a standable start that does it
	// (reachability below); reachDone and no reach = none found
	bool reachDone = false, hasReach = false;
	double reachSpeed = 0;
	int reachYaw = 0;
	V3 reachStart;
};

// firstPerPair: stop looking at a wall pair (pushing wall, clipped wall) once
// one clip through it is found (like wall_clip_tester.lua's
// RECORD_ONE_PER_PAIR) - one point per pair, much faster.
vector<Clip> scan(const Model& m, int threads, bool firstPerPair = false);

// --max-per-pair N: at most n points per wall pair (and per row the viewer
// shows it in: crossing / standing, walking / falling), spread out evenly
// (farthest point sampling on the clip points). Always kept: the lowest
// --min-speed reach, and an acute point of an acute pair. Keeps the order.
// Returns how many were left out.
size_t thinClips(vector<Clip>& clips, int n);

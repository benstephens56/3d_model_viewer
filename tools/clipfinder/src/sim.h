// clipfinder --sim: one frame from a standing start, printed step by step.
#pragma once

#include "slope.h"

// --sim: Link standing at (x, y, z) (feet), moving at yaw / speed for
// one frame, posNext GROUND_DROP (or DROP) below; every step printed, and
// whether it's a wall push clip or a slope clip. SPEED as "15/7": a frame per
// speed (same yaw), each with its floor check, then two standing still.
// Returns the exit code (2 for a bad --sim value).
int runSim(const Model& m, const string& simArg);

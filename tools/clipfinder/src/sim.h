// clipfinder --sim: one frame from a standing start, printed step by step.
#pragma once

#include "frame.h"

// --sim: Link standing at (x, y, z) (feet), moving at yaw / speed for
// one frame, posNext GROUND_DROP (or DROP) below; every step printed
// Returns the exit code (2 for a bad --sim value).
int runSim(const Model& m, const string& simArg);

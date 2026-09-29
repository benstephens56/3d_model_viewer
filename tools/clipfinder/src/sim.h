// clipfinder --sim: one frame from a standing start, printed step by step.
#pragma once

#include "ground.h"
#include "slope.h"

// --sim: Link standing at (x, y, z) (feet), moving at yaw / speed for
// one frame, posNext GROUND_DROP (or DROP) below; every step printed, and
// whether it's a wall push clip or a slope clip. SPEED as "15/7": a frame per
// speed (same yaw), each with its floor check, then two standing still.
// Returns the exit code (2 for a bad --sim value).
// X,Y,Z,FACING,@ACTION: that action (action.h) from a standing start, e.g. @1h-slash (the form's own).
int runSim(const Model& m, const string& simArg, const string& game, const string& formUpper);

// --tri: each poly's vertices, normal, plane distance and type (dynapolys by their scan ids).
int printTris(const Model& m, const string& ids);

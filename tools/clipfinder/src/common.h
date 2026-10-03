// clipfinder: f32 helpers, V3, the sine table and a frame of walking.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;
using std::vector;

static const double PI = 3.14159265358979323846;
static const double SQRT1_2 = 0.70710678118654752440;

////////////////////////////////////////
// f32 helpers (Math.fround)
////////////////////////////////////////

inline double F(double v) { return (double)(float)v; }
static const double NORMAL_FRAC = F(1.0 / 32767.0);
// IS_ZERO's threshold (js/global.js EPS): OoT3D / MM3D use a smaller one
inline double EPSILON = F(0.008);
inline bool isZero(double v) { return std::fabs(v) < EPSILON; }
inline double sq(double v) { return F(v * v); }
inline int32_t toI32(double v) { return (int32_t)(int64_t)std::trunc(v); } // JS `| 0` for in-range values

struct V3 { double x = 0, y = 0, z = 0; };

// wall_push_clips.js GROUND_DROP: walking, posNext is 7.5 below the floor
// (velocity.y -4 after the floor check, -5 after gravity, x1.5)
// SPEED_RATE: Actor_UpdatePos moves velocity x 1.5 a frame (20 fps).
// OoT3D / MM3D run at 30 fps and move velocity x 1.0 a frame, so walking
// posNext is 5 below the floor there. Set once in main (setGameRate),
// before any scan.
inline double GROUND_DROP = 7.5;
inline double SPEED_RATE = 1.5;
// BgCheck_CheckWallImpl's line test runs at Link's feet, floors included,
// when checkHeight + dy < 5 (dy = posNext.y - prevPos.y). On the 3DS dy counts
// as the N64's move, velocity x 1.5 (LINE_DY_SCALE: x1.5 / x1.0), not the 30
// fps move: in game OoT3D Shadow Temple, adult, from (-1763.541, -63.00192, 77)
// at yaw 0, y velocity -20 (dy -20, 26 - 20 = 6) ground clips under TRI 48 at
// speed 25 and not 20 - exactly the feet-level line test's answer.
inline double LINE_DY_SCALE = 1.0;
inline bool feetLine(double checkHeight, double dy) {
	return F(checkHeight + (LINE_DY_SCALE == 1.0 ? F(dy) : F(dy * LINE_DY_SCALE))) < 5;
}
// The most a frame can fall below its start and still get the wall pushes
// (checkHeight - 5, on the 3DS / 1.5)
inline double maxPushDrop(double checkHeight) { return (checkHeight - 5) / LINE_DY_SCALE; }
// the 3DS versions (OOT3D / MM3D): 30 fps, f32 plane distances, IS_ZERO 0.00008
inline bool IS_3DS = false;
inline void setGameRate(bool is3ds) {
	IS_3DS = is3ds;
	SPEED_RATE = is3ds ? 1.0 : 1.5;
	GROUND_DROP = 5 * SPEED_RATE;
	EPSILON = is3ds ? F(0.00008) : F(0.008);
	LINE_DY_SCALE = is3ds ? 1.5 : 1.0;
}

#include "sintable.h"

// libultra sins() and Math_SinS / Math_CosS (js/libultra_sins.js)
inline int sins(int x) {
	x = (x & 0xFFFF) >> 4;
	int val = (x & 0x400) ? SINTABLE[0x3FF - (x & 0x3FF)] : SINTABLE[x & 0x3FF];
	return (x & 0x800) ? -val : val;
}
static const double SHRT_INV = F(1.0 / 32767.0);
inline double sinS(int yaw) { return F(sins(yaw) * SHRT_INV); }
inline double cosS(int yaw) { return F(sins(yaw + 0x4000) * SHRT_INV); }
// The s16 yaw along (dx, dz), as a u16 (JS Math.round(...) & 0xFFFF).
inline int yawOf(double dx, double dz) {
	return (int)(int64_t)std::floor(std::atan2(dx, dz) / (2 * PI) * 65536 + 0.5) & 0xFFFF;
}
// Link walking on the ground for a frame (Actor_UpdateVelocityWithGravity + Actor_UpdatePos).
inline V3 moveStep(const V3& from, int yaw, double speed) {
	return { F(from.x + F(F(speed * sinS(yaw)) * SPEED_RATE)), F(from.y - GROUND_DROP), F(from.z + F(F(speed * cosS(yaw)) * SPEED_RATE)) };
}

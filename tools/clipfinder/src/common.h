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
static const double EPSILON = F(0.008);
inline bool isZero(double v) { return std::fabs(v) < EPSILON; }
inline double sq(double v) { return F(v * v); }
inline int32_t toI32(double v) { return (int32_t)(int64_t)std::trunc(v); } // JS `| 0` for in-range values

struct V3 { double x = 0, y = 0, z = 0; };

// wall_push_clips.js GROUND_DROP: walking, posNext is 7.5 below the floor
// (velocity.y -4 after the floor check, -5 after gravity, x1.5)
static const double GROUND_DROP = 7.5;
static const double SPEED_RATE = 1.5;

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
	return { F(from.x + F(F(speed * sinS(yaw)) * 1.5)), F(from.y - GROUND_DROP), F(from.z + F(F(speed * cosS(yaw)) * 1.5)) };
}

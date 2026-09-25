// clipfinder: the viewer's wall push clip scan (js/wall_push_clips.js), native
// and multithreaded. Reads an OoT / MM scene from models/, builds the same
// collision model the viewer does (js/parse_model.js, js/subdivisions.js), runs
// the same search and writes the points as JSON for the viewer's "Import
// results" button.
//
// Every computation mirrors the JS line for line: doubles with F() wherever the
// JS has Math.fround, so the numbers come out identical. See the JS for why
// each step is the way it is; the comments here only cover what's different.
//
// Build (MSYS2 mingw64):  see build.sh next to this file.
// Usage:
//   clipfinder --game MM --map "Laundry Pool" --form Human [--falling] [-o out.json]
//   clipfinder --game OOT --all --form Adult [--falling] --out-dir results/
// Options: --root <viewer dir> (default: two levels up from the exe's dir, or
// the current dir if it has models/), --threads N, --radius R (overrides --form).

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

static inline double F(double v) { return (double)(float)v; }
static const double NORMAL_FRAC = F(1.0 / 32767.0);
static const double EPSILON = F(0.008);
static inline bool isZero(double v) { return std::fabs(v) < EPSILON; }
static inline double sq(double v) { return F(v * v); }
static inline int32_t toI32(double v) { return (int32_t)(int64_t)std::trunc(v); } // JS `| 0` for in-range values

struct V3 { double x = 0, y = 0, z = 0; };

// wall_push_clips.js GROUND_DROP: walking, posNext is 7.5 below the floor
// (velocity.y -4 after the floor check, -5 after gravity, x1.5)
static const double GROUND_DROP = 7.5;
static const double SPEED_RATE = 1.5;

#include "sintable.h"

// libultra sins() and Math_SinS / Math_CosS (js/libultra_sins.js)
static int sins(int x) {
	x = (x & 0xFFFF) >> 4;
	int val = (x & 0x400) ? SINTABLE[0x3FF - (x & 0x3FF)] : SINTABLE[x & 0x3FF];
	return (x & 0x800) ? -val : val;
}
static const double SHRT_INV = F(1.0 / 32767.0);
static inline double sinS(int yaw) { return F(sins(yaw) * SHRT_INV); }
static inline double cosS(int yaw) { return F(sins(yaw + 0x4000) * SHRT_INV); }
// The s16 yaw along (dx, dz), as a u16 (JS Math.round(...) & 0xFFFF).
static inline int yawOf(double dx, double dz) {
	return (int)(int64_t)std::floor(std::atan2(dx, dz) / (2 * PI) * 65536 + 0.5) & 0xFFFF;
}
// Link walking on the ground for a frame (Actor_UpdateVelocityWithGravity + Actor_UpdatePos).
static V3 moveStep(const V3& from, int yaw, double speed) {
	return { F(from.x + F(F(speed * sinS(yaw)) * 1.5)), F(from.y - GROUND_DROP), F(from.z + F(F(speed * cosS(yaw)) * 1.5)) };
}

////////////////////////////////////////
// Scene / collision header (parse_model.js)
////////////////////////////////////////

struct Tri {
	int id;
	int v[3][3];
	int n[3];
	int d;
};

struct ColHeader {
	int minB[3], maxB[3];
	int numPolygons = 0;
	int camType = -1;
};

static uint32_t be32(const vector<uint8_t>& b, size_t o) {
	return (uint32_t)b.at(o) << 24 | (uint32_t)b.at(o + 1) << 16 | (uint32_t)b.at(o + 2) << 8 | b.at(o + 3);
}
static uint16_t be16(const vector<uint8_t>& b, size_t o) { return (uint16_t)(b.at(o) << 8 | b.at(o + 1)); }
static int16_t bes16(const vector<uint8_t>& b, size_t o) { return (int16_t)be16(b, o); }

static bool parseScene(const vector<uint8_t>& buf, const string& game, ColHeader& ch, vector<Tri>& tris) {
	const int64_t off = -0x02000000;
	size_t addr = 0;
	int64_t colAddr = -1;
	while (addr + 8 <= buf.size()) {
		int cmd = buf[addr];
		if (cmd == 0x14) break;
		if (cmd == 0x19 && game == "OOT") ch.camType = buf[addr + 1];
		else if (cmd == 0x03) { colAddr = (int64_t)be32(buf, addr + 4) + off; break; }
		addr += 8;
	}
	if (colAddr < 0) return false;
	size_t h = (size_t)colAddr;
	for (int i = 0; i < 3; i++) { ch.minB[i] = bes16(buf, h + i * 2); ch.maxB[i] = bes16(buf, h + 6 + i * 2); }
	int numVtx = be16(buf, h + 0x0C);
	size_t vtxList = (size_t)((int64_t)be32(buf, h + 0x10) + off);
	ch.numPolygons = be16(buf, h + 0x14);
	size_t polyList = (size_t)((int64_t)be32(buf, h + 0x18) + off);
	vector<std::array<int, 3>> verts(numVtx);
	for (int i = 0; i < numVtx; i++)
		for (int k = 0; k < 3; k++) verts[i][k] = bes16(buf, vtxList + i * 6 + k * 2);
	for (int i = 0; i < ch.numPolygons; i++) {
		size_t p = polyList + (size_t)i * 0x10;
		if (polyList + 0x10 > buf.size()) break;
		uint16_t ta = be16(buf, p + 2), tb = be16(buf, p + 4), tc = be16(buf, p + 6);
		int xpFlags = ta >> 13;
		if (xpFlags & 2) continue; // intangible: not in the collision model
		int vi[3] = { ta & 0x1FFF, tb & 0x1FFF, tc & 0x1FFF };
		Tri t;
		t.id = i;
		for (int k = 0; k < 3; k++)
			for (int j = 0; j < 3; j++) t.v[k][j] = verts.at(vi[k])[j];
		for (int k = 0; k < 3; k++) t.n[k] = bes16(buf, p + 8 + k * 2);
		t.d = bes16(buf, p + 14);
		tris.push_back(t);
	}
	return true;
}

////////////////////////////////////////
// Subdivisions (subdivisions.js)
////////////////////////////////////////

static const double OVERLAP = F(50.0);
static const double SUBDIV_MIN = F(150.0);

struct ColCtx {
	int amt[3];
	double minB[3], maxB[3], len[3], inv[3];
	vector<vector<int>> subFloors, subWalls; // poly ids, in poly index order
};

static void setDim(double mn, int amount, double mx, double& newMax, double& len, double& inv) {
	mn = F(mn); mx = F(mx);
	double length = F(F(mx) - F(mn));
	int temp = toI32(F(length / amount));
	double l = F(temp + 1);
	if (l < SUBDIV_MIN) l = SUBDIV_MIN;
	l = F(l);
	inv = F(F(1.0) / l);
	newMax = F(F(l * amount) + mn);
	len = l;
}

static void initColCtx(ColCtx& c, const string& game, const string& mapName, const ColHeader& ch) {
	static const std::map<string, std::array<int, 3>> ootList = { { "Shadow Temple", { 23, 7, 14 } }, { "Forest Temple", { 38, 1, 38 } } };
	static const std::map<string, std::array<int, 3>> mmList = { { "Termina Field", { 36, 1, 36 } }, { "Great Bay Coast", { 40, 1, 40 } }, { "Zora Cape", { 40, 1, 40 } } };
	std::array<int, 3> a = { 16, 4, 16 };
	if (game == "OOT" && (ch.camType == 0x10 || ch.camType == 0x20 || ch.camType == 0x30 || ch.camType == 0x40)) a = { 2, 2, 2 };
	else if (game == "OOT" && ootList.count(mapName)) a = ootList.at(mapName);
	else if (game == "MM" && mmList.count(mapName)) a = mmList.at(mapName);
	for (int i = 0; i < 3; i++) {
		c.amt[i] = a[i];
		c.minB[i] = F(ch.minB[i]);
		setDim(c.minB[i], a[i], F(ch.maxB[i]), c.maxB[i], c.len[i], c.inv[i]);
	}
	size_t total = (size_t)a[0] * a[1] * a[2];
	c.subFloors.assign(total, {});
	c.subWalls.assign(total, {});
}

static bool triIntersectsCube(const Tri& t, const double box[6]) {
	// box: xmin, xmax, ymin, ymax, zmin, zmax
	double v[3][3];
	for (int k = 0; k < 3; k++) for (int j = 0; j < 3; j++) v[k][j] = t.v[k][j];
	for (int ax = 0; ax < 3; ax++) {
		double mn = std::min({ v[0][ax], v[1][ax], v[2][ax] }), mx = std::max({ v[0][ax], v[1][ax], v[2][ax] });
		if (mx < box[ax * 2] || mn > box[ax * 2 + 1]) return false;
	}
	double c[3], h[3];
	for (int ax = 0; ax < 3; ax++) {
		c[ax] = F((box[ax * 2] + box[ax * 2 + 1]) * 0.5);
		h[ax] = F((box[ax * 2 + 1] - box[ax * 2]) * 0.5);
	}
	double tv[3][3];
	for (int k = 0; k < 3; k++) for (int ax = 0; ax < 3; ax++) tv[k][ax] = F(v[k][ax] - c[ax]);
	double e[3][3];
	for (int ax = 0; ax < 3; ax++) {
		e[0][ax] = F(tv[1][ax] - tv[0][ax]);
		e[1][ax] = F(tv[2][ax] - tv[1][ax]);
		e[2][ax] = F(tv[0][ax] - tv[2][ax]);
	}
	for (int ax = 0; ax < 3; ax++) {
		if (std::max({ tv[0][ax], tv[1][ax], tv[2][ax] }) < -h[ax] || std::min({ tv[0][ax], tv[1][ax], tv[2][ax] }) > h[ax]) return false;
	}
	double n[3] = {
		F(e[0][1] * e[1][2] - e[0][2] * e[1][1]),
		F(e[0][2] * e[1][0] - e[0][0] * e[1][2]),
		F(e[0][0] * e[1][1] - e[0][1] * e[1][0]),
	};
	double r = F(h[0] * std::fabs(n[0]) + h[1] * std::fabs(n[1]) + h[2] * std::fabs(n[2]));
	double d = F(n[0] * tv[0][0] + n[1] * tv[0][1] + n[2] * tv[0][2]);
	if (d > r || d < -r) return false;
	auto axisTest = [&](const double* ed) {
		double p[3], mn, mx, rad;
		for (int k = 0; k < 3; k++) p[k] = F(ed[2] * tv[k][1] - ed[1] * tv[k][2]);
		mn = std::min({ p[0], p[1], p[2] }); mx = std::max({ p[0], p[1], p[2] });
		rad = F(std::fabs(ed[1]) * h[2] + std::fabs(ed[2]) * h[1]);
		if (mn > rad || mx < -rad) return false;
		for (int k = 0; k < 3; k++) p[k] = F(ed[0] * tv[k][2] - ed[2] * tv[k][0]);
		mn = std::min({ p[0], p[1], p[2] }); mx = std::max({ p[0], p[1], p[2] });
		rad = F(std::fabs(ed[0]) * h[2] + std::fabs(ed[2]) * h[0]);
		if (mn > rad || mx < -rad) return false;
		for (int k = 0; k < 3; k++) p[k] = F(ed[1] * tv[k][0] - ed[0] * tv[k][1]);
		mn = std::min({ p[0], p[1], p[2] }); mx = std::max({ p[0], p[1], p[2] });
		rad = F(std::fabs(ed[0]) * h[1] + std::fabs(ed[1]) * h[0]);
		if (mn > rad || mx < -rad) return false;
		return true;
	};
	return axisTest(e[0]) && axisTest(e[1]) && axisTest(e[2]);
}

static void subdivMinBounds(const ColCtx& c, const double pos[3], int out[3]) {
	for (int ax = 0; ax < 3; ax++) {
		double d = F(pos[ax] - c.minB[ax]);
		int s = toI32(F(d * c.inv[ax]));
		int di = toI32(d), subi = toI32(c.len[ax]);
		if ((di % subi) < OVERLAP && s > 0) s -= 1;
		out[ax] = s;
	}
}

static void subdivMaxBounds(const ColCtx& c, const double pos[3], int out[3]) {
	for (int ax = 0; ax < 3; ax++) {
		double d = F(F(pos[ax]) - F(c.minB[ax]));
		int s = toI32(F(d * c.inv[ax]));
		int sub = toI32(c.len[ax]);
		if ((sub - OVERLAP) < (toI32(d) % sub) && s < c.amt[ax] - 1) s += 1;
		out[ax] = s;
	}
}

static void initializeSubdivisions(ColCtx& c, const vector<Tri>& tris) {
	const double lenX = F(c.len[0] + F(2 * OVERLAP)), lenY = F(c.len[1] + F(2 * OVERLAP)), lenZ = F(c.len[2] + F(2 * OVERLAP));
	const int amtXY = c.amt[0] * c.amt[1];
	for (const Tri& t : tris) {
		double mn[3], mx[3];
		for (int ax = 0; ax < 3; ax++) mn[ax] = mx[ax] = F(t.v[0][ax]);
		for (int k = 1; k < 3; k++) {
			for (int ax = 0; ax < 3; ax++) {
				double v = F(t.v[k][ax]);
				if (mn[ax] > v) mn[ax] = v; else if (mx[ax] < v) mx[ax] = v;
			}
		}
		int lo[3], hi[3];
		subdivMinBounds(c, mn, lo);
		subdivMaxBounds(c, mx, hi);
		double ny = F(t.n[1] * NORMAL_FRAC);
		int baseZ = lo[2] * amtXY;
		double curMinZ = F(F(c.len[2] * lo[2]) + c.minB[2] - OVERLAP);
		double curMaxZ = F(curMinZ + lenZ);
		for (int sz = lo[2]; sz <= hi[2]; sz++) {
			int baseY = lo[1] * c.amt[0];
			double curMinY = F(F(c.len[1] * lo[1]) + c.minB[1] - OVERLAP);
			double curMaxY = F(curMinY + lenY);
			for (int sy = lo[1]; sy <= hi[1]; sy++) {
				int index = baseZ + baseY + lo[0];
				double curMinX = F(F(c.len[0] * lo[0]) + c.minB[0] - OVERLAP);
				double curMaxX = F(curMinX + lenX);
				for (int sx = lo[0]; sx <= hi[0]; sx++) {
					double box[6] = { curMinX, curMaxX, curMinY, curMaxY, curMinZ, curMaxZ };
					if (index >= 0 && index < (int)c.subWalls.size() && triIntersectsCube(t, box)) {
						if (ny > 0.5) c.subFloors[index].push_back(t.id);
						else if (ny < -0.8) { /* ceiling */ }
						else c.subWalls[index].push_back(t.id);
					}
					curMinX = F(curMinX + c.len[0]);
					curMaxX = F(curMaxX + c.len[0]);
					index++;
				}
				curMinY = F(curMinY + c.len[1]);
				curMaxY = F(curMaxY + c.len[1]);
				baseY += c.amt[0];
			}
			curMinZ = F(curMinZ + c.len[2]);
			curMaxZ = F(curMaxZ + c.len[2]);
			baseZ += amtXY;
		}
	}
}

struct CellIdx { int sx, sy, sz, index; };

static CellIdx pointCell(const ColCtx& c, double x, double y, double z) {
	double p[3] = { x, y, z };
	int s[3];
	for (int ax = 0; ax < 3; ax++) {
		double d = F(F(p[ax]) - F(c.minB[ax]));
		s[ax] = toI32(F(d * c.inv[ax]));
		s[ax] = std::min(std::max(s[ax], 0), c.amt[ax] - 1);
	}
	return { s[0], s[1], s[2], s[2] * c.amt[0] * c.amt[1] + s[1] * c.amt[0] + s[0] };
}

////////////////////////////////////////
// Collision model (wall_push_clips.js)
////////////////////////////////////////

static const int SNORMAL_FLOOR = (int)std::trunc(0.5 * 32767);
static const int SNORMAL_CEIL = (int)std::trunc(-0.8 * 32767);

struct Poly {
	bool exists = false;
	int id;
	double ax, ay, az, bx, by, bz, cx, cy, cz;
	int sx, sy, sz;
	double nx, ny, nz, dist, nMag, nXZ, invNXZ;
	double minX, maxX, minY, maxY, minZ, maxZ, sortY, tz, tx;
	bool isFloor, isCeiling, isWall;
};

struct Tol { double detMax, chkDist, lineChkDist; };
static const Tol LOOSE = { 300, 1, 1 };
static const Tol STRICT = { 0, 0, 0 };

static double planeDist(const Poly& p, double x, double y, double z) {
	if (isZero(p.nMag)) return 0;
	return F(F(F(F(F(p.nx * x) + F(p.ny * y)) + F(p.nz * z)) + p.dist) / p.nMag);
}

static double edgeDistSq(double x0, double y0, double x1, double y1, double x2, double y2) {
	double dx = F(x2 - x1), dy = F(y2 - y1);
	double lenSq = F(sq(dx) + sq(dy));
	if (isZero(lenSq)) return INFINITY;
	double t = F(F(F(F(x0 - x1) * dx) + F(F(y0 - y1) * dy)) / lenSq);
	if (!(t >= 0 && t <= 1)) return INFINITY;
	return F(sq(F(F(F(dx * t) + x1) - x0)) + sq(F(F(F(dy * t) + y1) - y0)));
}

static bool triChkPara(double a0, double b0, double a1, double b1, double a2, double b2, double pa, double pb,
	double detMax, double chkDist, double nComp) {
	if (!(F(std::min({ a0, a1, a2 }) - chkDist) <= pa && F(std::max({ a0, a1, a2 }) + chkDist) >= pa &&
		F(std::min({ b0, b1, b2 }) - chkDist) <= pb && F(std::max({ b0, b1, b2 }) + chkDist) >= pb)) return false;
	double chkSq = sq(chkDist);
	if (F(sq(F(a0 - pa)) + sq(F(b0 - pb))) < chkSq || F(sq(F(a1 - pa)) + sq(F(b1 - pb))) < chkSq ||
		F(sq(F(a2 - pa)) + sq(F(b2 - pb))) < chkSq) return true;
	double d01 = F(F(F(a0 - pa) * F(b1 - pb)) - F(F(b0 - pb) * F(a1 - pa)));
	double d12 = F(F(F(a1 - pa) * F(b2 - pb)) - F(F(b1 - pb) * F(a2 - pa)));
	double d20 = F(F(F(a2 - pa) * F(b0 - pb)) - F(F(b2 - pb) * F(a0 - pa)));
	if ((d01 <= detMax && d12 <= detMax && d20 <= detMax) || (d01 >= -detMax && d12 >= -detMax && d20 >= -detMax)) return true;
	if (std::fabs(nComp) > 0.5) {
		if (edgeDistSq(pa, pb, a0, b0, a1, b1) < chkSq || edgeDistSq(pa, pb, a1, b1, a2, b2) < chkSq ||
			edgeDistSq(pa, pb, a2, b2, a0, b0) < chkSq) return true;
	}
	return false;
}
static bool triChkX(const Poly& p, double y, double z, double dm, double ck) { return triChkPara(p.ay, p.az, p.by, p.bz, p.cy, p.cz, y, z, dm, ck, p.nx); }
static bool triChkY(const Poly& p, double z, double x, double dm, double ck) { return triChkPara(p.az, p.ax, p.bz, p.bx, p.cz, p.cx, z, x, dm, ck, p.ny); }
static bool triChkZ(const Poly& p, double x, double y, double dm, double ck) { return triChkPara(p.ax, p.ay, p.bx, p.by, p.cx, p.cy, x, y, dm, ck, p.nz); }

static bool pointInTri3D(const Poly& p, double x, double y, double z, double tol) {
	double ax = std::fabs(p.nx), ay = std::fabs(p.ny), az = std::fabs(p.nz);
	double a0, b0, a1, b1, a2, b2, pa, pb;
	if (ax >= ay && ax >= az) { a0 = p.ay; b0 = p.az; a1 = p.by; b1 = p.bz; a2 = p.cy; b2 = p.cz; pa = y; pb = z; }
	else if (az >= ay) { a0 = p.ax; b0 = p.ay; a1 = p.bx; b1 = p.by; a2 = p.cx; b2 = p.cy; pa = x; pb = y; }
	else { a0 = p.az; b0 = p.ax; a1 = p.bz; b1 = p.bx; a2 = p.cz; b2 = p.cx; pa = z; pb = x; }
	double d01 = (a0 - pa) * (b1 - pb) - (b0 - pb) * (a1 - pa);
	double d12 = (a1 - pa) * (b2 - pb) - (b1 - pb) * (a2 - pa);
	double d20 = (a2 - pa) * (b0 - pb) - (b2 - pb) * (a0 - pa);
	if ((d01 >= 0 && d12 >= 0 && d20 >= 0) || (d01 <= 0 && d12 <= 0 && d20 <= 0)) return true;
	double tSq = tol * tol;
	auto near = [&](double x0, double y0, double x1, double y1, double x2, double y2) {
		double dx = x2 - x1, dy = y2 - y1, len = dx * dx + dy * dy;
		if (len == 0) return false;
		double t = std::max(0.0, std::min(1.0, ((x0 - x1) * dx + (y0 - y1) * dy) / len));
		return std::pow(x1 + dx * t - x0, 2) + std::pow(y1 + dy * t - y0, 2) <= tSq;
	};
	return near(pa, pb, a0, b0, a1, b1) || near(pa, pb, a1, b1, a2, b2) || near(pa, pb, a2, b2, a0, b0);
}

struct Push { int poly; V3 from, to; bool line = false; };
struct Hit { int poly; double x, y, z; };

// Fixed-capacity inline lists. The scan's inner loops used to allocate their
// results on the heap, and with many threads the C runtime's allocator lock
// made the threads wait on each other more than they worked.
template <typename T, int N>
struct InlineList {
	int n = 0;
	T a[N];
	void push_back(const T& v) { if (n < N) a[n++] = v; }
	bool empty() const { return n == 0; }
	int size() const { return n; }
	const T* begin() const { return a; }
	const T* end() const { return a + n; }
	const T& operator[](int i) const { return a[i]; }
	void clear() { n = 0; }
};
using FloorList = InlineList<double, 16>;  // floor heights under one point
using PushList = InlineList<Push, 64>;     // one frame's wall pushes

// Per-thread scratch: an open-addressing floorsNear cache (reset per wall or
// wall pair), a visited-stamp array and reusable id lists.
struct Scratch {
	static const int CAP = 1 << 15;
	vector<int64_t> cacheKey = vector<int64_t>(CAP);
	vector<uint32_t> cacheGen = vector<uint32_t>(CAP, 0);
	vector<FloorList> cacheVal = vector<FloorList>(CAP);
	uint32_t gen = 1;
	int used = 0;
	void clearCache() { gen++; used = 0; }

	vector<uint32_t> stamp;
	uint32_t curStamp = 0;
	vector<int> wallsBuf, cellsBuf;
	uint32_t nextStamp() {
		if (++curStamp == 0) { std::fill(stamp.begin(), stamp.end(), 0); curStamp = 1; }
		return curStamp;
	}
};

struct Model {
	ColCtx colCtx;
	double radius, checkHeight;
	int lowDrop;
	vector<Poly> polys;
	vector<vector<int>> cellWallsL, cellFloorsL; // sorted, per subdivision
	std::unordered_map<int64_t, vector<int>> floorGrid;
	const double floorCell = 128;

	static int64_t key2(int64_t a, int64_t b) { return (a << 32) ^ (b & 0xFFFFFFFF); }

	void build(const vector<Tri>& tris, int numPolygons) {
		polys.assign(numPolygons, Poly{});
		for (const Tri& t : tris) {
			Poly& p = polys[t.id];
			p.exists = true;
			p.id = t.id;
			p.ax = t.v[0][0]; p.ay = t.v[0][1]; p.az = t.v[0][2];
			p.bx = t.v[1][0]; p.by = t.v[1][1]; p.bz = t.v[1][2];
			p.cx = t.v[2][0]; p.cy = t.v[2][1]; p.cz = t.v[2][2];
			p.sx = t.n[0]; p.sy = t.n[1]; p.sz = t.n[2];
			p.nx = F(p.sx * NORMAL_FRAC); p.ny = F(p.sy * NORMAL_FRAC); p.nz = F(p.sz * NORMAL_FRAC);
			p.nXZ = F(std::sqrt(F(sq(p.nx) + sq(p.nz))));
			p.dist = t.d;
			p.nMag = F(std::sqrt(F(F(sq(p.nx) + sq(p.ny)) + sq(p.nz))));
			p.invNXZ = p.nXZ > 0 ? F(1 / p.nXZ) : 0;
			p.minX = std::min({ p.ax, p.bx, p.cx }); p.maxX = std::max({ p.ax, p.bx, p.cx });
			p.minY = std::min({ p.ay, p.by, p.cy }); p.maxY = std::max({ p.ay, p.by, p.cy });
			p.minZ = std::min({ p.az, p.bz, p.cz }); p.maxZ = std::max({ p.az, p.bz, p.cz });
			p.isFloor = p.sy > SNORMAL_FLOOR;
			p.isCeiling = p.sy < SNORMAL_CEIL;
			p.isWall = !p.isFloor && !p.isCeiling;
			p.sortY = (p.sy == 32767 || p.sy == -32767) ? p.ay : p.minY;
			p.tz = p.nXZ > 0 ? F(std::fabs(p.nz) * p.invNXZ) : 0;
			p.tx = p.nXZ > 0 ? F(std::fabs(p.nx) * p.invNXZ) : 0;
		}
		auto sorted = [&](const vector<int>& ids, bool walls) {
			vector<int> out;
			for (int id : ids) {
				const Poly& p = polys[id];
				if (!p.exists) continue;
				if (walls ? p.isWall : p.isFloor) out.push_back(id);
			}
			std::stable_sort(out.begin(), out.end(), [&](int a, int b) {
				if (polys[a].sortY != polys[b].sortY) return polys[a].sortY < polys[b].sortY;
				return a < b;
			});
			return out;
		};
		cellWallsL.resize(colCtx.subWalls.size());
		cellFloorsL.resize(colCtx.subWalls.size());
		for (size_t i = 0; i < colCtx.subWalls.size(); i++) {
			cellWallsL[i] = sorted(colCtx.subWalls[i], true);
			cellFloorsL[i] = sorted(colCtx.subFloors[i], false);
		}
		for (const Poly& p : polys) {
			if (!p.exists || !p.isFloor) continue;
			int64_t x0 = (int64_t)std::floor(p.minX / floorCell), x1 = (int64_t)std::floor(p.maxX / floorCell);
			int64_t z0 = (int64_t)std::floor(p.minZ / floorCell), z1 = (int64_t)std::floor(p.maxZ / floorCell);
			for (int64_t gx = x0; gx <= x1; gx++)
				for (int64_t gz = z0; gz <= z1; gz++) floorGrid[key2(gx, gz)].push_back(p.id);
		}
	}

	const vector<int>& cellWalls(double x, double y, double z) const { return cellWallsL[pointCell(colCtx, x, y, z).index]; }

	FloorList floorsAt(double x, double z) const {
		FloorList out;
		auto it = floorGrid.find(key2((int64_t)std::floor(x / floorCell), (int64_t)std::floor(z / floorCell)));
		if (it == floorGrid.end()) return out;
		for (int id : it->second) {
			const Poly& p = polys[id];
			if (x < p.minX - 1 || x > p.maxX + 1 || z < p.minZ - 1 || z > p.maxZ + 1) continue;
			if (!triChkY(p, z, x, 0, 1)) continue;
			out.push_back(F(F(F(F(-p.nx * x) - F(p.nz * z)) - p.dist) / p.ny));
		}
		return out;
	}

	const FloorList& floorsNear(Scratch& s, double x, double z) const {
		int64_t kx = (int64_t)std::floor(x * 4 + 0.5), kz = (int64_t)std::floor(z * 4 + 0.5);
		int64_t k = key2(kx, kz);
		if (s.used > Scratch::CAP * 7 / 10) s.clearCache();
		uint64_t h = (uint64_t)k * 0x9E3779B97F4A7C15ull;
		int i = (int)(h >> 49);  // top 15 bits
		while (s.cacheGen[i] == s.gen) {
			if (s.cacheKey[i] == k) return s.cacheVal[i];
			i = (i + 1) & (Scratch::CAP - 1);
		}
		s.cacheGen[i] = s.gen;
		s.cacheKey[i] = k;
		s.cacheVal[i] = floorsAt(kx / 4.0, kz / 4.0);
		s.used++;
		return s.cacheVal[i];
	}

	V3 sphereStep(const V3& pos, const Tol& tol, PushList* trace) const {
		const double R = radius;
		const double sphY = F(pos.y + checkHeight);
		const vector<int>& list = cellWalls(pos.x, pos.y, pos.z);
		double rx = pos.x, rz = pos.z;
		for (int pass = 0; pass < 2; pass++) {
			for (int id : list) {
				const Poly& p = polys[id];
				if (sphY < p.minY) break;
				double pd = planeDist(p, rx, sphY, rz);
				if (R < std::fabs(pd)) continue;
				bool hit = false;
				if (pass == 0) {
					if (p.tz < F(0.4)) continue;
					if (rz < F(p.minZ - R) || rz > F(p.maxZ + R)) continue;
					if (isZero(p.nz) || !triChkZ(p, rx, sphY, tol.detMax, tol.chkDist)) continue;
					double inter = F(F(F(F(-p.nx * rx) - F(p.ny * sphY)) - p.dist) / p.nz);
					double d = F(inter - rz);
					hit = std::fabs(d) <= F(R / p.tz) && F(d * p.nz) <= 4.0;
				} else {
					if (p.tx < F(0.4)) continue;
					if (rx < F(p.minX - R) || F(p.maxX + R) < rx) continue;
					if (isZero(p.nx) || !triChkX(p, sphY, rz, tol.detMax, tol.chkDist)) continue;
					double inter = F(F(F(F(-p.ny * sphY) - F(p.nz * rz)) - p.dist) / p.nx);
					double d = F(inter - rx);
					hit = std::fabs(d) <= F(R / p.tx) && F(d * p.nx) <= 4.0;
				}
				if (hit) {
					double disp = F(F(R - pd) * p.invNXZ);
					V3 from = { rx, pos.y, rz };
					rx = F(rx + F(disp * p.nx));
					rz = F(rz + F(disp * p.nz));
					if (trace) trace->push_back({ id, from, { rx, pos.y, rz } });
				}
			}
		}
		return { rx, pos.y, rz };
	}

	// Where Link comes to rest standing at `pos` (pushes until they stop, at
	// most 4 frames), or none.
	std::optional<V3> restingSpot(const V3& pos) const {
		V3 cur = pos;
		for (int i = 0; i < 4; i++) {
			V3 next = sphereStep(cur, LOOSE, nullptr);
			if (std::fabs(next.x - cur.x) <= 0.01 && std::fabs(next.z - cur.z) <= 0.01) return cur;
			cur = next;
		}
		return std::nullopt;
	}

	std::optional<V3> lineVsPoly(const Poly& p, const V3& a, const V3& b, double chkDist, bool oneFace) const {
		double planeA = F(F(F(F(F(p.sx * a.x) + F(p.sy * a.y)) + F(p.sz * a.z)) * NORMAL_FRAC) + p.dist);
		double planeB = F(F(F(F(F(p.sx * b.x) + F(p.sy * b.y)) + F(p.sz * b.z)) * NORMAL_FRAC) + p.dist);
		double delta = F(planeA - planeB);
		if ((planeA >= 0 && planeB >= 0) || (planeA < 0 && planeB < 0) || (oneFace && planeA < 0 && planeB > 0) ||
			isZero(delta)) return std::nullopt;
		double t = F(planeA / delta);
		V3 i = { F(F(F(b.x - a.x) * t) + a.x), F(F(F(b.y - a.y) * t) + a.y), F(F(F(b.z - a.z) * t) + a.z) };
		if ((std::fabs(p.nx) > 0.5 && !isZero(p.nx) && triChkX(p, i.y, i.z, 0, chkDist)) ||
			(std::fabs(p.ny) > 0.5 && !isZero(p.ny) && triChkY(p, i.z, i.x, 0, chkDist)) ||
			(std::fabs(p.nz) > 0.5 && !isZero(p.nz) && triChkZ(p, i.x, i.y, 0, chkDist))) return i;
		return std::nullopt;
	}

	std::optional<Hit> lineHit(Scratch& s, const V3& a, const V3& b, const Tol& tol, bool floors, bool oneFace = false) const {
		CellIdx ia = pointCell(colCtx, a.x, a.y, a.z), ib = pointCell(colCtx, b.x, b.y, b.z);
		int cells[64];
		int nCells = 0;
		vector<int>& many = s.cellsBuf;
		many.clear();
		if (ia.index == ib.index) cells[nCells++] = ia.index;
		else {
			for (int sz = std::min(ia.sz, ib.sz); sz <= std::max(ia.sz, ib.sz); sz++)
				for (int sy = std::min(ia.sy, ib.sy); sy <= std::max(ia.sy, ib.sy); sy++)
					for (int sx = std::min(ia.sx, ib.sx); sx <= std::max(ia.sx, ib.sx); sx++)
						many.push_back(sz * colCtx.amt[0] * colCtx.amt[1] + sy * colCtx.amt[0] + sx);
		}
		uint32_t st = s.nextStamp();
		std::optional<Hit> best;
		double bestDistSq = 1.0e38;
		V3 end = b;
		auto scan = [&](const vector<int>& list) {
			for (int id : list) {
				if (s.stamp[id] == st) continue;
				s.stamp[id] = st;
				const Poly& p = polys[id];
				if (a.y < p.sortY && end.y < p.sortY) break;
				auto i = lineVsPoly(p, a, end, tol.lineChkDist, oneFace);
				if (!i) continue;
				double d = F(F(sq(F(a.x - i->x)) + sq(F(a.y - i->y))) + sq(F(a.z - i->z)));
				if (d < bestDistSq) {
					bestDistSq = d;
					best = Hit{ id, i->x, i->y, i->z };
					end = *i;
				}
			}
		};
		auto doCell = [&](int index) {
			if (floors) scan(cellFloorsL[index]);
			scan(cellWallsL[index]);
		};
		if (nCells) doCell(cells[0]);
		for (int index : many) doCell(index);
		return best;
	}

	// wallsAlong, as a list of poly ids (deduplicated): the cell's own list, or
	// s.wallsBuf.
	const vector<int>& wallsAlong(Scratch& s, const V3& a, const V3& b) const {
		int steps = std::max(1, (int)std::ceil(std::hypot(b.x - a.x, b.z - a.z) / 40));
		if (steps == 1) {
			int ia = pointCell(colCtx, a.x, a.y, a.z).index, ib = pointCell(colCtx, b.x, a.y, b.z).index;
			if (ia == ib) return cellWallsL[ia];
		}
		vector<int>& out = s.wallsBuf;
		out.clear();
		uint32_t st = s.nextStamp();
		for (int k = 0; k <= steps; k++) {
			double t = (double)k / steps;
			for (int id : cellWalls(a.x + (b.x - a.x) * t, a.y, a.z + (b.z - a.z) * t)) {
				if (s.stamp[id] == st) continue;
				s.stamp[id] = st;
				out.push_back(id);
			}
		}
		return out;
	}

	int crossedWall(Scratch& s, const V3& a, const V3& b, bool exiting = false) const {
		double y = a.y + checkHeight;
		int best = -1;
		double bestT = INFINITY;
		for (int id : wallsAlong(s, a, b)) {
			const Poly& p = polys[id];
			if (y < p.minY || y > p.maxY) continue;
			double dA = planeDist(p, a.x, y, a.z), dB = planeDist(p, b.x, y, b.z);
			if (exiting) { dA = -dA; dB = -dB; }
			if (!(dA > 0 && dB < 0)) continue;
			double t = dA / (dA - dB);
			if (t >= bestT) continue;
			double ix = a.x + (b.x - a.x) * t, iz = a.z + (b.z - a.z) * t;
			if (pointInTri3D(p, ix, y, iz, 0.25)) { best = id; bestT = t; }
		}
		return best;
	}

	bool behindWall(const V3& pos) const {
		double y = pos.y + checkHeight;
		for (int id : cellWalls(pos.x, pos.y, pos.z)) {
			const Poly& p = polys[id];
			if (y < p.minY || y > p.maxY) continue;
			double d = planeDist(p, pos.x, y, pos.z);
			if (!(d < 0 && d > -2 * radius)) continue;
			double k = d / p.nMag;
			if (pointInTri3D(p, pos.x - k * p.nx, y - k * p.ny, pos.z - k * p.nz, 0)) return true;
		}
		return false;
	}

	bool isInBounds(Scratch& s, const V3& pos) const {
		if (behindWall(pos)) return false;
		double y = F(pos.y + checkHeight);
		const double len = 400;
		for (int i = 0; i < 8; i++) {
			double ang = i * PI / 4;
			V3 a = { pos.x, y, pos.z };
			V3 b = { F(pos.x + std::sin(ang) * len), y, F(pos.z + std::cos(ang) * len) };
			auto hit = lineHit(s, a, b, STRICT, false);
			if (hit && planeDist(polys[hit->poly], a.x, a.y, a.z) < 0) return false;
		}
		return true;
	}
};

////////////////////////////////////////
// Search
////////////////////////////////////////

static const double NEXT_STEP = 0.5;
static const double CROSS_STEP = 0.25;
static const double FLOOR_BLOCK = 4;
static const double MOVE_STEPS[] = { 2, 4, 6, 8, 12, 16, 20 };
static const double REACH = 14;

struct ClipResult { int crossed, pusher; V3 end; };

static std::optional<ClipResult> clipFromFrame(const Model& m, Scratch& s, const V3& prev, const V3& res,
	const PushList& trace, const Tol& tol) {
	if (trace.empty()) return std::nullopt;
	int crossed = m.crossedWall(s, prev, res);
	if (crossed < 0) return std::nullopt;
	V3 s1 = m.sphereStep(res, tol, nullptr);
	V3 s2 = m.sphereStep(s1, tol, nullptr);
	if (m.crossedWall(s, prev, s2) != crossed) return std::nullopt;
	if (m.crossedWall(s, prev, s2, true) >= 0) return std::nullopt;
	const Poly& C = m.polys[crossed];
	double sphY = prev.y + m.checkHeight;
	int pusher = -1;
	for (const Push& t : trace) {
		if (t.poly == crossed) continue;
		double before = planeDist(C, t.from.x, sphY, t.from.z), after = planeDist(C, t.to.x, sphY, t.to.z);
		if (before >= 0 && after < 0) { pusher = t.poly; break; }
	}
	if (pusher < 0) {
		for (int i = trace.size() - 1; i >= 0; i--) if (trace[i].poly != crossed) { pusher = trace[i].poly; break; }
	}
	if (pusher < 0) return std::nullopt;
	return ClipResult{ crossed, pusher, s2 };
}

struct Pair { int A, B; double cosAB, lo, hi, x0, x1, z0, z1; };

// Triangle-triangle distance (wall_push_clips.js triTriDist): 0 if an edge of
// one passes through the other, else the smallest vertex-triangle / edge-edge
// distance.
using D3 = std::array<double, 3>;
static D3 sub3(const D3& a, const D3& b) { return { a[0] - b[0], a[1] - b[1], a[2] - b[2] }; }
static double dot3(const D3& a, const D3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static D3 cross3(const D3& a, const D3& b) { return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] }; }

static D3 closestOnTri(const D3& p, const D3& a, const D3& b, const D3& c) {
	D3 ab = sub3(b, a), ac = sub3(c, a), ap = sub3(p, a);
	double d1 = dot3(ab, ap), d2 = dot3(ac, ap);
	if (d1 <= 0 && d2 <= 0) return a;
	D3 bp = sub3(p, b);
	double d3 = dot3(ab, bp), d4 = dot3(ac, bp);
	if (d3 >= 0 && d4 <= d3) return b;
	double vc = d1 * d4 - d3 * d2;
	if (vc <= 0 && d1 >= 0 && d3 <= 0) { double v = d1 / (d1 - d3); return { a[0] + v * ab[0], a[1] + v * ab[1], a[2] + v * ab[2] }; }
	D3 cp = sub3(p, c);
	double d5 = dot3(ab, cp), d6 = dot3(ac, cp);
	if (d6 >= 0 && d5 <= d6) return c;
	double vb = d5 * d2 - d1 * d6;
	if (vb <= 0 && d2 >= 0 && d6 <= 0) { double w = d2 / (d2 - d6); return { a[0] + w * ac[0], a[1] + w * ac[1], a[2] + w * ac[2] }; }
	double va = d3 * d6 - d5 * d4;
	if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0) {
		double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return { b[0] + w * (c[0] - b[0]), b[1] + w * (c[1] - b[1]), b[2] + w * (c[2] - b[2]) };
	}
	double denom = 1 / (va + vb + vc), v = vb * denom, w = vc * denom;
	return { a[0] + ab[0] * v + ac[0] * w, a[1] + ab[1] * v + ac[1] * w, a[2] + ab[2] * v + ac[2] * w };
}

static double segSegDistSq(const D3& p1, const D3& q1, const D3& p2, const D3& q2) {
	D3 d1 = sub3(q1, p1), d2 = sub3(q2, p2), r = sub3(p1, p2);
	double a = dot3(d1, d1), e = dot3(d2, d2), f = dot3(d2, r), s, t;
	auto clamp01 = [](double v) { return std::min(std::max(v, 0.0), 1.0); };
	if (a <= 1e-12 && e <= 1e-12) return dot3(r, r);
	if (a <= 1e-12) { s = 0; t = clamp01(f / e); }
	else {
		double c = dot3(d1, r);
		if (e <= 1e-12) { t = 0; s = clamp01(-c / a); }
		else {
			double b = dot3(d1, d2), denom = a * e - b * b;
			s = denom != 0 ? clamp01((b * f - c * e) / denom) : 0;
			t = (b * s + f) / e;
			if (t < 0) { t = 0; s = clamp01(-c / a); }
			else if (t > 1) { t = 1; s = clamp01((b - c) / a); }
		}
	}
	D3 c1 = { p1[0] + d1[0] * s, p1[1] + d1[1] * s, p1[2] + d1[2] * s };
	D3 c2 = { p2[0] + d2[0] * t, p2[1] + d2[1] * t, p2[2] + d2[2] * t };
	D3 d = sub3(c1, c2);
	return dot3(d, d);
}

static bool segHitsTri(const D3& p, const D3& q, const D3& a, const D3& b, const D3& c) {
	D3 n = cross3(sub3(b, a), sub3(c, a));
	double dp = dot3(n, sub3(p, a)), dq = dot3(n, sub3(q, a));
	if ((dp > 0 && dq > 0) || (dp < 0 && dq < 0) || dp == dq) return false;
	double t = dp / (dp - dq);
	D3 x = { p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t, p[2] + (q[2] - p[2]) * t };
	double s1 = dot3(n, cross3(sub3(b, a), sub3(x, a)));
	double s2 = dot3(n, cross3(sub3(c, b), sub3(x, b)));
	double s3 = dot3(n, cross3(sub3(a, c), sub3(x, c)));
	return (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0);
}

static double triTriDist(const Poly& A, const Poly& B) {
	D3 ta[3] = { { A.ax, A.ay, A.az }, { A.bx, A.by, A.bz }, { A.cx, A.cy, A.cz } };
	D3 tb[3] = { { B.ax, B.ay, B.az }, { B.bx, B.by, B.bz }, { B.cx, B.cy, B.cz } };
	for (int i = 0; i < 3; i++) {
		if (segHitsTri(ta[i], ta[(i + 1) % 3], tb[0], tb[1], tb[2])) return 0;
		if (segHitsTri(tb[i], tb[(i + 1) % 3], ta[0], ta[1], ta[2])) return 0;
	}
	double best = INFINITY;
	for (int k = 0; k < 3; k++) {
		D3 d = sub3(ta[k], closestOnTri(ta[k], tb[0], tb[1], tb[2]));
		best = std::min(best, dot3(d, d));
		d = sub3(tb[k], closestOnTri(tb[k], ta[0], ta[1], ta[2]));
		best = std::min(best, dot3(d, d));
	}
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			best = std::min(best, segSegDistSq(ta[i], ta[(i + 1) % 3], tb[j], tb[(j + 1) % 3]));
	return std::sqrt(best);
}

static vector<Pair> wallPairCandidates(const Model& m) {
	vector<Pair> pairs;
	std::unordered_set<int64_t> seen;
	const double R = m.radius, E = R + REACH;
	const double reach = 2 * m.radius + 30; // wall_push_clips.js pairReach
	for (const auto& sub : m.colCtx.subWalls) {
		if (sub.size() < 2) continue;
		vector<int> walls;
		for (int id : sub) { const Poly& p = m.polys[id]; if (p.exists && p.isWall && p.nXZ > 0) walls.push_back(id); }
		for (size_t i = 0; i < walls.size(); i++) {
			for (size_t j = 0; j < walls.size(); j++) {
				if (i == j) continue;
				const Poly& A = m.polys[walls[i]];
				const Poly& B = m.polys[walls[j]];
				int64_t key = (int64_t)A.id * 65536 + B.id;
				if (seen.count(key)) continue;
				double cosAB = (A.nx * B.nx + A.nz * B.nz) * A.invNXZ * B.invNXZ;
				if (cosAB > -0.02) continue;
				double lo = std::max(A.minY, B.minY) - 1, hi = std::min(A.maxY, B.maxY) + 1;
				if (hi < lo) continue;
				double x0 = std::max(A.minX, B.minX) - E, x1 = std::min(A.maxX, B.maxX) + E;
				double z0 = std::max(A.minZ, B.minZ) - E, z1 = std::min(A.maxZ, B.maxZ) + E;
				if (x1 < x0 || z1 < z0) continue;
				seen.insert(key);
				if (triTriDist(A, B) > reach) continue;
				pairs.push_back({ A.id, B.id, cosAB, lo, hi, x0, x1, z0, z1 });
			}
		}
	}
	return pairs;
}

struct NextPos { V3 p; double lo, hi; };

// Where walls A and B's planes meet (top down) at check height h, offset to
// signed distances a from A and b from B. False for walls under ~3 degrees apart.
static bool planesMeet(const Poly& A, const Poly& B, double h, double a, double b, double& x, double& z) {
	double det = A.nx * B.nz - A.nz * B.nx;
	if (std::fabs(det) < 0.05 * A.nXZ * B.nXZ) return false;
	double ra = a * A.nMag - A.ny * h - A.dist, rb = b * B.nMag - B.ny * h - B.dist;
	x = (ra * B.nz - A.nz * rb) / det;
	z = (A.nx * rb - ra * B.nx) / det;
	return true;
}

// wall_push_clips.js cornerBox: the bounding box of the parallelogram (within
// radius in front of A or 4 behind it, at most radius + 4 in front of B)
// around where the planes meet, over check heights [lo, hi], plus a unit.
static bool cornerBox(const Poly& A, const Poly& B, double R, double lo, double hi, double box[4]) {
	double x0 = INFINITY, x1 = -INFINITY, z0 = INFINITY, z1 = -INFINITY;
	for (double h : { lo, hi })
		for (double a : { -(4 / A.nXZ) - 1, R })
			for (double b : { 0.0, R + 4 }) {
				double x, z;
				if (!planesMeet(A, B, h, a, b, x, z)) return false;
				x0 = std::min(x0, x); x1 = std::max(x1, x);
				z0 = std::min(z0, z); z1 = std::max(z1, z);
			}
	box[0] = x0 - 1; box[1] = x1 + 1; box[2] = z0 - 1; box[3] = z1 + 1;
	return true;
}

static void nextPositionsForPair(const Model& m, Scratch& s, const Pair& pair, const std::function<void(const NextPos&)>& yield) {
	const Poly& A = m.polys[pair.A];
	const Poly& B = m.polys[pair.B];
	const double R = m.radius, ch = m.checkHeight, lo = pair.lo, hi = pair.hi, cosAB = pair.cosAB;
	double x0 = std::max(pair.x0, std::min(A.minX, B.minX) - R), x1 = std::min(pair.x1, std::max(A.maxX, B.maxX) + R);
	double z0 = std::max(pair.z0, std::min(A.minZ, B.minZ) - R), z1 = std::min(pair.z1, std::max(A.maxZ, B.maxZ) + R);
	double cb[4];
	if (cornerBox(A, B, R, lo, hi, cb)) {
		x0 = std::max(x0, cb[0]); x1 = std::min(x1, cb[1]);
		z0 = std::max(z0, cb[2]); z1 = std::min(z1, cb[3]);
		if (x1 < x0 || z1 < z0) return;
	}
	double step = NEXT_STEP;
	while (((x1 - x0) / step) * ((z1 - z0) / step) > 40000) step *= 1.25;
	double x = 0, z = 0;
	auto reachable = [&](double h) {
		double dA = planeDist(A, x, h, z);
		if (dA > R || dA < -(4 / A.nXZ) - 1) return false;
		double dB = planeDist(B, x, h, z);
		if (dB < 0 || dB > R + 4) return false;
		if (dB + (R - dA) * cosAB > -3) return false;
		// near the triangles themselves, not just their planes (slack: the
		// extended plane and pushes from walls before A in the list)
		const double slack = R;
		double kA = dA / A.nMag;
		if (!pointInTri3D(A, x - kA * A.nx, h - kA * A.ny, z - kA * A.nz, slack)) return false;
		double disp = (R - dA) * A.invNXZ;
		double qx = x + disp * A.nx, qz = z + disp * A.nz;
		double dBq = planeDist(B, qx, h, qz);
		if (dBq >= 0) return true;
		double t = dB / (dB - dBq);
		return pointInTri3D(B, x + (qx - x) * t, h, z + (qz - z) * t, slack);
	};
	double aX = A.nx * A.invNXZ, aZ = A.nz * A.invNXZ, bX = B.nx * B.invNXZ, bZ = B.nz * B.invNXZ;
	double mLen = std::hypot(aX + bX, aZ + bZ);
	if (mLen == 0) mLen = 1;
	const double outDirs[3][2] = { { aX, aZ }, { bX, bZ }, { (aX + bX) / mLen, (aZ + bZ) / mLen } };
	auto anyHeight = [&]() {
		double hMin = lo, hMax = hi;
		double dA0 = planeDist(A, x, 0, z), dA1 = planeDist(A, x, 1, z) - dA0;
		double dB0 = planeDist(B, x, 0, z), dB1 = planeDist(B, x, 1, z) - dB0;
		auto le = [&](double k, double mm) {
			if (std::fabs(mm) < 1e-9) { if (k > 0) hMax = -INFINITY; return; }
			double root = -k / mm;
			if (mm > 0) hMax = std::min(hMax, root); else hMin = std::max(hMin, root);
		};
		le(dA0 - R, dA1);
		le(-dA0 - (4 / A.nXZ) - 1, -dA1);
		le(-dB0, -dB1);
		le(dB0 - R - 4, dB1);
		le(dB0 + (R - dA0) * cosAB + 3, dB1 - dA1 * cosAB);
		return hMin <= hMax;
	};
	vector<double> ys;
	for (x = std::ceil(x0 / step) * step; x <= x1; x += step) {
		for (z = std::ceil(z0 / step) * step; z <= z1; z += step) {
			if (!anyHeight()) continue;
			// floorsNear() of the JS: a Set in insertion order
			ys.clear();
			auto add = [&](double y) { if (std::find(ys.begin(), ys.end(), y) == ys.end()) ys.push_back(y); };
			for (double y : m.floorsAt(x, z)) add(y);
			for (double d : { 8.0, 16.0, 24.0 })
				for (const auto& od : outDirs)
					for (double y : m.floorsNear(s, x + d * od[0], z + d * od[1])) add(y);
			for (double fy : ys) {
				double h = fy - GROUND_DROP + ch;
				if (h < lo || h > hi + m.lowDrop) continue;
				double hTop = std::min(h, hi), hLow = std::max(lo, h - m.lowDrop);
				if (!reachable(hTop) && !(m.lowDrop && (reachable(hLow) || reachable((hTop + hLow) / 2)))) continue;
				yield({ { F(x), fy, F(z) }, lo, hi });
			}
		}
	}
}

struct CrossPoint { V3 p; double floorY; int drop; double spotU, spotY; };

static void crossingPointsForWall(const Model& m, Scratch& s, const Poly& A, const vector<const Pair*>& pairsA,
	const std::function<void(const CrossPoint&)>& yield) {
	const double ch = m.checkHeight;
	double nx = A.nx * A.invNXZ, nz = A.nz * A.invNXZ;
	double tx = -nz, tz = nx;
	double us[3] = { A.ax * tx + A.az * tz, A.bx * tx + A.bz * tz, A.cx * tx + A.cz * tz };
	double u0 = std::min({ us[0], us[1], us[2] }) - 2, u1 = std::max({ us[0], us[1], us[2] }) + 2;
	vector<std::pair<double, double>> nearR;
	for (const Pair* pr : pairsA) {
		const Poly& B = m.polys[pr->B];
		double bu[3] = { B.ax * tx + B.az * tz, B.bx * tx + B.bz * tz, B.cx * tx + B.cz * tz };
		double pad = m.radius + 30;
		double a = std::min({ bu[0], bu[1], bu[2] }) - pad, b = std::max({ bu[0], bu[1], bu[2] }) + pad;
		// Snapped radius in front of A, Link is only behind B near where the
		// planes meet: within (5 radius + 25) / sin(angle) of it along A.
		double xl, zl, xh, zh;
		if (planesMeet(A, B, pr->lo, 0, 0, xl, zl) && planesMeet(A, B, pr->hi, 0, 0, xh, zh)) {
			double sinAB = std::fabs(A.nx * B.nz - A.nz * B.nx) / (A.nXZ * B.nXZ);
			double w = (5 * m.radius + 25) / sinAB;
			double ul = xl * tx + zl * tz, uh = xh * tx + zh * tz;
			a = std::max(a, std::min(ul, uh) - w);
			b = std::min(b, std::max(ul, uh) + w);
			if (b < a) continue;
		}
		nearR.push_back({ a, b });
	}
	auto isNear = [&](double u) { for (auto& r : nearR) if (u >= r.first && u <= r.second) return true; return false; };
	auto onPlane = [&](double u, double h) {
		double c = -(A.dist + A.ny * h) / (A.nXZ * A.nXZ);
		return std::pair<double, double>(c * A.nx + u * tx, c * A.nz + u * tz);
	};
	const double c45 = SQRT1_2;
	const double outDirs[3][2] = { { nx, nz }, { (nx - nz) * c45, (nz + nx) * c45 }, { (nx + nz) * c45, (nz - nx) * c45 } };
	auto floorsBeside = [&](std::pair<double, double> q) {
		vector<double> out;
		for (double side : { -12.0, -2.0, 4.0, 14.0, 28.0 })
			for (const auto& od : outDirs)
				for (double y : m.floorsNear(s, q.first + side * od[0], q.second + side * od[1]))
					if (std::find(out.begin(), out.end(), y) == out.end()) out.push_back(y);
		return out;
	};
	const double seedHeights[3] = { A.minY, (A.minY + A.maxY) / 2, A.maxY };
	auto floorsFor = [&](double u) {
		// floorsBeside of a position already looked up in this block (on a
		// vertical wall the plane is in the same place at every height)
		vector<std::tuple<double, double, vector<double>>> memo;
		auto beside = [&](std::pair<double, double> q) {
			for (auto& e : memo) if (std::get<0>(e) == q.first && std::get<1>(e) == q.second) return std::get<2>(e);
			memo.emplace_back(q.first, q.second, floorsBeside(q));
			return std::get<2>(memo.back());
		};
		vector<double> ys, seeds;
		for (double hs : seedHeights)
			for (double y : beside(onPlane(u, hs)))
				if (std::find(seeds.begin(), seeds.end(), y) == seeds.end()) seeds.push_back(y);
		auto same = [&](double y) { for (double v : ys) if (std::fabs(v - y) < 0.5) return true; return false; };
		for (double y0 : seeds) {
			for (double y : beside(onPlane(u, y0 + ch))) {
				double h = y + ch;
				if (h - std::max((double)m.lowDrop, GROUND_DROP) > A.maxY + 1 || h < A.minY - 1 || same(y)) continue;
				ys.push_back(y);
			}
		}
		return ys;
	};
	bool haveBlock = false;
	double block = 0;
	vector<double> blockYs;
	int ui = 0;
	for (double u = u0; u <= u1; ui++, u += CROSS_STEP) {
		if (!isNear(u)) continue;
		double b = std::floor(u / FLOOR_BLOCK);
		if (!haveBlock || b != block) {
			haveBlock = true;
			block = b;
			blockYs = floorsFor((b + 0.5) * FLOOR_BLOCK);
		}
		for (double y : blockYs) {
			for (int drop = 0; drop <= m.lowDrop; drop += drop == 0 ? 2 : 4) {
				if (drop > 0 && (ui % 2)) break;
				// (walking, posNext is GROUND_DROP below the floor)
				double low = F(y - (drop ? drop : GROUND_DROP));
				double h = F(low + ch);
				if (h < A.minY - 1 || h > A.maxY + 1) continue;
				auto i = onPlane(u, h);
				yield({ { F(i.first), low, F(i.second) }, y, drop, u, y });
			}
		}
	}
}

struct LineFrameR { Hit hit; V3 res; PushList trace; };

static std::optional<LineFrameR> lineFrame(const Model& m, Scratch& s, const V3& prev, const V3& next, const Tol& tol) {
	double h = F(next.y + m.checkHeight);
	// one face only, and floors too when moving more than the radius
	double dx = F(next.x - prev.x), dz = F(next.z - prev.z);
	bool floors = sq(m.radius) < F(sq(dx) + sq(dz));
	auto hit = m.lineHit(s, { prev.x, h, prev.z }, { next.x, h, next.z }, tol, floors, true);
	if (!hit || isZero(m.polys[hit->poly].nXZ)) return std::nullopt;
	const Poly& P = m.polys[hit->poly];
	double k = F(m.radius * F(1 / P.nXZ));
	V3 snapped = { F(F(k * P.nx) + hit->x), next.y, F(F(k * P.nz) + hit->z) };
	LineFrameR r;
	r.hit = *hit;
	r.trace.push_back({ hit->poly, next, snapped, true });
	r.res = m.sphereStep(snapped, tol, &r.trace);
	return r;
}

static std::optional<V3> landing(const Model& m, Scratch& s, const V3& res, double floorY, bool& noFloor) {
	noFloor = false;
	double land = -INFINITY;
	bool any = false;
	for (double y : m.floorsAt(res.x, res.z)) if (y <= floorY + 50 && (!any || y > land)) { land = y; any = true; }
	if (!any) { noFloor = true; return res; }
	V3 end = m.sphereStep(m.sphereStep({ res.x, land, res.z }, LOOSE, nullptr), LOOSE, nullptr);
	if (m.isInBounds(s, end)) return std::nullopt;
	return end;
}

// The highest floor within 10 of ref, then the top one of the floors at most
// 3 above that (wall_push_clips.js standSpot's floorAt).
static std::optional<double> standFloor(const Model& m, double x, double z, double ref) {
	FloorList ys = m.floorsAt(x, z);
	std::optional<double> best;
	for (double y : ys) if (std::fabs(y - ref) <= 10 && (!best || y > *best)) best = y;
	if (!best) return best;
	double top = -INFINITY;
	for (double v : ys) if (v <= *best + 3 && v > top) top = v;
	return top;
}

// wall_push_clips.js standSpot: where Link can stand still near (x, z).
static std::optional<V3> standSpot(const Model& m, double x, double z, double floorY) {
	auto y = standFloor(m, x, z, floorY);
	if (!y) return std::nullopt;
	double cy = *y;
	for (int i = 0; i < 3; i++) {
		auto rest = m.restingSpot({ x, cy, z });
		if (!rest) return std::nullopt;
		auto ry = standFloor(m, rest->x, rest->z, floorY);
		if (!ry) return std::nullopt;
		if (rest->x == x && rest->z == z && *ry == cy) return rest;
		x = rest->x; z = rest->z; cy = *ry;
	}
	return std::nullopt;
}

struct Clip {
	int kind = 0; // 0 acute, 1 extended, 2 low
	bool cross = false;
	int drop = 0;
	int pusher = -1, crossed = -1;
	V3 from, prev, next, res, end;
	bool hasNext = false, hasFloorY = false, endNoFloor = false;
	double floorY = 0;
	vector<int> yaws;
	int yaw = 0;          // crossings: the exact move (s16 yaw, f32 speed)
	double speed = 0;
};

struct CrossFound { ClipResult clip; V3 prev, next, res, at; bool noFloor = false; int yaw = 0; double speed = 0; };

static std::optional<std::pair<CrossFound, vector<int>>> crossingClip(const Model& m, Scratch& s, const Poly& A,
	const CrossPoint& cp, const Tol& tol) {
	vector<int> yaws;
	std::optional<CrossFound> first;
	std::set<std::pair<double, double>> tried;
	for (int i = 0; i < 32; i++) {
		int yaw0 = i * 0x800;
		double dx0 = std::sin(yaw0 / 65536.0 * 2 * PI), dz0 = std::cos(yaw0 / 65536.0 * 2 * PI);
		if (std::fabs(dx0 * A.nx + dz0 * A.nz) * A.invNXZ < 0.1) continue;
		std::optional<CrossFound> found;
		for (double dist : MOVE_STEPS) {
			// standing still at the start, moving from there through the point
			auto prevO = standSpot(m, F(cp.p.x - dist * dx0), F(cp.p.z - dist * dz0), cp.floorY);
			if (!prevO) continue;
			V3 prev = *prevO;
			if (!tried.insert({ prev.x, prev.z }).second) continue;
			double vx = cp.p.x - prev.x, vz = cp.p.z - prev.z, len = std::hypot(vx, vz);
			if (len < 0.5) continue;
			double dx = vx / len, dz = vz / len;
			if (std::fabs(dx * A.nx + dz * A.nz) * A.invNXZ < 0.1) continue;
			// the game's move: s16 yaw, speed 1 unit past the point, sine table
			int yaw = yawOf(vx, vz);
			double speed = F((len + 1) / SPEED_RATE);
			V3 next = moveStep(prev, yaw, speed);
			if (cp.drop > 0) next.y = cp.p.y;
			auto f = lineFrame(m, s, prev, next, tol);
			if (!f || f->hit.poly != A.id) continue;
			auto clip = clipFromFrame(m, s, prev, f->res, f->trace, tol);
			if (!clip || !m.isInBounds(s, prev)) continue;
			bool noFloor = false;
			if (cp.drop > 0) {
				auto end = landing(m, s, f->res, cp.floorY, noFloor);
				if (!end) continue;
				clip->end = *end;
			} else if (m.isInBounds(s, clip->end)) {
				continue;
			}
			found = CrossFound{ *clip, prev, next, f->res, { f->hit.x, next.y, f->hit.z }, noFloor, yaw, speed };
			break;
		}
		if (!found) continue;
		if (std::find(yaws.begin(), yaws.end(), found->yaw) == yaws.end()) yaws.push_back(found->yaw);
		if (!first) first = found;
	}
	if (!first) return std::nullopt;
	return std::make_pair(*first, yaws);
}

static std::optional<V3> reachFrom(const Model& m, Scratch& s, const V3& p, double floorY) {
	double h = F(p.y + m.checkHeight);
	for (double dist : MOVE_STEPS) {
		for (int i = 0; i < 16; i++) {
			double ang = i / 16.0 * 2 * PI;
			auto prevO = standSpot(m, F(p.x - dist * std::sin(ang)), F(p.z - dist * std::cos(ang)), floorY);
			if (!prevO) continue;
			V3 prev = *prevO;
			double x = prev.x, z = prev.z;
			if (std::hypot(p.x - x, p.z - z) > 30) continue; // REACH_DIST
			if (m.lineHit(s, { x, h, z }, { p.x, h, p.z }, LOOSE, false, true)) continue;
			if (m.isInBounds(s, prev)) return prev;
		}
	}
	return std::nullopt;
}

static std::optional<Clip> standingClip(const Model& m, Scratch& s, const V3& floorPt) {
	const V3 p = { floorPt.x, F(floorPt.y - GROUND_DROP), floorPt.z };
	PushList trace;
	V3 res = m.sphereStep(p, LOOSE, &trace);
	if (trace.empty()) return std::nullopt;
	auto clip = clipFromFrame(m, s, p, res, trace, LOOSE);
	if (!clip) return std::nullopt;
	if (!m.isInBounds(s, floorPt) || m.isInBounds(s, clip->end)) return std::nullopt;
	auto prev = reachFrom(m, s, p, floorPt.y);
	if (!prev) return std::nullopt;
	PushList st;
	V3 sres = m.sphereStep(p, STRICT, &st);
	bool strict = (bool)clipFromFrame(m, s, p, sres, st, STRICT);
	Clip c;
	c.kind = strict ? 0 : 1;
	c.from = p; c.floorY = floorPt.y; c.hasFloorY = true; c.prev = *prev; c.res = res; c.end = clip->end;
	c.crossed = clip->crossed; c.pusher = clip->pusher;
	return c;
}

static std::optional<Clip> lowClip(const Model& m, Scratch& s, const V3& p, int drop) {
	V3 low = { p.x, F(p.y - drop), p.z };
	PushList trace;
	V3 res = m.sphereStep(low, LOOSE, &trace);
	if (trace.empty()) return std::nullopt;
	auto clip = clipFromFrame(m, s, low, res, trace, LOOSE);
	if (!clip) return std::nullopt;
	if (!m.isInBounds(s, p)) return std::nullopt;
	bool noFloor = false;
	auto end = landing(m, s, res, p.y, noFloor);
	if (!end) return std::nullopt;
	auto prev = reachFrom(m, s, low, p.y);
	if (!prev) return std::nullopt;
	Clip c;
	c.kind = 2; c.drop = drop;
	c.from = low; c.floorY = p.y; c.hasFloorY = true; c.prev = *prev; c.res = res; c.end = *end; c.endNoFloor = noFloor;
	c.crossed = clip->crossed; c.pusher = clip->pusher;
	return c;
}

// A point (and falling drop) seen by any thread, sharded to keep lock
// contention down.
struct SeenKey {
	double x, z, y;
	int drop;
	bool operator==(const SeenKey& o) const { return x == o.x && z == o.z && y == o.y && drop == o.drop; }
};
struct SeenHash {
	size_t operator()(const SeenKey& k) const {
		uint64_t h = 1469598103934665603ull;
		for (double v : { k.x, k.z, k.y }) {
			uint64_t b;
			memcpy(&b, &v, 8);
			h = (h ^ b) * 1099511628211ull;
		}
		return (size_t)(h ^ (uint64_t)k.drop * 0x9E3779B97F4A7C15ull);
	}
};
struct SharedSet {
	static const int N = 64;
	std::mutex mu[N];
	std::unordered_set<SeenKey, SeenHash> sets[N];
	bool insert(const SeenKey& k) {
		size_t h = SeenHash{}(k) % N;
		std::lock_guard<std::mutex> g(mu[h]);
		return sets[h].insert(k).second;
	}
};

static string keyOf(double a, double b, double c) {
	char buf[96];
	snprintf(buf, sizeof buf, "%.9g,%.9g,%.9g", a, b, c);
	return buf;
}

static vector<Clip> scan(const Model& m, int threads) {
	auto t0 = std::chrono::steady_clock::now();
	vector<Pair> pairs = wallPairCandidates(m);
	SharedSet seen;
	std::mutex outMu;
	vector<Clip> clips;
	std::atomic<size_t> nextPair{ 0 }, pairsDone{ 0 };

	auto progress = [&](const char* phase, size_t done, size_t total) {
		static std::mutex pm;
		static auto last = std::chrono::steady_clock::now();
		std::lock_guard<std::mutex> g(pm);
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration<double>(now - last).count() < 0.5 && done != total) return;
		last = now;
		fprintf(stderr, "\r  %s %zu / %zu (%.0fs)   ", phase, done, total, std::chrono::duration<double>(now - t0).count());
	};

	auto worker1 = [&]() {
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		vector<Clip> local;
		for (;;) {
			size_t pi = nextPair++;
			if (pi >= pairs.size()) break;
			s.clearCache();
			nextPositionsForPair(m, s, pairs[pi], [&](const NextPos& np) {
				const V3& p = np.p;
				double h = p.y + m.checkHeight;
				if (h - GROUND_DROP >= np.lo && h - GROUND_DROP <= np.hi && seen.insert({ p.x, p.z, p.y, 0 })) {
					if (auto c = standingClip(m, s, p)) { local.push_back(*c); return; }
				}
				for (int k = 2; k <= m.lowDrop; k += 2) {
					double hk = h - k;
					if (hk < np.lo || hk > np.hi) continue;
					if (!seen.insert({ p.x, p.z, p.y, k })) continue;
					if (auto c = lowClip(m, s, p, k)) { local.push_back(*c); break; }
				}
			});
			progress("wall pairs", ++pairsDone, pairs.size());
		}
		std::lock_guard<std::mutex> g(outMu);
		clips.insert(clips.end(), local.begin(), local.end());
	};

	// Pushers and the walls each pushes against.
	std::map<int, vector<int>> partnersOf;
	std::map<int, vector<const Pair*>> pairsOf;
	vector<int> pushers;
	for (const Pair& p : pairs) {
		if (!partnersOf.count(p.A)) pushers.push_back(p.A);
		partnersOf[p.A].push_back(p.B);
		pairsOf[p.A].push_back(&p);
	}
	std::atomic<size_t> nextPusher{ 0 }, pushersDone{ 0 };

	auto worker2 = [&]() {
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		vector<Clip> local;
		for (;;) {
			size_t ai = nextPusher++;
			if (ai >= pushers.size()) break;
			s.clearCache();
			const Poly& A = m.polys[pushers[ai]];
			const vector<int>& partners = partnersOf[A.id];
			double k = F(m.radius * F(1 / A.nXZ));
			std::set<std::pair<double, double>> done;
			// one point per (start, move) frame (JS crossFrames; the frame's
			// line check has to hit A, so it can only come from this pusher)
			std::set<std::array<double, 6>> frames;
			crossingPointsForWall(m, s, A, pairsOf[A.id], [&](const CrossPoint& cp) {
				if (done.count({ cp.spotU, cp.spotY })) return;
				V3 snapped = { F(F(k * A.nx) + cp.p.x), cp.p.y, F(F(k * A.nz) + cp.p.z) };
				V3 res = m.sphereStep(snapped, LOOSE, nullptr);
				double h = cp.p.y + m.checkHeight;
				bool behind = false;
				for (int bid : partners) {
					const Poly& B = m.polys[bid];
					double d = planeDist(B, res.x, h, res.z);
					if (d >= -3.5 || d < -4 * m.radius) continue;
					double t = d / B.nMag;
					if (pointInTri3D(B, res.x - t * B.nx, h - t * B.ny, res.z - t * B.nz, 1)) { behind = true; break; }
				}
				if (!behind) return;
				double nx = A.nx * A.invNXZ, nz = A.nz * A.invNXZ;
				bool anyIn = false;
				for (double sd : { 3.0, -3.0, 12.0, -12.0 })
					if (m.isInBounds(s, { cp.p.x + sd * nx, cp.floorY, cp.p.z + sd * nz })) { anyIn = true; break; }
				if (!anyIn) return;
				auto r = crossingClip(m, s, A, cp, LOOSE);
				if (!r) return;
				done.insert({ cp.spotU, cp.spotY });
				const CrossFound& f0 = r->first;
				if (!frames.insert({ f0.prev.x, f0.prev.y, f0.prev.z, f0.next.x, f0.next.y, f0.next.z }).second) return;
				bool strict = cp.drop == 0 && (bool)crossingClip(m, s, A, cp, STRICT);
				const CrossFound& f = r->first;
				Clip c;
				c.kind = cp.drop > 0 ? 2 : strict ? 0 : 1;
				c.cross = true; c.drop = cp.drop;
				c.from = f.at; c.floorY = cp.floorY; c.hasFloorY = true;
				c.prev = f.prev; c.next = f.next; c.hasNext = true; c.res = f.res; c.end = f.clip.end; c.endNoFloor = f.noFloor;
				c.yaws = r->second;
				c.yaw = f.yaw; c.speed = f.speed;
				c.crossed = f.clip.crossed; c.pusher = A.id;
				local.push_back(c);
			});
			progress("crossing walls", ++pushersDone, pushers.size());
		}
		std::lock_guard<std::mutex> g(outMu);
		clips.insert(clips.end(), local.begin(), local.end());
	};

	fprintf(stderr, "  %zu wall pairs, %zu pushing walls, %d threads\n", pairs.size(), pushers.size(), threads);
	{
		vector<std::thread> ts;
		for (int i = 0; i < threads; i++) ts.emplace_back(worker1);
		for (auto& t : ts) t.join();
	}
	fprintf(stderr, "\n  standing points: %.1fs\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
	{
		vector<std::thread> ts;
		for (int i = 0; i < threads; i++) ts.emplace_back(worker2);
		for (auto& t : ts) t.join();
	}
	fprintf(stderr, "\n");
	// Deterministic order: standing points first, then crossings, by position.
	std::sort(clips.begin(), clips.end(), [](const Clip& a, const Clip& b) {
		if (a.cross != b.cross) return !a.cross;
		if (a.pusher != b.pusher) return a.pusher < b.pusher;
		if (a.from.x != b.from.x) return a.from.x < b.from.x;
		if (a.from.z != b.from.z) return a.from.z < b.from.z;
		if (a.from.y != b.from.y) return a.from.y < b.from.y;
		if (a.drop != b.drop) return a.drop < b.drop;
		if (a.crossed != b.crossed) return a.crossed < b.crossed;
		if (a.floorY != b.floorY) return a.floorY < b.floorY;
		if (a.prev.x != b.prev.x) return a.prev.x < b.prev.x;
		if (a.prev.z != b.prev.z) return a.prev.z < b.prev.z;
		return a.kind < b.kind;
	});
	// Low standing points can be found through more than one wall pair (the
	// JS keeps the first); keep one per position.
	vector<Clip> out;
	std::unordered_set<string> keep;
	for (const Clip& c : clips) {
		string k = (c.cross ? "c" : "s") + std::to_string(c.pusher) + ":" + keyOf(c.from.x, c.from.z, c.hasFloorY ? c.floorY : c.from.y);
		if (!c.cross && !keep.insert(k).second) continue;
		out.push_back(c);
	}
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	fprintf(stderr, "  %zu clip points in %.1fs\n", out.size(), secs);
	return out;
}

////////////////////////////////////////
// Output
////////////////////////////////////////

static string num(double v) {
	char buf[40];
	// %.9g round-trips any f32
	snprintf(buf, sizeof buf, "%.9g", v);
	return buf;
}
static string vec(const V3& v) { return "[" + num(v.x) + "," + num(v.y) + "," + num(v.z) + "]"; }
static string jsonStr(const string& s) {
	string o = "\"";
	for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
	return o + "\"";
}

static string toJson(const string& game, const string& map, const string& form, const Model& m, int numPolygons,
	bool falling, const vector<Clip>& clips) {
	static const char* kinds[] = { "acute", "extended", "low" };
	std::ostringstream o;
	o << "{\n  \"format\": \"wall-push-clips-1\",\n";
	o << "  \"game\": " << jsonStr(game) << ", \"map\": " << jsonStr(map) << ", \"form\": " << jsonStr(form) << ",\n";
	o << "  \"radius\": " << num(m.radius) << ", \"checkHeight\": " << num(m.checkHeight)
		<< ", \"falling\": " << (falling ? "true" : "false") << ", \"numPolygons\": " << numPolygons << ",\n";
	o << "  \"clips\": [";
	for (size_t i = 0; i < clips.size(); i++) {
		const Clip& c = clips[i];
		o << (i ? ",\n    " : "\n    ");
		o << "{\"kind\":\"" << kinds[c.kind] << "\",\"cross\":" << (c.cross ? "true" : "false")
			<< ",\"drop\":" << c.drop << ",\"pusher\":" << c.pusher << ",\"crossed\":" << c.crossed
			<< ",\"from\":" << vec(c.from) << ",\"prev\":" << vec(c.prev);
		if (c.hasNext) o << ",\"next\":" << vec(c.next);
		o << ",\"res\":" << vec(c.res) << ",\"end\":" << vec(c.end);
		if (c.endNoFloor) o << ",\"endNoFloor\":true";
		if (c.hasFloorY) o << ",\"floorY\":" << num(c.floorY);
		if (c.cross) {
			o << ",\"yaws\":[";
			for (size_t k = 0; k < c.yaws.size(); k++) o << (k ? "," : "") << c.yaws[k];
			o << "],\"yaw\":" << c.yaw << ",\"speed\":" << num(c.speed);
		}
		o << "}";
	}
	o << "\n  ]\n}\n";
	return o.str();
}

////////////////////////////////////////
// Main
////////////////////////////////////////

struct MapEntry { string name, file; };

static vector<MapEntry> readMapList(const string& root, const string& game) {
	std::ifstream f(root + "/js/model_list.js");
	std::stringstream ss;
	ss << f.rdbuf();
	string text = ss.str();
	string head = "const " + game + "_Maps = [";
	size_t a = text.find(head);
	vector<MapEntry> out;
	if (a == string::npos) return out;
	size_t b = text.find("];", a);
	string body = text.substr(a, b - a);
	std::regex re("\\{\\s*name:\\s*\"([^\"]*)\",\\s*file:\\s*\"([^\"]*)\"");
	for (auto it = std::sregex_iterator(body.begin(), body.end(), re); it != std::sregex_iterator(); ++it)
		out.push_back({ (*it)[1], (*it)[2] });
	return out;
}

static bool readFile(const string& path, vector<uint8_t>& out) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

static bool exists(const string& p) { std::ifstream f(p); return (bool)f; }

static string safeName(const string& s) {
	string o;
	for (char c : s) o += (isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
	return o;
}

int main(int argc, char** argv) {
	string game, mapName, form, out, outDir, root;
	double radius = 0;
	bool falling = false, all = false;
	int threads = (int)std::max(1u, std::thread::hardware_concurrency());
	for (int i = 1; i < argc; i++) {
		string a = argv[i];
		auto val = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(2); } return string(argv[++i]); };
		if (a == "--game") game = val();
		else if (a == "--map") mapName = val();
		else if (a == "--form") form = val();
		else if (a == "--radius") radius = std::stod(val());
		else if (a == "--falling") falling = true;
		else if (a == "--all") all = true;
		else if (a == "-o" || a == "--out") out = val();
		else if (a == "--out-dir") outDir = val();
		else if (a == "--root") root = val();
		else if (a == "--threads") threads = std::max(1, std::stoi(val()));
		else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
	}
	for (auto& ch : game) ch = (char)toupper((unsigned char)ch);
	if ((game != "OOT" && game != "MM") || (mapName.empty() && !all)) {
		fprintf(stderr,
			"usage: clipfinder --game OOT|MM (--map \"<name in the viewer's map list>\" | --all)\n"
			"                  [--form Adult|Child|Human|Deku|Zora|Goron|FierceDeity] [--radius R] [--falling]\n"
			"                  [-o out.json | --out-dir dir] [--root viewer_dir] [--threads N]\n");
		return 2;
	}
	// ageProperties->wallCheckRadius (z_player.c)
	static const std::map<string, double> radii = {
		{ "ADULT", 18 }, { "CHILD", 14 }, { "HUMAN", 14 }, { "DEKU", 14 }, { "ZORA", 18 }, { "GORON", 19.5 },
		{ "FIERCEDEITY", 27 }, { "FIERCE_DEITY", 27 }, { "FD", 27 },
	};
	if (form.empty()) form = game == "OOT" ? "Adult" : "Human";
	if (radius == 0) {
		string f = form;
		for (auto& ch : f) ch = (char)toupper((unsigned char)ch);
		if (!radii.count(f)) { fprintf(stderr, "unknown form %s (or pass --radius)\n", form.c_str()); return 2; }
		radius = radii.at(f);
	}
	if (root.empty()) {
		string exe = argv[0];
		size_t sl = exe.find_last_of("/\\");
		string dir = sl == string::npos ? "." : exe.substr(0, sl);
		root = exists("js/model_list.js") ? "." : dir + "/../..";
	}
	vector<MapEntry> maps = readMapList(root, game);
	if (maps.empty()) { fprintf(stderr, "no %s maps found in %s/js/model_list.js (use --root)\n", game.c_str(), root.c_str()); return 1; }
	vector<MapEntry> todo;
	for (const MapEntry& e : maps) if (all || e.name == mapName) todo.push_back(e);
	if (todo.empty()) { fprintf(stderr, "no map named \"%s\" in the %s list\n", mapName.c_str(), game.c_str()); return 1; }

	const double checkHeight = game == "OOT" ? 26.0 : F(F(268 * F(0.1)));
	int failures = 0;
	for (const MapEntry& e : todo) {
		fprintf(stderr, "%s - %s (%s, radius %g%s)\n", game.c_str(), e.name.c_str(), form.c_str(), radius, falling ? ", falling" : "");
		vector<uint8_t> buf;
		if (!readFile(root + "/models/" + game + "/" + e.file, buf)) { fprintf(stderr, "  can't read models/%s/%s\n", game.c_str(), e.file.c_str()); failures++; continue; }
		ColHeader ch;
		vector<Tri> tris;
		try {
			if (!parseScene(buf, game, ch, tris)) { fprintf(stderr, "  no collision header\n"); failures++; continue; }
		} catch (const std::exception& ex) { fprintf(stderr, "  bad scene file: %s\n", ex.what()); failures++; continue; }
		Model m;
		initColCtx(m.colCtx, game, e.name, ch);
		initializeSubdivisions(m.colCtx, tris);
		m.radius = F(radius);
		m.checkHeight = F(checkHeight);
		m.lowDrop = falling ? 30 : 0;
		m.build(tris, ch.numPolygons);
		vector<Clip> clips = scan(m, threads);
		string json = toJson(game, e.name, form, m, ch.numPolygons, falling, clips);
		string path = out;
		if (path.empty() || all) {
			string dir = outDir.empty() ? "." : outDir;
			path = dir + "/" + safeName(game + "_" + e.name + "_" + form) + (falling ? "_falling" : "") + ".json";
		}
		std::ofstream f(path, std::ios::binary);
		f << json;
		if (!f) { fprintf(stderr, "  can't write %s\n", path.c_str()); failures++; continue; }
		fprintf(stderr, "  wrote %s\n", path.c_str());
	}
	return failures ? 1 : 0;
}

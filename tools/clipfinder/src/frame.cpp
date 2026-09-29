#include "frame.h"

vector<double> MOVE_STEPS = { 2, 4, 6, 8, 12, 16, 20, 24, 28, 32 };
double REACH_DIST = DEFAULT_MAX_MOVE;

void setMaxMove(double n) {
	vector<double> steps;
	for (double d : { 2, 4, 6, 8, 12, 16, 20, 24, 28, 32 }) if (d <= n) steps.push_back(d);
	if (n > DEFAULT_MAX_MOVE) for (double d = 36; d <= n; d += 4) steps.push_back(d);
	MOVE_STEPS = steps;
	REACH_DIST = n;
}
// Whether a wall push started with Link in front of the wall itself: his
// sphere centre, projected onto the wall along its normal, lands on the
// triangle. The game's wall check projects along the Z or X axis instead
// (CollisionPoly_Check[ZX]IntersectApprox), so a diagonal wall also pushes
// Link standing past its end, in front of its extended plane - as far past as
// he is in front of it at 45 degrees - on top of the 1 unit / detMax 300
// tolerance. A line test's snap is on the triangle already.
// The distance is measured from a vertex, not with the poly's stored plane
// distance: that's a whole number, so the plane can sit up to 0.5 off the
// triangle, enough to put a point on the edge two walls share (OoT Hyrule
// Field TRI 1286 / 1288, 0.006 inside 1286) past it. The 0.1 slack covers the
// rest (the normal is stored as s16s).
bool pushOnFace(const Model& m, const Push& t) {
	if (t.line) return true;
	const Poly& P = m.polys[t.poly];
	double y = t.from.y + m.checkHeight;
	double k = ((t.from.x - P.ax) * P.nx + (y - P.ay) * P.ny + (t.from.z - P.az) * P.nz) / (P.nMag * P.nMag);
	return pointInTri3D(P, t.from.x - k * P.nx, y - k * P.ny, t.from.z - k * P.nz, 0.1);
}
// rayFromY (prevPos.y, walking; NAN = none): the frame's floor check first
// (wall_push_clips.js clipFromFrame).
std::optional<ClipResult> clipFromFrame(const Model& m, Scratch& s, const V3& prev, const V3& res,
	const PushList& trace, const Tol& tol, double rayFromY, const Move* move) {
	if (trace.empty()) return std::nullopt;
	const V3 from = { prev.x, res.y, prev.z };
	int crossed = m.crossedWall(s, from, res);
	if (crossed < 0) return std::nullopt;
	V3 at = res;
	bool landed = false;
	double landY = 0;
	if (!std::isnan(rayFromY)) {
		auto fy = m.floorCheck(res.x, res.z, F(rayFromY + 50));
		if (fy && F(*fy - res.y) >= -11) {
			landed = true;
			landY = *fy;
			at = { res.x, F(*fy - GROUND_DROP), res.z };
		}
	}
	// Two more frames standing still from `at0`: still through the same wall,
	// or landed at another height through any (wall_push_clips.js
	// clipFromFrame: MM Treasure Chest Shop TRI 50 / 90).
	auto standStill = [&](const V3& at0, bool landed0, double landY0, V3& endOut) {
		V3 s1 = m.sphereStep(at0, tol, nullptr);
		V3 s2 = m.sphereStep(s1, tol, nullptr);
		const V3 from2 = { prev.x, at0.y, prev.z };
		int held = m.crossedWall(s, from2, s2);
		if (landed0 ? held < 0 : held != crossed) return false;
		// out the other side of a thin wall: through it, not out of bounds - but
		// through a dynapoly (a gate, a fence) that's what the clip is for
		if (m.polys[crossed].bg < 0 && m.crossedWall(s, from2, s2, true) >= 0) return false;
		endOut = landed0 ? V3{ s2.x, landY0, s2.z } : s2;
		return true;
	};
	V3 end;
	bool hold = false;
	if (!standStill(at, landed, landY, end)) {
		// Standing still, the wall he went through pushes him back out (he's
		// less than 4 behind it). Holding the stick, the next frame's move can
		// take him further behind it before its check runs, and then it can't
		// (OoT Ice Cavern: the rock TRI 721 puts Link 1.5 behind the red ice).
		// One more frame of the same move from where he landed, then standing.
		if (!move || !landed) return std::nullopt;
		V3 st2 = { res.x, landY, res.z };
		V3 nx2 = moveStep(st2, move->yaw, move->speed);
		auto lf = lineFrame(m, s, st2, nx2, tol);
		V3 r2 = lf ? lf->res : m.sphereStep(nx2, tol, nullptr, &st2);
		auto fy = m.floorCheck(r2.x, r2.z, F(st2.y + 50));
		if (!fy || F(*fy - r2.y) < -11) return std::nullopt;
		if (!standStill({ r2.x, F(*fy - GROUND_DROP), r2.z }, true, *fy, end)) return std::nullopt;
		hold = true;
	}
	const Poly& C = m.polys[crossed];
	double sphY = res.y + m.checkHeight;
	const Push* pusher = nullptr;
	for (const Push& t : trace) {
		if (t.poly == crossed) continue;
		double before = planeDist(C, t.from.x, sphY, t.from.z), after = planeDist(C, t.to.x, sphY, t.to.z);
		if (before >= 0 && after < 0) { pusher = &t; break; }
	}
	if (!pusher) {
		for (int i = trace.size() - 1; i >= 0; i--) if (trace[i].poly != crossed) { pusher = &trace[i]; break; }
	}
	if (!pusher) return std::nullopt;
	return ClipResult{ crossed, pusher->poly, end, pushOnFace(m, *pusher), hold };
}
std::optional<LineFrameR> lineFrame(const Model& m, Scratch& s, const V3& prev, const V3& next, const Tol& tol) {
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
	r.res = m.sphereStep(snapped, tol, &r.trace, &prev, m.polys[hit->poly].bg >= 0);
	return r;
}
std::optional<V3> landing(const Model& m, Scratch& s, const V3& res, double floorY, bool& noFloor, int crossed) {
	noFloor = false;
	auto land = m.floorCheck(res.x, res.z, F(floorY + 50));
	if (!land) { noFloor = true; return res; }
	V3 st = m.sphereStep(m.sphereStep({ res.x, F(*land - GROUND_DROP), res.z }, LOOSE, nullptr), LOOSE, nullptr);
	V3 end = { st.x, *land, st.z };
	if (!m.endCounts(s, crossed, end)) return std::nullopt;
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
// wall_push_clips.js standSpot: where Link can stand still near (x, z). Not
// under a floor within 50 above his feet: the floor check (from pos.y + 50)
// would put him up on it (OoT Kokiri Forest: the ground under a tree's roots).
std::optional<V3> standSpot(const Model& m, double x, double z, double floorY) {
	auto y = standFloor(m, x, z, floorY);
	if (!y) return std::nullopt;
	double cy = *y;
	for (int i = 0; i < 3; i++) {
		auto rest = m.restingSpot({ x, cy, z });
		if (!rest) return std::nullopt;
		auto ry = standFloor(m, rest->x, rest->z, floorY);
		if (!ry) return std::nullopt;
		if (rest->x == x && rest->z == z && *ry == cy) {
			auto fy = m.floorCheck(rest->x, rest->z, F(rest->y + 50));
			if (fy && *fy > rest->y) return std::nullopt;
			return rest;
		}
		x = rest->x; z = rest->z; cy = *ry;
	}
	return std::nullopt;
}
// standSpot through the thread's cache (Scratch::standSpots).
std::optional<V3> standSpotCached(const Model& m, Scratch& s, double x, double z, double floorY) {
	auto bits = [](double v) { float f = (float)v; uint32_t u; memcpy(&u, &f, 4); return u; };
	Scratch::SpotKey key{ bits(x), bits(z), bits(floorY) };
	auto it = s.standSpots.find(key);
	if (it != s.standSpots.end()) return it->second;
	if (s.standSpots.size() > 500000) s.standSpots.clear();
	auto r = standSpot(m, x, z, floorY);
	s.standSpots.emplace(key, r);
	return r;
}

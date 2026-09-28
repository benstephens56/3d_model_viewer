#include "slope.h"

// How far apart the points along a wall's bottom edge are
static const double SLOPE_STEP = 1;
// How far behind the wall's bottom edge the frame's move ends (posNext): a
// slope only lifts Link past its edge by its 1 unit tolerance, a floor
// behind the wall can be anywhere. In front of it too (negative): a wall
// that leans out over the slope (an overhang, its normal pointing down) is
// further out at his check height than at its bottom, so he can be behind
// it still on the slope (OoT Death Mountain Trail: TRI 642 over TRI 675).
static const double BEHIND[] = { -24, -18, -13, -9, -6, -4, -2.5, -1.5, -0.9, -0.5, -0.25,
	0.05, 0.25, 0.5, 0.9, 1.5, 2.5, 4, 6, 9, 13, 18, 24 };
// Directions the start is tried in, from straight into the wall (degrees)
static const double FAN[] = { 0, -15, 15, -30, 30, -45, 45, -60, 60, -75, 75 };

std::optional<Clip> slopeFrame(const Model& m, Scratch& s, const V3& start, int yaw, double speed, int wall) {
	V3 next = moveStep(start, yaw, speed);
	PushList trace;
	V3 res;
	if (auto f = lineFrame(m, s, start, next, LOOSE)) { res = f->res; trace = f->trace; }
	else res = m.sphereStep(next, LOOSE, &trace, &start);
	// the floor check lifts him
	int floorPoly = -1;
	auto fy = m.floorCheck(res.x, res.z, F(start.y + 50), &floorPoly);
	if (!fy || !(*fy > start.y) || F(*fy - res.y) < -11 || floorPoly < 0) return std::nullopt;
	const double y1 = *fy, low1 = F(y1 - GROUND_DROP);
	// behind a wall at his new check height, and at the frame's own behind
	// none, so the lift did it: not the pushes or the line test's snap (a wall
	// push clip, the scan's other phases)
	const int crossed = m.crossedWall(s, { start.x, low1, start.z }, { res.x, low1, res.z });
	if (crossed < 0 || (wall >= 0 && crossed != wall)) return std::nullopt;
	if (m.crossedWall(s, { start.x, next.y, start.z }, res) >= 0) return std::nullopt;
	for (const Push& t : trace) if (t.poly == crossed) return std::nullopt;
	if (m.dynaPairsOnly && m.polys[crossed].bg < 0 && m.polys[floorPoly].bg < 0) return std::nullopt;

	// Standing still for two frames from (at, on floor landY): still behind
	// the wall, and that counts (out of bounds, or past a dynapoly)
	auto standStill = [&](const V3& at, double landY, V3& end) {
		V3 s1 = m.sphereStep({ at.x, F(landY - GROUND_DROP), at.z }, LOOSE, nullptr);
		V3 s2 = m.sphereStep(s1, LOOSE, nullptr);
		if (m.crossedWall(s, { start.x, s2.y, start.z }, s2) != crossed) return false;
		// out the other side of a thin wall: through it, not out of bounds - but
		// through a dynapoly that's what the clip is for (clipFromFrame)
		if (m.polys[crossed].bg < 0 && m.crossedWall(s, { start.x, s2.y, start.z }, s2, true) >= 0) return false;
		end = { s2.x, landY, s2.z };
		return m.endCounts(s, crossed, end);
	};
	Clip c;
	c.kind = 2;
	c.cross = true;
	c.pusher = floorPoly;
	c.crossed = crossed;
	c.from = next; c.prev = start; c.next = next; c.hasNext = true;
	c.res = { res.x, y1, res.z };
	c.floorY = start.y; c.hasFloorY = true;
	c.yaw = yaw; c.speed = speed; c.hasMove = true;
	c.yaws = { yaw };
	bool ok = standStill(c.res, y1, c.end);
	// Standing still, the wall pushes him back out (he's at most 4 behind
	// it): one more frame of the same yaw, slowest speed first
	for (int v2 = 1; !ok && v2 * SPEED_RATE <= REACH_DIST; v2++) {
		const V3 st2 = c.res;
		V3 nx2 = moveStep(st2, yaw, v2);
		auto lf = lineFrame(m, s, st2, nx2, LOOSE);
		V3 r2 = lf ? lf->res : m.sphereStep(nx2, LOOSE, nullptr, &st2);
		if (m.crossedWall(s, { start.x, nx2.y, start.z }, r2) != crossed) continue;
		auto fy2 = m.floorCheck(r2.x, r2.z, F(y1 + 50));
		if (!fy2) {
			// nothing under him at all: he falls out
			c.end = r2;
			c.endNoFloor = true;
			ok = true;
		} else if (F(*fy2 - r2.y) >= -11) {
			ok = standStill(r2, *fy2, c.end);
		} else {
			bool noFloor;
			if (auto end = landing(m, s, r2, y1, noFloor, crossed)) { c.end = *end; c.endNoFloor = noFloor; ok = true; }
		}
		if (ok) c.speed2 = v2;
	}
	if (!ok) return std::nullopt;
	// the move itself is from a standing start
	c.reachDone = c.hasReach = true;
	c.reachSpeed = std::max(speed, c.speed2);
	c.reachYaw = yaw;
	c.reachStart = start;
	return c;
}

void slopeClipsForWall(const Model& m, Scratch& s, const Poly& W,
	const std::function<bool(int, int)>& pairDone, const std::function<void(const Clip&)>& yield) {
	const double ch = m.checkHeight;
	// out of the wall's front, and along it (top down)
	const double nx = W.nx * W.invNXZ, nz = W.nz * W.invNXZ;
	const double tx = -nz, tz = nx;
	const double vu[3] = { W.ax * tx + W.az * tz, W.bx * tx + W.bz * tz, W.cx * tx + W.cz * tz };
	const double vy[3] = { W.ay, W.by, W.cy };
	auto onPlane = [&](double u, double h) {
		double c = -(W.dist + W.ny * h) / (W.nXZ * W.nXZ);
		return std::pair<double, double>(c * W.nx + u * tx, c * W.nz + u * tz);
	};
	const double u0 = std::min({ vu[0], vu[1], vu[2] }), u1 = std::max({ vu[0], vu[1], vu[2] });
	std::set<std::array<double, 4>> tried;
	for (double u = u0 + SLOPE_STEP / 2; u < u1; u += SLOPE_STEP) {
		// the wall's bottom and top at u
		double bottom = INFINITY, top = -INFINITY;
		for (int i = 0; i < 3; i++) {
			int j = (i + 1) % 3;
			double a = vu[i], b = vu[j];
			if ((u < a && u < b) || (u > a && u > b) || a == b) continue;
			double y = vy[i] + (vy[j] - vy[i]) * (u - a) / (b - a);
			bottom = std::min(bottom, y);
			top = std::max(top, y);
		}
		if (!(bottom <= top)) continue;
		const auto base = onPlane(u, bottom);
		// Floors that put his check height on the wall, behind its plane
		vector<std::pair<double, vector<double>>> behind;
		double hiY = -INFINITY;
		for (double e : BEHIND) {
			vector<double> ys;
			const double qx = base.first - e * nx, qz = base.second - e * nz;
			for (double y : m.floorsAt(qx, qz)) {
				double h = y + ch - GROUND_DROP;
				if (h < bottom - 1 || h > top + 1 || planeDist(W, qx, h, qz) >= 0) continue;
				ys.push_back(y);
				hiY = std::max(hiY, y);
			}
			if (!ys.empty()) behind.push_back({ e, ys });
		}
		if (behind.empty()) continue;
		// Floors in front whose check height is under its bottom (a unit of
		// slack: the extended planes) and below one behind
		vector<double> front;
		for (double d : { 1.0, 3.0, 6.0, 10.0, 15.0, 21.0, 28.0, 36.0, 45.0 }) {
			if (d > REACH_DIST) break;
			for (double y : m.floorsAt(base.first + d * nx, base.second + d * nz)) {
				if (!(y + ch - GROUND_DROP < bottom + 2) || !(y < hiY) || hiY - y > 50) continue;
				if (std::none_of(front.begin(), front.end(), [&](double v) { return std::fabs(v - y) < 0.5; })) front.push_back(y);
			}
		}
		if (front.empty()) continue;
		bool found = false;
		for (double y0 : front) {
			for (const auto& [e, ys] : behind) {
				if (std::none_of(ys.begin(), ys.end(), [&](double y) { return y > y0 && y - y0 <= 50; })) continue;
				const double qx = base.first - e * nx, qz = base.second - e * nz;
				for (double deg : FAN) {
					const double a = deg * PI / 180;
					// into the wall, turned by a
					const double dx = -nx * std::cos(a) + tx * std::sin(a), dz = -nz * std::cos(a) + tz * std::sin(a);
					for (double dist : MOVE_STEPS) {
						auto startO = standSpotCached(m, s, F(qx - dist * dx), F(qz - dist * dz), y0);
						if (!startO) continue;
						const V3 start = *startO;
						if (planeDist(W, start.x, F(start.y + ch - GROUND_DROP), start.z) <= 0) continue;
						double vx = qx - start.x, vz = qz - start.z, len = std::hypot(vx, vz);
						if (len < 0.5 || len > REACH_DIST) continue;
						if (!tried.insert({ start.x, start.z, F(qx), F(qz) }).second) continue;
						if (!m.isInBounds(s, start, true)) continue;
						auto c = slopeFrame(m, s, start, yawOf(vx, vz), F(len / SPEED_RATE), W.id);
						if (!c || pairDone(c->pusher, c->crossed)) continue;
						yield(*c);
						found = true;
						break;
					}
					if (found) break;
				}
				if (found) break;
			}
			if (found) break;
		}
	}
}

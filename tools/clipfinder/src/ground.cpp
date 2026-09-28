#include "ground.h"

const double GROUND_VYS[] = { -20 };
const int GROUND_NVY = sizeof(GROUND_VYS) / sizeof(GROUND_VYS[0]);

// How far past the wall's bottom edge the frame's move ends (posNext)
static const double PAST[] = { 1, 2.5, 4, 6, 9, 13, 18, 24 };
// How far in front of the wall the start is looked for
static const double FRONT[] = { 0, 1, 2, 4, 6, 9, 13, 18, 24 };
// Directions the start is tried in, from straight into the wall (degrees)
static const double FAN[] = { 0, -20, 20, -40, 40, -60, 60 };

GroundLine groundLine(const Model& m, Scratch& s, const V3& prev, const V3& next) {
	GroundLine r;
	r.res = next;
	auto hit = m.lineHit(s, prev, next, LOOSE, true, true);
	if (!hit) return r;
	const Poly& P = m.polys[hit->poly];
	r.poly = hit->poly;
	if (P.ny > 0.5) {
		// a floor: just under it
		r.res = { hit->x, F(hit->y - (m.checkHeight > 1 ? 1 : m.checkHeight)), hit->z };
	} else {
		// a wall: the radius out along its (whole) normal
		r.res = { F(F(m.radius * P.nx) + hit->x), F(F(m.radius * P.ny) + hit->y), F(F(m.radius * P.nz) + hit->z) };
	}
	return r;
}

std::optional<Clip> groundFrame(const Model& m, Scratch& s, const V3& start, int yaw, double speed, double vy, int wall) {
	const V3 next = { F(start.x + F(F(speed * sinS(yaw)) * SPEED_RATE)), F(start.y + F(vy * SPEED_RATE)),
		F(start.z + F(F(speed * cosS(yaw)) * SPEED_RATE)) };
	if (!(F(m.checkHeight + F(next.y - start.y)) < 5)) return std::nullopt;
	int startFloor = -1;
	if (!m.floorCheck(start.x, start.z, F(start.y + 1), &startFloor) || startFloor < 0) return std::nullopt;
	const GroundLine gl = groundLine(m, s, start, next);
	// stopped on the floor he's on (the usual)
	if (gl.poly >= 0 && m.polys[gl.poly].ny > 0.5 && F(start.y - gl.res.y) <= 1.01) return std::nullopt;
	PushList trace;
	if (gl.poly >= 0) trace.push_back({ gl.poly, next, gl.res, true });
	const V3 res = m.sphereStep(gl.res, LOOSE, &trace, &start, gl.poly >= 0 && m.polys[gl.poly].bg >= 0);
	// the floor check, from prevPos.y + 50
	int floorPoly = -1;
	auto fy = m.floorCheck(res.x, res.z, F(start.y + 50), &floorPoly);
	Clip c;
	c.kind = 3;
	c.cross = true;
	c.pusher = startFloor;
	c.from = next; c.prev = start; c.next = next; c.hasNext = true;
	c.floorY = start.y; c.hasFloorY = true;
	c.yaw = yaw; c.speed = speed; c.hasMove = true; c.vy = vy;
	c.yaws = { yaw };
	int crossed;
	if (fy && F(*fy - res.y) >= 0) {
		// landed there: behind a wall at his check height, and still after two
		// frames standing still
		const double y1 = *fy, low1 = F(y1 - GROUND_DROP);
		crossed = m.crossedWall(s, { start.x, low1, start.z }, { res.x, low1, res.z });
		if (crossed < 0 || (wall >= 0 && crossed != wall)) return std::nullopt;
		V3 s1 = m.sphereStep({ res.x, low1, res.z }, LOOSE, nullptr);
		V3 s2 = m.sphereStep(s1, LOOSE, nullptr);
		if (m.crossedWall(s, { start.x, s2.y, start.z }, s2) != crossed) return std::nullopt;
		if (m.polys[crossed].bg < 0 && m.crossedWall(s, { start.x, s2.y, start.z }, s2, true) >= 0) return std::nullopt;
		c.res = { res.x, y1, res.z };
		c.end = { s2.x, y1, s2.z };
		if (!m.endCounts(s, crossed, c.end)) return std::nullopt;
	} else {
		// in the air under the ground (or nothing below at all): falls
		crossed = m.crossedWall(s, { start.x, res.y, start.z }, res);
		if (crossed < 0) {
			// under the wall's bottom at this height: the wall he passed under at
			// the start's check height
			crossed = m.crossedWall(s, { start.x, F(start.y - GROUND_DROP), start.z }, { res.x, F(start.y - GROUND_DROP), res.z });
		}
		if (crossed < 0 || (wall >= 0 && crossed != wall)) return std::nullopt;
		c.res = res;
		bool noFloor;
		auto end = landing(m, s, res, start.y, noFloor, crossed);
		if (!end) return std::nullopt;
		c.end = *end;
		c.endNoFloor = noFloor;
	}
	// the line test or a push put him there: an ordinary wall push clip
	for (const Push& t : trace) if (t.poly == crossed) return std::nullopt;
	if (m.dynaPairsOnly && m.polys[crossed].bg < 0 && m.polys[startFloor].bg < 0) return std::nullopt;
	c.crossed = crossed;
	// the move itself is from a standing start
	c.reachDone = c.hasReach = true;
	c.reachSpeed = speed;
	c.reachYaw = yaw;
	c.reachStart = start;
	return c;
}

void groundClipsForWall(const Model& m, Scratch& s, const Poly& W,
	const std::function<bool(int, int)>& pairDone, const std::function<void(const Clip&)>& yield) {
	const double ch = m.checkHeight;
	const double nx = W.nx * W.invNXZ, nz = W.nz * W.invNXZ;
	const double tx = -nz, tz = nx;
	const double vu[3] = { W.ax * tx + W.az * tz, W.bx * tx + W.bz * tz, W.cx * tx + W.cz * tz };
	const double vy[3] = { W.ay, W.by, W.cy };
	auto onPlane = [&](double u, double h) {
		double c = -(W.dist + W.ny * h) / (W.nXZ * W.nXZ);
		return std::pair<double, double>(c * W.nx + u * tx, c * W.nz + u * tz);
	};
	// the deepest the frame's wall check can run below the start's floor
	const double lowest = F(GROUND_VYS[0] * SPEED_RATE) + ch;
	const double u0 = std::min({ vu[0], vu[1], vu[2] }), u1 = std::max({ vu[0], vu[1], vu[2] });
	std::set<std::array<double, 4>> tried;
	for (double u = u0 + 0.5; u < u1; u += 1) {
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
		// A floor in front, at the wall's bottom: the wall rises out of the
		// ground there (the line test, under the ground, passes under it)
		vector<double> front;
		for (double y : m.floorsAt(base.first + 1 * nx, base.second + 1 * nz))
			if (std::fabs(y - bottom) <= 3 && y + lowest < bottom) front.push_back(y);
		if (front.empty()) continue;
		bool found = false;
		for (double y0 : front) {
			for (double e : PAST) {
				const double qx = base.first - e * nx, qz = base.second - e * nz;
				for (double deg : FAN) {
					const double a = deg * PI / 180;
					const double dx = -nx * std::cos(a) + tx * std::sin(a), dz = -nz * std::cos(a) + tz * std::sin(a);
					for (double d : FRONT) {
						// starts from pressed against the wall to d further back
						const double back = e + m.radius + d;
						auto startO = standSpotCached(m, s, F(qx - back * dx), F(qz - back * dz), y0);
						if (!startO) continue;
						V3 start = *startO;
						if (planeDist(W, start.x, F(start.y + ch), start.z) <= 0) continue;
						const double vx = qx - start.x, vz = qz - start.z, len = std::hypot(vx, vz);
						if (len < 0.5 || len > REACH_DIST) continue;
						if (!tried.insert({ start.x, start.z, F(qx), F(qz) }).second) continue;
						if (!m.isInBounds(s, start, true)) continue;
						for (int k = 0; k < GROUND_NVY && !found; k++) {
							auto c = groundFrame(m, s, start, yawOf(vx, vz), F(len / SPEED_RATE), GROUND_VYS[k], W.id);
							if (!c || pairDone(c->pusher, c->crossed)) continue;
							yield(*c);
							found = true;
						}
						if (found) break;
					}
					if (found) break;
				}
				if (found) break;
			}
			if (found) break;
		}
	}
}

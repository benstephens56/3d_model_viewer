#include "sim.h"

static const char* P(const V3& v) {
	static char b[4][96];
	static int k = 0;
	k = (k + 1) % 4;
	snprintf(b[k], 96, "(%.9g, %.9g, %.9g)", v.x, v.y, v.z);
	return b[k];
}

// SPEED as "15/7": a frame of walking per speed (the same yaw), each ending
// with the floor check, then two frames standing still. For slope clips
// (slope.h), where the frame after the one through the wall can matter.
static int runSimFrames(const Model& m, const V3& start, int yaw, const vector<double>& speeds) {
	Scratch s;
	s.stamp.assign(m.polys.size(), 0);
	printf("start %s  yaw 0x%04X, in bounds: %s\n", P(start), yaw, m.isInBounds(s, start, true) ? "yes" : "NO");
	V3 cur = start;
	for (size_t i = 0; i < speeds.size(); i++) {
		V3 next = moveStep(cur, yaw, F(speeds[i]));
		printf("frame %zu: speed %.9g, posNext %s\n", i + 1, F(speeds[i]), P(next));
		PushList trace;
		V3 res;
		if (auto f = lineFrame(m, s, cur, next, LOOSE)) {
			printf("  line test at y %.9g hits TRI %d, snapped to %s\n", F(next.y + m.checkHeight), f->hit.poly, P(f->trace[0].to));
			res = f->res;
			trace = f->trace;
		} else res = m.sphereStep(next, LOOSE, &trace, &cur);
		for (const Push& t : trace) if (!t.line) printf("  %s pushes %s -> %s\n", m.polyName(t.poly).c_str(), P(t.from), P(t.to));
		int floorPoly = -1;
		auto fy = m.floorCheck(res.x, res.z, F(cur.y + 50), &floorPoly);
		if (!fy) { printf("  no floor under %s at all: falls out\n", P(res)); return 0; }
		if (F(*fy - res.y) < -11) { printf("  floor %s at y %.9g is more than 11 below: falls (not modelled further)\n", m.polyName(floorPoly).c_str(), *fy); return 0; }
		cur = { res.x, *fy, res.z };
		const double low = F(cur.y - GROUND_DROP);
		int behind = m.crossedWall(s, { start.x, low, start.z }, { cur.x, low, cur.z });
		printf("  on %s: %s, %s%s\n", m.polyName(floorPoly).c_str(), P(cur), m.isInBounds(s, cur) ? "in bounds" : "OUT OF BOUNDS",
			behind >= 0 ? (", behind " + m.polyName(behind) + " (from the start, at this check height)").c_str() : "");
	}
	V3 s1 = m.sphereStep({ cur.x, F(cur.y - GROUND_DROP), cur.z }, LOOSE, nullptr);
	V3 s2 = m.sphereStep(s1, LOOSE, nullptr);
	V3 end = { s2.x, cur.y, s2.z };
	printf("2 frames standing: %s, %s\n", P(end), m.isInBounds(s, end) ? "in bounds" : "OUT OF BOUNDS");
	return 0;
}

int runSim(const Model& m, const string& simArg) {
	{
		// SPEED/SPEED/...: several frames
		char speeds[256] = {}, yawS[32] = {};
		double x, y, z;
		if (sscanf(simArg.c_str(), "%lf,%lf,%lf,%31[^,],%255[^,]", &x, &y, &z, yawS, speeds) == 5 && strchr(speeds, '/')) {
			vector<double> sp;
			for (char* t = strtok(speeds, "/"); t; t = strtok(nullptr, "/")) sp.push_back(atof(t));
			return runSimFrames(m, { F(x), F(y), F(z) }, (int)strtol(yawS, nullptr, 0) & 0xFFFF, sp);
		}
	}
	double sx, sy, sz, speed, drop = 0;
	char yawStr[32] = {};
	int n = sscanf(simArg.c_str(), "%lf,%lf,%lf,%31[^,],%lf,%lf", &sx, &sy, &sz, yawStr, &speed, &drop);
	if (n < 5) { fprintf(stderr, "--sim wants X,Y,Z,YAW,SPEED[,DROP] (YAW as 0x1234 or decimal)\n"); return 2; }
	int yaw = (int)strtol(yawStr, nullptr, 0) & 0xFFFF;
	Scratch s;
	s.stamp.assign(m.polys.size(), 0);
	V3 start = { F(sx), F(sy), F(sz) };
	V3 next = moveStep(start, yaw, F(speed));
	if (drop > 0) next.y = F(start.y - drop);
	printf("start %s  yaw 0x%04X  speed %.9g -> posNext %s\n", P(start), yaw, F(speed), P(next));
	printf("start in bounds: %s\n", m.isInBounds(s, start, true) ? "yes" : "NO");
	auto rest = m.restingSpot(start);
	printf("start is a resting spot: %s\n", rest && rest->x == start.x && rest->z == start.z ? "yes" : rest ? (string("no, rests at ") + P(*rest)).c_str() : "no (pushes don't settle)");
	if (F(m.checkHeight + F(next.y - start.y)) < 5) printf("checkHeight + dy < 5: the game's line test runs at the feet, floors included (not modelled)\n");
	V3 res;
	PushList trace;
	auto f = lineFrame(m, s, start, next, LOOSE);
	if (f) {
		printf("line test at y %.9g hits TRI %d at (%.9g, %.9g), snapped to %s\n", F(next.y + m.checkHeight), f->hit.poly, f->hit.x, f->hit.z, P(f->trace[0].to));
		res = f->res;
		trace = f->trace;
	} else {
		printf("line test at y %.9g: nothing hit\n", F(next.y + m.checkHeight));
		res = m.sphereStep(next, LOOSE, &trace, &start);
	}
	for (const Push& t : trace) {
		if (!t.line) printf("  %s pushes %s -> %s\n", m.polyName(t.poly).c_str(), P(t.from), P(t.to));
		else if (&t != &trace[0]) printf("  a dynapoly collision: the one-face line check stops him on %s at %s\n", m.polyName(t.poly).c_str(), P(t.to));
	}
	printf("after the pushes: %s\n", P(res));
	const Move mv{ yaw, F(speed) };
	auto clip = clipFromFrame(m, s, start, res, trace, LOOSE, drop > 0 ? NAN : start.y, &mv);
	// walking: the floor check lifting him behind a wall (slope.h)
	auto slope = !clip && drop <= 0 ? slopeFrame(m, s, start, yaw, F(speed)) : std::nullopt;
	if (slope) {
		const Clip& c = *slope;
		printf("SLOPE CLIP: the floor check puts Link on %s at %s, behind %s (the frame's wall check at y %.9g was under its bottom)\n",
			m.polyName(c.pusher).c_str(), P(c.res), m.polyName(c.crossed).c_str(), F(next.y + m.checkHeight));
		if (c.speed2 > 0) printf("  standing still, %s pushes him back out: one more frame at speed %g (the same yaw) takes him further behind\n",
			m.polyName(c.crossed).c_str(), c.speed2);
		printf("  %s %s, %s\n", c.endNoFloor ? "no floor under" : "ends at", P(c.end),
			c.endNoFloor ? "falls out" : !m.isInBounds(s, c.end) ? "OUT OF BOUNDS" : "in bounds, past the dynapoly (counts)");
		printf("  (--sim X,Y,Z,YAW,SPEED/SPEED2 runs the frames one by one)\n");
	} else if (!clip) {
		int crossed = m.crossedWall(s, { start.x, res.y, start.z }, res);
		printf("no clip (%s)\n", crossed < 0 ? "not through any wall between start and there"
			: ("through TRI " + std::to_string(crossed) + ", but the next frames' pushes put him back / not held").c_str());
	} else {
		printf("CLIP: TRI %d pushes Link through TRI %d%s; after 2 more frames %s, %s\n", clip->pusher, clip->crossed,
			clip->hold ? " (keeping the stick held one more frame)" : "", P(clip->end),
			!m.isInBounds(s, clip->end) ? "OUT OF BOUNDS" : m.polys[clip->crossed].bg >= 0 ? "in bounds, past the dynapoly (counts)" : "in bounds (doesn't count)");
		for (int id : { clip->pusher, clip->crossed }) {
			const Poly& q = m.polys[id];
			printf("  TRI %d: (%g, %g, %g) (%g, %g, %g) (%g, %g, %g)  normal (%.4f, %.4f, %.4f)\n", id,
				q.ax, q.ay, q.az, q.bx, q.by, q.bz, q.cx, q.cy, q.cz, q.nx / q.nMag, q.ny / q.nMag, q.nz / q.nMag);
		}
		PushList st;
		V3 sres = m.sphereStep(next, STRICT, &st, &start);
		for (const Push& t : st) if (!t.line) printf("  without the extended planes: TRI %d pushes %s -> %s\n", t.poly, P(t.from), P(t.to));
		auto sclip = clipFromFrame(m, s, start, sres, st, STRICT, drop > 0 ? NAN : start.y, &mv);
		printf("without the extended planes: %s\n", !sclip ? "no clip (extended)"
			: sclip->onFace ? "still clips, pushed from in front of the pusher's face (acute)"
			: "still clips, but pushed from beside the pusher, past its edge (extended)");
		if (drop > 0) {
			bool noFloor;
			auto land = landing(m, s, res, start.y, noFloor, clip->crossed);
			printf("falling: %s\n", !land ? "lands in bounds" : noFloor ? "no floor under him: falls out"
				: (string(m.isInBounds(s, *land) ? "lands in bounds past the dynapoly (counts) at " : "lands out of bounds at ") + P(*land)).c_str());
		}
	}
	return 0;
}

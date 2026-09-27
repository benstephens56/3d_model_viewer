#include "sim.h"

int runSim(const Model& m, const string& simArg) {
	double sx, sy, sz, speed, drop = 0;
	char yawStr[32] = {};
	int n = sscanf(simArg.c_str(), "%lf,%lf,%lf,%31[^,],%lf,%lf", &sx, &sy, &sz, yawStr, &speed, &drop);
	if (n < 5) { fprintf(stderr, "--sim wants X,Y,Z,YAW,SPEED[,DROP] (YAW as 0x1234 or decimal)\n"); return 2; }
	int yaw = (int)strtol(yawStr, nullptr, 0) & 0xFFFF;
	Scratch s;
	s.stamp.assign(m.polys.size(), 0);
	auto P = [](const V3& v) { static char b[4][96]; static int k = 0; k = (k + 1) % 4; snprintf(b[k], 96, "(%.9g, %.9g, %.9g)", v.x, v.y, v.z); return b[k]; };
	V3 start = { F(sx), F(sy), F(sz) };
	V3 next = moveStep(start, yaw, F(speed));
	if (drop > 0) next.y = F(start.y - drop);
	printf("start %s  yaw 0x%04X  speed %.9g -> posNext %s\n", P(start), yaw, F(speed), P(next));
	printf("start in bounds: %s\n", m.isInBounds(s, start) ? "yes" : "NO");
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
	if (!clip) {
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

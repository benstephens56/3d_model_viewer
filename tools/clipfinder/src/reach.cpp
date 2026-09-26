#include "reach.h"

// wall_push_clips.js reachability: the lowest speed Link can do clip `c` at,
// from a standable in-bounds start one frame's move away (32 directions, every
// REACH_STEP up to REACH_DIST). Crossings: the game's move at that yaw and
// speed (0.5 past the plane) has to hit the pusher and clip through the same
// wall. Standing points: unlike the JS (which only checks the line), the frame
// is run too - the move at that yaw and speed has to clip through the same
// wall and end out of bounds - so the speed is one that works exactly.
void reachability(const Model& m, Scratch& s, Clip& c) {
	const double REACH_STEP = 1;
	c.reachDone = true;
	const double floorRef = c.hasFloorY ? c.floorY : c.from.y;
	const V3 P = c.from;
	const double over = c.cross ? 0.5 : 0;
	std::set<std::pair<double, double>> tried;
	bool have = false;
	for (int i = 0; i < 32; i++) {
		double ang = i / 32.0 * 2 * PI;
		for (double d = REACH_STEP; d <= REACH_DIST; d += REACH_STEP) {
			if (have && (d + over) / SPEED_RATE >= c.reachSpeed + 2) break;
			auto startO = standSpot(m, F(P.x - d * std::sin(ang)), F(P.z - d * std::cos(ang)), floorRef);
			if (!startO) continue;
			V3 start = *startO;
			if (!tried.insert({ start.x, start.z }).second) continue;
			double vx = P.x - start.x, vz = P.z - start.z, len = std::hypot(vx, vz);
			if (len < 0.01 || len > REACH_DIST) continue;
			double speed = F((len + over) / SPEED_RATE);
			if (have && speed >= c.reachSpeed) continue;
			int yaw = yawOf(vx, vz);
			V3 next = moveStep(start, yaw, speed);
			if (c.drop > 0) next.y = P.y;
			// (moving, a drop with checkHeight + dy < 5 gets the feet-level line
			// test that stops him on his floor: see crossingPointsForWall)
			if (F(m.checkHeight + F(next.y - start.y)) < 5) continue;
			if (c.cross) {
				auto f = lineFrame(m, s, start, next, LOOSE);
				if (!f || f->hit.poly != c.pusher) continue;
				auto clip = clipFromFrame(m, s, start, f->res, f->trace, LOOSE, c.drop > 0 ? NAN : start.y);
				if (!clip || clip->crossed != c.crossed) continue;
				bool noFloor;
				if (c.drop > 0 ? !landing(m, s, f->res, floorRef, noFloor) : m.isInBounds(s, clip->end)) continue;
			} else {
				// nothing in the way, then the frame's pushes clip through the same wall
				if (lineFrame(m, s, start, next, LOOSE)) continue;
				PushList tr;
				V3 res = m.sphereStep(next, LOOSE, &tr);
				auto clip = clipFromFrame(m, s, start, res, tr, LOOSE, c.drop > 0 ? NAN : start.y);
				if (!clip || clip->crossed != c.crossed) continue;
				bool noFloor;
				if (c.drop > 0 ? !landing(m, s, res, floorRef, noFloor) : m.isInBounds(s, clip->end)) continue;
			}
			if (!m.isInBounds(s, start)) continue;
			have = true;
			c.hasReach = true;
			c.reachSpeed = speed;
			c.reachYaw = yaw;
			c.reachStart = start;
		}
	}
}

// One walking frame from a standing start: does moving at yaw / speed make
// TRI pusher push Link through TRI crossed and leave him out of bounds?
// (line test, pushes, floor check, two more frames: the scan's own checks)
static std::optional<ClipResult> walkFrameClips(const Model& m, Scratch& s, const V3& start, int yaw, double speed,
	int pusher, int crossed) {
	V3 next = moveStep(start, yaw, speed);
	V3 res;
	PushList trace;
	auto f = lineFrame(m, s, start, next, LOOSE);
	if (f) { res = f->res; trace = f->trace; }
	else res = m.sphereStep(next, LOOSE, &trace);
	auto clip = clipFromFrame(m, s, start, res, trace, LOOSE, start.y);
	if (!clip || clip->crossed != crossed || clip->pusher != pusher) return std::nullopt;
	if (m.isInBounds(s, clip->end)) return std::nullopt;
	return clip;
}


// Clips at this speed and at every 0.0025 up to 0.01 more: not a single-f32
// coincidence (e.g. posNext landing exactly on a wall's plane, which the
// one-face line test then counts from behind)
static bool robustClip(const Model& m, Scratch& s, const V3& S, int yaw, double sp, int pusher, int crossed) {
	for (int k = 0; k <= 4; k++)
		if (!walkFrameClips(m, s, S, yaw, F(sp + k * 0.0025), pusher, crossed)) return false;
	return true;
}

// The lowest robustly clipping speed from S at one yaw, in [lower, limit):
// every 0.02, then bisected to the f32 boundary. 0 if none.
static double minSpeedAtYaw(const Model& m, Scratch& s, const V3& S, int yaw, int pusher, int crossed,
	double lower, double limit) {
	double prevFail = std::max(0.0, lower - 0.02);
	for (double sp = std::max(0.02, lower); sp < limit; sp += 0.02) {
		// (the plain check first: most speeds don't clip at all)
		if (!walkFrameClips(m, s, S, yaw, F(sp), pusher, crossed) || !robustClip(m, s, S, yaw, F(sp), pusher, crossed)) {
			prevFail = sp;
			continue;
		}
		double a = prevFail, b = F(sp);
		for (int k = 0; k < 40; k++) {
			double mid = F((a + b) / 2);
			if (mid <= a || mid >= b) break;
			if (robustClip(m, s, S, yaw, mid, pusher, crossed)) b = mid; else a = mid;
		}
		return b;
	}
	return 0.0;
}

// --angles: from the refined start, every one of the 4096 directions the sine
// table tells apart (yaw >> 4): its lowest robust speed up to REACH_DIST / 1.5, and whether
// the refined speed works there. Printed as runs of neighbouring yaws.
void angleRanges(const Model& m, const Refined& r, int pusher, int crossed, int threads) {
	vector<double> minSp(4096, 0);
	vector<char> atRefined(4096, 0);
	std::atomic<int> next{ 0 };
	auto work = [&]() {
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		for (int c; (c = next++) < 4096;) {
			int yaw = c << 4;
			minSp[c] = minSpeedAtYaw(m, s, r.start, yaw, pusher, crossed, 0, REACH_DIST / SPEED_RATE);
			atRefined[c] = walkFrameClips(m, s, r.start, yaw, r.speed, pusher, crossed).has_value();
		}
	};
	vector<std::thread> ts;
	for (int t = 0; t < threads; t++) ts.emplace_back(work);
	for (auto& t : ts) t.join();
	auto run = [&](const char* title, auto pred, bool withMin) {
		printf("%s\n", title);
		// start the scan at a class that doesn't match, so a run through 0 isn't split
		int s0 = 0;
		while (s0 < 4096 && pred(s0)) s0++;
		if (s0 == 4096) { printf("  all yaws\n"); return; }
		bool any = false;
		for (int k = 1; k <= 4096; k++) {
			int c = (s0 + k) & 0xFFF;
			if (!pred(c)) continue;
			int first = c, n = 0;
			double lo = 1e9;
			int loYaw = 0;
			while (pred((first + n) & 0xFFF)) {
				int cc = (first + n) & 0xFFF;
				if (minSp[cc] > 0 && minSp[cc] < lo) { lo = minSp[cc]; loYaw = cc << 4; }
				n++;
			}
			int last = (first + n - 1) & 0xFFF;
			printf("  0x%04X - 0x%04X  (%d directions)", first << 4, (last << 4) | 0xF, n);
			if (withMin) printf("  lowest speed %.9g at 0x%04X", lo, loYaw);
			printf("\n");
			any = true;
			k += n - 1;
		}
		if (!any) printf("  none\n");
	};
	printf("From start %.9g, %.9g, %.9g (TRI %d -> %d); yaws that differ only in the low 4 bits move the same:\n",
		r.start.x, r.start.y, r.start.z, pusher, crossed);
	char title[128];
	snprintf(title, sizeof title, "Yaws that clip at speed %.9g:", r.speed);
	run(title, [&](int c) { return atRefined[c] != 0; }, false);
	snprintf(title, sizeof title, "Yaws that clip at some speed up to %g (with the lowest speed in each run):", REACH_DIST / SPEED_RATE);
	run(title, [&](int c) { return minSp[c] > 0; }, true);
	printf("Lowest speed per direction where it clips (runs of the same speed merged):\n");
	for (int c = 0; c < 4096;) {
		if (minSp[c] <= 0) { c++; continue; }
		int e = c;
		while (e + 1 < 4096 && minSp[e + 1] == minSp[c]) e++;
		printf("  0x%04X - 0x%04X: %.9g%s\n", c << 4, (e << 4) | 0xF, minSp[c], atRefined[c] ? "" : "  (not at that speed)");
		c = e + 1;
	}
}

// --refine: the lowest walking speed for one wall pair, searched finer than
// the scan's grid. Starts: every standable in-bounds resting spot on a 0.25
// grid within 24 of the scan's best start, nearest to the clip points first
// (a start can't do better than its distance to them / 1.5). Yaws: toward
// the clip points, every 8, then every 1 around the best. Speeds: every 0.02
// up to the best so far, and at the first that clips, bisected down to the
// f32 boundary. Returns the best found.
Refined refineMinSpeed(const Model& m, const vector<Clip>& clips, int pusher, int crossed, int threads) {
	Refined best;
	const Clip* seed = nullptr;
	// (walking only: falling clips need a y velocity as well)
	for (const Clip& c : clips) if (c.drop == 0 && c.hasReach && (!seed || c.reachSpeed < seed->reachSpeed)) seed = &c;
	if (!seed) return best;
	best.found = true;
	best.speed = seed->reachSpeed;
	best.yaw = seed->reachYaw;
	best.start = seed->reachStart;
	vector<V3> targets;
	for (const Clip& c : clips) if (c.drop == 0) targets.push_back(c.from);
	auto nearest = [&](const V3& p) {
		double d = 1e9;
		for (const V3& t : targets) d = std::min(d, std::hypot(t.x - p.x, t.z - p.z));
		return d;
	};
	// candidate starts
	vector<std::pair<double, V3>> starts;
	{
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		std::set<std::pair<float, float>> seen;
		const V3 B = seed->reachStart;
		for (double dx = -24; dx <= 24; dx += 0.25) {
			for (double dz = -24; dz <= 24; dz += 0.25) {
				if (dx * dx + dz * dz > 24 * 24) continue;
				auto st = standSpot(m, F(B.x + dx), F(B.z + dz), B.y);
				if (!st || !seen.insert({ (float)st->x, (float)st->z }).second) continue;
				if (!m.isInBounds(s, *st)) continue;
				starts.push_back({ nearest(*st), *st });
			}
		}
	}
	std::sort(starts.begin(), starts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	std::mutex mu;
	std::atomic<size_t> next{ 0 }, done{ 0 };
	auto bestSpeed = [&]() { std::lock_guard<std::mutex> g(mu); return best.speed; };
	auto work = [&]() {
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		for (size_t i; (i = next++) < starts.size();) {
			const V3 S = starts[i].second;
			done++;
			// (nearest clip point minus 2 units of slack: can't clip slower)
			double lower = std::max(0.0, (starts[i].first - 2) / SPEED_RATE);
			if (lower >= bestSpeed()) continue;
			// yaws toward the clip points within reach
			int lo = INT32_MAX, hi = INT32_MIN;
			int center = -1;
			for (const V3& t : targets) {
				double dx = t.x - S.x, dz = t.z - S.z;
				if (std::hypot(dx, dz) > bestSpeed() * SPEED_RATE + 2) continue;
				int y = yawOf(dx, dz);
				if (center < 0) center = y;
				int rel = (int16_t)(uint16_t)(y - center);
				lo = std::min(lo, rel);
				hi = std::max(hi, rel);
			}
			if (center < 0) continue;
			auto minAtYaw = [&](int yaw, double limit) { return minSpeedAtYaw(m, s, S, yaw, pusher, crossed, lower, limit); };
			double myBest = 0;
			int myYaw = 0;
			for (int rel = lo - 0x100; rel <= hi + 0x100; rel += 8) {
				int yaw = (center + rel) & 0xFFFF;
				double sp = minAtYaw(yaw, myBest ? myBest : bestSpeed());
				if (sp > 0 && (!myBest || sp < myBest)) { myBest = sp; myYaw = yaw; }
			}
			if (!myBest) continue;
			for (int d = -8; d <= 8; d++) {
				int yaw = (myYaw + d) & 0xFFFF;
				double sp = minAtYaw(yaw, myBest);
				if (sp > 0 && sp < myBest) { myBest = sp; myYaw = yaw; }
			}
			std::lock_guard<std::mutex> g(mu);
			if (myBest < best.speed) {
				best.speed = myBest;
				best.yaw = myYaw;
				best.start = S;
			}
		}
	};
	vector<std::thread> ts;
	for (int t = 0; t < threads; t++) ts.emplace_back(work);
	for (auto& t : ts) t.join();
	best.starts = (int)starts.size();
	Scratch s;
	s.stamp.assign(m.polys.size(), 0);
	if (auto c = walkFrameClips(m, s, best.start, best.yaw, best.speed, pusher, crossed)) best.end = c->end;
	return best;
}

// --min-speed: reachability for every clip (on `threads` threads), then
// the lowest speed per wall pair, crossing / standing and kind, printed.
void findMinSpeeds(const Model& m, vector<Clip>& found, int threads) {
	auto r0 = std::chrono::steady_clock::now();
	std::atomic<size_t> next{ 0 }, done{ 0 };
	auto work = [&]() {
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		for (size_t i; (i = next++) < found.size();) {
			reachability(m, s, found[i]);
			size_t d = ++done;
			if (d % 64 == 0 || d == found.size()) fprintf(stderr, "\r  min speed %zu / %zu   ", d, found.size());
		}
	};
	vector<std::thread> ts;
	for (int t = 0; t < threads; t++) ts.emplace_back(work);
	for (auto& t : ts) t.join();
	fprintf(stderr, "(%.1fs)\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - r0).count());
	// the lowest per wall pair, crossing / standing and walking / falling
	std::map<std::tuple<int, int, bool, bool>, const Clip*> best;
	for (const Clip& c : found) {
		if (!c.hasReach) continue;
		auto k = std::make_tuple(c.pusher, c.crossed, c.cross, c.drop > 0);
		if (!best.count(k) || c.reachSpeed < best[k]->reachSpeed) best[k] = &c;
	}
	static const char* kinds[] = { "acute", "extended" };
	for (auto& [k, c] : best) {
		fprintf(stderr, "  %s %s TRI %d -> %d: min speed %.4f  start %.3f, %.3f, %.3f  yaw 0x%04X  (clip point %.3f, %.3f, %.3f%s)\n",
			kinds[c->kind], c->cross ? "cross" : "stand", c->pusher, c->crossed, c->reachSpeed,
			c->reachStart.x, c->reachStart.y, c->reachStart.z, c->reachYaw & 0xFFFF,
			c->from.x, c->from.y, c->from.z, c->drop ? (", drop " + std::to_string(c->drop)).c_str() : "");
	}
	size_t none = std::count_if(found.begin(), found.end(), [](const Clip& c) { return !c.hasReach; });
	if (none) fprintf(stderr, "  (%zu clip points not reachable from a standable start at up to speed %g)\n", none, REACH_DIST / SPEED_RATE);
}

// The clips written are just the refined one, as an ordinary
// clip (the frame run again for its fields), so the viewer and
// wall_clip_tester.lua use its exact start, yaw and speed.
std::optional<Clip> refinedClip(const Model& m, const Refined& r, int pusher, int crossed) {
	Scratch s;
	s.stamp.assign(m.polys.size(), 0);
	auto frame = [&](const Tol& tol, V3& res, V3& at, bool& cross) {
		V3 nx = moveStep(r.start, r.yaw, r.speed);
		PushList trace;
		auto f = lineFrame(m, s, r.start, nx, tol);
		cross = (bool)f;
		if (f) { res = f->res; trace = f->trace; at = { f->hit.x, nx.y, f->hit.z }; }
		else { res = m.sphereStep(nx, tol, &trace); at = nx; }
		auto cl = clipFromFrame(m, s, r.start, res, trace, tol, r.start.y);
		if (cl && (cl->crossed != crossed || cl->pusher != pusher)) cl.reset();
		return cl;
	};
	V3 res, at, sres, sat;
	bool cross, scross;
	auto cl = frame(LOOSE, res, at, cross);
	if (cl) {
		Clip c;
		auto scl = frame(STRICT, sres, sat, scross);
		c.acutePoint = scl && scl->onFace;
		c.cross = cross;
		c.pusher = pusher; c.crossed = crossed;
		c.prev = r.start; c.next = moveStep(r.start, r.yaw, r.speed); c.hasNext = true;
		c.from = at; c.res = res; c.end = cl->end;
		c.floorY = r.start.y; c.hasFloorY = true;
		c.yaw = r.yaw; c.speed = r.speed; c.hasMove = true;
		if (cross) c.yaws = { r.yaw };
		c.reachDone = c.hasReach = true;
		c.reachSpeed = r.speed; c.reachYaw = r.yaw; c.reachStart = r.start;
		return c;
	}
	return std::nullopt;
}

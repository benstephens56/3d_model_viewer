#include "action.h"

// Generated from the decomps' animation data (link_animetion,
// gPlayerAnim_link_fighter_{normal,pierce,Lnormal,Lpierce}_kiru and their _end)
// by a script replaying z_player.c: each game frame of the lunge from the one
// after the attack starts, { root x, root z, prevTransl x, prevTransl z, speedXZ }.
//  OoT: the first frame's prevTransl is sSkeletonBaseTransl (-57, 3377, 0) x the
//   age's unk_08, a Vec3s (child 11/17: -36): Link steps back first.
//  MM: ANIM_FLAG_NOMOVE zeroes that first diff, so no step back (dropped here).
//  The animation plays 2/3 x 1.5 = 1 frame a game frame; when it ends, the _end
//  animation takes over at 1.5 frames a game frame and its root motion carries on.
//  swing: the frames whose collision runs with the swing active (after the
//   attack actions at curFrame 0 .. endFrame - 1, within the table's unk_0D;
//   the _end switch clears it). Leaving the ground then puts Link back at
//   prevPos, speedXZ zeroed (OoT func_8083AA10, MM func_8083827C; tested in
//   game: OoT Lost Woods, the 2h stab off TRI 974 was put back).
//  Not modelled: MM also puts him back during any root motion if there's a
//   floor within 10 in front of prevPos (func_808381F8, a ledge check).
//  Not modelled: the sword hitting a wall from animation frame 2 on
//  (func_80842DF4 / func_808401F4): speedXZ -14, a recoil back.
// Only grounded lunges: the jumpslash needs y velocity. The Deku stick (OoT
// child, MM Human) has its own key, stick-slash: it counts as two-handed
// (OoT Player_HoldsTwoHandedWeapon, MM Player_IsHoldingTwoHandedWeapon) and
// always does the forward slash, so it's the 2h slash's frames. OoT child has
// no other two-handed weapon.
const vector<Action> ACTIONS = {
	{ "1h-slash", "adult lunge 1h slash", "OOT", { "ADULT" }, { { 166, -1066, -57, 0, 0, false }, { -11, -439, 166, -1066, 10, true }, { -90, -441, -11, -439, 5, true }, { -167, -117, -90, -441, 0, true }, { -224, 134, -167, -117, 0, true }, { -223, 126, -224, 134, 0, false }, { -173, 71, -223, 126, 0, false }, { -52, -66, -173, 71, 0, false }, { -5, -86, -52, -66, 0, false }, { 48, -56, -5, -86, 0, false }, { -41, -7, 48, -56, 0, false } } },
	{ "1h-stab", "adult lunge 1h stab", "OOT", { "ADULT" }, { { -48, -1144, -57, 0, 0, false }, { -66, -953, -48, -1144, 10, true }, { -84, -659, -66, -953, 5, true }, { -129, -511, -84, -659, 0, true }, { -178, -458, -129, -511, 0, false }, { -206, -424, -178, -458, 0, false }, { -170, -434, -206, -424, 0, false }, { -142, -421, -170, -434, 0, false }, { -94, -220, -142, -421, 0, false }, { -71, -88, -94, -220, 0, false }, { -58, -7, -71, -88, 0, false } } },
	{ "2h-slash", "adult lunge 2h slash", "OOT", { "ADULT" }, { { -175, -545, -57, 0, 0, false }, { -217, -392, -175, -545, 10, true }, { -261, -244, -217, -392, 5, true }, { -299, -122, -261, -244, 0, true }, { -331, -103, -299, -122, 0, true }, { -337, -150, -331, -103, 0, false }, { -297, -133, -337, -150, 0, false }, { -206, -83, -297, -133, 0, false }, { -167, -61, -206, -83, 0, false }, { -87, -15, -167, -61, 0, false }, { -63, -1, -87, -15, 0, false }, { -57, 2, -63, -1, 0, false } } },
	{ "2h-stab", "adult lunge 2h stab", "OOT", { "ADULT" }, { { -35, 1134, -57, 0, 0, false }, { 76, 1964, -35, 1134, 10, true }, { 123, 2463, 76, 1964, 5, true }, { 123, 2463, 123, 2463, 0, false }, { 104, 2301, 123, 2463, 0, false }, { 21, 1766, 104, 2301, 0, false }, { -7, 1377, 21, 1766, 0, false }, { -39, 475, -7, 1377, 0, false }, { -48, 194, -39, 475, 0, false }, { -57, 2, -48, 194, 0, false } } },
	{ "1h-slash", "child lunge 1h slash", "OOT", { "CHILD" }, { { 166, -1066, -36, 0, 0, false }, { -11, -439, 166, -1066, 10, true }, { -90, -441, -11, -439, 5, true }, { -167, -117, -90, -441, 0, true }, { -224, 134, -167, -117, 0, true }, { -223, 126, -224, 134, 0, false }, { -173, 71, -223, 126, 0, false }, { -52, -66, -173, 71, 0, false }, { -5, -86, -52, -66, 0, false }, { 48, -56, -5, -86, 0, false }, { -41, -7, 48, -56, 0, false } } },
	{ "1h-stab", "child lunge 1h stab", "OOT", { "CHILD" }, { { -48, -1144, -36, 0, 0, false }, { -66, -953, -48, -1144, 10, true }, { -84, -659, -66, -953, 5, true }, { -129, -511, -84, -659, 0, true }, { -178, -458, -129, -511, 0, false }, { -206, -424, -178, -458, 0, false }, { -170, -434, -206, -424, 0, false }, { -142, -421, -170, -434, 0, false }, { -94, -220, -142, -421, 0, false }, { -71, -88, -94, -220, 0, false }, { -58, -7, -71, -88, 0, false } } },
	{ "stick-slash", "child lunge Deku stick slash", "OOT", { "CHILD" }, { { -175, -545, -36, 0, 0, false }, { -217, -392, -175, -545, 10, true }, { -261, -244, -217, -392, 5, true }, { -299, -122, -261, -244, 0, true }, { -331, -103, -299, -122, 0, true }, { -337, -150, -331, -103, 0, false }, { -297, -133, -337, -150, 0, false }, { -206, -83, -297, -133, 0, false }, { -167, -61, -206, -83, 0, false }, { -87, -15, -167, -61, 0, false }, { -63, -1, -87, -15, 0, false }, { -57, 2, -63, -1, 0, false } } },
	{ "1h-slash", "lunge 1h slash", "MM", { "HUMAN" }, { { -11, -439, 166, -1066, 10, true }, { -90, -441, -11, -439, 5, true }, { -167, -117, -90, -441, 0, true }, { -224, 134, -167, -117, 0, true }, { -223, 126, -224, 134, 0, false }, { -173, 71, -223, 126, 0, false }, { -52, -66, -173, 71, 0, false }, { -5, -86, -52, -66, 0, false }, { 48, -56, -5, -86, 0, false }, { -41, -7, 48, -56, 0, false } } },
	{ "1h-stab", "lunge 1h stab", "MM", { "HUMAN" }, { { -66, -953, -48, -1144, 10, true }, { -84, -659, -66, -953, 5, true }, { -129, -511, -84, -659, 0, true }, { -178, -458, -129, -511, 0, false }, { -206, -424, -178, -458, 0, false }, { -170, -434, -206, -424, 0, false }, { -142, -421, -170, -434, 0, false }, { -94, -220, -142, -421, 0, false }, { -71, -88, -94, -220, 0, false }, { -58, -7, -71, -88, 0, false } } },
	{ "2h-slash", "lunge 2h slash", "MM", { "HUMAN" }, { { -217, -392, -175, -545, 10, true }, { -261, -244, -217, -392, 5, true }, { -299, -122, -261, -244, 0, true }, { -331, -103, -299, -122, 0, true }, { -337, -150, -331, -103, 0, false }, { -297, -133, -337, -150, 0, false }, { -206, -83, -297, -133, 0, false }, { -167, -61, -206, -83, 0, false }, { -87, -15, -167, -61, 0, false }, { -63, -1, -87, -15, 0, false }, { -57, 2, -63, -1, 0, false } } },
	{ "2h-stab", "lunge 2h stab", "MM", { "HUMAN" }, { { 76, 1964, -35, 1134, 10, true }, { 123, 2463, 76, 1964, 5, true }, { 123, 2463, 123, 2463, 0, false }, { 104, 2301, 123, 2463, 0, false }, { 21, 1766, 104, 2301, 0, false }, { -7, 1377, 21, 1766, 0, false }, { -39, 475, -7, 1377, 0, false }, { -48, 194, -39, 475, 0, false }, { -57, 2, -48, 194, 0, false } } },
	{ "stick-slash", "lunge Deku stick slash", "MM", { "HUMAN" }, { { -217, -392, -175, -545, 10, true }, { -261, -244, -217, -392, 5, true }, { -299, -122, -261, -244, 0, true }, { -331, -103, -299, -122, 0, true }, { -337, -150, -331, -103, 0, false }, { -297, -133, -337, -150, 0, false }, { -206, -83, -297, -133, 0, false }, { -167, -61, -206, -83, 0, false }, { -87, -15, -167, -61, 0, false }, { -63, -1, -87, -15, 0, false }, { -57, 2, -63, -1, 0, false } } },
};

static const double ACTION_SCALE = F(0.01);
static const double MM_HUMAN_SCALE = F(11.0 / 17.0);  // MM Human's ageProperties->unk_08 (the diffScale)

V3 actionStep(const Action& a, const ActionFrame& f, const V3& pos, int facing, bool noSpeed) {
	const double sn = sinS(facing), cs = cosS(facing);
	double dx, dz;
	if (a.game == "MM") {
		const double x = F(f.jx - f.px), z = F(f.jz - f.pz);
		dx = F(F(F(x * cs) + F(z * sn)) * ACTION_SCALE);
		dz = F(F(F(z * cs) - F(x * sn)) * ACTION_SCALE);
		dx = F(dx * MM_HUMAN_SCALE);
		dz = F(dz * MM_HUMAN_SCALE);
	} else {
		dx = F(F(F(f.jx * cs) + F(f.jz * sn)) - F(F(f.px * cs) + F(f.pz * sn)));
		dz = F(F(F(f.jz * cs) - F(f.jx * sn)) - F(F(f.pz * cs) - F(f.px * sn)));
		dx = F(dx * ACTION_SCALE);
		dz = F(dz * ACTION_SCALE);
	}
	V3 p = { F(pos.x + dx), pos.y, F(pos.z + dz) };
	return noSpeed ? V3{ p.x, F(p.y - GROUND_DROP), p.z } : moveStep(p, facing, F(f.speed));
}

void actionFrameMove(const Action& a, const ActionFrame& f, double& speed, int& angle) {
	const V3 d = actionStep(a, f, { 0, GROUND_DROP, 0 }, 0);
	speed = std::hypot(d.x, d.z) / SPEED_RATE;
	angle = speed > 0 ? yawOf(d.x, d.z) : 0;
}

vector<int> parseActions(const string& game, const string& list, string& err) {
	vector<int> out;
	for (size_t a = 0; a <= list.size();) {
		size_t b = list.find(',', a);
		if (b == string::npos) b = list.size();
		string k = list.substr(a, b - a);
		a = b + 1;
		if (k.empty()) continue;
		bool any = false;
		for (size_t i = 0; i < ACTIONS.size(); i++) {
			if (ACTIONS[i].game != game || (k != "all" && k != ACTIONS[i].key)) continue;
			if (std::find(out.begin(), out.end(), (int)i) == out.end()) out.push_back((int)i);
			any = true;
		}
		if (!any) {
			err = "unknown action \"" + k + "\" for " + game + " (";
			for (const Action& x : ACTIONS) if (x.game == game) err += x.key + ", ";
			err += "or all)";
			return {};
		}
	}
	return out;
}

bool actionForForm(const Action& a, const string& formUpper) {
	return std::find(a.forms.begin(), a.forms.end(), formUpper) != a.forms.end();
}

namespace {
struct FrameOut {
	V3 prev, next, res;
	PushList trace;
	bool landed = false;
	double landY = 0;
	int floorPoly = -1;
	bool reverted = false;  // off the ground during the swing: put back at prev
};

// The action's frames from `start`, each a walking frame: the move, the
// line test (or the pushes), then the floor check from prevPos.y + 50. Stops
// early if Link leaves the ground (no floor within 11 below posNext).
void runFrames(const Model& m, Scratch& s, const V3& start, int facing, const Action& a, const Tol& tol, vector<FrameOut>& out) {
	out.clear();
	V3 pos = start;
	bool noSpeed = false;  // (a swing frame put him back: speedXZ zeroed)
	for (const ActionFrame& af : a.frames) {
		FrameOut& o = out.emplace_back();
		o.prev = pos;
		o.next = actionStep(a, af, pos, facing, noSpeed);
		if (auto lf = lineFrame(m, s, pos, o.next, tol)) { o.res = lf->res; o.trace = lf->trace; }
		else o.res = m.sphereStep(o.next, tol, &o.trace, &pos);
		auto fy = m.floorCheck(o.res.x, o.res.z, F(pos.y + 50), &o.floorPoly);
		o.landed = fy && F(*fy - o.res.y) >= -11;
		if (!o.landed && af.swing) {
			// off the ground with the swing active: back where the frame started
			o.res = pos;
			o.trace.clear();
			o.landed = true;
			o.landY = pos.y;
			o.reverted = true;
			noSpeed = true;
			continue;
		}
		if (!o.landed) break;
		o.landY = *fy;
		pos = { o.res.x, o.landY, o.res.z };
	}
}

struct Verdict { int crossed = -1, pusher = -1, frame = -1, kind = 1; bool onFace = false, cross = false, noFloor = false; V3 end; };

// Whether the frames take Link through a wall: the first frame that leaves
// him behind one, at its own posNext height (a wall push: that frame's pushes
// or line test snap), or failing that on the floor its floor check lifts him
// onto (a slope clip, as slope.h). Then, as clipFromFrame: standing still
// for two frames after the last frame he's still behind a wall (another one
// at the height he landed at is fine; a slope clip: the same one), and that
// counts - or, off the ground, he lands out of bounds.
std::optional<Verdict> judge(const Model& m, Scratch& s, const V3& start, const vector<FrameOut>& fr, const Tol& tol) {
	Verdict v;
	for (size_t k = 0; k < fr.size() && v.frame < 0; k++) {
		const FrameOut& o = fr[k];
		const int cw = m.crossedWall(s, { start.x, o.res.y, start.z }, o.res);
		if (cw >= 0) {
			const Poly& C = m.polys[cw];
			const double sphY = o.res.y + m.checkHeight;
			const Push* p = nullptr;
			for (const Push& t : o.trace) {
				if (t.poly == cw) continue;
				if (planeDist(C, t.from.x, sphY, t.from.z) >= 0 && planeDist(C, t.to.x, sphY, t.to.z) < 0) { p = &t; break; }
			}
			if (!p) for (int i = (int)o.trace.size() - 1; i >= 0; i--) if (o.trace[i].poly != cw) { p = &o.trace[i]; break; }
			if (!p) return std::nullopt;  // (no push did it)
			v.crossed = cw;
			v.pusher = p->poly;
			v.frame = (int)k;
			v.onFace = pushOnFace(m, *p);
			v.cross = o.trace[0].line;
			v.kind = 1;
		} else if (o.landed && o.floorPoly >= 0) {
			const double low = F(o.landY - GROUND_DROP);
			const int lw = m.crossedWall(s, { start.x, low, start.z }, { o.res.x, low, o.res.z });
			if (lw < 0 || std::any_of(o.trace.begin(), o.trace.end(), [&](const Push& t) { return t.poly == lw; })) continue;
			v.crossed = lw;
			v.pusher = o.floorPoly;
			v.frame = (int)k;
			v.kind = 2;
			v.cross = true;
		}
	}
	if (v.frame < 0) return std::nullopt;
	const FrameOut& L = fr.back();
	if (L.landed) {
		V3 s1 = m.sphereStep({ L.res.x, F(L.landY - GROUND_DROP), L.res.z }, tol, nullptr);
		V3 s2 = m.sphereStep(s1, tol, nullptr);
		const V3 from = { start.x, s2.y, start.z };
		const int held = m.crossedWall(s, from, s2);
		bool ok = held >= 0 && !(v.kind == 2 && held != v.crossed);
		// out the other side of a thin wall: through it, not out of bounds - but
		// through a dynapoly that's what the clip is for (clipFromFrame)
		if (ok && m.polys[held].bg < 0 && m.crossedWall(s, from, s2, true) >= 0) ok = false;
		v.end = { s2.x, L.landY, s2.z };
		// or back out in front, somewhere he couldn't walk to (Model::walkUnreachable)
		if (!ok && (!m.isInBounds(s, v.end) || !m.walkUnreachable(s, v.end, start))) return std::nullopt;
		if (!m.endCounts(s, v.crossed, v.end, &start)) return std::nullopt;
	} else {
		auto land = landing(m, s, L.res, L.prev.y, v.noFloor, v.crossed);
		if (!land) return std::nullopt;
		v.end = *land;
	}
	return v;
}
}  // namespace

std::optional<Clip> actionClip(const Model& m, Scratch& s, const V3& start, int facing, int action) {
	const Action& a = ACTIONS[action];
	vector<FrameOut> fr;
	runFrames(m, s, start, facing, a, LOOSE, fr);
	auto v = judge(m, s, start, fr, LOOSE);
	if (!v) return std::nullopt;
	if (m.dynaPairsOnly && m.polys[v->crossed].bg < 0 && m.polys[v->pusher].bg < 0) return std::nullopt;
	Clip c;
	c.kind = v->kind;
	if (v->kind < 2) {
		// the same without the extended planes, pushed from in front of the pusher's face
		vector<FrameOut> sfr;
		runFrames(m, s, start, facing, a, STRICT, sfr);
		auto sv = judge(m, s, start, sfr, STRICT);
		c.acutePoint = sv && sv->kind < 2 && sv->crossed == v->crossed && sv->pusher == v->pusher && sv->onFace;
	}
	const FrameOut& o = fr[v->frame];
	const ActionFrame& af = a.frames[std::min((size_t)v->frame, a.frames.size() - 1)];
	c.cross = v->cross;
	c.pusher = v->pusher;
	c.crossed = v->crossed;
	c.from = o.next; c.prev = start; c.next = o.next; c.hasNext = true;
	c.res = o.landed && v->kind == 2 ? V3{ o.res.x, o.landY, o.res.z } : o.res;
	c.end = v->end;
	c.endNoFloor = v->noFloor;
	c.floorY = start.y; c.hasFloorY = true;
	// the clip frame's move, as a yaw and speed (for display)
	c.yaw = yawOf(o.next.x - o.prev.x, o.next.z - o.prev.z);
	c.speed = F(std::hypot(o.next.x - o.prev.x, o.next.z - o.prev.z) / SPEED_RATE);
	c.hasMove = true;
	if (c.cross) c.yaws = { c.yaw };
	c.action = action;
	c.facing = facing & 0xFFFF;
	for (const FrameOut& f : fr) c.frames.push_back(f.landed ? V3{ f.res.x, f.landY, f.res.z } : f.res);
	// no stick speed to reach it with: the action is the move
	c.reachDone = c.hasReach = true;
	c.reachSpeed = 0;
	c.reachYaw = c.facing;
	c.reachStart = start;
	return c;
}

vector<Clip> actionScan(const Model& m, const vector<Clip>& targets, const vector<int>& actions, int threads) {
	auto t0 = std::chrono::steady_clock::now();
	// the scan's walking and slope clip points (not falling or ground: those
	// need y velocity)
	vector<const Clip*> todo;
	for (const Clip& c : targets) if (c.hasNext && c.drop == 0 && (c.kind < 2 || c.kind == 2)) todo.push_back(&c);
	// Directions the clip frame's move is tried in: the target's own move and
	// the crossing yaws that worked, each turned by these.
	static const int FAN[] = { 0, -0x80, 0x80, -0x100, 0x100, -0x200, 0x200, -0x400, 0x400, -0x800, 0x800, -0x1000, 0x1000 };
	// Starts along the move, from the one that puts posNext on the target
	static const double ALONG[] = { 0, -0.5, 0.5, -1.5, 1.5 };
	std::atomic<size_t> next{ 0 }, done{ 0 };
	std::mutex outMu;
	vector<Clip> clips;
	auto worker = [&]() {
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		vector<Clip> local;
		std::set<std::array<double, 4>> tried;
		for (;;) {
			size_t i = next++;
			if (i >= todo.size()) break;
			const Clip& t = *todo[i];
			const V3 P = t.next;
			const double floorRef = t.prev.y;
			vector<int> dirs;
			auto addDir = [&](int y) { y &= 0xFFFF; if (std::find(dirs.begin(), dirs.end(), y) == dirs.end()) dirs.push_back(y); };
			for (int d : FAN) addDir(t.yaw + d);
			for (int y : t.yaws) addDir(y);
			for (int ai : actions) {
				const Action& a = ACTIONS[ai];
				bool found = false;
				for (size_t k = 0; k < a.frames.size() && !found; k++) {
					// (only the frames that move Link a fair way can take him through)
					double kSpeed;
					int kAngle;
					actionFrameMove(a, a.frames[k], kSpeed, kAngle);
					if (kSpeed < 2) continue;
					for (int d : dirs) {
						if (found) break;
						// facing so that frame k moves along d
						const int facing = (d - kAngle) & 0xFFFF;
						// frames 0..k's moves, unobstructed
						V3 D = { 0, 0, 0 };
						for (size_t j = 0; j <= k; j++) D = actionStep(a, a.frames[j], D, facing);
						const V3 u = moveStep({ 0, 0, 0 }, d, 1 / SPEED_RATE);
						for (double al : ALONG) {
							auto st = standSpotCached(m, s, F(P.x - D.x + al * u.x), F(P.z - D.z + al * u.z), floorRef);
							if (!st) continue;
							if (!tried.insert({ st->x, st->z, (double)facing, (double)ai }).second) continue;
							if (!m.keepLoadVoid && m.startOnLoadVoid(*st)) continue;
							auto c = actionClip(m, s, *st, facing, ai);
							if (!c || !m.isInBounds(s, *st, true)) continue;
							local.push_back(*c);
							found = true;
							break;
						}
					}
				}
			}
			if (tried.size() > 2000000) tried.clear();
			size_t n = ++done;
			static std::mutex pm;
			static auto last = std::chrono::steady_clock::now();
			std::lock_guard<std::mutex> g(pm);
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration<double>(now - last).count() >= 0.5 || n == todo.size()) {
				last = now;
				fprintf(stderr, "\r  action targets %zu / %zu (%.0fs)   ", n, todo.size(), std::chrono::duration<double>(now - t0).count());
			}
		}
		std::lock_guard<std::mutex> g(outMu);
		clips.insert(clips.end(), local.begin(), local.end());
	};
	{
		vector<std::thread> ts;
		for (int i = 0; i < threads; i++) ts.emplace_back(worker);
		for (auto& t : ts) t.join();
	}
	fprintf(stderr, "\n");
	// Deterministic order, and one clip per start and action
	std::sort(clips.begin(), clips.end(), [](const Clip& a, const Clip& b) {
		if (a.action != b.action) return a.action < b.action;
		if (a.pusher != b.pusher) return a.pusher < b.pusher;
		if (a.crossed != b.crossed) return a.crossed < b.crossed;
		if (a.prev.x != b.prev.x) return a.prev.x < b.prev.x;
		if (a.prev.z != b.prev.z) return a.prev.z < b.prev.z;
		return a.facing < b.facing;
	});
	clips.erase(std::unique(clips.begin(), clips.end(), [](const Clip& a, const Clip& b) {
		return a.action == b.action && a.prev.x == b.prev.x && a.prev.z == b.prev.z && a.prev.y == b.prev.y && a.facing == b.facing;
	}), clips.end());
	{
		Scratch s;
		s.stamp.assign(m.polys.size(), 0);
		for (Clip& c : clips) c.inBounds = !c.endNoFloor && m.isInBounds(s, c.end);
	}
	// One category per wall pair and action: acute if any of its points is
	std::set<std::tuple<int, int, int>> acute;
	for (const Clip& c : clips) if (c.acutePoint && c.kind < 2) acute.insert({ c.action, c.pusher, c.crossed });
	for (Clip& c : clips) if (c.kind < 2) c.kind = acute.count({ c.action, c.pusher, c.crossed }) ? 0 : 1;
	// per action: wall pairs and points
	for (int ai : actions) {
		std::set<std::pair<int, int>> pairs;
		size_t n = 0;
		for (const Clip& c : clips) if (c.action == ai) { pairs.insert({ c.pusher, c.crossed }); n++; }
		fprintf(stderr, "  %s: %zu wall pairs, %zu points\n", ACTIONS[ai].name.c_str(), pairs.size(), n);
	}
	fprintf(stderr, "  action clips: %zu points from %zu targets in %.1fs\n", clips.size(), todo.size(),
		std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
	return clips;
}

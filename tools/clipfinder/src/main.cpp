// clipfinder: the wall push clip scan, native and multithreaded. Reads an
// OoT / MM scene from models/, builds the same collision model the viewer does
// (js/parse_model.js, js/subdivisions.js), runs the search and writes the
// points as JSON for the viewer's "Import results" button.
//
// The collision model, clipFromFrame, standSpot, landing and reachability are
// also in js/wall_push_clips.js (for the viewer's "Reachable only") and have to
// stay in step with it: doubles with F() wherever the JS has Math.fround, so
// the numbers come out identical. The JS explains the game side of each step.
//
// The source files, from the bottom up:
//   common.h         f32 helpers (F = Math.fround), V3, the sine table, a frame's walk
//   scene.h/.cpp     the scene file's collision header and its subdivisions
//   collision.h/.cpp the game's static collision checks, as a Model
//   frame.h/.cpp     one frame: does it clip; where Link can stand; --max-move
//   search.h/.cpp    the scan over a whole map
//   reach.h/.cpp     --min-speed, --refine, --angles
//   output.h/.cpp    the results JSON
//   sim.h/.cpp       --sim
//   main.cpp         options, forms and the loop over maps
//
// Build (MSYS2 mingw64):  see build.sh next to this file.
// Usage:
//   clipfinder --game MM --map "Laundry Pool" --form Human [--falling] [--extended-only] [--first-per-pair] [-o out.json]
//     (--first-per-pair: one clip point per wall pair, the first found - much faster)
//   clipfinder --game OOT --all --form Adult [--falling] --out-dir results/
//   clipfinder --game OOT --map "Spot 01 - Kakariko Village" --form All -o kak.json
//     (--form All: every form's clips in the one JSON, each marked with its form;
//      --form Adult,Child: just those forms, the same way)
// Options: --root <viewer dir> (default: two levels up from the exe's dir, or
// the current dir if it has models/), --threads N, --radius R (overrides --form).
// Every option is explained in README.md next to this file.

#include "output.h"
#include "reach.h"
#include "scene.h"
#include "search.h"
#include "sim.h"

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
	string game, mapName, form, out, outDir, root, after;
	double radius = 0;
	bool falling = false, all = false, extendedOnly = false, firstPerPair = false, minSpeed = false, refine = false, angles = false;
	int onlyPusher = -1, onlyCrossed = -1;
	string simArg;  // --sim x,y,z,yaw,speed[,drop]
	bool haveFrom = false;
	double fromX = 0, fromY = 0, fromZ = 0, fromSpeed = 0;  // --from
	int threads = (int)std::max(1u, std::thread::hardware_concurrency());
	for (int i = 1; i < argc; i++) {
		string a = argv[i];
		auto val = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(2); } return string(argv[++i]); };
		if (a == "--game") game = val();
		else if (a == "--map") mapName = val();
		else if (a == "--form") form = val();
		else if (a == "--radius") radius = std::stod(val());
		else if (a == "--falling") falling = true;
		else if (a == "--extended-only") extendedOnly = true;
		else if (a == "--first-per-pair") firstPerPair = true;
		else if (a == "--min-speed") minSpeed = true;
		else if (a == "--refine") { refine = true; minSpeed = true; }
		else if (a == "--angles") { angles = true; refine = true; minSpeed = true; }
		else if (a == "--from") {
			// --angles from this start (and speed) instead of the refined one
			string v = val();
			int n = sscanf(v.c_str(), "%lf,%lf,%lf,%lf", &fromX, &fromY, &fromZ, &fromSpeed);
			if (n < 3) { fprintf(stderr, "--from wants X,Y,Z[,SPEED]\n"); return 2; }
			haveFrom = true;
			angles = refine = minSpeed = true;
		}
		else if (a == "--sim") simArg = val();
		else if (a == "--pair") {
			string v = val();
			if (sscanf(v.c_str(), "%d,%d", &onlyPusher, &onlyCrossed) != 2) { fprintf(stderr, "--pair wants PUSHER,CROSSED (TRI ids), e.g. --pair 757,714\n"); return 2; }
		}
		else if (a == "--all") all = true;
		else if (a == "--after") after = val();
		else if (a == "-o" || a == "--out") out = val();
		else if (a == "--out-dir") outDir = val();
		else if (a == "--root") root = val();
		else if (a == "--max-move") {
			double n = std::stod(val());
			if (!(n > 0)) { fprintf(stderr, "--max-move wants a distance > 0\n"); return 2; }
			setMaxMove(n);
		}
		else if (a == "--threads") threads = std::max(1, std::stoi(val()));
		else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
	}
	for (auto& ch : game) ch = (char)toupper((unsigned char)ch);
	if ((game != "OOT" && game != "MM") || (mapName.empty() && !all)) {
		fprintf(stderr,
			"usage: clipfinder --game OOT|MM (--map \"<name in the viewer's map list>\" | --all)\n"
			"                  [--form Adult|Child|Crawlspace|Human|Deku|Zora|Goron|FierceDeity|All, or a list: Adult,Child] [--radius R] [--falling] [--extended-only] [--first-per-pair]\n"
			"                  [--min-speed] [--pair PUSHER,CROSSED] [--refine (with --pair: the exact lowest walking speed)]\n"
			"                  [--angles (with --pair: also every yaw that works from the refined start)]\n"
			"                  [--from X,Y,Z[,SPEED] (--angles from this start instead, and at this speed)]\n"
			"                  [--sim X,Y,Z,YAW,SPEED[,DROP]]  (one frame from a standing start, printed step by step)\n"
			"                  [--max-move N]  (units Link can move in one frame: default 45, speed 30)\n"
			"                  [-o out.json | --out-dir dir] [--root viewer_dir] [--threads N]\n");
		return 2;
	}
	// ageProperties->wallCheckRadius (z_player.c)
	static const std::map<string, double> radii = {
		{ "ADULT", 18 }, { "CHILD", 14 }, { "HUMAN", 14 }, { "DEKU", 14 }, { "ZORA", 18 }, { "GORON", 19.5 },
		{ "FIERCEDEITY", 27 }, { "FIERCE_DEITY", 27 }, { "FD", 27 },
		{ "CRAWLSPACE", 10 }, { "CRAWL", 10 },
	};
	// --form All: every form of the game (the viewer's RADIUS_OPTIONS). A
	// smaller radius's clips aren't a subset of a bigger one's: resting spots,
	// what fits between walls and which walls are in reach all change with it.
	static const std::map<string, vector<string>> allForms = {
		{ "OOT", { "Adult", "Child", "Crawlspace" } },
		{ "MM", { "Human", "Deku", "Zora", "Goron", "FierceDeity" } },
	};
	if (form.empty()) form = game == "OOT" ? "Adult" : "Human";
	auto upper = [](string v) { for (auto& ch : v) ch = (char)toupper((unsigned char)ch); return v; };
	struct Variant { string form; double radius, checkHeight; };
	vector<Variant> variants;
	{
		// --form All, or a list: --form Adult,Child
		vector<string> list;
		for (size_t a = 0; a <= form.size();) {
			size_t b = form.find(',', a);
			if (b == string::npos) b = form.size();
			string f = form.substr(a, b - a);
			if (!f.empty()) {
				if (upper(f) == "ALL") list.insert(list.end(), allForms.at(game).begin(), allForms.at(game).end());
				else list.push_back(f);
			}
			a = b + 1;
		}
		if (list.size() > 1 && radius != 0) { fprintf(stderr, "--radius can't be used with several forms\n"); return 2; }
		for (const string& f : list) {
			const string fu = upper(f);
			double r = radius;
			if (r == 0) {
				if (!radii.count(fu)) { fprintf(stderr, "unknown form %s (or pass --radius)\n", f.c_str()); return 2; }
				r = radii.at(fu);
			}
			// OoT's PLAYER_STATE2_CRAWLING checks walls at 15 instead of 26 (z_player.c)
			const bool crawl = game == "OOT" && (fu == "CRAWLSPACE" || fu == "CRAWL");
			variants.push_back({ f, r, crawl ? 15.0 : game == "OOT" ? 26.0 : F(F(268 * F(0.1))) });
		}
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
	// --after "<map>": resume an --all run, skipping the maps up to and
	// including that one
	if (!after.empty()) {
		auto it = std::find_if(todo.begin(), todo.end(), [&](const MapEntry& e) { return e.name == after; });
		if (it == todo.end()) { fprintf(stderr, "--after: no map named \"%s\" in the %s list\n", after.c_str(), game.c_str()); return 1; }
		todo.erase(todo.begin(), it + 1);
		fprintf(stderr, "starting after %s: %zu maps to go\n", after.c_str(), todo.size());
	}

	int failures = 0;
	for (const MapEntry& e : todo) {
		vector<uint8_t> buf;
		if (!readFile(root + "/models/" + game + "/" + e.file, buf)) { fprintf(stderr, "%s - %s: can't read models/%s/%s\n", game.c_str(), e.name.c_str(), game.c_str(), e.file.c_str()); failures++; continue; }
		ColHeader ch;
		vector<Tri> tris;
		try {
			if (!parseScene(buf, game, ch, tris)) { fprintf(stderr, "%s - %s: no collision header\n", game.c_str(), e.name.c_str()); failures++; continue; }
		} catch (const std::exception& ex) { fprintf(stderr, "%s - %s: bad scene file: %s\n", game.c_str(), e.name.c_str(), ex.what()); failures++; continue; }
		// The output file is opened before the scan, so a path that can't be
		// written (e.g. a missing directory) stops the run straight away
		// instead of after the scan, and a write that fails stops it too.
		string path = out;
		if (path.empty() || all) {
			string dir = outDir.empty() ? "." : outDir;
			path = dir + "/" + safeName(game + "_" + e.name + "_" + form) + (falling ? "_falling" : "") + (extendedOnly ? "_extended" : "") + ".json";
		}
		std::ofstream f(path, std::ios::binary);
		if (!f) {
			// (the full path: a Windows exe reads "/dir" as the root of the
			// current drive, not as relative to the current directory)
			std::error_code ec;
			string full = std::filesystem::absolute(path, ec).string();
			fprintf(stderr, "can't write %s (%s - does that directory exist?) - stopping\n", path.c_str(), ec ? path.c_str() : full.c_str());
			return 1;
		}
		// Forms with the same radius and check height (Human / Deku) share a
		// scan, listed once as "Human/Deku".
		vector<FormResult> results;
		for (const Variant& v : variants) {
			auto same = std::find_if(results.begin(), results.end(),
				[&](const FormResult& r) { return r.radius == v.radius && r.checkHeight == v.checkHeight; });
			if (same != results.end()) {
				fprintf(stderr, "%s - %s (%s): same radius and check height as %s, sharing its scan\n",
					game.c_str(), e.name.c_str(), v.form.c_str(), same->form.c_str());
				same->form += "/" + v.form;
				continue;
			}
			fprintf(stderr, "%s - %s (%s, radius %g%s)\n", game.c_str(), e.name.c_str(), v.form.c_str(), v.radius, falling ? ", falling" : "");
			Model m;
			initColCtx(m.colCtx, game, e.name, ch);
			initializeSubdivisions(m.colCtx, tris);
			m.radius = F(v.radius);
			m.checkHeight = F(v.checkHeight);
			m.lowDrop = falling ? 30 : 0;
			m.extendedOnly = extendedOnly;
			m.build(tris, ch.numPolygons);
			if (!simArg.empty()) return runSim(m, simArg);
			vector<Clip> found = scan(m, threads, firstPerPair);
			// --pair: just the clips of that wall pair
			if (onlyPusher >= 0) {
				found.erase(std::remove_if(found.begin(), found.end(),
					[&](const Clip& c) { return c.pusher != onlyPusher || c.crossed != onlyCrossed; }), found.end());
				fprintf(stderr, "  %zu clip points of TRI %d through TRI %d\n", found.size(), onlyPusher, onlyCrossed);
			}
			if (minSpeed && !found.empty()) {
				findMinSpeeds(m, found, threads);
				if (refine) {
					if (onlyPusher < 0) { fprintf(stderr, "--refine needs --pair PUSHER,CROSSED\n"); return 2; }
					auto t0r = std::chrono::steady_clock::now();
					Refined r = refineMinSpeed(m, found, onlyPusher, onlyCrossed, threads);
					if (!r.found) fprintf(stderr, "  refine: no walking clip of this pair to start from\n");
					else fprintf(stderr, "  REFINED TRI %d -> %d: min walking speed %.9g  start %.9g, %.9g, %.9g  yaw 0x%04X  -> end %.9g, %.9g, %.9g  (%d starts tried, %.1fs)\n",
						onlyPusher, onlyCrossed, r.speed, r.start.x, r.start.y, r.start.z, r.yaw & 0xFFFF, r.end.x, r.end.y, r.end.z,
						r.starts, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0r).count());
					if (angles && (r.found || haveFrom)) {
						auto ta = std::chrono::steady_clock::now();
						Refined from = r;
						if (haveFrom) {
							from.start = { F(fromX), F(fromY), F(fromZ) };
							if (fromSpeed > 0) from.speed = F(fromSpeed);
							else if (!r.found) from.speed = 0;
							Scratch s;
							s.stamp.assign(m.polys.size(), 0);
							auto rest = m.restingSpot(from.start);
							if (!rest || rest->x != from.start.x || rest->z != from.start.z)
								printf("(note: Link doesn't stand still at that start: the pushes move him%s)\n",
									rest ? (string(" to ") + std::to_string(rest->x) + ", " + std::to_string(rest->z)).c_str() : "");
							if (!m.isInBounds(s, from.start)) printf("(note: that start is out of bounds)\n");
						}
						angleRanges(m, from, onlyPusher, onlyCrossed, threads);
						fprintf(stderr, "  (angles: %.1fs)\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - ta).count());
					}
					if (r.found) {
						if (auto c = refinedClip(m, r, onlyPusher, onlyCrossed)) {
							c->kind = found.front().kind; // the pair's category
							found = { *c };
						}
					}
				}
			}
			results.push_back({ v.form, v.radius, F(v.checkHeight), std::move(found) });
		}
		f << toJson(game, e.name, ch.numPolygons, falling, extendedOnly, results);
		f.close();
		if (!f) {
			fprintf(stderr, "can't write %s - stopping\n", path.c_str());
			return 1;
		}
		fprintf(stderr, "  wrote %s\n", path.c_str());
	}
	return failures ? 1 : 0;
}

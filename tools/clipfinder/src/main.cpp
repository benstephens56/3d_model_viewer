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
//   collision.h/.cpp the game's collision checks (static and dynapoly), as a Model
//   dyna.h/.cpp      --dyna: the viewer's dynapoly export, added to the Model
//   frame.h/.cpp     one frame: does it clip; where Link can stand; --max-move
//   search.h/.cpp    the scan over a whole map
//   slope.h/.cpp     slope clips: the floor check lifting Link behind a wall
//   ground.h/.cpp    ground clips: falling fast through the floor, under a wall
//   reach.h/.cpp     --min-speed, --refine, --angles, --yaw
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
//   clipfinder --game MM --map "South Clock Town" --form Human --dyna MM_dyna_all.json [--setup N] [--dyna-only] -o sct.json
//     (--dyna: the scene's dynapoly actors too, from the viewer's "Export all dynapolys";
//      --dyna-only: just the wall pairs with a dynapoly wall in them)
//   clipfinder --game OOT --all --form All --dyna OOT_dyna_all.json --dyna-only --out-dir results/
//     (the viewer's "Export all dynapolys": each map scanned once per distinct
//      set of dynapolys its setups load, the output named and marked with the setups)
//     (--form All: every form's clips in the one JSON, each marked with its form;
//      --form Adult,Child: just those forms, the same way)
// Options: --root <viewer dir> (default: two levels up from the exe's dir, or
// the current dir if it has models/), --threads N, --radius R (overrides --form).
// Every option is explained in README.md next to this file.

#include "dyna.h"
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
	string game, mapName, form, out, outDir, root, after, dynaPath;
	bool dynaOnly = false, noSlope = false, slopeOnly = false, noGround = false, groundOnly = false, slopeStarts = false, keepLoadVoid = false;
	int onlySetup = -1;  // --setup N: just the dynapolys of that setup
	int maxPerPair = 0;  // --max-per-pair N: at most N points a wall pair, spread out (thinClips)
	double radius = 0;
	bool falling = false, all = false, extendedOnly = false, firstPerPair = false, minSpeed = false, refine = false, angles = false;
	bool angleSweep = false;  // --angles: every yaw that clips, each from its own start (like --yaw)
	int angleGap = 16;        // --angle-gap: --angles stops a way after this many yaws in a row that don't clip
	int onlyPusher = -1, onlyCrossed = -1;
	string simArg;  // --sim x,y,z,yaw,speed[,drop]
	string triArg;  // --tri ID[,ID...]: print those polys
	bool haveFrom = false;
	double fromX = 0, fromY = 0, fromZ = 0, fromSpeed = 0;  // --from
	int atYaw = -1, yawTo = -1;  // --yaw YAW or FROM-TO
	double maxSpeed = 0;    // --max-speed (with --yaw)
	double sideStep = 0.002;  // --side-step (with --yaw)
	bool exact = false;       // --exact (with --yaw)
	double gridSpeed = 0;     // --speed (with --yaw): the CSV at exactly this speed
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
		else if (a == "--max-per-pair") {
			maxPerPair = std::stoi(val());
			if (maxPerPair < 1) { fprintf(stderr, "--max-per-pair wants a number of points >= 1\n"); return 2; }
			MAX_PER_PAIR = maxPerPair;
		}
		else if (a == "--min-speed") minSpeed = true;
		else if (a == "--refine") { refine = true; minSpeed = true; }
		else if (a == "--angles") angleSweep = true;
		else if (a == "--angle-gap") {
			angleGap = std::stoi(val());
			if (angleGap < 1) { fprintf(stderr, "--angle-gap wants a number of yaws >= 1\n"); return 2; }
		}
		else if (a == "--from") {
			// every direction from this one start (and speed), after --refine
			string v = val();
			int n = sscanf(v.c_str(), "%lf,%lf,%lf,%lf", &fromX, &fromY, &fromZ, &fromSpeed);
			if (n < 3) { fprintf(stderr, "--from wants X,Y,Z[,SPEED]\n"); return 2; }
			haveFrom = true;
			angles = refine = minSpeed = true;
		}
		else if (a == "--yaw") {
			// YAW, or a range FROM-TO (going up from FROM, through 0xFFFF -> 0 if TO is below it)
			string v = val();
			char* end;
			atYaw = (int)strtol(v.c_str(), &end, 0) & 0xFFFF;
			yawTo = atYaw;
			if (*end == '-') yawTo = (int)strtol(end + 1, &end, 0) & 0xFFFF;
			if (end == v.c_str() || *end) { fprintf(stderr, "--yaw wants YAW or FROM-TO, e.g. 0xFFC0 or 0xFF80-0x0040\n"); return 2; }
		}
		else if (a == "--max-speed") {
			maxSpeed = std::stod(val());
			if (!(maxSpeed > 0)) { fprintf(stderr, "--max-speed wants a speed > 0\n"); return 2; }
		}
		else if (a == "--side-step") {
			sideStep = std::stod(val());
			if (!(sideStep > 0 && sideStep <= 3)) { fprintf(stderr, "--side-step wants a distance > 0 and <= 3\n"); return 2; }
		}
		else if (a == "--exact") exact = true;
		else if (a == "--speed") {
			gridSpeed = std::stod(val());
			if (!(gridSpeed > 0)) { fprintf(stderr, "--speed wants a speed > 0\n"); return 2; }
		}
		else if (a == "--sim") simArg = val();
		else if (a == "--tri") triArg = val();
		else if (a == "--dyna") dynaPath = val();
		else if (a == "--dyna-only") dynaOnly = true;
		else if (a == "--no-slope") noSlope = true;
		else if (a == "--slope-only") slopeOnly = true;
		else if (a == "--slope-starts") slopeStarts = true;
		else if (a == "--no-ground") noGround = true;
		else if (a == "--ground-only") groundOnly = true;
		else if (a == "--keep-load-void") keepLoadVoid = true;
		else if (a == "--setup") {
			// one setup (std::stoi took "0,1,2,3" as 0 without a word)
			string v = val();
			char* end;
			long n = strtol(v.c_str(), &end, 10);
			if (v.empty() || *end || n < 0) {
				fprintf(stderr, "--setup wants one setup number, e.g. --setup 2 (OoT: leave it out and each form gets the setups it plays in)\n");
				return 2;
			}
			onlySetup = (int)n;
		}
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
	if (noSlope && slopeOnly) { fprintf(stderr, "--no-slope and --slope-only can't go together\n"); return 2; }
	if (noGround && groundOnly) { fprintf(stderr, "--no-ground and --ground-only can't go together\n"); return 2; }
	if (slopeOnly && groundOnly) { fprintf(stderr, "--slope-only and --ground-only can't go together\n"); return 2; }
	if (exact && atYaw < 0) { fprintf(stderr, "--exact needs --yaw\n"); return 2; }
	if (gridSpeed > 0 && atYaw < 0 && !angleSweep) { fprintf(stderr, "--speed needs --yaw or --angles\n"); return 2; }
	// (--speed without --max-speed: the search for starts goes up to that speed)
	if (gridSpeed > 0 && maxSpeed <= 0) maxSpeed = gridSpeed;
	if (atYaw >= 0 && (onlyPusher < 0 || maxSpeed <= 0)) { fprintf(stderr, "--yaw needs --pair PUSHER,CROSSED and --max-speed S\n"); return 2; }
	if (atYaw >= 0 && minSpeed) { fprintf(stderr, "--yaw can't be used with --min-speed / --refine / --angles / --from\n"); return 2; }
	// --angles --from X,Y,Z: every direction from that one start (the old --angles)
	if (angleSweep && haveFrom) angleSweep = false;
	if (angleSweep && (onlyPusher < 0 || maxSpeed <= 0)) { fprintf(stderr, "--angles needs --pair PUSHER,CROSSED and --max-speed S\n"); return 2; }
	if (angleSweep && (atYaw >= 0 || minSpeed)) { fprintf(stderr, "--angles can't be used with --yaw / --min-speed / --refine\n"); return 2; }
	for (auto& ch : game) ch = (char)toupper((unsigned char)ch);
	if ((game != "OOT" && game != "MM") || (mapName.empty() && !all)) {
		fprintf(stderr,
			"usage: clipfinder --game OOT|MM (--map \"<name in the viewer's map list>\" | --all)\n"
			"                  [--form Adult|Child|Crawlspace|Human|Deku|Zora|Goron|FierceDeity|All, or a list: Adult,Child] [--radius R] [--falling] [--extended-only] [--first-per-pair]\n"
			"                  [--min-speed] [--pair PUSHER,CROSSED] [--refine (with --pair: the exact lowest walking speed)]\n"
			"                  [--angles (with --pair: also every yaw that works from the refined start)]\n"
			"                  [--from X,Y,Z[,SPEED] (--angles from this start instead, and at this speed)]\n"
			"                  [--yaw YAW|FROM-TO --max-speed S (with --pair: the lowest speed up to S that clips moving at exactly YAW, or at each yaw FROM-TO every 0x10)]\n"
			"                  [--side-step D (with --yaw: starts every D across the yaw, default 0.002)]\n"
			"                  [--exact (with --yaw: then every f32 x, z around each region found)]\n"
			"                  [--speed S (with --yaw: the CSV grids at exactly speed S; stands in for --max-speed)]\n"
			"                  [--sim X,Y,Z,YAW,SPEED[,DROP | ,vVY]]  (one frame from a standing start, printed step by step; SPEED as 15/7: a frame per speed)\n"
			"                  [--tri ID[,ID...]]  (print those polys: vertices, normal, type)\n"
			"                  [--max-move N]  (units Link can move in one frame: default 45, speed 30)\n"
			"                  [--dyna FILE [--dyna-only] [--setup N]]  (the viewer's dynapoly export, one map's or every map's: the actors' collision too)\n"
			"                  [--no-slope | --slope-only] [--slope-starts] [--keep-load-void]  (slope clips: the floor check lifting Link behind a wall)\n"
			"                  [--no-ground | --ground-only]  (ground clips: falling at velocity.y -20 from the floor, through it and under a wall)\n"
			"                  [--max-per-pair N]  (at most N points per wall pair, spread out evenly: smaller files)\n"
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

	// --dyna: the dynapoly actors the viewer loads: one map's ("Export
	// dynapolys") or every map's, per setup ("Export all dynapolys"). Each map
	// is scanned once per export of it (a set of setups with the same
	// dynapolys); a map without one once without dynapolys (not at all with
	// --dyna-only).
	vector<DynaFile> dynas;
	if (dynaOnly && dynaPath.empty()) { fprintf(stderr, "--dyna-only needs --dyna FILE\n"); return 2; }
	if (onlySetup >= 0 && dynaPath.empty()) { fprintf(stderr, "--setup needs --dyna FILE\n"); return 2; }
	if (!dynaPath.empty()) {
		string err;
		if (!readDynaFile(dynaPath, dynas, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
		for (const DynaFile& d : dynas)
			if (!d.game.empty() && upper(d.game) != game) { fprintf(stderr, "%s is a %s export, not %s\n", dynaPath.c_str(), d.game.c_str(), game.c_str()); return 1; }
		// one map's export with --map: it has to be that map's
		if (!all && dynas.size() == 1 && !dynas[0].map.empty() && dynas[0].map != mapName) {
			fprintf(stderr, "%s is %s's export, not %s's\n", dynaPath.c_str(), dynas[0].map.c_str(), mapName.c_str());
			return 1;
		}
		if (onlySetup >= 0) {
			dynas.erase(std::remove_if(dynas.begin(), dynas.end(), [&](const DynaFile& d) {
				return std::find(d.setups.begin(), d.setups.end(), onlySetup) == d.setups.end();
			}), dynas.end());
		}
		size_t n = 0, acts = 0;
		for (const DynaFile& d : dynas) {
			acts += d.actors.size();
			for (const DynaActorIn& a : d.actors) n += a.polys.size();
		}
		fprintf(stderr, "dynapolys: %zu exports, %zu actors, %zu polys from %s\n", dynas.size(), acts, n, dynaPath.c_str());
		if (!all && std::none_of(dynas.begin(), dynas.end(), [&](const DynaFile& d) { return d.map.empty() || d.map == mapName; })) {
			// (not an error: the scan goes on without dynapolys - with
			// --dyna-only there's nothing to scan, and the map is skipped)
			fprintf(stderr, "warning: %s has no dynapolys for %s%s - scanning %s\n", dynaPath.c_str(), mapName.c_str(),
				onlySetup >= 0 ? (" setup " + std::to_string(onlySetup)).c_str() : "",
				dynaOnly ? "nothing (--dyna-only)" : "without them");
		}
	}
	const DynaFile noDyna;

	// OoT: which setups each scene has (the viewer's setup list), so each form
	// is scanned with the dynapolys of the setups it plays in
	std::map<string, vector<bool>> sceneSetups;
	if (game == "OOT" && !dynas.empty() && onlySetup < 0) {
		string err;
		if (!readSceneSetups(root + "/models/OOT/actors/OOT_actors_by_scene.json", sceneSetups, err))
			fprintf(stderr, "%s - every form gets every setup's dynapolys\n", err.c_str());
	}
	// The setups a form plays in, empty for any: OoT child (and crawling)
	// setups 0 / 1, adult 2 / 3 (SCENE_LAYER_*), one the scene doesn't have
	// resolved the way Scene_CommandAlternateHeaderList does - adult night
	// falls back to adult day, anything else to setup 0. Cutscene setups (4+)
	// only with --setup, which also takes any form.
	auto formSetups = [&](const Variant& v, const string& sceneFile) {
		vector<int> out;
		auto it = sceneSetups.find(sceneFile);
		if (it == sceneSetups.end()) return out;
		const vector<bool>& present = it->second;
		auto has = [&](int l) { return l < (int)present.size() && present[l]; };
		const string fu = upper(v.form);
		vector<int> want;
		if (fu == "CHILD" || fu == "CRAWLSPACE" || fu == "CRAWL") want = { 0, 1 };
		else if (fu == "ADULT") want = { 2, 3 };
		else return out;
		for (int l : want) {
			int r = l == 0 || has(l) ? l : l == 3 && has(2) ? 2 : 0;
			if (std::find(out.begin(), out.end(), r) == out.end()) out.push_back(r);
		}
		return out;
	};

	int failures = 0;
	for (const MapEntry& e : todo) {
		vector<uint8_t> buf;
		if (!readFile(root + "/models/" + game + "/" + e.file, buf)) { fprintf(stderr, "%s - %s: can't read models/%s/%s\n", game.c_str(), e.name.c_str(), game.c_str(), e.file.c_str()); failures++; continue; }
		ColHeader ch;
		vector<Tri> tris;
		try {
			if (!parseScene(buf, game, ch, tris)) { fprintf(stderr, "%s - %s: no collision header\n", game.c_str(), e.name.c_str()); failures++; continue; }
		} catch (const std::exception& ex) { fprintf(stderr, "%s - %s: bad scene file: %s\n", game.c_str(), e.name.c_str(), ex.what()); failures++; continue; }
		// this map's dynapoly exports, each with the forms that play in its
		// setups (OoT: formSetups), and the forms none of them is for without
		// dynapolys (the static scan; not with --dyna-only)
		struct Job { const DynaFile* dyna; vector<const Variant*> forms; };
		vector<Job> jobs;
		vector<bool> covered(variants.size(), false);
		if (!sceneSetups.empty()) {
			string line;
			for (const Variant& v : variants) {
				vector<int> ls = formSetups(v, e.file);
				if (ls.empty()) continue;
				line += (line.empty() ? "" : ", ") + v.form + " setup" + (ls.size() > 1 ? "s " : " ");
				for (size_t k = 0; k < ls.size(); k++) line += (k ? "/" : "") + std::to_string(ls[k]);
			}
			if (!line.empty()) fprintf(stderr, "%s - %s: %s (--setup N for another)\n", game.c_str(), e.name.c_str(), line.c_str());
		}
		for (const DynaFile& d : dynas) {
			if (!d.map.empty() && d.map != e.name) continue;
			Job j{ &d, {} };
			for (size_t k = 0; k < variants.size(); k++) {
				vector<int> ls = formSetups(variants[k], e.file);
				bool in = ls.empty() || d.setups.empty() ||
					std::any_of(ls.begin(), ls.end(), [&](int l) { return std::find(d.setups.begin(), d.setups.end(), l) != d.setups.end(); });
				if (in) { j.forms.push_back(&variants[k]); covered[k] = true; }
			}
			if (!j.forms.empty()) jobs.push_back(j);
		}
		{
			Job j{ &noDyna, {} };
			for (size_t k = 0; k < variants.size(); k++) if (!covered[k]) j.forms.push_back(&variants[k]);
			if (!j.forms.empty()) {
				if (dynaOnly) {
					string names;
					for (const Variant* v : j.forms) names += (names.empty() ? "" : ", ") + v->form;
					fprintf(stderr, "%s - %s (%s): no dynapolys in %s setups, skipped\n", game.c_str(), e.name.c_str(), names.c_str(),
						j.forms.size() > 1 ? "their" : "its");
				} else jobs.push_back(j);
			}
		}
		if (jobs.empty()) continue;
		for (const Job& job : jobs) {
			const DynaFile& dyna = *job.dyna;
			if (dyna.numPolygons >= 0 && dyna.numPolygons != ch.numPolygons) {
				fprintf(stderr, "%s: %s was exported with %d static polys, the scene file has %d\n", dynaPath.c_str(), e.name.c_str(), dyna.numPolygons, ch.numPolygons);
				return 1;
			}
			// the setups these dynapolys are for: "_setup0-2" in file names
			string setupTag;
			for (size_t k = 0; k < dyna.setups.size(); k++) setupTag += (k ? "-" : "_setup") + std::to_string(dyna.setups[k]);
			if (!dyna.setups.empty())
				fprintf(stderr, "%s - %s: setup%s %s: %zu dynapoly actors\n", game.c_str(), e.name.c_str(), dyna.setups.size() > 1 ? "s" : "",
					setupTag.substr(6).c_str(), dyna.actors.size());
			// The output file is opened before the scan, so a path that can't be
			// written (e.g. a missing directory) stops the run straight away
			// instead of after the scan, and a write that fails stops it too.
			string path = out;
			if (path.empty() || all) {
				string dir = outDir.empty() ? "." : outDir;
				path = dir + "/" + safeName(game + "_" + e.name + "_" + form) + (falling ? "_falling" : "") + (extendedOnly ? "_extended" : "") + (firstPerPair ? "_first" : "") + (slopeOnly ? "_slope" : groundOnly ? "_ground" : "") + setupTag + (dyna.raw.empty() ? "" : "_dyna") +
					// (--pair: its own file, not over the whole map's scan)
					(onlyPusher >= 0 ? "_pair" + std::to_string(onlyPusher) + "-" + std::to_string(onlyCrossed) : "") + ".json";
			}
			// -o with several exports of the map: one file each
			else if (jobs.size() > 1) {
				const bool js = path.size() > 5 && path.compare(path.size() - 5, 5, ".json") == 0;
				path = (js ? path.substr(0, path.size() - 5) : path) + setupTag + ".json";
			}
			// (--sim writes nothing, so it doesn't open, and leave empty, the file)
			std::ofstream f;
			if (simArg.empty() && triArg.empty()) f.open(path, std::ios::binary);
			if (simArg.empty() && triArg.empty() && !f) {
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
			vector<string> csvPaths;  // --yaw: the CSV written per yaw
			for (const Variant* vp : job.forms) {
				const Variant& v = *vp;
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
				addDynaActors(m, dyna);
				m.dynaPairsOnly = dynaOnly;
				m.slope = !noSlope;
				m.slopeOnly = slopeOnly;
				m.slopeStarts = slopeStarts;
				m.ground = !noGround;
				m.groundOnly = groundOnly;
				m.keepLoadVoid = keepLoadVoid;
				// --pair: the scan only looks near those two polys
				if (onlyPusher >= 0) {
					if (onlyPusher >= (int)m.polys.size() || onlyCrossed >= (int)m.polys.size() || !m.polys[onlyPusher].exists || !m.polys[onlyCrossed].exists) {
						fprintf(stderr, "--pair %d,%d: no such polys in this map (%zu)\n", onlyPusher, onlyCrossed, m.polys.size());
						return 2;
					}
					m.focusA = onlyPusher;
					m.focusB = onlyCrossed;
				}
				if (!simArg.empty()) return runSim(m, simArg);
				if (!triArg.empty()) return printTris(m, triArg);
				vector<Clip> found = scan(m, threads, firstPerPair);
				// --pair: just the clips of that wall pair
				if (onlyPusher >= 0) {
					found.erase(std::remove_if(found.begin(), found.end(),
						[&](const Clip& c) { return c.pusher != onlyPusher || c.crossed != onlyCrossed; }), found.end());
					fprintf(stderr, "  %zu clip points of TRI %d through TRI %d\n", found.size(), onlyPusher, onlyCrossed);
					if (found.empty() && m.polys[onlyPusher].isWall && !m.polys[onlyCrossed].isWall)
						fprintf(stderr, "  (TRI %d isn't a wall: for a slope / ground clip --pair is the floor first, then the wall: --pair %d,%d)\n",
							onlyCrossed, onlyCrossed, onlyPusher);
				}
				// --yaw / --max-speed: that pair at each yaw, from any start. A
				// range goes every 0x10 (the sine table ignores the low 4 bits).
				// --angles: the same for every yaw that clips, found by walking out
				// from the yaws of the scan's clip points (see below).
				if ((atYaw >= 0 || angleSweep) && !found.empty()) {
					// (only clips at those yaws are written: none found, no clips)
					vector<Clip> atYaws;
					vector<Refined> rs;
					std::map<int, size_t> tried;  // yaw -> its index in rs
					// --speed S: per yaw, a start that clips at exactly S (startAtSpeed)
					std::map<int, std::optional<V3>> atSpeed;
					auto doYaw = [&](int yaw) -> bool {
						auto ty = std::chrono::steady_clock::now();
						Refined r = clipAtYaw(m, found, onlyPusher, onlyCrossed, yaw, maxSpeed, sideStep, exact, gridSpeed, threads, !angleSweep);
						double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - ty).count();
						if (!r.found) fprintf(stderr, "  YAW 0x%04X TRI %d -> %d: no clip at speeds up to %g (%d starts tried, %.1fs)\n",
							yaw, onlyPusher, onlyCrossed, maxSpeed, r.starts, secs);
						else {
							fprintf(stderr, "  YAW 0x%04X TRI %d -> %d: min speed %.9g  start %.9g, %.9g, %.9g  -> end %.9g, %.9g, %.9g  (%d starts tried, %.1fs)\n",
								yaw, onlyPusher, onlyCrossed, r.speed, r.start.x, r.start.y, r.start.z, r.end.x, r.end.y, r.end.z, r.starts, secs);
							Refined rc = r;
							if (gridSpeed > 0) {
								// (the JSON's clip is the move at exactly S, for the tester)
								auto st = startAtSpeed(m, r, yaw, gridSpeed, onlyPusher, onlyCrossed);
								atSpeed[yaw] = st;
								if (st) fprintf(stderr, "    at exactly speed %.9g: start %.9g, %.9g, %.9g\n", F(gridSpeed), st->x, st->y, st->z);
								else fprintf(stderr, "    at exactly speed %.9g: no start found\n", F(gridSpeed));
								if (st) { rc.start = *st; rc.speed = F(gridSpeed); }
							}
							if (gridSpeed <= 0 || atSpeed[yaw]) {
								if (auto c = refinedClip(m, rc, onlyPusher, onlyCrossed)) {
									c->kind = found.front().kind; // the pair's category
									atYaws.push_back(*c);
								}
							}
						}
						tried[yaw] = rs.size();
						rs.push_back(r);
						return r.found;
					};
					if (atYaw >= 0) {
						const int n = ((yawTo - atYaw) & 0xFFFF) / 16 + 1;
						for (int k = 0; k < n; k++) doYaw((atYaw + k * 16) & 0xFFFF);
					} else {
						// Every yaw the scan's clip points were found moving at, then
						// out from each, 0x10 at a time both ways, until --angle-gap (16) yaws
						// in a row don't clip: the yaws that work come in runs, with
						// the odd gap (Treasure Chest Shop 50 -> 90 at speed 11:
						// 0xFF40-0xFFE0, then 0x0010 on, not 0xFFF0 / 0x0000).
						// The refined yaw (--refine: the lowest speed there is) first,
						// then the scan's clip moves already at speed S or less,
						// nearest it first. Only a seed that clips is walked out from.
						// (Not every yaw in the scan points' lists: those come from 32
						// start directions at any speed up to 30, and on Treasure
						// Chest Shop 50 -> 90 at 10 all 28 of them failed, 7 s each.)
						const int ANGLE_GAP = angleGap;
						std::set<int> seedSet;
						for (const Clip& c : found)
							if (c.hasMove && c.drop == 0 && c.speed <= maxSpeed) seedSet.insert(c.yaw & 0xFFF0);
						int best = -1;
						if (std::any_of(found.begin(), found.end(), [](const Clip& c) { return c.drop == 0; })) {
							findMinSpeeds(m, found, threads);
							Refined rf = refineMinSpeed(m, found, onlyPusher, onlyCrossed, threads);
							if (rf.found) {
								best = rf.yaw & 0xFFF0;
								fprintf(stderr, "  refined: min speed %.9g at yaw 0x%04X - starting there\n", rf.speed, rf.yaw & 0xFFFF);
							}
						}
						vector<int> seeds(seedSet.begin(), seedSet.end());
						if (best >= 0) {
							auto dist = [&](int y) { int d = (y - best) & 0xFFFF; return std::min(d, 0x10000 - d); };
							std::stable_sort(seeds.begin(), seeds.end(), [&](int a, int b) { return dist(a) < dist(b); });
							seeds.erase(std::remove(seeds.begin(), seeds.end(), best), seeds.end());
							seeds.insert(seeds.begin(), best);
						}
						for (int seed : seeds) {
							if (tried.count(seed)) continue;
							if (!doYaw(seed)) continue;
							for (int dir : { 1, -1 }) {
								int misses = 0, y = seed;
								while (misses < ANGLE_GAP) {
									y = (y + dir * 16) & 0xFFFF;
									if (tried.count(y)) break;
									misses = doYaw(y) ? 0 : misses + 1;
								}
							}
						}
						// in yaw order, starting after the widest gap (a run through 0 in one piece)
						vector<Refined> sorted;
						for (auto& [y, i] : tried) sorted.push_back(rs[i]);
						size_t startAt = 0;
						int widest = -1;
						for (size_t i = 0; i < sorted.size(); i++) {
							int prev = sorted[(i + sorted.size() - 1) % sorted.size()].yaw;
							int gap = (sorted[i].yaw - prev) & 0xFFFF;
							if (sorted.size() == 1) gap = 0x10000;
							if (gap > widest) { widest = gap; startAt = i; }
						}
						std::rotate(sorted.begin(), sorted.begin() + startAt, sorted.end());
						rs = std::move(sorted);
						// the runs that clip, on one line
						string runs;
						for (size_t i = 0; i < rs.size(); i++) {
							auto ok = [&](size_t k) { return rs[k].found && (gridSpeed <= 0 || atSpeed[rs[k].yaw]); };
							if (!ok(i)) continue;
							size_t j = i;
							while (j + 1 < rs.size() && ok(j + 1) && ((rs[j + 1].yaw - rs[j].yaw) & 0xFFFF) == 16) j++;
							char b[40];
							snprintf(b, sizeof b, "%s0x%04X-0x%04X", runs.empty() ? "" : ", ", rs[i].yaw, rs[j].yaw | 0xF);
							runs += b;
							i = j;
						}
						if (gridSpeed > 0) printf("\nYaws that clip at exactly speed %.9g: %s\n", F(gridSpeed), runs.empty() ? "none" : runs.c_str());
						else printf("\nYaws that clip at speed up to %g: %s\n", maxSpeed, runs.empty() ? "none" : runs.c_str());
					}
					// every yaw's answer together, on stdout, one row each: its
					// minimum speed and the x / z range of all the starts that clip at
					// up to maxSpeed. Several separate regions: each on a row below.
					printf("\nTRI %d -> %d, %s, speed up to %g\n", onlyPusher, onlyCrossed, v.form.c_str(), maxSpeed);
					// One start per yaw, one that clips at its lowest speed: not the
					// starts' bounding box, whose points mostly don't (the starts that
					// work are thin strips, and a box around them from far apart yaws
					// or regions says nothing). The exact f32s: positions to set Link
					// at, where 4 decimals is off by more than the clip's window.
					printf("  yaw      min speed    start (x, y, z)\n");
					for (const Refined& r : rs) {
						if (!r.found) { printf("  0x%04X   none\n", r.yaw); continue; }
						printf("  0x%04X   %-11.9g  %.9g, %.9g, %.9g\n", r.yaw, r.speed, r.start.x, r.start.y, r.start.z);
						if (gridSpeed > 0) {
							auto it = atSpeed.find(r.yaw);
							if (it != atSpeed.end() && it->second)
								printf("    at exactly %.9g: start %.9g, %.9g, %.9g\n", F(gridSpeed), it->second->x, it->second->y, it->second->z);
							else printf("    at exactly %.9g: no start found\n", F(gridSpeed));
						}
						if (r.regions.size() > 1)
							for (const StartRegion& g : r.regions)
								printf("    region %-11.9g  %.9g, %.9g, %.9g\n", g.speed, g.start.x, g.start.y, g.start.z);
					}
					// then a CSV per yaw that clips, <output>_<YAW>.csv (with the form
					// too when there are several): a grid of round x (columns) and z
					// (rows) values over its starts that clip, each Yes if Link
					// standing exactly there clips at some speed up to maxSpeed, else No.
					// And <output>_<YAW>_speeds.csv, the same grid with the speed to
					// check each cell at in game (wall_clip_tester.lua CSV_TESTS): a
					// Yes cell's lowest speed that clips, a No cell's maxSpeed.
					const string base = path.size() > 5 && path.compare(path.size() - 5, 5, ".json") == 0 ? path.substr(0, path.size() - 5) : path;
					auto writeGrid = [](const string& file, const YawGrid& G, const std::function<string(size_t)>& cell) {
						FILE* cf = fopen(file.c_str(), "w");
						if (!cf) return false;
						fprintf(cf, "z \\ x");
						for (double x : G.xs) fprintf(cf, ",%.*f", G.xDecimals, x);
						fprintf(cf, "\n");
						for (size_t zi = 0; zi < G.zs.size(); zi++) {
							fprintf(cf, "%.*f", G.zDecimals, G.zs[zi]);
							for (size_t xi = 0; xi < G.xs.size(); xi++) fprintf(cf, ",%s", cell(zi * G.xs.size() + xi).c_str());
							fprintf(cf, "\n");
						}
						bool bad = ferror(cf) != 0;
						return fclose(cf) == 0 && !bad;
					};
					// (--angles: no CSVs - a grid per yaw is --yaw FROM-TO's job)
					for (const Refined& r : rs) {
						if (!r.found || angleSweep) continue;
						char yawName[8];
						snprintf(yawName, sizeof yawName, "%04X", r.yaw);
						const string csvBase = base + (variants.size() > 1 ? "_" + safeName(v.form) : "") + "_" + yawName;
						const YawGrid& G = r.grid;
						if (!writeGrid(csvBase + ".csv", G, [&](size_t i) { return string(G.ok[i] ? "Yes" : "No"); })) {
							fprintf(stderr, "can't write %s.csv - stopping\n", csvBase.c_str());
							return 1;
						}
						if (!writeGrid(csvBase + "_speeds.csv", G, [&](size_t i) {
							char b[40];
							snprintf(b, sizeof b, "%.9g", G.ok[i] ? G.speed[i] : F(gridSpeed > 0 ? gridSpeed : maxSpeed));
							return string(b);
						})) {
							fprintf(stderr, "can't write %s_speeds.csv - stopping\n", csvBase.c_str());
							return 1;
						}
						csvPaths.push_back(csvBase + ".csv");
					}
					found = std::move(atYaws);
				}
				else if (minSpeed && !found.empty()) {
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
								if (!m.isInBounds(s, from.start, true)) printf("(note: that start is out of bounds)\n");
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
				// --max-per-pair: after --min-speed, so the slowest reach is kept
				// (not for --refine / --yaw / --angles: their clips are the answer)
				if (maxPerPair > 0 && !refine && atYaw < 0 && !angleSweep) {
					size_t before = found.size(), dropped = thinClips(found, maxPerPair);
					if (dropped) fprintf(stderr, "  --max-per-pair %d: kept %zu of %zu clip points\n", maxPerPair, found.size(), before);
				}
				results.push_back({ v.form, v.radius, F(v.checkHeight), std::move(found) });
			}
			f << toJson(game, e.name, ch.numPolygons, falling, extendedOnly, results, dyna.raw, dyna.setups);
			f.close();
			if (!f) {
				fprintf(stderr, "can't write %s - stopping\n", path.c_str());
				return 1;
			}
			fprintf(stderr, "  wrote %s\n", path.c_str());
			if (!csvPaths.empty()) {
				// (on stdout too, after the table)
				printf("\nA grid of the positions that clip, per yaw:\n");
				for (const string& c : csvPaths) { printf("  %s\n", c.c_str()); fprintf(stderr, "  wrote %s\n", c.c_str()); }
			}
		}
	}
	return failures ? 1 : 0;
}

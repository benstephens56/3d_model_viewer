#include "output.h"

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


// Format 2: every form's clips in one file, each clip marked with its form
// (one of `forms`), so the viewer can show them all at once.
string toJson(const string& game, const string& map, int numPolygons, bool falling, bool extendedOnly,
	const vector<FormResult>& forms, const string& dynaRaw) {
	static const char* kinds[] = { "acute", "extended" };
	std::ostringstream o;
	o << "{\n  \"format\": \"wall-push-clips-2\",\n";
	o << "  \"game\": " << jsonStr(game) << ", \"map\": " << jsonStr(map)
		<< ", \"falling\": " << (falling ? "true" : "false") << ", \"extendedOnly\": " << (extendedOnly ? "true" : "false") << ", \"numPolygons\": " << numPolygons;
	if (REACH_DIST != DEFAULT_MAX_MOVE) o << ", \"maxMove\": " << num(REACH_DIST);
	o << ",\n";
	o << "  \"forms\": [";
	for (size_t i = 0; i < forms.size(); i++) {
		o << (i ? ",\n    " : "\n    ") << "{\"form\":" << jsonStr(forms[i].form) << ",\"radius\":" << num(forms[i].radius)
			<< ",\"checkHeight\":" << num(forms[i].checkHeight) << "}";
	}
	o << "\n  ],\n";
	o << "  \"clips\": [";
	bool first = true;
	for (const FormResult& fr : forms) for (const Clip& c : fr.clips) {
		o << (first ? "\n    " : ",\n    ");
		first = false;
		o << "{\"form\":" << jsonStr(fr.form) << ",\"kind\":\"" << kinds[c.kind] << "\",\"cross\":" << (c.cross ? "true" : "false")
			<< ",\"drop\":" << c.drop << ",\"pusher\":" << c.pusher << ",\"crossed\":" << c.crossed
			<< ",\"from\":" << vec(c.from) << ",\"prev\":" << vec(c.prev);
		if (c.hasNext) o << ",\"next\":" << vec(c.next);
		o << ",\"res\":" << vec(c.res) << ",\"end\":" << vec(c.end);
		if (c.endNoFloor) o << ",\"endNoFloor\":true";
		// only with the stick held one more frame (the same yaw and speed)
		if (c.hold) o << ",\"hold\":true";
		if (c.hasFloorY) o << ",\"floorY\":" << num(c.floorY);
		if (c.cross) {
			o << ",\"yaws\":[";
			for (size_t k = 0; k < c.yaws.size(); k++) o << (k ? "," : "") << c.yaws[k];
			o << "]";
		}
		if (c.hasMove) o << ",\"yaw\":" << c.yaw << ",\"speed\":" << num(c.speed);
		// --min-speed: the slowest move that does it, or null for none
		if (c.reachDone) {
			if (c.hasReach) o << ",\"reach\":{\"speed\":" << num(c.reachSpeed) << ",\"yaw\":" << c.reachYaw << ",\"start\":" << vec(c.reachStart) << "}";
			else o << ",\"reach\":null";
		}
		o << "}";
	}
	o << "\n  ]";
	// --dyna: the export as read, so the viewer rebuilds the same dynapolys
	// (and poly ids) when it imports the results
	if (!dynaRaw.empty()) {
		size_t e = dynaRaw.find_last_not_of(" \t\r\n");
		o << ",\n  \"dyna\": " << dynaRaw.substr(0, e + 1);
	}
	o << "\n}\n";
	return o.str();
}

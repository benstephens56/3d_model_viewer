// clipfinder: the results JSON (format wall-push-clips-2).
#pragma once

#include "search.h"

// One scan's results: the form name (forms sharing a radius and check height
// share a scan: "Human/Deku"), its radius and check height, and its clips.
struct FormResult {
	string form;
	double radius, checkHeight;
	vector<Clip> clips;
};

// Format 2: every form's clips in one file, each clip marked with its form
// (one of `forms`), so the viewer can show them all at once.
string toJson(const string& game, const string& map, int numPolygons, bool falling, bool extendedOnly,
	const vector<FormResult>& forms);

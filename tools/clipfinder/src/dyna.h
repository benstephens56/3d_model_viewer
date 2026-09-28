// clipfinder: the dynapoly actors a scene has loaded, from the viewer's
// "Export all dynapolys" (--dyna FILE), added to a Model as bg actors.
#pragma once

#include "collision.h"

struct DynaActorIn {
	string name;
	double center[3], radius, minY, maxY;
	vector<Model::DynaPolyIn> polys;
};

struct DynaFile {
	string game, map;
	int numPolygons = -1;  // the scene's static poly count the export was made with
	vector<int> setups;    // the scene setups that load these dynapolys (none: not recorded)
	string raw;            // the export as read, passed through to the results
	vector<DynaActorIn> actors;
};

// Reads a dynapoly export: every map's and setup's (dynapoly-set-1, "Export
// all dynapolys": one dynapoly-1 per distinct set of a map's dynapolys, each
// with its setups), or one of those dynapoly-1s on its own (e.g. the "dyna" a
// results file carries, or a file from the old single-map export).
// false (with `err` set) if it can't.
bool readDynaFile(const string& path, vector<DynaFile>& out, string& err);

// Adds every actor to the model, in the file's order (bgId order).
void addDynaActors(Model& m, const DynaFile& d);

// Which setups each scene has (the viewer's setup list): per scene file name,
// present[i] for setup i. false (with `err` set) if the file can't be read.
bool readSceneSetups(const string& path, std::map<string, vector<bool>>& out, string& err);

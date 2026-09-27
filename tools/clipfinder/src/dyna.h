// clipfinder: the dynapoly actors a scene has loaded, from the viewer's
// "Export dynapolys" (--dyna FILE), added to a Model as bg actors.
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
	string raw;            // the file as read, passed through to the results
	vector<DynaActorIn> actors;
};

// Reads a dynapoly export; false (with `err` set) if it can't.
bool readDynaFile(const string& path, DynaFile& out, string& err);

// Adds every actor to the model, in the file's order (bgId order).
void addDynaActors(Model& m, const DynaFile& d);

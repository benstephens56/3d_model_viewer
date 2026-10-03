// clipfinder: the scene file's collision header (parse_model.js) and its subdivisions (subdivisions.js).
#pragma once

#include "common.h"

struct Tri {
	int id;
	int v[3][3];
	int n[3];
	double d; // the plane distance: s16 on N64, f32 on 3DS
	uint32_t surf0 = 0; // its SurfaceType's data[0] (exit index, floor property, ...)
	uint32_t surf1 = 0; // and data[1] (floor effect, ...)
};
struct ColHeader {
	int minB[3], maxB[3];
	int numPolygons = 0;
	int camType = -1;
};
struct ColCtx {
	int amt[3];
	double minB[3], maxB[3], len[3], inv[3];
	vector<vector<int>> subFloors, subWalls; // poly ids, in poly index order
};
struct CellIdx { int sx, sy, sz, index; };
inline CellIdx pointCell(const ColCtx& c, double x, double y, double z) {
	double p[3] = { x, y, z };
	int s[3];
	for (int ax = 0; ax < 3; ax++) {
		double d = F(F(p[ax]) - F(c.minB[ax]));
		s[ax] = toI32(F(d * c.inv[ax]));
		s[ax] = std::min(std::max(s[ax], 0), c.amt[ax] - 1);
	}
	return { s[0], s[1], s[2], s[2] * c.amt[0] * c.amt[1] + s[1] * c.amt[0] + s[0] };
}

// game: OOT / MM (N64 scene files) or OOT3D / MM3D (.zsi, little-endian).
// mapName: Termina Field (Credits Cutscene 2)'s .zsi has its commands further in.
bool parseScene(const vector<uint8_t>& buf, const string& game, const string& mapName, ColHeader& ch, vector<Tri>& tris);

void initColCtx(ColCtx& c, const string& game, const string& mapName, const ColHeader& ch);

void initializeSubdivisions(ColCtx& c, const vector<Tri>& tris);

// The subdivisions a poly goes in (StaticLookup_AddPoly's cube test), as indices.
void subdivisionCellsOf(const ColCtx& c, const Tri& t, vector<int>& out);

#include "scene.h"

////////////////////////////////////////
// Scene / collision header (parse_model.js)
////////////////////////////////////////



static uint32_t be32(const vector<uint8_t>& b, size_t o) {
	return (uint32_t)b.at(o) << 24 | (uint32_t)b.at(o + 1) << 16 | (uint32_t)b.at(o + 2) << 8 | b.at(o + 3);
}
static uint16_t be16(const vector<uint8_t>& b, size_t o) { return (uint16_t)(b.at(o) << 8 | b.at(o + 1)); }
static int16_t bes16(const vector<uint8_t>& b, size_t o) { return (int16_t)be16(b, o); }

static uint32_t le32(const vector<uint8_t>& b, size_t o) {
	return (uint32_t)b.at(o) | (uint32_t)b.at(o + 1) << 8 | (uint32_t)b.at(o + 2) << 16 | (uint32_t)b.at(o + 3) << 24;
}
static uint16_t le16(const vector<uint8_t>& b, size_t o) { return (uint16_t)(b.at(o) | b.at(o + 1) << 8); }
static int16_t les16(const vector<uint8_t>& b, size_t o) { return (int16_t)le16(b, o); }
static float lef32(const vector<uint8_t>& b, size_t o) {
	uint32_t u = le32(b, o);
	float f;
	memcpy(&f, &u, 4);
	return f;
}

// OoT3D / MM3D .zsi (parse_model.js, game OOT3D / MM3D): little-endian, the
// commands from 0x10 and pointers relative to it, 0x14-byte polys with the
// normal at 0xA (OoT3D) / 0x8 (MM3D) and an f32 plane distance at 0x10.
static bool parseScene3DS(const vector<uint8_t>& buf, const string& game, const string& mapName, ColHeader& ch, vector<Tri>& tris) {
	const size_t off = 0x10;
	const bool mm = game == "MM3D";
	size_t addr = mapName == "Termina Field (Credits Cutscene 2)" ? 0x60 : 0x10;
	int64_t colAddr = -1;
	while (addr + 8 <= buf.size()) {
		int cmd = buf[addr];
		if (cmd == 0x14) break;
		if (cmd == 0x19 && !mm) ch.camType = buf[addr + 1];
		else if (cmd == 0x03) { colAddr = (int64_t)le32(buf, addr + 4) + off; break; }
		addr += 8;
	}
	if (colAddr < 0) return false;
	const size_t h = (size_t)colAddr;
	const size_t b0 = mm ? 2 : 0;
	for (int i = 0; i < 3; i++) { ch.minB[i] = les16(buf, h + b0 + i * 2); ch.maxB[i] = les16(buf, h + b0 + 6 + i * 2); }
	const int numVtx = le16(buf, h + (mm ? 0x0E : 0x0C));
	ch.numPolygons = le16(buf, h + (mm ? 0x10 : 0x0E));
	const int numSurf = le16(buf, h + (mm ? 0x12 : 0x10));
	const size_t vtxList = le32(buf, h + 0x18) + off;
	const size_t polyList = le32(buf, h + 0x1C) + off;
	const size_t surfList = le32(buf, h + 0x20) + off;
	vector<std::array<int, 3>> verts(numVtx);
	for (int i = 0; i < numVtx; i++)
		for (int k = 0; k < 3; k++) verts[i][k] = les16(buf, vtxList + i * 6 + k * 2);
	const size_t nOff = mm ? 0x8 : 0xA;
	for (int i = 0; i < ch.numPolygons; i++) {
		size_t p = polyList + (size_t)i * 0x14;
		if (p + 0x14 > buf.size()) break;
		uint16_t ta = le16(buf, p + 2), tb = le16(buf, p + 4), tc = le16(buf, p + 6);
		int xpFlags = ta >> 13;
		if (xpFlags & 2) continue; // intangible: not in the collision model
		int vi[3] = { ta & 0x1FFF, tb & 0x1FFF, tc & 0x1FFF };
		Tri t;
		t.id = i;
		for (int k = 0; k < 3; k++)
			for (int j = 0; j < 3; j++) t.v[k][j] = verts.at(vi[k])[j];
		for (int k = 0; k < 3; k++) t.n[k] = les16(buf, p + nOff + k * 2);
		t.d = lef32(buf, p + 0x10);
		const uint16_t type = le16(buf, p);
		if (type < numSurf && surfList + (size_t)type * 8 + 8 <= buf.size()) {
			t.surf0 = le32(buf, surfList + (size_t)type * 8);
			t.surf1 = le32(buf, surfList + (size_t)type * 8 + 4);
		}
		tris.push_back(t);
	}
	return true;
}

bool parseScene(const vector<uint8_t>& buf, const string& game, const string& mapName, ColHeader& ch, vector<Tri>& tris) {
	if (game == "OOT3D" || game == "MM3D") return parseScene3DS(buf, game, mapName, ch, tris);
	const int64_t off = -0x02000000;
	size_t addr = 0;
	int64_t colAddr = -1;
	while (addr + 8 <= buf.size()) {
		int cmd = buf[addr];
		if (cmd == 0x14) break;
		if (cmd == 0x19 && game == "OOT") ch.camType = buf[addr + 1];
		else if (cmd == 0x03) { colAddr = (int64_t)be32(buf, addr + 4) + off; break; }
		addr += 8;
	}
	if (colAddr < 0) return false;
	size_t h = (size_t)colAddr;
	for (int i = 0; i < 3; i++) { ch.minB[i] = bes16(buf, h + i * 2); ch.maxB[i] = bes16(buf, h + 6 + i * 2); }
	int numVtx = be16(buf, h + 0x0C);
	size_t vtxList = (size_t)((int64_t)be32(buf, h + 0x10) + off);
	ch.numPolygons = be16(buf, h + 0x14);
	size_t polyList = (size_t)((int64_t)be32(buf, h + 0x18) + off);
	// (0: none - the game's SurfaceType_GetData then reads 0)
	const uint32_t surfRaw = be32(buf, h + 0x1C);
	const int64_t surfList = surfRaw ? (int64_t)surfRaw + off : -1;
	vector<std::array<int, 3>> verts(numVtx);
	for (int i = 0; i < numVtx; i++)
		for (int k = 0; k < 3; k++) verts[i][k] = bes16(buf, vtxList + i * 6 + k * 2);
	for (int i = 0; i < ch.numPolygons; i++) {
		size_t p = polyList + (size_t)i * 0x10;
		if (polyList + 0x10 > buf.size()) break;
		uint16_t ta = be16(buf, p + 2), tb = be16(buf, p + 4), tc = be16(buf, p + 6);
		int xpFlags = ta >> 13;
		if (xpFlags & 2) continue; // intangible: not in the collision model
		int vi[3] = { ta & 0x1FFF, tb & 0x1FFF, tc & 0x1FFF };
		Tri t;
		t.id = i;
		for (int k = 0; k < 3; k++)
			for (int j = 0; j < 3; j++) t.v[k][j] = verts.at(vi[k])[j];
		for (int k = 0; k < 3; k++) t.n[k] = bes16(buf, p + 8 + k * 2);
		t.d = bes16(buf, p + 14);
		const uint16_t type = be16(buf, p);
		if (surfList >= 0 && (size_t)surfList + (size_t)type * 8 + 4 <= buf.size()) t.surf0 = be32(buf, (size_t)surfList + (size_t)type * 8);
		if (surfList >= 0 && (size_t)surfList + (size_t)type * 8 + 8 <= buf.size()) t.surf1 = be32(buf, (size_t)surfList + (size_t)type * 8 + 4);
		tris.push_back(t);
	}
	return true;
}

////////////////////////////////////////
// Subdivisions (subdivisions.js)
////////////////////////////////////////

static const double OVERLAP = F(50.0);
static const double SUBDIV_MIN = F(150.0);


static void setDim(double mn, int amount, double mx, double& newMax, double& len, double& inv) {
	mn = F(mn); mx = F(mx);
	double length = F(F(mx) - F(mn));
	int temp = toI32(F(length / amount));
	double l = F(temp + 1);
	if (l < SUBDIV_MIN) l = SUBDIV_MIN;
	l = F(l);
	inv = F(F(1.0) / l);
	newMax = F(F(l * amount) + mn);
	len = l;
}

void initColCtx(ColCtx& c, const string& game, const string& mapName, const ColHeader& ch) {
	static const std::map<string, std::array<int, 3>> ootList = { { "Shadow Temple", { 23, 7, 14 } }, { "Forest Temple", { 38, 1, 38 } } };
	static const std::map<string, std::array<int, 3>> mmList = { { "Termina Field", { 36, 1, 36 } }, { "Great Bay Coast", { 40, 1, 40 } }, { "Zora Cape", { 40, 1, 40 } } };
	const bool oot = game == "OOT" || game == "OOT3D", mm = game == "MM" || game == "MM3D";
	std::array<int, 3> a = { 16, 4, 16 };
	if (oot && (ch.camType == 0x10 || ch.camType == 0x20 || ch.camType == 0x30 || ch.camType == 0x40)) a = { 2, 2, 2 };
	else if (oot && ootList.count(mapName)) a = ootList.at(mapName);
	else if (mm && mmList.count(mapName)) a = mmList.at(mapName);
	// OoT3D's overworld (Spot) scenes: 32 x 8 x 32 (subdivisions.js spotScenes)
	else if (game == "OOT3D" && mapName.rfind("Spot ", 0) == 0 && mapName != "Spot 99 - Hyrule Field (Title)") a = { 32, 8, 32 };
	for (int i = 0; i < 3; i++) {
		c.amt[i] = a[i];
		c.minB[i] = F(ch.minB[i]);
		setDim(c.minB[i], a[i], F(ch.maxB[i]), c.maxB[i], c.len[i], c.inv[i]);
	}
	size_t total = (size_t)a[0] * a[1] * a[2];
	c.subFloors.assign(total, {});
	c.subWalls.assign(total, {});
}

static bool triIntersectsCube(const Tri& t, const double box[6]) {
	// box: xmin, xmax, ymin, ymax, zmin, zmax
	double v[3][3];
	for (int k = 0; k < 3; k++) for (int j = 0; j < 3; j++) v[k][j] = t.v[k][j];
	for (int ax = 0; ax < 3; ax++) {
		double mn = std::min({ v[0][ax], v[1][ax], v[2][ax] }), mx = std::max({ v[0][ax], v[1][ax], v[2][ax] });
		if (mx < box[ax * 2] || mn > box[ax * 2 + 1]) return false;
	}
	double c[3], h[3];
	for (int ax = 0; ax < 3; ax++) {
		c[ax] = F((box[ax * 2] + box[ax * 2 + 1]) * 0.5);
		h[ax] = F((box[ax * 2 + 1] - box[ax * 2]) * 0.5);
	}
	double tv[3][3];
	for (int k = 0; k < 3; k++) for (int ax = 0; ax < 3; ax++) tv[k][ax] = F(v[k][ax] - c[ax]);
	double e[3][3];
	for (int ax = 0; ax < 3; ax++) {
		e[0][ax] = F(tv[1][ax] - tv[0][ax]);
		e[1][ax] = F(tv[2][ax] - tv[1][ax]);
		e[2][ax] = F(tv[0][ax] - tv[2][ax]);
	}
	for (int ax = 0; ax < 3; ax++) {
		if (std::max({ tv[0][ax], tv[1][ax], tv[2][ax] }) < -h[ax] || std::min({ tv[0][ax], tv[1][ax], tv[2][ax] }) > h[ax]) return false;
	}
	double n[3] = {
		F(e[0][1] * e[1][2] - e[0][2] * e[1][1]),
		F(e[0][2] * e[1][0] - e[0][0] * e[1][2]),
		F(e[0][0] * e[1][1] - e[0][1] * e[1][0]),
	};
	double r = F(h[0] * std::fabs(n[0]) + h[1] * std::fabs(n[1]) + h[2] * std::fabs(n[2]));
	double d = F(n[0] * tv[0][0] + n[1] * tv[0][1] + n[2] * tv[0][2]);
	if (d > r || d < -r) return false;
	auto axisTest = [&](const double* ed) {
		double p[3], mn, mx, rad;
		for (int k = 0; k < 3; k++) p[k] = F(ed[2] * tv[k][1] - ed[1] * tv[k][2]);
		mn = std::min({ p[0], p[1], p[2] }); mx = std::max({ p[0], p[1], p[2] });
		rad = F(std::fabs(ed[1]) * h[2] + std::fabs(ed[2]) * h[1]);
		if (mn > rad || mx < -rad) return false;
		for (int k = 0; k < 3; k++) p[k] = F(ed[0] * tv[k][2] - ed[2] * tv[k][0]);
		mn = std::min({ p[0], p[1], p[2] }); mx = std::max({ p[0], p[1], p[2] });
		rad = F(std::fabs(ed[0]) * h[2] + std::fabs(ed[2]) * h[0]);
		if (mn > rad || mx < -rad) return false;
		for (int k = 0; k < 3; k++) p[k] = F(ed[1] * tv[k][0] - ed[0] * tv[k][1]);
		mn = std::min({ p[0], p[1], p[2] }); mx = std::max({ p[0], p[1], p[2] });
		rad = F(std::fabs(ed[0]) * h[1] + std::fabs(ed[1]) * h[0]);
		if (mn > rad || mx < -rad) return false;
		return true;
	};
	return axisTest(e[0]) && axisTest(e[1]) && axisTest(e[2]);
}

static void subdivMinBounds(const ColCtx& c, const double pos[3], int out[3]) {
	for (int ax = 0; ax < 3; ax++) {
		double d = F(pos[ax] - c.minB[ax]);
		int s = toI32(F(d * c.inv[ax]));
		int di = toI32(d), subi = toI32(c.len[ax]);
		if ((di % subi) < OVERLAP && s > 0) s -= 1;
		out[ax] = s;
	}
}

static void subdivMaxBounds(const ColCtx& c, const double pos[3], int out[3]) {
	for (int ax = 0; ax < 3; ax++) {
		double d = F(F(pos[ax]) - F(c.minB[ax]));
		int s = toI32(F(d * c.inv[ax]));
		int sub = toI32(c.len[ax]);
		if ((sub - OVERLAP) < (toI32(d) % sub) && s < c.amt[ax] - 1) s += 1;
		out[ax] = s;
	}
}

// The subdivisions a poly goes in (StaticLookup_AddPoly's cube test).
void subdivisionCellsOf(const ColCtx& c, const Tri& t, vector<int>& out) {
	const double lenX = F(c.len[0] + F(2 * OVERLAP)), lenY = F(c.len[1] + F(2 * OVERLAP)), lenZ = F(c.len[2] + F(2 * OVERLAP));
	const int amtXY = c.amt[0] * c.amt[1];
	double mn[3], mx[3];
	for (int ax = 0; ax < 3; ax++) mn[ax] = mx[ax] = F(t.v[0][ax]);
	for (int k = 1; k < 3; k++) {
		for (int ax = 0; ax < 3; ax++) {
			double v = F(t.v[k][ax]);
			if (mn[ax] > v) mn[ax] = v; else if (mx[ax] < v) mx[ax] = v;
		}
	}
	int lo[3], hi[3];
	subdivMinBounds(c, mn, lo);
	subdivMaxBounds(c, mx, hi);
	int baseZ = lo[2] * amtXY;
	double curMinZ = F(F(c.len[2] * lo[2]) + c.minB[2] - OVERLAP);
	double curMaxZ = F(curMinZ + lenZ);
	for (int sz = lo[2]; sz <= hi[2]; sz++) {
		int baseY = lo[1] * c.amt[0];
		double curMinY = F(F(c.len[1] * lo[1]) + c.minB[1] - OVERLAP);
		double curMaxY = F(curMinY + lenY);
		for (int sy = lo[1]; sy <= hi[1]; sy++) {
			int index = baseZ + baseY + lo[0];
			double curMinX = F(F(c.len[0] * lo[0]) + c.minB[0] - OVERLAP);
			double curMaxX = F(curMinX + lenX);
			for (int sx = lo[0]; sx <= hi[0]; sx++) {
				double box[6] = { curMinX, curMaxX, curMinY, curMaxY, curMinZ, curMaxZ };
				if (index >= 0 && index < (int)c.subWalls.size() && triIntersectsCube(t, box)) out.push_back(index);
				curMinX = F(curMinX + c.len[0]);
				curMaxX = F(curMaxX + c.len[0]);
				index++;
			}
			curMinY = F(curMinY + c.len[1]);
			curMaxY = F(curMaxY + c.len[1]);
			baseY += c.amt[0];
		}
		curMinZ = F(curMinZ + c.len[2]);
		curMaxZ = F(curMaxZ + c.len[2]);
		baseZ += amtXY;
	}
}

void initializeSubdivisions(ColCtx& c, const vector<Tri>& tris) {
	vector<int> cells;
	for (const Tri& t : tris) {
		cells.clear();
		subdivisionCellsOf(c, t, cells);
		double ny = F(t.n[1] * NORMAL_FRAC);
		for (int index : cells) {
			if (ny > 0.5) c.subFloors[index].push_back(t.id);
			else if (ny < -0.8) { /* ceiling */ }
			else c.subWalls[index].push_back(t.id);
		}
	}
}

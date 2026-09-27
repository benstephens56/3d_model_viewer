#include "collision.h"

void Model::build(const vector<Tri>& tris, int numPolygons) {
	polys.assign(numPolygons, Poly{});
	for (const Tri& t : tris) {
		Poly& p = polys[t.id];
		p.exists = true;
		p.id = t.id;
		p.ax = t.v[0][0]; p.ay = t.v[0][1]; p.az = t.v[0][2];
		p.bx = t.v[1][0]; p.by = t.v[1][1]; p.bz = t.v[1][2];
		p.cx = t.v[2][0]; p.cy = t.v[2][1]; p.cz = t.v[2][2];
		p.sx = t.n[0]; p.sy = t.n[1]; p.sz = t.n[2];
		p.nx = F(p.sx * NORMAL_FRAC); p.ny = F(p.sy * NORMAL_FRAC); p.nz = F(p.sz * NORMAL_FRAC);
		p.nXZ = F(std::sqrt(F(sq(p.nx) + sq(p.nz))));
		p.dist = t.d;
		p.nMag = F(std::sqrt(F(F(sq(p.nx) + sq(p.ny)) + sq(p.nz))));
		p.invNXZ = p.nXZ > 0 ? F(1 / p.nXZ) : 0;
		p.minX = std::min({ p.ax, p.bx, p.cx }); p.maxX = std::max({ p.ax, p.bx, p.cx });
		p.minY = std::min({ p.ay, p.by, p.cy }); p.maxY = std::max({ p.ay, p.by, p.cy });
		p.minZ = std::min({ p.az, p.bz, p.cz }); p.maxZ = std::max({ p.az, p.bz, p.cz });
		p.isFloor = p.sy > SNORMAL_FLOOR;
		p.isCeiling = p.sy < SNORMAL_CEIL;
		p.isWall = !p.isFloor && !p.isCeiling;
		p.sortY = (p.sy == 32767 || p.sy == -32767) ? p.ay : p.minY;
		p.tz = p.nXZ > 0 ? F(std::fabs(p.nz) * p.invNXZ) : 0;
		p.tx = p.nXZ > 0 ? F(std::fabs(p.nx) * p.invNXZ) : 0;
	}
	auto sorted = [&](const vector<int>& ids, bool walls) {
		vector<int> out;
		for (int id : ids) {
			const Poly& p = polys[id];
			if (!p.exists) continue;
			if (walls ? p.isWall : p.isFloor) out.push_back(id);
		}
		std::stable_sort(out.begin(), out.end(), [&](int a, int b) {
			if (polys[a].sortY != polys[b].sortY) return polys[a].sortY < polys[b].sortY;
			return a < b;
		});
		return out;
	};
	cellWallsL.resize(colCtx.subWalls.size());
	cellFloorsL.resize(colCtx.subWalls.size());
	for (size_t i = 0; i < colCtx.subWalls.size(); i++) {
		cellWallsL[i] = sorted(colCtx.subWalls[i], true);
		cellFloorsL[i] = sorted(colCtx.subFloors[i], false);
	}
	for (const Poly& p : polys) {
		if (!p.exists || !p.isFloor) continue;
		int64_t x0 = (int64_t)std::floor(p.minX / floorCell), x1 = (int64_t)std::floor(p.maxX / floorCell);
		int64_t z0 = (int64_t)std::floor(p.minZ / floorCell), z1 = (int64_t)std::floor(p.maxZ / floorCell);
		for (int64_t gx = x0; gx <= x1; gx++)
			for (int64_t gz = z0; gz <= z1; gz++) floorGrid[key2(gx, gz)].push_back(p.id);
	}
}

FloorList Model::floorsAt(double x, double z) const {
	FloorList out;
	auto it = floorGrid.find(key2((int64_t)std::floor(x / floorCell), (int64_t)std::floor(z / floorCell)));
	if (it == floorGrid.end()) return out;
	for (int id : it->second) {
		const Poly& p = polys[id];
		if (x < p.minX - 1 || x > p.maxX + 1 || z < p.minZ - 1 || z > p.maxZ + 1) continue;
		if (!triChkY(p, z, x, 0, 1)) continue;
		out.push_back(F(F(F(F(-p.nx * x) - F(p.nz * z)) - p.dist) / p.ny));
	}
	return out;
}

const FloorList& Model::floorsNear(Scratch& s, double x, double z) const {
	int64_t kx = (int64_t)std::floor(x * 4 + 0.5), kz = (int64_t)std::floor(z * 4 + 0.5);
	int64_t k = key2(kx, kz);
	if (s.used > Scratch::CAP * 7 / 10) s.clearCache();
	uint64_t h = (uint64_t)k * 0x9E3779B97F4A7C15ull;
	int i = (int)(h >> 49);  // top 15 bits
	while (s.cacheGen[i] == s.gen) {
		if (s.cacheKey[i] == k) return s.cacheVal[i];
		i = (i + 1) & (Scratch::CAP - 1);
	}
	s.cacheGen[i] = s.gen;
	s.cacheKey[i] = k;
	s.cacheVal[i] = floorsAt(kx / 4.0, kz / 4.0);
	s.used++;
	return s.cacheVal[i];
}

V3 Model::sphereStep(const V3& pos, const Tol& tol, PushList* trace) const {
	const double R = radius;
	const double sphY = F(pos.y + checkHeight);
	const vector<int>& list = cellWalls(pos.x, pos.y, pos.z);
	double rx = pos.x, rz = pos.z;
	for (int pass = 0; pass < 2; pass++) {
		for (int id : list) {
			const Poly& p = polys[id];
			if (sphY < p.minY) break;
			double pd = planeDist(p, rx, sphY, rz);
			if (R < std::fabs(pd)) continue;
			bool hit = false;
			if (pass == 0) {
				if (p.tz < F(0.4)) continue;
				if (rz < F(p.minZ - R) || rz > F(p.maxZ + R)) continue;
				if (isZero(p.nz) || !triChkZ(p, rx, sphY, tol.detMax, tol.chkDist)) continue;
				double inter = F(F(F(F(-p.nx * rx) - F(p.ny * sphY)) - p.dist) / p.nz);
				double d = F(inter - rz);
				hit = std::fabs(d) <= F(R / p.tz) && F(d * p.nz) <= 4.0;
			} else {
				if (p.tx < F(0.4)) continue;
				if (rx < F(p.minX - R) || F(p.maxX + R) < rx) continue;
				if (isZero(p.nx) || !triChkX(p, sphY, rz, tol.detMax, tol.chkDist)) continue;
				double inter = F(F(F(F(-p.ny * sphY) - F(p.nz * rz)) - p.dist) / p.nx);
				double d = F(inter - rx);
				hit = std::fabs(d) <= F(R / p.tx) && F(d * p.nx) <= 4.0;
			}
			if (hit) {
				double disp = F(F(R - pd) * p.invNXZ);
				V3 from = { rx, pos.y, rz };
				rx = F(rx + F(disp * p.nx));
				rz = F(rz + F(disp * p.nz));
				if (trace) trace->push_back({ id, from, { rx, pos.y, rz } });
			}
		}
	}
	return { rx, pos.y, rz };
}

// (Still means not moved at all: pushes under 0.01 used to count as still,
// but the game does push Link those last thousandths - e.g. out to exactly
// the radius from a wall's stored plane - and a start there isn't one he
// stays at: MM Treasure Chest Shop Deku, TRI 90, 0.0007-0.01 off.)
std::optional<V3> Model::restingSpot(const V3& pos) const {
	V3 cur = pos;
	for (int i = 0; i < 8; i++) {
		V3 next = sphereStep({ cur.x, F(pos.y - GROUND_DROP), cur.z }, LOOSE, nullptr);
		if (next.x == cur.x && next.z == cur.z) return cur;
		cur = { next.x, pos.y, next.z };
	}
	return std::nullopt;
}

std::optional<double> Model::floorCheck(double x, double z, double y) const {
	const double* mn = colCtx.minB;
	const double* mx = colCtx.maxB;
	if (x < mn[0] || x > mx[0] || z < mn[2] || z > mx[2]) return std::nullopt;
	for (double cy = y; cy >= mn[1]; cy = F(cy - colCtx.len[1])) {
		if (cy > mx[1]) continue;
		int idx = pointCell(colCtx, x, cy, z).index;
		std::optional<double> best;
		auto scan = [&](const vector<int>& list, bool walls) {
			for (int id : list) {
				const Poly& p = polys[id];
				if (y < p.minY) break;
				if (walls && p.sy < 0) continue;
				if (isZero(p.ny) || !triChkY(p, z, x, 0, 1)) continue;
				double yi = F(F(F(F(-p.nx * x) - F(p.nz * z)) - p.dist) / p.ny);
				if (yi < y && (!best || yi > *best)) best = yi;
			}
		};
		scan(cellFloorsL[idx], false);
		scan(cellWallsL[idx], true);
		if (best) return best;
	}
	return std::nullopt;
}

std::optional<V3> Model::lineVsPoly(const Poly& p, const V3& a, const V3& b, double chkDist, bool oneFace) const {
	double planeA = F(F(F(F(F(p.sx * a.x) + F(p.sy * a.y)) + F(p.sz * a.z)) * NORMAL_FRAC) + p.dist);
	double planeB = F(F(F(F(F(p.sx * b.x) + F(p.sy * b.y)) + F(p.sz * b.z)) * NORMAL_FRAC) + p.dist);
	double delta = F(planeA - planeB);
	if ((planeA >= 0 && planeB >= 0) || (planeA < 0 && planeB < 0) || (oneFace && planeA < 0 && planeB > 0) ||
		isZero(delta)) return std::nullopt;
	double t = F(planeA / delta);
	V3 i = { F(F(F(b.x - a.x) * t) + a.x), F(F(F(b.y - a.y) * t) + a.y), F(F(F(b.z - a.z) * t) + a.z) };
	if ((std::fabs(p.nx) > 0.5 && !isZero(p.nx) && triChkX(p, i.y, i.z, 0, chkDist)) ||
		(std::fabs(p.ny) > 0.5 && !isZero(p.ny) && triChkY(p, i.z, i.x, 0, chkDist)) ||
		(std::fabs(p.nz) > 0.5 && !isZero(p.nz) && triChkZ(p, i.x, i.y, 0, chkDist))) return i;
	return std::nullopt;
}

std::optional<Hit> Model::lineHit(Scratch& s, const V3& a, const V3& b, const Tol& tol, bool floors, bool oneFace) const {
	CellIdx ia = pointCell(colCtx, a.x, a.y, a.z), ib = pointCell(colCtx, b.x, b.y, b.z);
	int cells[64];
	int nCells = 0;
	vector<int>& many = s.cellsBuf;
	many.clear();
	if (ia.index == ib.index) cells[nCells++] = ia.index;
	else {
		for (int sz = std::min(ia.sz, ib.sz); sz <= std::max(ia.sz, ib.sz); sz++)
			for (int sy = std::min(ia.sy, ib.sy); sy <= std::max(ia.sy, ib.sy); sy++)
				for (int sx = std::min(ia.sx, ib.sx); sx <= std::max(ia.sx, ib.sx); sx++)
					many.push_back(sz * colCtx.amt[0] * colCtx.amt[1] + sy * colCtx.amt[0] + sx);
	}
	uint32_t st = s.nextStamp();
	std::optional<Hit> best;
	double bestDistSq = 1.0e38;
	V3 end = b;
	auto scan = [&](const vector<int>& list) {
		for (int id : list) {
			if (s.stamp[id] == st) continue;
			s.stamp[id] = st;
			const Poly& p = polys[id];
			if (a.y < p.sortY && end.y < p.sortY) break;
			auto i = lineVsPoly(p, a, end, tol.lineChkDist, oneFace);
			if (!i) continue;
			double d = F(F(sq(F(a.x - i->x)) + sq(F(a.y - i->y))) + sq(F(a.z - i->z)));
			if (d < bestDistSq) {
				bestDistSq = d;
				best = Hit{ id, i->x, i->y, i->z };
				end = *i;
			}
		}
	};
	auto doCell = [&](int index) {
		if (floors) scan(cellFloorsL[index]);
		scan(cellWallsL[index]);
	};
	if (nCells) doCell(cells[0]);
	for (int index : many) doCell(index);
	return best;
}

const vector<int>& Model::wallsAlong(Scratch& s, const V3& a, const V3& b) const {
	int steps = std::max(1, (int)std::ceil(std::hypot(b.x - a.x, b.z - a.z) / 40));
	if (steps == 1) {
		int ia = pointCell(colCtx, a.x, a.y, a.z).index, ib = pointCell(colCtx, b.x, a.y, b.z).index;
		if (ia == ib) return cellWallsL[ia];
	}
	vector<int>& out = s.wallsBuf;
	out.clear();
	uint32_t st = s.nextStamp();
	for (int k = 0; k <= steps; k++) {
		double t = (double)k / steps;
		for (int id : cellWalls(a.x + (b.x - a.x) * t, a.y, a.z + (b.z - a.z) * t)) {
			if (s.stamp[id] == st) continue;
			s.stamp[id] = st;
			out.push_back(id);
		}
	}
	return out;
}

int Model::crossedWall(Scratch& s, const V3& a, const V3& b, bool exiting) const {
	double y = a.y + checkHeight;
	int best = -1;
	double bestT = INFINITY;
	for (int id : wallsAlong(s, a, b)) {
		const Poly& p = polys[id];
		if (y < p.minY || y > p.maxY) continue;
		double dA = planeDist(p, a.x, y, a.z), dB = planeDist(p, b.x, y, b.z);
		if (exiting) { dA = -dA; dB = -dB; }
		if (!(dA > 0 && dB < 0)) continue;
		double t = dA / (dA - dB);
		if (t >= bestT) continue;
		double ix = a.x + (b.x - a.x) * t, iz = a.z + (b.z - a.z) * t;
		if (pointInTri3D(p, ix, y, iz, 0.25)) { best = id; bestT = t; }
	}
	return best;
}

bool Model::behindWall(const V3& pos) const {
	double y = pos.y + checkHeight;
	for (int id : cellWalls(pos.x, pos.y, pos.z)) {
		const Poly& p = polys[id];
		if (y < p.minY || y > p.maxY) continue;
		double d = planeDist(p, pos.x, y, pos.z);
		if (!(d < 0 && d > -2 * radius)) continue;
		double k = d / p.nMag;
		if (pointInTri3D(p, pos.x - k * p.nx, y - k * p.ny, pos.z - k * p.nz, 0)) return true;
	}
	return false;
}

bool Model::isInBounds(Scratch& s, const V3& pos) const {
	if (behindWall(pos)) return false;
	double y = F(pos.y + checkHeight);
	const double len = 400;
	for (int i = 0; i < 8; i++) {
		double ang = i * PI / 4;
		V3 a = { pos.x, y, pos.z };
		V3 b = { F(pos.x + std::sin(ang) * len), y, F(pos.z + std::cos(ang) * len) };
		auto hit = lineHit(s, a, b, STRICT, false);
		if (hit && planeDist(polys[hit->poly], a.x, a.y, a.z) < 0) return false;
	}
	return true;
}

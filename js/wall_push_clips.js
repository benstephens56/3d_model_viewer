import * as THREE from 'three';
import { currentColCtx } from './parse_model.js';
import { getPointSubdivisionIndex } from './subdivisions.js';
import { addModelCheckbox, primaryColorTarget } from './render.js';
import { sins } from './libultra_sins.js';

////////////////////////////////////////
// System: Wall Push Clips (OOT / MM)
////////////////////////////////////////

/*
Finds in-bounds spots where one static wall pushes Link far enough behind
another static wall that he clips out of bounds.

BgCheck_CheckWallImpl (z_bgcheck.c) runs a line test from prevPos to the new
position, then BgCheck_SphVsStaticWall pushes the sphere (feet + checkHeight,
wallCheckRadius) out of every wall in the subdivision's wall list one poly at a
time: first every wall with |nz|/|n.xz| >= 0.4 (Z pass), then every wall with
|nx|/|n.xz| >= 0.4 (X pass), each push moving the position the next poly is
tested against. A wall only pushes if the sphere centre is within `radius` of
its plane and at most 4 units behind it, so a push that leaves Link more than
4 units behind some other wall B leaves B no longer holding him: he is out of
bounds. The simplest layout that does this is an acute pair of walls.

The walls' "extended planes" come from CollisionPoly_Check[XZ]IntersectApprox
(Math3D_TriChkPoint Para[XZ]Impl with detMax 300, chkDist 1): the sphere
centre only has to be within 1 unit of the triangle's projected bounding box
and within 300/edgeLength of its edges, so a wall pushes a little past its own
ends. Clips that only work thanks to that are reported separately: every clip
found is re-simulated with the tolerance removed (detMax 0, chkDist 0), and the
ones that disappear are "extended plane" clips.

Search: for every pair of walls (A pushing against the back of B) sharing a
subdivision, test every point on the floor within A's reach and in front of B
on a NEXT_STEP grid: Link standing at the point (feet there, as after his
movement for the frame) gets the frame's wall pushes. A point clips when that
takes him through a wall from its front to its back and two more frames of
standing still leave him there, starting from an in-bounds point and ending
out of bounds. All of it is f32, in the game's operation order. Dynapolys are
ignored (static collision only).
*/

// MM: `268 * 0.1f` (z_player.c) - the 0.1 is an f32 before the multiply
const WALL_CHECK_HEIGHT = { OOT: 26.0, MM: Math.fround(268 * Math.fround(0.1)) };

// ageProperties->wallCheckRadius (z_player.c)
// and R_RUN_SPEED_LIMIT / 100 of the form's boots (z_player_lib.c), the
// default max speed for "Reachable only", and the wall check height when it
// isn't the game's usual one. OoT's PLAYER_STATE2_CRAWLING (z_player.c,
// Player_UpdateBgCheck) checks walls with radius 10 at height 15 whatever the
// age, and the flag stays set if Link gets out of the crawl action some other
// way (child only: Player_TryEnteringCrawlspace checks !LINK_IS_ADULT).
const RADIUS_OPTIONS = {
    OOT: [["Adult (18)", 18.0, 6.0], ["Child (14)", 14.0, 6.0], ["Crawlspace (10, child)", 10.0, 6.0, 15.0]],
    MM: [["Human (14)", 14.0, 5.5], ["Deku (14)", 14.0, 6.0], ["Zora (18)", 18.0, 6.0], ["Goron (19.5)", 19.5, 6.0],
        ["Fierce Deity (27)", 27.0, 10.0]],
};

const NEXT_STEP = 0.5; // standing point sample spacing
const CROSS_STEP = 0.25; // crossing point spacing along a wall
const FLOOR_BLOCK = 4; // how often along a wall its floors are looked up
// How far before a point the previous frame's position is tried from. Going up
// to 32: against a slope the point can be well into the slope's footprint, and
// deep in an acute corner the nearest resting spot is far back (OoT Kakariko
// polys 323/730 clip as adult from ~33 away, speed 22).
// (How fast that is, is the Reachable filter's business.)
const MOVE_STEPS = [2, 4, 6, 8, 12, 16, 20, 24, 28, 32];
// Falling, Link's wall check runs at posNext.y + checkHeight before the floor
// check lands him, so posNext can be up to 30 below the floor (terminal y
// velocity -20 * 1.5): low walls, below the check when standing, can push.
const LOW_DROP = 30;
// Walking on the ground, Link's posNext is below his feet too: the floor check
// leaves a grounded actor's velocity.y at -4 (Actor_UpdateBgCheckInfo), gravity
// makes it -5 before he moves and Actor_UpdatePos moves 1.5x that, so the wall
// check runs 7.5 below the floor (seen in-game: MM Laundry Pool trace).
const GROUND_DROP = 7.5;

// Reachability: starts up to REACH_DIST away (speed 30 moves 45 a frame), every
// REACH_STEP, in 32 directions. Actor_UpdatePos moves speed * 1.5 a frame.
const REACH_DIST = 45;
const REACH_STEP = 1;
const SPEED_RATE = 1.5;
const REACH = 14; // how far past the radius of both walls to sample

const ACUTE_COLOR = 0xff3030;
const EXTENDED_COLOR = 0xff40ff;
const LOW_COLOR = 0x30c8ff;
const PUSHER_COLOR = 0xffd000;

const SNORMAL_FLOOR = Math.trunc(0.5 * 32767);   // COLPOLY_SNORMAL(0.5f)
const SNORMAL_CEIL = Math.trunc(-0.8 * 32767);   // COLPOLY_SNORMAL(-0.8f)

////////////////////////////////////////
// Collision model (f32)
////////////////////////////////////////

// Everything the game computes is rounded to f32 after every operation, in the
// decomp's evaluation order, so positions match the game's bit for bit (MIPS
// has no fused multiply-add). A double product or quotient of two f32s rounded
// once to f32 is the correctly rounded f32 result.
const F = Math.fround;
const NORMAL_FRAC = F(1.0 / 32767.0);   // COLPOLY_NORMAL_FRAC
const SHT_MINV = F(1.0 / 32767.0);
const EPSILON = F(0.008);               // IS_ZERO
const isZero = v => Math.abs(v) < EPSILON;
const sq = v => F(v * v);

// Math_SinS / Math_CosS: sins(angle) * (1.0f / SHRT_MAX)
const SHRT_INV = F(1 / 32767);
const sinS = yaw => F(sins(yaw) * SHRT_INV);
const cosS = yaw => F(sins(yaw + 0x4000) * SHRT_INV);
// The s16 yaw pointing along (dx, dz) (x = sin, z = cos), as a u16.
const yawOf = (dx, dz) => Math.round(Math.atan2(dx, dz) / (2 * Math.PI) * 0x10000) & 0xFFFF;
// Link walking on the ground for a frame at `yaw` with speed `speed`:
// Actor_UpdateVelocityWithGravity + Actor_UpdatePos (x1.5, velocity.y -4 + gravity -1).
function moveStep(from, yaw, speed) {
    return {
        x: F(from.x + F(F(speed * sinS(yaw)) * 1.5)),
        y: F(from.y - GROUND_DROP),
        z: F(from.z + F(F(speed * cosS(yaw)) * 1.5)),
    };
}

function buildPoly(tri) {
    const [a, b, c] = tri.vtxs;
    const [sx, sy, sz] = tri.normals;
    const nx = F(sx * NORMAL_FRAC), ny = F(sy * NORMAL_FRAC), nz = F(sz * NORMAL_FRAC);
    const nXZ = F(Math.sqrt(F(sq(nx) + sq(nz))));
    const p = {
        id: tri.id, tri,
        ax: a.x, ay: a.y, az: a.z, bx: b.x, by: b.y, bz: b.z, cx: c.x, cy: c.y, cz: c.z,
        sx, sy, sz, nx, ny, nz, dist: tri.d,
        nMag: F(Math.sqrt(F(F(sq(nx) + sq(ny)) + sq(nz)))),
        nXZ, invNXZ: nXZ > 0 ? F(1 / nXZ) : 0,
        minX: Math.min(a.x, b.x, c.x), maxX: Math.max(a.x, b.x, c.x),
        minY: Math.min(a.y, b.y, c.y), maxY: Math.max(a.y, b.y, c.y),
        minZ: Math.min(a.z, b.z, c.z), maxZ: Math.max(a.z, b.z, c.z),
        isFloor: sy > SNORMAL_FLOOR,
        isCeiling: sy < SNORMAL_CEIL,
    };
    // CollisionPoly_GetMinY, including its bug for polys with ny = +-1
    p.sortY = (sy === 32767 || sy === -32767) ? a.y : p.minY;
    p.isWall = !p.isFloor && !p.isCeiling;
    p.tz = nXZ > 0 ? F(Math.abs(nz) * p.invNXZ) : 0;
    p.tx = nXZ > 0 ? F(Math.abs(nx) * p.invNXZ) : 0;
    return p;
}

// Math3D_DistPlaneToPos
function planeDist(p, x, y, z) {
    if (isZero(p.nMag)) return 0;
    return F(F(F(F(F(p.nx * x) + F(p.ny * y)) + F(p.nz * z)) + p.dist) / p.nMag);
}

// Math3D_TriChkPointPara[XYZ]Impl, generic over the projected axes (a, b):
// X = (y, z), Y = (z, x), Z = (x, y).
function triChkPara(a0, b0, a1, b1, a2, b2, pa, pb, detMax, chkDist, nComp) {
    // Math3D_CirSquareVsTriSquare
    if (!(F(Math.min(a0, a1, a2) - chkDist) <= pa && F(Math.max(a0, a1, a2) + chkDist) >= pa &&
          F(Math.min(b0, b1, b2) - chkDist) <= pb && F(Math.max(b0, b1, b2) + chkDist) >= pb)) {
        return false;
    }
    const chkSq = sq(chkDist);
    if (F(sq(F(a0 - pa)) + sq(F(b0 - pb))) < chkSq || F(sq(F(a1 - pa)) + sq(F(b1 - pb))) < chkSq ||
        F(sq(F(a2 - pa)) + sq(F(b2 - pb))) < chkSq) {
        return true;
    }
    const d01 = F(F(F(a0 - pa) * F(b1 - pb)) - F(F(b0 - pb) * F(a1 - pa)));
    const d12 = F(F(F(a1 - pa) * F(b2 - pb)) - F(F(b1 - pb) * F(a2 - pa)));
    const d20 = F(F(F(a2 - pa) * F(b0 - pb)) - F(F(b2 - pb) * F(a0 - pa)));
    if ((d01 <= detMax && d12 <= detMax && d20 <= detMax) || (d01 >= -detMax && d12 >= -detMax && d20 >= -detMax)) {
        return true;
    }
    if (Math.abs(nComp) > 0.5) {
        if (edgeDistSq(pa, pb, a0, b0, a1, b1) < chkSq || edgeDistSq(pa, pb, a1, b1, a2, b2) < chkSq ||
            edgeDistSq(pa, pb, a2, b2, a0, b0) < chkSq) {
            return true;
        }
    }
    return false;
}

// Math3D_PointDistSqToLine2D; Infinity where it returns false
function edgeDistSq(x0, y0, x1, y1, x2, y2) {
    const dx = F(x2 - x1), dy = F(y2 - y1);
    const lenSq = F(sq(dx) + sq(dy));
    if (isZero(lenSq)) return Infinity;
    const t = F(F(F(F(x0 - x1) * dx) + F(F(y0 - y1) * dy)) / lenSq);
    if (!(t >= 0 && t <= 1)) return Infinity;
    return F(sq(F(F(F(dx * t) + x1) - x0)) + sq(F(F(F(dy * t) + y1) - y0)));
}

const triChkX = (p, y, z, detMax, chk) => triChkPara(p.ay, p.az, p.by, p.bz, p.cy, p.cz, y, z, detMax, chk, p.nx);
const triChkY = (p, z, x, detMax, chk) => triChkPara(p.az, p.ax, p.bz, p.bx, p.cz, p.cx, z, x, detMax, chk, p.ny);
const triChkZ = (p, x, y, detMax, chk) => triChkPara(p.ax, p.ay, p.bx, p.by, p.cx, p.cy, x, y, detMax, chk, p.nz);

// The game's tolerances (the extended plane), or none at all.
const LOOSE = { detMax: 300, chkDist: 1, lineChkDist: 1 };
const STRICT = { detMax: 0, chkDist: 0, lineChkDist: 0 };

class CollisionModel {
    // falling: also search clips with Link falling (LOW_DROP), which is slow
    constructor(colCtx, triangles, radius, checkHeight, falling = false) {
        this.colCtx = colCtx;
        this.lowDrop = falling ? LOW_DROP : 0;
        this.radius = F(radius);
        this.checkHeight = F(checkHeight);
        this.polys = new Map();
        for (const tri of triangles) this.polys.set(tri.id, buildPoly(tri));
        this.cellCache = new Map();
        this.floorCache = new Map();
        this.buildFloorGrid();
    }

    cellIndex(x, y, z) {
        return getPointSubdivisionIndex(this.colCtx, { x, y, z }).index;
    }

    // A subdivision's floor and wall lists in the game's order:
    // StaticLookup_AddPolyToSSList inserts each poly (in poly index order)
    // before the first one whose vertices are all above its CollisionPoly_GetMinY,
    // i.e. a stable sort on that.
    cell(index) {
        let c = this.cellCache.get(index);
        if (!c) {
            const sub = this.colCtx.subdivisions[index];
            const list = ids => (ids || []).map(id => this.polys.get(id)).filter(Boolean)
                .sort((a, b) => a.sortY - b.sortY || a.id - b.id);
            c = sub ? { walls: list(sub.walls).filter(p => p.isWall), floors: list(sub.floors).filter(p => p.isFloor) }
                    : { walls: [], floors: [] };
            this.cellCache.set(index, c);
        }
        return c;
    }

    cellWalls(x, y, z) {
        return this.cell(this.cellIndex(x, y, z)).walls;
    }

    buildFloorGrid() {
        this.floorCell = 128;
        this.floorGrid = new Map();
        for (const p of this.polys.values()) {
            if (!p.isFloor) continue;
            const x0 = Math.floor(p.minX / this.floorCell), x1 = Math.floor(p.maxX / this.floorCell);
            const z0 = Math.floor(p.minZ / this.floorCell), z1 = Math.floor(p.maxZ / this.floorCell);
            for (let gx = x0; gx <= x1; gx++) {
                for (let gz = z0; gz <= z1; gz++) {
                    const key = gx + "," + gz;
                    let cell = this.floorGrid.get(key);
                    if (!cell) this.floorGrid.set(key, cell = []);
                    cell.push(p);
                }
            }
        }
    }

    // Heights of the static floors under (x, z): CollisionPoly_CheckYIntersect
    // (detMax 0) and its y = ((-nx * x) - (nz * z) - dist) / ny.
    floorsAt(x, z) {
        const cell = this.floorGrid.get(Math.floor(x / this.floorCell) + "," + Math.floor(z / this.floorCell));
        const out = [];
        if (!cell) return out;
        for (const p of cell) {
            if (x < p.minX - 1 || x > p.maxX + 1 || z < p.minZ - 1 || z > p.maxZ + 1) continue;
            if (!triChkY(p, z, x, 0, 1)) continue;
            out.push(F(F(F(F(-p.nx * x) - F(p.nz * z)) - p.dist) / p.ny));
        }
        return out;
    }

    // BgCheck_SphVsStaticWall on the sphere at `pos` + checkHeight, looked up in
    // the subdivision of `pos` (BgCheck_GetNearestStaticLookup(posResult)).
    // Returns the displaced feet position; each push is appended to `trace` as
    // { poly, from, to }.
    sphereStep(pos, tol, trace) {
        const R = this.radius;
        const sphY = F(pos.y + this.checkHeight);
        const list = this.cellWalls(pos.x, pos.y, pos.z);
        let rx = pos.x, rz = pos.z;

        for (let pass = 0; pass < 2; pass++) {
            for (let i = 0; i < list.length; i++) {
                const p = list[i];
                if (sphY < p.minY) break;
                const pd = planeDist(p, rx, sphY, rz);
                if (R < Math.abs(pd)) continue;

                let hit = false;
                if (pass === 0) {
                    if (p.tz < F(0.4)) continue;
                    if (rz < F(p.minZ - R) || rz > F(p.maxZ + R)) continue;
                    // CollisionPoly_CheckZIntersectApprox
                    if (isZero(p.nz) || !triChkZ(p, rx, sphY, tol.detMax, tol.chkDist)) continue;
                    const inter = F(F(F(F(-p.nx * rx) - F(p.ny * sphY)) - p.dist) / p.nz);
                    const d = F(inter - rz);
                    hit = Math.abs(d) <= F(R / p.tz) && F(d * p.nz) <= 4.0;
                } else {
                    if (p.tx < F(0.4)) continue;
                    if (rx < F(p.minX - R) || F(p.maxX + R) < rx) continue;
                    // CollisionPoly_CheckXIntersectApprox
                    if (isZero(p.nx) || !triChkX(p, sphY, rz, tol.detMax, tol.chkDist)) continue;
                    const inter = F(F(F(F(-p.ny * sphY) - F(p.nz * rz)) - p.dist) / p.nx);
                    const d = F(inter - rx);
                    hit = Math.abs(d) <= F(R / p.tx) && F(d * p.nx) <= 4.0;
                }
                if (hit) {
                    // BgCheck_ComputeWallDisplacement
                    const disp = F(F(R - pd) * p.invNXZ);
                    const from = { x: rx, y: pos.y, z: rz };
                    rx = F(rx + F(disp * p.nx));
                    rz = F(rz + F(disp * p.nz));
                    if (trace) trace.push({ poly: p, from, to: { x: rx, y: pos.y, z: rz } });
                }
            }
        }
        return { x: rx, y: pos.y, z: rz };
    }

    // CollisionPoly_LineVsPoly. With `oneFace` (BGCHECK_CHECK_ONE_FACE) a poly
    // crossed from its back to its front doesn't count. Returns the
    // intersection or null.
    lineVsPoly(p, a, b, chkDist, oneFace) {
        const planeA = F(F(F(F(F(p.sx * a.x) + F(p.sy * a.y)) + F(p.sz * a.z)) * NORMAL_FRAC) + p.dist);
        const planeB = F(F(F(F(F(p.sx * b.x) + F(p.sy * b.y)) + F(p.sz * b.z)) * NORMAL_FRAC) + p.dist);
        const delta = F(planeA - planeB);
        if ((planeA >= 0 && planeB >= 0) || (planeA < 0 && planeB < 0) || (oneFace && planeA < 0 && planeB > 0) ||
            isZero(delta)) return null;
        // Math3D_LineSplitRatio
        const t = F(planeA / delta);
        const i = {
            x: F(F(F(b.x - a.x) * t) + a.x),
            y: F(F(F(b.y - a.y) * t) + a.y),
            z: F(F(F(b.z - a.z) * t) + a.z),
        };
        if ((Math.abs(p.nx) > 0.5 && !isZero(p.nx) && triChkX(p, i.y, i.z, 0, chkDist)) ||
            (Math.abs(p.ny) > 0.5 && !isZero(p.ny) && triChkY(p, i.z, i.x, 0, chkDist)) ||
            (Math.abs(p.nz) > 0.5 && !isZero(p.nz) && triChkZ(p, i.x, i.y, 0, chkDist))) {
            return i;
        }
        return null;
    }

    // BgCheck_CheckLineImpl over static floors (if `floors`) and walls: the
    // nearest intersection from a to b, or null. Subdivisions are every cell
    // between the two ends' (the Math3D_LineVsCube cull only skips cells the
    // segment misses, which can't produce a hit anyway).
    lineHit(a, b, tol, floors, oneFace = false) {
        const ia = getPointSubdivisionIndex(this.colCtx, a);
        const ib = getPointSubdivisionIndex(this.colCtx, b);
        const cells = [];
        if (ia.index === ib.index) {
            cells.push(ia.index);
        } else {
            const amt = this.colCtx.subdivAmount;
            for (let sz = Math.min(ia.sz, ib.sz); sz <= Math.max(ia.sz, ib.sz); sz++)
                for (let sy = Math.min(ia.sy, ib.sy); sy <= Math.max(ia.sy, ib.sy); sy++)
                    for (let sx = Math.min(ia.sx, ib.sx); sx <= Math.max(ia.sx, ib.sx); sx++)
                        cells.push(sz * amt.x * amt.y + sy * amt.x + sx);
        }
        const checked = new Set();
        let best = null, bestDistSq = 1.0e38;
        let end = b;
        const scan = list => {
            for (const p of list) {
                if (checked.has(p)) continue;
                checked.add(p);
                if (a.y < p.sortY && end.y < p.sortY) break;
                const i = this.lineVsPoly(p, a, end, tol.lineChkDist, oneFace);
                if (!i) continue;
                const d = F(F(sq(F(a.x - i.x)) + sq(F(a.y - i.y))) + sq(F(a.z - i.z)));
                if (d < bestDistSq) {
                    bestDistSq = d;
                    best = { poly: p, ...i };
                    end = i;
                }
            }
        };
        for (const index of cells) {
            const c = this.cell(index);
            if (floors) scan(c.floors);
            scan(c.walls);
        }
        return best;
    }

    wallsAlong(a, b) {
        const steps = Math.max(1, Math.ceil(Math.hypot(b.x - a.x, b.z - a.z) / 40));
        if (steps === 1) {
            const ia = this.cellIndex(a.x, a.y, a.z);
            const ib = this.cellIndex(b.x, a.y, b.z);
            if (ia === ib) return this.cell(ia).walls;
        }
        const seen = new Set();
        for (let s = 0; s <= steps; s++) {
            const t = s / steps;
            for (const p of this.cellWalls(a.x + (b.x - a.x) * t, a.y, a.z + (b.z - a.z) * t)) seen.add(p);
        }
        return seen;
    }

    // First wall crossed from its front to its back going from a to b at sphere
    // height (or from its back to its front, with `exiting`). Not a game
    // function: this is how the search decides Link went through a wall.
    crossedWall(a, b, exiting = false) {
        const y = a.y + this.checkHeight;
        let best = null, bestT = Infinity;
        for (const p of this.wallsAlong(a, b)) {
            if (y < p.minY || y > p.maxY) continue;
            let dA = planeDist(p, a.x, y, a.z);
            let dB = planeDist(p, b.x, y, b.z);
            if (exiting) { dA = -dA; dB = -dB; }
            if (!(dA > 0 && dB < 0)) continue;
            const t = dA / (dA - dB);
            if (t >= bestT) continue;
            const ix = a.x + (b.x - a.x) * t, iz = a.z + (b.z - a.z) * t;
            if (pointInTri3D(p, ix, y, iz, 0.25)) {
                best = p;
                bestT = t;
            }
        }
        return best;
    }

    // Where Link comes to rest standing at `pos`: the wall pushes applied until
    // they stop moving him (at most 4 frames), or null if they don't settle.
    // (Standing still, posNext is GROUND_DROP below his feet every frame, so
    // that's the height the pushes run at: it matters for leaning walls.)
    restingSpot(pos) {
        let cur = pos;
        for (let i = 0; i < 4; i++) {
            const next = this.sphereStep({ x: cur.x, y: F(pos.y - GROUND_DROP), z: cur.z }, LOOSE, null);
            if (Math.abs(next.x - cur.x) <= 0.01 && Math.abs(next.z - cur.z) <= 0.01) return cur;
            cur = { x: next.x, y: pos.y, z: next.z };
        }
        return null;
    }

    // BgCheck_RaycastFloorImpl for the actor floor check (flags 0x1C): the
    // highest static floor, or wall whose normal doesn't point down, under
    // (x, z) and below y, from the subdivision y is in (stepping down a
    // subdivision at a time while there's nothing). Null if none.
    floorCheck(x, z, y) {
        const c = this.colCtx, mn = c.minBounds, mx = c.maxBounds;
        if (x < mn.x || x > mx.x || z < mn.z || z > mx.z) return null;
        for (let cy = y; cy >= mn.y; cy = F(cy - c.subdivLength.y)) {
            if (cy > mx.y) continue;
            const cell = this.cell(this.cellIndex(x, cy, z));
            let best = null;
            const scan = (list, walls) => {
                for (const p of list) {
                    if (y < p.minY) break;
                    if (walls && p.sy < 0) continue;
                    if (isZero(p.ny) || !triChkY(p, z, x, 0, 1)) continue;
                    const yi = F(F(F(F(-p.nx * x) - F(p.nz * z)) - p.dist) / p.ny);
                    if (yi < y && (best === null || yi > best)) best = yi;
                }
            };
            scan(cell.floors, false);
            scan(cell.walls, true);
            if (best !== null) return best;
        }
        return null;
    }

    // Floor heights near (x, z), rounded to a quarter unit and cached: for
    // finding which floor Link could be standing on around a point, where the
    // exact spot doesn't matter.
    floorsNear(x, z) {
        const kx = Math.round(x * 4), kz = Math.round(z * 4);
        const key = kx * 4194304 + kz;
        let ys = this.floorCache.get(key);
        if (!ys) {
            ys = this.floorsAt(kx / 4, kz / 4);
            this.floorCache.set(key, ys);
        }
        return ys;
    }

    // A wall that `pos` (at sphere height) is inside: less than 2 radii behind
    // it and within its triangle, i.e. inside the solid it bounds.
    behindWall(pos) {
        const y = pos.y + this.checkHeight;
        for (const p of this.cellWalls(pos.x, pos.y, pos.z)) {
            if (y < p.minY || y > p.maxY) continue;
            const d = planeDist(p, pos.x, y, pos.z);
            if (!(d < 0 && d > -2 * this.radius)) continue;
            const k = d / p.nMag;
            if (pointInTri3D(p, pos.x - k * p.nx, y - k * p.ny, pos.z - k * p.nz, 0)) return p;
        }
        return null;
    }

    // Not inside a wall (behindWall), and horizontal rays at sphere height
    // don't see the back of a wall first in any direction.
    isInBounds(pos) {
        if (this.behindWall(pos)) return false;
        const y = F(pos.y + this.checkHeight);
        const len = 400;
        for (let i = 0; i < 8; i++) {
            const ang = i * Math.PI / 4;
            const a = { x: pos.x, y, z: pos.z };
            const b = { x: F(pos.x + Math.sin(ang) * len), y, z: F(pos.z + Math.cos(ang) * len) };
            const hit = this.lineHit(a, b, STRICT, false);
            if (hit && planeDist(hit.poly, a.x, a.y, a.z) < 0) return false;
        }
        return true;
    }
}

function pointInTri3D(p, x, y, z, tolerance) {
    // Project on the dominant normal axis and test with an edge tolerance.
    const ax = Math.abs(p.nx), ay = Math.abs(p.ny), az = Math.abs(p.nz);
    let a0, b0, a1, b1, a2, b2, pa, pb;
    if (ax >= ay && ax >= az) { [a0, b0, a1, b1, a2, b2, pa, pb] = [p.ay, p.az, p.by, p.bz, p.cy, p.cz, y, z]; }
    else if (az >= ay) { [a0, b0, a1, b1, a2, b2, pa, pb] = [p.ax, p.ay, p.bx, p.by, p.cx, p.cy, x, y]; }
    else { [a0, b0, a1, b1, a2, b2, pa, pb] = [p.az, p.ax, p.bz, p.bx, p.cz, p.cx, z, x]; }
    const d01 = (a0 - pa) * (b1 - pb) - (b0 - pb) * (a1 - pa);
    const d12 = (a1 - pa) * (b2 - pb) - (b1 - pb) * (a2 - pa);
    const d20 = (a2 - pa) * (b0 - pb) - (b2 - pb) * (a0 - pa);
    if ((d01 >= 0 && d12 >= 0 && d20 >= 0) || (d01 <= 0 && d12 <= 0 && d20 <= 0)) return true;
    const tSq = tolerance * tolerance;
    const near = (x0, y0, x1, y1, x2, y2) => {
        const dx = x2 - x1, dy = y2 - y1, len = dx * dx + dy * dy;
        if (len === 0) return false;
        const t = Math.max(0, Math.min(1, ((x0 - x1) * dx + (y0 - y1) * dy) / len));
        return (x1 + dx * t - x0) ** 2 + (y1 + dy * t - y0) ** 2 <= tSq;
    };
    return near(pa, pb, a0, b0, a1, b1) || near(pa, pb, a1, b1, a2, b2) || near(pa, pb, a2, b2, a0, b0);
}

////////////////////////////////////////
// Search
////////////////////////////////////////

// Checks the frame prev -> res (whose pushes are in `trace`) for a clip: the
// result is behind a wall it was in front of, and two more frames of standing
// still leave it there. Returns the wall crossed, the push that crossed it and
// where Link ends up, or null.
//
// With `rayFromY` (prevPos.y, Link walking) the frame's floor check runs first
// (func_800B7678): a ray down from rayFromY + 50 finds the highest floor - or
// wall facing up at all - under where he was pushed to, and he lands on it if
// it's above him or at most 11 below. Pushed into a sloped rock he can land on
// top of it that way, in front of the wall he went through.
function clipFromFrame(model, prev, res, trace, tol, rayFromY = null) {
    if (trace.length === 0) return null;
    // (at the frame's height: prevPos.xz, posNext.y)
    const from = { x: prev.x, y: res.y, z: prev.z };
    const crossed = model.crossedWall(from, res);
    if (!crossed) return null;

    let at = res, landY = null;
    if (rayFromY !== null) {
        const fy = model.floorCheck(res.x, res.z, F(rayFromY + 50));
        if (fy !== null && F(fy - res.y) >= -11) {
            landY = fy;
            // standing there from then on: the following frames' posNext
            at = { x: res.x, y: F(fy - GROUND_DROP), z: res.z };
        }
    }
    const s1 = model.sphereStep(at, tol, null);
    const s2 = model.sphereStep(s1, tol, null);
    const from2 = { x: prev.x, y: at.y, z: prev.z };
    // Still through the same wall - or, landed at another height, through any:
    // there it can be another triangle (MM Treasure Chest Shop: pushed through
    // the 40 high counter front TRI 90, he lands on its top, behind TRI 73/74
    // of the wall above it - in-game that clips walking at speed 11)
    const held = model.crossedWall(from2, s2);
    if (landY === null ? held !== crossed : !held) return null;
    // Out the other side of a thin wall: through it, not out of bounds.
    if (model.crossedWall(from2, s2, true)) return null;
    const end = landY === null ? s2 : { x: s2.x, y: landY, z: s2.z };

    // The push that took Link through `crossed`.
    const sphY = res.y + model.checkHeight;
    let pusher = null;
    for (const t of trace) {
        if (t.poly === crossed) continue;
        const before = planeDist(crossed, t.from.x, sphY, t.from.z);
        const after = planeDist(crossed, t.to.x, sphY, t.to.z);
        if (before >= 0 && after < 0) { pusher = t; break; }
    }
    if (!pusher) {
        for (let i = trace.length - 1; i >= 0; i--) {
            if (trace[i].poly !== crossed) { pusher = trace[i]; break; }
        }
    }
    if (!pusher) return null;
    return { crossed, pusher: pusher.poly, end };
}

// Triangle-triangle distance (3D): 0 if an edge of one passes through the
// other, else the smallest vertex-triangle / edge-edge distance. Plain doubles:
// only used to rule out wall pairs that are too far apart to matter.
const triVerts = p => [[p.ax, p.ay, p.az], [p.bx, p.by, p.bz], [p.cx, p.cy, p.cz]];
const sub3 = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const dot3 = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross3 = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];

// Closest point on triangle abc to p (Ericson, Real-Time Collision Detection 5.1.5).
function closestOnTri(p, a, b, c) {
    const ab = sub3(b, a), ac = sub3(c, a), ap = sub3(p, a);
    const d1 = dot3(ab, ap), d2 = dot3(ac, ap);
    if (d1 <= 0 && d2 <= 0) return a;
    const bp = sub3(p, b), d3 = dot3(ab, bp), d4 = dot3(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    const vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { const v = d1 / (d1 - d3); return [a[0] + v * ab[0], a[1] + v * ab[1], a[2] + v * ab[2]]; }
    const cp = sub3(p, c), d5 = dot3(ab, cp), d6 = dot3(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    const vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { const w = d2 / (d2 - d6); return [a[0] + w * ac[0], a[1] + w * ac[1], a[2] + w * ac[2]]; }
    const va = d3 * d6 - d5 * d4;
    if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0) {
        const w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return [b[0] + w * (c[0] - b[0]), b[1] + w * (c[1] - b[1]), b[2] + w * (c[2] - b[2])];
    }
    const denom = 1 / (va + vb + vc), v = vb * denom, w = vc * denom;
    return [a[0] + ab[0] * v + ac[0] * w, a[1] + ab[1] * v + ac[1] * w, a[2] + ab[2] * v + ac[2] * w];
}

// Squared distance between segments p1q1 and p2q2 (Ericson 5.1.9).
function segSegDistSq(p1, q1, p2, q2) {
    const d1 = sub3(q1, p1), d2 = sub3(q2, p2), r = sub3(p1, p2);
    const a = dot3(d1, d1), e = dot3(d2, d2), f = dot3(d2, r);
    let s, t;
    if (a <= 1e-12 && e <= 1e-12) return dot3(r, r);
    if (a <= 1e-12) { s = 0; t = Math.min(Math.max(f / e, 0), 1); }
    else {
        const c = dot3(d1, r);
        if (e <= 1e-12) { t = 0; s = Math.min(Math.max(-c / a, 0), 1); }
        else {
            const b = dot3(d1, d2), denom = a * e - b * b;
            s = denom !== 0 ? Math.min(Math.max((b * f - c * e) / denom, 0), 1) : 0;
            t = (b * s + f) / e;
            if (t < 0) { t = 0; s = Math.min(Math.max(-c / a, 0), 1); }
            else if (t > 1) { t = 1; s = Math.min(Math.max((b - c) / a, 0), 1); }
        }
    }
    const c1 = [p1[0] + d1[0] * s, p1[1] + d1[1] * s, p1[2] + d1[2] * s];
    const c2 = [p2[0] + d2[0] * t, p2[1] + d2[1] * t, p2[2] + d2[2] * t];
    const d = sub3(c1, c2);
    return dot3(d, d);
}

// Whether segment pq passes through triangle abc.
function segHitsTri(p, q, a, b, c) {
    const n = cross3(sub3(b, a), sub3(c, a));
    const dp = dot3(n, sub3(p, a)), dq = dot3(n, sub3(q, a));
    if ((dp > 0 && dq > 0) || (dp < 0 && dq < 0) || dp === dq) return false;
    const t = dp / (dp - dq);
    const x = [p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t, p[2] + (q[2] - p[2]) * t];
    const s1 = dot3(n, cross3(sub3(b, a), sub3(x, a)));
    const s2 = dot3(n, cross3(sub3(c, b), sub3(x, b)));
    const s3 = dot3(n, cross3(sub3(a, c), sub3(x, c)));
    return (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0);
}

function triTriDist(A, B) {
    const ta = triVerts(A), tb = triVerts(B);
    for (let i = 0; i < 3; i++) {
        if (segHitsTri(ta[i], ta[(i + 1) % 3], tb[0], tb[1], tb[2])) return 0;
        if (segHitsTri(tb[i], tb[(i + 1) % 3], ta[0], ta[1], ta[2])) return 0;
    }
    let best = Infinity;
    for (const [pts, [a, b, c]] of [[ta, tb], [tb, ta]]) {
        for (const p of pts) {
            const d = sub3(p, closestOnTri(p, a, b, c));
            best = Math.min(best, dot3(d, d));
        }
    }
    for (let i = 0; i < 3; i++)
        for (let j = 0; j < 3; j++)
            best = Math.min(best, segSegDistSq(ta[i], ta[(i + 1) % 3], tb[j], tb[(j + 1) % 3]));
    return Math.sqrt(best);
}

// How far apart a wall pair's triangles can be and still clip: Link is within
// about `radius` of A (plus the extended plane's slack), and the push or the
// line check's snap moves him at most about radius + 4 through B - or, for a
// crossing, he can come from up to 32 away. Anything further apart can't clip.
const pairReach = model => 2 * model.radius + 42;

function wallPairCandidates(model) {
    const pairs = new Map();
    const R = model.radius, E = R + REACH, reach = pairReach(model);
    for (const sub of model.colCtx.subdivisions) {
        if (!sub || sub.walls.length < 2) continue;
        const walls = sub.walls.map(id => model.polys.get(id)).filter(p => p && p.isWall && p.nXZ > 0);
        for (let i = 0; i < walls.length; i++) {
            for (let j = 0; j < walls.length; j++) {
                if (i === j) continue;
                const A = walls[i], B = walls[j];
                const key = A.id * 65536 + B.id;
                if (pairs.has(key)) continue;
                // A must push against B's back
                const cosAB = (A.nx * B.nx + A.nz * B.nz) * A.invNXZ * B.invNXZ;
                if (cosAB > -0.02) continue;
                const lo = Math.max(A.minY, B.minY) - 1, hi = Math.min(A.maxY, B.maxY) + 1;
                if (hi < lo) continue;
                const x0 = Math.max(A.minX, B.minX) - E, x1 = Math.min(A.maxX, B.maxX) + E;
                const z0 = Math.max(A.minZ, B.minZ) - E, z1 = Math.min(A.maxZ, B.maxZ) + E;
                if (x1 < x0 || z1 < z0) continue;
                // Bounding boxes that overlap don't mean the triangles are close
                // (long diagonal and sloped walls): measure the triangles.
                if (triTriDist(A, B) > reach) {
                    pairs.set(key, null);
                    continue;
                }
                pairs.set(key, { A, B, cosAB, lo, hi, x0, x1, z0, z1 });
            }
        }
    }
    return [...pairs.values()].filter(Boolean);
}

// Where the points that can clip for a wall pair are: within `radius` in front
// of A (or 4 behind it) and at most radius + 4 in front of B. Top down that's a
// parallelogram around where the two planes meet at the check height; this is
// its bounding box over the pair's check heights [lo, hi] (the corners at both
// ends, plus a unit of margin). Null for walls too close to parallel, whose
// planes meet far away (the whole overlap is searched then).
function cornerBox(A, B, R, lo, hi) {
    let x0 = Infinity, x1 = -Infinity, z0 = Infinity, z1 = -Infinity;
    for (const h of [lo, hi]) {
        for (const a of [-(4 / A.nXZ) - 1, R]) {
            for (const b of [0, R + 4]) {
                const p = planesMeet(A, B, h, a, b);
                if (!p) return null;
                x0 = Math.min(x0, p.x); x1 = Math.max(x1, p.x);
                z0 = Math.min(z0, p.z); z1 = Math.max(z1, p.z);
            }
        }
    }
    return { x0: x0 - 1, x1: x1 + 1, z0: z0 - 1, z1: z1 + 1 };
}

// Where walls A and B's planes meet (top down) at check height h, offset to
// signed distances a from A and b from B: nA.xz * p = a * |nA| - nA.y * h -
// distA, the same for B. Null for walls under ~3 degrees apart.
function planesMeet(A, B, h, a, b) {
    const det = A.nx * B.nz - A.nz * B.nx;
    if (Math.abs(det) < 0.05 * A.nXZ * B.nXZ) return null;
    const ra = a * A.nMag - A.ny * h - A.dist, rb = b * B.nMag - B.ny * h - B.dist;
    return { x: (ra * B.nz - A.nz * rb) / det, z: (A.nx * rb - ra * B.nx) / det };
}

// Positions Link could reach this frame (posNext) where A's push (to `radius`
// in front of A) can land him more than 4 units behind B: within A's reach
// (at most `radius` in front of A or 4 behind it) and in front of B.
function* nextPositionsForPair(model, pair) {
    const { A, B, cosAB, lo, hi } = pair;
    const R = model.radius, ch = model.checkHeight;
    let x0 = Math.max(pair.x0, Math.min(A.minX, B.minX) - R), x1 = Math.min(pair.x1, Math.max(A.maxX, B.maxX) + R);
    let z0 = Math.max(pair.z0, Math.min(A.minZ, B.minZ) - R), z1 = Math.min(pair.z1, Math.max(A.maxZ, B.maxZ) + R);
    const corner = cornerBox(A, B, R, lo, hi);
    if (corner) {
        x0 = Math.max(x0, corner.x0); x1 = Math.min(x1, corner.x1);
        z0 = Math.max(z0, corner.z0); z1 = Math.min(z1, corner.z1);
        if (x1 < x0 || z1 < z0) return;
    }
    let step = NEXT_STEP;
    while (((x1 - x0) / step) * ((z1 - z0) / step) > 40000) step *= 1.25;
    // Link's check height is fixed by the floor he's on, and a sloped wall's
    // distance changes with height, so the walls are measured there.
    // ...and near the triangles themselves, not just their planes: the point
    // is within reach of A's triangle, and where A's push would take it
    // through B's plane is on or near B's triangle. EXTENT_SLACK covers the
    // extended plane and pushes from walls before A in the list.
    const reachable = h => {
        const dA = planeDist(A, x, h, z);
        if (dA > R || dA < -(4 / A.nXZ) - 1) return false;
        const dB = planeDist(B, x, h, z);
        if (dB < 0 || dB > R + 4) return false;
        // B's distance after A's push
        if (dB + (R - dA) * cosAB > -3) return false;
        const kA = dA / A.nMag;
        if (!pointInTri3D(A, x - kA * A.nx, h - kA * A.ny, z - kA * A.nz, EXTENT_SLACK)) return false;
        const disp = (R - dA) * A.invNXZ;
        const qx = x + disp * A.nx, qz = z + disp * A.nz;
        const dBq = planeDist(B, qx, h, qz);
        if (dBq >= 0) return true; // (the plane test above was an estimate)
        const t = dB / (dB - dBq);
        return pointInTri3D(B, x + (qx - x) * t, h, z + (qz - z) * t, EXTENT_SLACK);
    };
    const EXTENT_SLACK = R;
    // Where Link's feet end up walking into a slope can be over no floor at
    // all (under the slope), so his height comes from the floors around the
    // point on the walls' fronts too.
    // Out from A, out from B, and between the two (in a V corner, straight out
    // from one wall can land on the other one's slope).
    const aX = A.nx * A.invNXZ, aZ = A.nz * A.invNXZ, bX = B.nx * B.invNXZ, bZ = B.nz * B.invNXZ;
    const mLen = Math.hypot(aX + bX, aZ + bZ) || 1;
    const outDirs = [[aX, aZ], [bX, bZ], [(aX + bX) / mLen, (aZ + bZ) / mLen]];
    const floorsNear = () => {
        const out = new Set(model.floorsAt(x, z));
        for (const d of [8, 16, 24]) {
            for (const [dx, dz] of outDirs) for (const y of model.floorsNear(x + d * dx, z + d * dz)) out.add(y);
        }
        return out;
    };
    // Whether `reachable` holds at any check height in [lo, hi]: every test in
    // it is linear in the height, so this intersects their ranges.
    const anyHeight = () => {
        let hMin = lo, hMax = hi;
        const dA0 = planeDist(A, x, 0, z), dA1 = planeDist(A, x, 1, z) - dA0;
        const dB0 = planeDist(B, x, 0, z), dB1 = planeDist(B, x, 1, z) - dB0;
        // keep the h where k + m*h <= 0
        const le = (k, m) => {
            if (Math.abs(m) < 1e-9) { if (k > 0) hMax = -Infinity; return; }
            const root = -k / m;
            if (m > 0) hMax = Math.min(hMax, root); else hMin = Math.max(hMin, root);
        };
        le(dA0 - R, dA1);
        le(-dA0 - (4 / A.nXZ) - 1, -dA1);
        le(-dB0, -dB1);
        le(dB0 - R - 4, dB1);
        le(dB0 + (R - dA0) * cosAB + 3, dB1 - dA1 * cosAB);
        return hMin <= hMax;
    };
    let x, z;
    for (x = Math.ceil(x0 / step) * step; x <= x1; x += step) {
        for (z = Math.ceil(z0 / step) * step; z <= z1; z += step) {
            if (!anyHeight()) continue;
            for (const fy of floorsNear()) {
                const h = fy - GROUND_DROP + ch;
                if (h < lo || h > hi + model.lowDrop) continue;
                // falling: anywhere from h down to h - lowDrop
                const hTop = Math.min(h, hi), hLow = Math.max(lo, h - model.lowDrop);
                if (!reachable(hTop) && !(model.lowDrop && (reachable(hLow) || reachable((hTop + hLow) / 2)))) continue;
                yield { x: F(x), y: fy, z: F(z), lo, hi };
            }
        }
    }
}

// Points on wall A's plane (at sphere height, feet on a floor next to it) that
// Link's movement could cross: within A's extent plus the line check's 1 unit
// of slack, every CROSS_STEP along the wall. Also with Link falling: feet
// `drop` (2, 6, .. LOW_DROP) below the floor, the line check runs that much lower.
// `spot` identifies the place, for keeping only its lowest-drop clip.
function* crossingPointsForWall(model, A, pairsA) {
    const ch = model.checkHeight;
    const nx = A.nx * A.invNXZ, nz = A.nz * A.invNXZ;
    const tx = -nz, tz = nx; // along the wall
    const us = [A.ax * tx + A.az * tz, A.bx * tx + A.bz * tz, A.cx * tx + A.cz * tz];
    const u0 = Math.min(...us) - 2, u1 = Math.max(...us) + 2;
    // Only the stretches of A near a wall it pushes against (a long wall can
    // be thousands of units long): each partner's extent along A, plus the
    // radius and room for the slope of a wall to move its plane sideways.
    // Snapped radius in front of A, Link is only behind B near where the
    // planes meet: within (5 radius + 25) / sin(angle) of it along A.
    const near = [];
    for (const { B, lo, hi } of pairsA) {
        const bu = [B.ax * tx + B.az * tz, B.bx * tx + B.bz * tz, B.cx * tx + B.cz * tz];
        const pad = model.radius + 30;
        let a = Math.min(...bu) - pad, b = Math.max(...bu) + pad;
        const pl = planesMeet(A, B, lo, 0, 0), ph = planesMeet(A, B, hi, 0, 0);
        if (pl && ph) {
            const sinAB = Math.abs(A.nx * B.nz - A.nz * B.nx) / (A.nXZ * B.nXZ);
            const w = (5 * model.radius + 25) / sinAB;
            const ul = pl.x * tx + pl.z * tz, uh = ph.x * tx + ph.z * tz;
            a = Math.max(a, Math.min(ul, uh) - w);
            b = Math.min(b, Math.max(ul, uh) + w);
            if (b < a) continue;
        }
        near.push([a, b]);
    }
    const isNear = u => near.some(([a, b]) => u >= a && u <= b);
    const onPlane = (u, h) => {
        // nx*x + nz*z = -(dist + ny*h) (unnormalised normal), plus u along the wall
        const c = -(A.dist + A.ny * h) / (A.nXZ * A.nXZ);
        return { x: c * A.nx + u * tx, z: c * A.nz + u * tz };
    };
    // Floors Link could be standing on while his check crosses the plane at q:
    // a sloped wall's plane at check height can be well away from where the
    // wall meets the floor, so look up to 30 in front of it and 12 behind,
    // straight out and at 45 degrees (in a V corner straight out can run onto
    // the other wall).
    const c45 = Math.SQRT1_2;
    const outDirs = [[nx, nz], [(nx - nz) * c45, (nz + nx) * c45], [(nx + nz) * c45, (nz - nx) * c45]];
    const floorsBeside = q => {
        const out = new Set();
        for (const side of [-12, -2, 4, 14, 28]) {
            for (const [dx, dz] of outDirs) {
                for (const y of model.floorsNear(q.x + side * dx, q.z + side * dz)) out.add(y);
            }
        }
        return [...out];
    };
    const seedHeights = [A.minY, (A.minY + A.maxY) / 2, A.maxY];
    // A sloped wall's plane moves sideways with height, so the floors next to
    // it are looked for where it is at their own check height: seeded from a
    // few heights and refined once. Floors barely change along a wall, so this
    // is done every FLOOR_BLOCK units of it.
    const floorsFor = u => {
        // floorsBeside of a position already looked up for this block (on a
        // vertical wall the plane is in the same place at every height)
        const memo = new Map();
        const beside = q => {
            const k = q.x + "," + q.z;
            let r = memo.get(k);
            if (!r) memo.set(k, r = floorsBeside(q));
            return r;
        };
        const ys = new Set();
        const seeds = new Set();
        for (const hs of seedHeights) for (const y of beside(onPlane(u, hs))) seeds.add(y);
        // (a sloped floor's height differs a little between lookups, so heights
        // within half a unit count as the same floor)
        const same = (list, y) => list.some(v => Math.abs(v - y) < 0.5);
        for (const y0 of seeds) {
            for (const y of beside(onPlane(u, y0 + ch))) {
                const h = y + ch;
                if (h - Math.max(model.lowDrop, GROUND_DROP) > A.maxY + 1 || h < A.minY - 1 || same([...ys], y)) continue;
                ys.add(y);
            }
        }
        return ys;
    };
    let block = null, blockYs = null;
    for (let ui = 0, u = u0; u <= u1; ui++, u += CROSS_STEP) {
        if (!isNear(u)) continue;
        const b = Math.floor(u / FLOOR_BLOCK);
        if (b !== block) {
            block = b;
            blockYs = floorsFor((b + 0.5) * FLOOR_BLOCK);
        }
        const ys = blockYs;
        for (const y of ys) {
            // Falling ones at half the spacing along the wall and every 4 units
            // of drop, to keep the scan time down.
            for (let drop = 0; drop <= model.lowDrop; drop += drop === 0 ? 2 : 4) {
                if (drop > 0 && ui % 2) break;
                // (walking, posNext is GROUND_DROP below the floor)
                const low = F(y - (drop || GROUND_DROP));
                // BgCheck_CheckWallImpl: when checkHeight + dy < 5 its line
                // test runs from prevPos to posNext at the feet, floors
                // included, instead of level at posNext.y + checkHeight. From a
                // start on the floor that line hits the floor where it starts
                // and Link stays put (seen in-game, MM Ikana Castle, drop 26),
                // so crossings with a bigger drop can't clip.
                if (F(ch + F(low - y)) < 5) break;
                const h = F(low + ch);
                if (h < A.minY - 1 || h > A.maxY + 1) continue;
                const i = onPlane(u, h);
                yield { x: F(i.x), y: low, z: F(i.z), floorY: y, drop, spot: u + "," + y };
            }
        }
    }
}

// Link moving from prev to next crosses a wall: BgCheck_CheckWallImpl's line
// check (at next.y + checkHeight) stops him at the nearest wall it hits and
// puts him `radius` in front of it, then the frame's pushes run.
function lineFrame(model, prev, next, tol) {
    const h = F(next.y + model.checkHeight);
    // BGCHECK_CHECK_ALL minus ceilings: one face only (a wall Link comes
    // through from behind doesn't stop him), and floors too when he moves
    // more than `radius` this frame.
    const dx = F(next.x - prev.x), dz = F(next.z - prev.z);
    const floors = sq(model.radius) < F(sq(dx) + sq(dz));
    const hit = model.lineHit({ x: prev.x, y: h, z: prev.z }, { x: next.x, y: h, z: next.z }, tol, floors, true);
    if (!hit || isZero(hit.poly.nXZ)) return null;
    const k = F(model.radius * F(1 / hit.poly.nXZ));
    const snapped = { x: F(F(k * hit.poly.nx) + hit.x), y: next.y, z: F(F(k * hit.poly.nz) + hit.z) };
    const trace = [{ poly: hit.poly, from: { ...next }, to: { ...snapped }, line: true }];
    const res = model.sphereStep(snapped, tol, trace);
    return { hit, res, trace };
}

// Does crossing wall A at point `p` clip? Tries 32 directions of movement
// through p, starting one step of MOVE_STEPS before it at an in-bounds spot
// on the ground and ending 1 unit past it. Beyond that the speed doesn't matter:
// the line check puts Link at the crossing point whatever it is. Returns the
// clip with the yaws that work, or null.
function crossingClip(model, A, p, tol) {
    const yaws = [];
    let first = null;
    const tried = new Set();
    for (let i = 0; i < 32; i++) {
        const yaw0 = i * 0x800;
        const dx0 = Math.sin(yaw0 / 0x10000 * 2 * Math.PI), dz0 = Math.cos(yaw0 / 0x10000 * 2 * Math.PI);
        if (Math.abs(dx0 * A.nx + dz0 * A.nz) * A.invNXZ < 0.1) continue; // along the wall
        let found = null;
        for (const dist of MOVE_STEPS) {
            // Link has to be standing still at the start (the walls don't
            // push him there), so the start is where he comes to rest from
            // `dist` back, and he moves from there straight through p.
            const prev = standSpot(model, F(p.x - dist * dx0), F(p.z - dist * dz0), p.floorY);
            if (!prev) continue;
            const key = prev.x + "," + prev.z;
            if (tried.has(key)) continue;
            tried.add(key);
            const vx = p.x - prev.x, vz = p.z - prev.z, len = Math.hypot(vx, vz);
            if (len < 0.5) continue;
            const dx = vx / len, dz = vz / len;
            if (Math.abs(dx * A.nx + dz * A.nz) * A.invNXZ < 0.1) continue;
            // The move the game makes toward p: the s16 yaw, and the speed that
            // gets 1 unit past p (Actor_UpdatePos, with the sine table: the
            // crossing can be a hair off the straight line, which matters
            // right at the edge of an extended plane).
            const yaw = yawOf(vx, vz);
            const speed = F((len + 1) / SPEED_RATE);
            const next = moveStep(prev, yaw, speed);
            // (falling: `drop` below the floor instead)
            if (p.drop > 0) next.y = p.y;
            const f = lineFrame(model, prev, next, tol);
            if (!f || f.hit.poly !== A) continue;
            const clip = clipFromFrame(model, prev, f.res, f.trace, tol, p.drop > 0 ? null : prev.y);
            if (!clip || !model.isInBounds(prev)) continue;
            if (p.drop > 0) {
                const end = landing(model, f.res, p.floorY);
                if (!end) continue;
                clip.end = end;
            } else if (model.isInBounds(clip.end)) {
                continue;
            }
            found = { ...clip, prev, next, yaw, speed, res: f.res, at: { x: f.hit.x, y: next.y, z: f.hit.z } };
            break;
        }
        if (!found) continue;
        if (!yaws.includes(found.yaw)) yaws.push(found.yaw);
        if (!first) first = found;
    }
    return first ? { ...first, yaws } : null;
}

// Where Link can stand still near (x, z): on the highest floor within 10 of
// floorY, moved to where the wall pushes leave him alone (e.g. radius out from
// a wall), with his height from the floor he ends up over. Null if there's no
// floor there or the pushes don't settle.
function standSpot(model, x, z, floorY) {
    // (then the top one of the floors right there: overlapping triangles of a
    // bumpy slope give a few heights at once, and Link stands on the highest)
    const floorAt = (fx, fz) => {
        const ys = model.floorsAt(fx, fz);
        const y = ys.filter(y => Math.abs(y - floorY) <= 10).sort((a, b) => b - a)[0];
        return y === undefined ? y : Math.max(...ys.filter(v => v <= y + 3));
    };
    let y = floorAt(x, z);
    if (y === undefined) return null;
    for (let i = 0; i < 3; i++) {
        const rest = model.restingSpot({ x, y, z });
        if (!rest) return null;
        const ry = floorAt(rest.x, rest.z);
        if (ry === undefined) return null;
        if (rest.x === x && rest.z === z && ry === y) return rest;
        x = rest.x; z = rest.z; y = ry;
    }
    return null;
}

// An in-bounds spot Link can stand still at, about one step of MOVE_STEPS from
// p (16 directions, on a floor near p's height), that he can move straight to
// p from: the line check at sphere height hits nothing on the way. Null if none.
function reachFrom(model, p, floorY = p.y) {
    const h = F(p.y + model.checkHeight);
    for (const dist of MOVE_STEPS) {
        for (let i = 0; i < 16; i++) {
            const ang = i / 16 * 2 * Math.PI;
            const prev = standSpot(model, F(p.x - dist * Math.sin(ang)), F(p.z - dist * Math.cos(ang)), floorY);
            if (!prev) continue;
            const { x, z } = prev;
            if (Math.hypot(p.x - x, p.z - z) > REACH_DIST) continue;
            if (model.lineHit({ x, y: h, z }, { x: p.x, y: h, z: p.z }, LOOSE, false, true)) continue;
            if (model.isInBounds(prev)) return prev;
        }
    }
    return null;
}

// Link at p (feet, as after his movement for the frame): this frame's wall
// pushes (no movement left, so no line check) take him through a wall and he
// stays there. He has to be able to move to p first.
function standingClip(model, floorPt) {
    const p = { x: floorPt.x, y: F(floorPt.y - GROUND_DROP), z: floorPt.z };
    const trace = [];
    const res = model.sphereStep(p, LOOSE, trace);
    if (trace.length === 0) return null;
    const clip = clipFromFrame(model, p, res, trace, LOOSE, floorPt.y);
    if (!clip) return null;
    if (!model.isInBounds(floorPt) || model.isInBounds(clip.end)) return null;
    // Link walks there himself, from standing still: the game's move toward p
    // (s16 yaw, f32 speed, the sine table) stops a hair off it, so the frame
    // is checked again exactly where he ends up.
    const w = walkInto(model, p, floorPt.y);
    if (!w) return null;
    const strictTrace = [];
    const strictRes = model.sphereStep(w.next, STRICT, strictTrace);
    const strict = clipFromFrame(model, w.prev, strictRes, strictTrace, STRICT, w.prev.y);
    return {
        kind: strict ? "acute" : "extended",
        from: w.next, floorY: w.prev.y, prev: w.prev, next: w.next, yaw: w.yaw, speed: w.speed,
        res: w.res, end: w.clip.end,
        crossed: w.clip.crossed, pusher: w.clip.pusher,
    };
}

// A standing start about one step of MOVE_STEPS from p (16 directions) that
// Link can walk from to (nearly) p in one frame and clip there: the game's
// move at the yaw toward p and the speed that covers the distance, no wall or
// floor in the way of its line check, then the frame's pushes clip. The first
// one found, or null.
function walkInto(model, p, floorY) {
    for (const dist of MOVE_STEPS) {
        for (let i = 0; i < 16; i++) {
            const ang = i / 16 * 2 * Math.PI;
            const prev = standSpot(model, F(p.x - dist * Math.sin(ang)), F(p.z - dist * Math.cos(ang)), floorY);
            if (!prev) continue;
            const vx = p.x - prev.x, vz = p.z - prev.z, len = Math.hypot(vx, vz);
            if (len < 0.01 || len > REACH_DIST) continue;
            const yaw = yawOf(vx, vz);
            const speed = F(len / SPEED_RATE);
            const next = moveStep(prev, yaw, speed);
            if (lineFrame(model, prev, next, LOOSE)) continue;
            const trace = [];
            const res = model.sphereStep(next, LOOSE, trace);
            const clip = clipFromFrame(model, prev, res, trace, LOOSE, prev.y);
            if (!clip || model.isInBounds(clip.end) || !model.isInBounds(prev)) continue;
            return { prev, next, yaw, speed, res, clip };
        }
    }
    return null;
}

// The same with Link falling: this frame's movement puts his feet `drop` below
// the floor at p, so the pushes run lower. Afterwards the floor check (a ray
// down from prevPos.y + 50, and he was at least at the floor's height) lands
// him on the highest floor at most 50 above the one at p under where he was
// pushed to, and he has to be out of bounds there.
// Where falling Link lands after being pushed to `res`, if that's out of
// bounds: the floor check's ray comes down from prevPos.y + 50 (he was at least
// at the height of the floor at floorY), so the highest floor under res at most
// 50 above that one, then two frames standing there. No floor: he falls out of
// bounds. Null if he lands in bounds.
function landing(model, res, floorY) {
    // (the game's floor check: floors and upward-facing walls, see floorCheck)
    const land = model.floorCheck(res.x, res.z, F(floorY + 50));
    if (land === null) return { x: res.x, y: res.y, z: res.z, noFloor: true };
    const low = F(land - GROUND_DROP);
    const s = model.sphereStep(model.sphereStep({ x: res.x, y: low, z: res.z }, LOOSE, null), LOOSE, null);
    const end = { x: s.x, y: land, z: s.z };
    return model.isInBounds(end) ? null : end;
}

function lowClip(model, p, drop) {
    const low = { x: p.x, y: F(p.y - drop), z: p.z };
    const trace = [];
    const res = model.sphereStep(low, LOOSE, trace);
    if (trace.length === 0) return null;
    const clip = clipFromFrame(model, low, res, trace, LOOSE);
    if (!clip) return null;
    if (!model.isInBounds({ x: p.x, y: p.y, z: p.z })) return null;
    const end = landing(model, res, p.y);
    if (!end) return null;
    const prev = reachFrom(model, low, p.y);
    if (!prev) return null;
    // Extended plane only: whether it also clips with them removed.
    let strict = false;
    if (model.extendedOnly) {
        const strictTrace = [];
        const strictRes = model.sphereStep(low, STRICT, strictTrace);
        strict = !!clipFromFrame(model, low, strictRes, strictTrace, STRICT);
    }
    return {
        kind: "low", drop, strict,
        from: low, floorY: p.y, prev, res, end,
        crossed: clip.crossed, pusher: clip.pusher,
    };
}

// Can Link get to clip `c` from standing still somewhere? A standable start:
// where he comes to rest (the wall pushes applied until they stop moving him,
// e.g. resting against a slope) from a spot one frame's movement away in one
// of 32 directions, on a floor near the clip's floor height and in bounds. The
// frame from there straight at the point has to produce the clip (for a
// standing point: nothing stops him on the way to it; for a crossing point:
// the line check stops him on the crossed wall and the clip follows). Returns
// the lowest speed that works, the start and the yaw, or null.
function reachability(model, c) {
    const floorRef = c.floorY ?? c.from.y;
    const P = c.from;
    const over = c.cross ? 0.5 : 0; // a crossing has to get past the plane
    const tried = new Set();
    let best = null;
    for (let i = 0; i < 32; i++) {
        const ang = i / 32 * 2 * Math.PI;
        for (let d = REACH_STEP; d <= REACH_DIST; d += REACH_STEP) {
            if (best && (d + over) / SPEED_RATE >= best.speed + 2) break;
            const sx = F(P.x - d * Math.sin(ang)), sz = F(P.z - d * Math.cos(ang));
            const start = standSpot(model, sx, sz, floorRef);
            if (!start) continue;
            const key = start.x + "," + start.z;
            if (tried.has(key)) continue;
            tried.add(key);
            const vx = P.x - start.x, vz = P.z - start.z;
            const len = Math.hypot(vx, vz);
            if (len < 0.01 || len > REACH_DIST) continue;
            const speed = F((len + over) / SPEED_RATE);
            if (best && speed >= best.speed) continue;
            const yaw = yawOf(vx, vz);
            if (c.cross) {
                // the game's move at that yaw and speed
                const next = moveStep(start, yaw, speed);
                if (c.drop > 0) next.y = P.y;
                const f = lineFrame(model, start, next, LOOSE);
                if (!f || f.hit.poly !== c.pusher) continue;
                const clip = clipFromFrame(model, start, f.res, f.trace, LOOSE, c.drop > 0 ? null : start.y);
                if (!clip || clip.crossed !== c.crossed) continue;
                if (c.drop > 0 ? !landing(model, f.res, floorRef) : model.isInBounds(clip.end)) continue;
            } else {
                const h = F(P.y + model.checkHeight);
                if (model.lineHit({ x: start.x, y: h, z: start.z }, { x: P.x, y: h, z: P.z }, LOOSE, false, true)) continue;
            }
            if (!model.isInBounds(start)) continue;
            best = { speed, start, yaw };
        }
    }
    return best;
}

// Yield to the page between chunks of work. A MessageChannel message, since
// setTimeout gets throttled to once a second in a background tab.
const yieldChannel = new MessageChannel();
const yieldQueue = [];
yieldChannel.port1.onmessage = () => yieldQueue.shift()?.();
const nextTask = () => new Promise(resolve => {
    yieldQueue.push(resolve);
    yieldChannel.port2.postMessage(null);
});

let scanToken = 0;

async function scanWallPushClips(model, onProgress) {
    const token = ++scanToken;
    const pairs = wallPairCandidates(model);
    const seen = new Set();
    const clips = [];
    // Extended plane only: wall pairs with a point that clips without the
    // extended planes too (acute angle clips)
    const acutePairs = new Set();
    const pairKey = c => c.pusher.id + ":" + c.crossed.id;

    let lastYield = performance.now();
    for (let pi = 0; pi < pairs.length; pi++) {
        for (const p of nextPositionsForPair(model, pairs[pi])) {
            const key = p.x + "," + p.z + "," + p.y;
            const h = p.y + model.checkHeight;
            if (h - GROUND_DROP >= p.lo && h - GROUND_DROP <= p.hi && !seen.has(key)) {
                seen.add(key);
                const c = standingClip(model, p);
                if (c) {
                    // (extended plane only: an acute one is still found, so the
                    // point is done with, just not kept - and its pair is acute)
                    if (model.extendedOnly && c.kind === "acute") acutePairs.add(pairKey(c));
                    else clips.push(c);
                    continue;
                }
            }
            // Falling: feet 2..lowDrop below the floor this frame.
            for (let k = 2; k <= model.lowDrop; k += 2) {
                const hk = h - k;
                if (hk < p.lo || hk > p.hi) continue;
                const lowKey = key + "," + k;
                if (seen.has(lowKey)) continue;
                seen.add(lowKey);
                const c = lowClip(model, p, k);
                if (c) {
                    if (model.extendedOnly && c.strict) acutePairs.add(pairKey(c));
                    else clips.push(c);
                    break;
                }
            }
        }

        if (performance.now() - lastYield > 30) {
            onProgress(pi + 1, pairs.length * 2, clips.length);
            await nextTask();
            if (token !== scanToken) return null;
            lastYield = performance.now();
        }
    }

    // Crossing points: the walls that push against another wall's back.
    const partnersOf = new Map(), pairsOf = new Map();
    for (const pair of pairs) {
        if (!partnersOf.has(pair.A)) { partnersOf.set(pair.A, []); pairsOf.set(pair.A, []); }
        partnersOf.get(pair.A).push(pair.B);
        pairsOf.get(pair.A).push(pair);
    }
    const pushers = [...partnersOf.keys()];
    const crossFrames = new Set();
    for (let ai = 0; ai < pushers.length; ai++) {
        const A = pushers[ai];
        const k = F(model.radius * F(1 / A.nXZ));
        const done = new Set();
        for (const p of crossingPointsForWall(model, A, pairsOf.get(A))) {
            if (performance.now() - lastYield > 30) {
                onProgress(pairs.length + Math.round(ai / pushers.length * pairs.length), pairs.length * 2, clips.length);
                await nextTask();
                if (token !== scanToken) return null;
                lastYield = performance.now();
            }
            if (done.has(p.spot)) continue;
            // Quick test: stopped here, is Link behind one of the walls A pushes
            // against, inside its triangle at his check height, and more than
            // the 4 units a wall still pushes him back from?
            const snapped = { x: F(F(k * A.nx) + p.x), y: p.y, z: F(F(k * A.nz) + p.z) };
            const res = model.sphereStep(snapped, LOOSE, null);
            const h = p.y + model.checkHeight;
            let behind = false;
            for (const B of partnersOf.get(A)) {
                const d = planeDist(B, res.x, h, res.z);
                if (d >= -3.5 || d < -4 * model.radius) continue;
                const t = d / B.nMag;
                if (pointInTri3D(B, res.x - t * B.nx, h - t * B.ny, res.z - t * B.nz, 1)) { behind = true; break; }
            }
            if (!behind) continue;
            // Somewhere in bounds next to the crossing to come from.
            const nx = A.nx * A.invNXZ, nz = A.nz * A.invNXZ;
            if (![3, -3, 12, -12].some(sd => model.isInBounds({ x: p.x + sd * nx, y: p.floorY, z: p.z + sd * nz }))) continue;

            const clip = crossingClip(model, A, p, LOOSE);
            if (!clip) continue;
            done.add(p.spot);
            // Floors at slightly different heights around a spot can come
            // to rest at the same start: one point per frame.
            const frameKey = [clip.prev.x, clip.prev.y, clip.prev.z, clip.next.x, clip.next.y, clip.next.z].join(",");
            if (crossFrames.has(frameKey)) continue;
            crossFrames.add(frameKey);
            // (extended plane only: falling ones are checked without the
            // extended planes too, and the ones that still clip left out)
            const strict = (p.drop === 0 || model.extendedOnly) && crossingClip(model, A, p, STRICT);
            if (model.extendedOnly && strict) { acutePairs.add(A.id + ":" + clip.crossed.id); continue; }
            clips.push({
                kind: p.drop > 0 ? "low" : strict ? "acute" : "extended", cross: true, drop: p.drop,
                from: clip.at, floorY: p.floorY, prev: clip.prev, next: clip.next, res: clip.res, end: clip.end, yaws: clip.yaws,
                yaw: clip.yaw, speed: clip.speed,
                crossed: clip.crossed, pusher: A,
            });
        }

        if (performance.now() - lastYield > 30) {
            onProgress(pairs.length + Math.round((ai + 1) / pushers.length * pairs.length), pairs.length * 2, clips.length);
            await nextTask();
            if (token !== scanToken) return null;
            lastYield = performance.now();
        }
    }

    // Extended plane only: a wall pair with any point that also clips without
    // the extended planes is an acute angle clip, so all its points go.
    if (model.extendedOnly) return clips.filter(c => !acutePairs.has(pairKey(c)));
    return clips;
}

// One group per (pusher, clipped wall, standing / crossing, kind): a pair's
// points that only clip thanks to the extended planes are their own group.
// (imported results with several forms: clips carry their `form` and the
// `model` (radius, check height) they were found with, and each group is one form's)
function groupClips(clips) {
    const groups = new Map();
    for (const c of clips) {
        const key = [c.form ?? "", c.pusher.id, c.crossed.id, c.cross ? "cross" : "stand", c.kind].join(":");
        let g = groups.get(key);
        if (!g) groups.set(key, g = { pusher: c.pusher, crossed: c.crossed, cross: !!c.cross, kind: c.kind, clips: [], form: c.form, model: c.model });
        g.clips.push(c);
    }
    return [...groups.values()];
}

////////////////////////////////////////
// Markers
////////////////////////////////////////

// Shortest decimal that reads back as the same f32, and the f32's bits.
function f32Str(v) {
    let str = String(v);
    for (let p = 1; p <= 9; p++) {
        const t = Number(v.toPrecision(p));
        if (F(t) === v) { str = String(t); break; }
    }
    const dv = new DataView(new ArrayBuffer(4));
    dv.setFloat32(0, v);
    return `${str} (0x${dv.getUint32(0).toString(16).toUpperCase().padStart(8, "0")})`;
}
const fmt = p => `x ${f32Str(p.x)}, y ${f32Str(p.y)}, z ${f32Str(p.z)}`;
const hex4 = n => "0x" + (n & 0xFFFF).toString(16).toUpperCase().padStart(4, "0");

function describeReach(c) {
    if (c.reach === undefined) return `  reachability: tick "Reachable only" to work it out`;
    if (!c.reach) return `  not reachable from a standable start (at up to speed ${REACH_DIST / SPEED_RATE})`;
    const r = c.reach;
    return `  reachable: stand at ${fmt(r.start)}, move at yaw ${hex4(r.yaw)} with speed ${r.speed.toFixed(2)} or more`;
}

function describeClip(g, c, checkHeight) {
    const form = g.form ? `  form: ${g.form} (radius ${g.model.radius}, check height ${+checkHeight.toPrecision(7)})\n` : "";
    const lines = describeClipLines(g, c, checkHeight).split("\n");
    return [lines[0], form + lines.slice(1).join("\n")].join("\n") + "\n" + describeReach(c);
}

function describeClipLines(g, c, checkHeight) {
    const behind = -planeDist(g.crossed, c.end.x, F(c.from.y + checkHeight), c.end.z);
    if (c.cross) {
        const title = { acute: "acute angle", extended: "extended plane only", low: "falling" }[g.kind];
        return [
            `WALL CROSSING CLIP (${title}): crossing TRI ${g.pusher.id} puts Link through TRI ${g.crossed.id}`,
            `  move through: ${fmt(c.from)} (feet; the crossing is ${+checkHeight.toPrecision(7)} above)`,
            ...(c.drop > 0 ? [`  that's ${c.drop} below the floor (y ${f32Str(c.floorY)}): falling at y velocity ` +
                `${(-c.drop / 1.5).toFixed(2)} or faster this frame`] : []),
            `  works moving at yaw ${c.yaws.map(hex4).join(", ")} (any speed that gets past TRI ${g.pusher.id}'s plane)`,
            `  e.g. standing still at ${fmt(c.prev)} (feet), moving to ${fmt(c.next)}` +
                (c.speed !== undefined ? ` (yaw ${hex4(c.yaw)}, speed ${f32Str(c.speed).split(" ")[0]})` : ""),
            `  line check + pushes put Link at: ${fmt(c.res)}`,
            c.drop > 0
                ? (c.end.noFloor ? `  no floor under where he's pushed to: falls out of bounds` : `  lands at: ${fmt(c.end)} (out of bounds)`)
                : `  after 2 more frames: ${fmt(c.end)} (${behind.toFixed(3)} units behind TRI ${g.crossed.id})`,
            ...(c.drop > 0 ? [] : [`  this point ${c.kind === "acute" ? "also clips" : "does not clip"} with the extended planes removed`]),
        ].join("\n");
    }
    if (c.kind === "low") {
        return [
            `LOW WALL CLIP (falling): TRI ${g.pusher.id} pushes Link through TRI ${g.crossed.id}`,
            `  Link at:   ${fmt(c.from)} (after moving there, e.g. from ${fmt(c.prev)})`,
            `  that's ${c.drop} below the floor (y ${f32Str(c.floorY)}): falling at y velocity ${(-c.drop / 1.5).toFixed(2)} or faster this frame`,
            `  pushed to: ${fmt(c.res)}`,
            c.end.noFloor ? `  no floor under where he's pushed to: falls out of bounds` : `  lands at:  ${fmt(c.end)} (out of bounds)`,
        ].join("\n");
    }
    return [
        `WALL PUSH CLIP (${g.kind === "acute" ? "acute angle" : "extended plane only"}): ` +
            `TRI ${g.pusher.id} pushes Link through TRI ${g.crossed.id}`,
        ...(c.speed !== undefined ? [
            `  stand still at ${fmt(c.prev)} (feet), move at yaw ${hex4(c.yaw)} with speed ${f32Str(c.speed).split(" ")[0]}`,
            `  Link at:   ${fmt(c.from)} (after that move)`,
        ] : [`  Link at:   ${fmt(c.from)} (after moving there, e.g. from ${fmt(c.prev)})`]),
        `  pushed to: ${fmt(c.res)}`,
        `  after 2 more frames: ${fmt(c.end)} (${behind.toFixed(3)} units behind TRI ${g.crossed.id})`,
        `  this point ${c.kind === "acute" ? "also clips" : "does not clip"} with the extended planes removed`,
    ].join("\n");
}

function buildMarkerGroup(model, groups, color, checkHeight) {
    const group = new THREE.Group();
    if (groups.length === 0) return group;

    const wallTris = (polys) => {
        const arr = [];
        for (const p of polys) arr.push(p.ax, p.ay, p.az, p.bx, p.by, p.bz, p.cx, p.cy, p.cz);
        return new THREE.Float32BufferAttribute(arr, 3);
    };
    const wallMesh = (polys, c, opacity) => {
        const geom = new THREE.BufferGeometry();
        geom.setAttribute("position", wallTris(polys));
        const mesh = new THREE.Mesh(geom, new THREE.MeshBasicMaterial({
            color: c, side: THREE.DoubleSide, transparent: true, opacity,
            depthWrite: false, polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2,
        }));
        mesh.renderOrder = 5;
        mesh.userData.unselectable = true;
        return mesh;
    };

    // Clipped walls in the marker colour first, so primaryColorTarget picks them.
    const crossed = new Set(), pushers = new Set();
    for (const g of groups) { crossed.add(g.crossed); pushers.add(g.pusher); }
    group.add(wallMesh([...crossed], color, 0.45));
    group.add(wallMesh([...pushers].filter(p => !crossed.has(p)), PUSHER_COLOR, 0.35));

    // Where Link is on the frame before each clip, and a line to where he ends up.
    // Each dot's description goes in userData.clipSpots (same order as the
    // positions) for selection.js to show when it's clicked.
    // A second, dimmer line runs from where Link stands still before the frame.
    const pts = [], lines = [], startLines = [], spots = [];
    const lift = 2;
    for (const g of groups) {
        for (const c of g.clips) {
            // Walking, the frame's position is below the floor (GROUND_DROP):
            // drawn on the floor under it instead, the end moved up with it.
            let up = lift;
            if (!(c.drop > 0) && c.floorY !== undefined) {
                const top = F(c.from.y + GROUND_DROP);
                // (only a floor near his height: past the edge of a ledge the
                // next one down can be far below)
                const under = model.floorsAt(c.from.x, c.from.z).filter(y => y <= top + 20 && y >= top - 20);
                up += (under.length ? Math.max(...under) : top) - c.from.y;
            }
            pts.push(c.from.x, c.from.y + up, c.from.z);
            lines.push(c.from.x, c.from.y + up, c.from.z, c.end.x, c.end.y + up, c.end.z);
            if (c.prev) startLines.push(c.prev.x, c.prev.y + lift, c.prev.z, c.from.x, c.from.y + up, c.from.z);
            spots.push(describeClip(g, c, checkHeight));
        }
    }
    const startGeom = new THREE.BufferGeometry();
    startGeom.setAttribute("position", new THREE.Float32BufferAttribute(startLines, 3));
    const startObj = new THREE.LineSegments(startGeom, new THREE.LineBasicMaterial({ color: 0x60ff60, depthTest: false, transparent: true, opacity: 0.5 }));
    startObj.renderOrder = 998;
    startObj.userData.unselectable = true;
    group.add(startObj);
    const ptGeom = new THREE.BufferGeometry();
    ptGeom.setAttribute("position", new THREE.Float32BufferAttribute(pts, 3));
    const points = new THREE.Points(ptGeom, new THREE.PointsMaterial({ color, size: 12, sizeAttenuation: false, depthTest: false }));
    points.renderOrder = 999;
    points.userData.unselectable = true;
    points.userData.clipSpots = spots;
    group.add(points);

    const lineGeom = new THREE.BufferGeometry();
    lineGeom.setAttribute("position", new THREE.Float32BufferAttribute(lines, 3));
    const lineObj = new THREE.LineSegments(lineGeom, new THREE.LineBasicMaterial({ color: 0xffffff, depthTest: false, transparent: true, opacity: 0.8 }));
    lineObj.renderOrder = 999;
    lineObj.userData.unselectable = true;
    group.add(lineObj);

    return group;
}

const MODEL_NAMES = { acute: "Acute Angle Clips", extended: "Extended Plane Clips", low: "Low Wall Clips (falling)" };

// The marker rows added (one per kind, or per kind and form for imported
// results with several forms), to take away again.
let markerNames = [];

function removeMarkerModels(scene) {
    const names = markerNames;
    markerNames = [];
    for (const name of names) {
        const idx = loadedModels.findIndex(m => m.name === name);
        if (idx < 0) continue;
        const old = loadedModels[idx];
        scene.remove(old.mesh);
        old.mesh.traverse(o => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
        loadedModels.splice(idx, 1);
        const row = Array.from(document.querySelectorAll('.controls > *')).find(el => el.dataset && el.dataset.modelName === name);
        if (row) row.remove();
    }
}

// The shown points in tools/clipfinder's JSON format (wall-push-clips-2), for
// tools/clipfinder/wall_clip_tester.lua (set its TESTS_FILE to the file) and for
// importing again. `info.form`: the form of a scan made here ({form, model}),
// used for groups without their own (imported ones have theirs). Numbers are
// the exact f32s, written the shortest way that reads back the same.
function exportJson(groups, info) {
    const num = v => (Number.isInteger(v) || F(v) !== v) ? String(v) : f32Str(v).split(" ")[0];
    const vec = p => `[${num(p.x)},${num(p.y)},${num(p.z)}]`;
    const forms = new Map();
    for (const g of groups) {
        const name = g.form ?? info.form.form;
        if (!forms.has(name)) forms.set(name, g.model ?? info.form.model);
    }
    const clips = [];
    for (const g of groups) {
        for (const c of g.clips) {
            const f = [
                `"form":${JSON.stringify(g.form ?? info.form.form)}`, `"kind":"${g.kind}"`, `"cross":${!!c.cross}`,
                `"drop":${c.drop ?? 0}`, `"pusher":${g.pusher.id}`, `"crossed":${g.crossed.id}`,
                `"from":${vec(c.from)}`, `"prev":${vec(c.prev)}`,
            ];
            if (c.next) f.push(`"next":${vec(c.next)}`);
            f.push(`"res":${vec(c.res)}`, `"end":${vec(c.end)}`);
            if (c.end.noFloor) f.push(`"endNoFloor":true`);
            if (c.floorY !== undefined) f.push(`"floorY":${num(c.floorY)}`);
            if (c.yaws) f.push(`"yaws":[${c.yaws.join(",")}]`);
            if (c.speed !== undefined) f.push(`"yaw":${c.yaw & 0xFFFF}`, `"speed":${num(c.speed)}`);
            if (c.strict) f.push(`"strict":true`);
            if (c.reach === null) f.push(`"reach":null`);
            else if (c.reach) f.push(`"reach":{"speed":${num(c.reach.speed)},"yaw":${c.reach.yaw & 0xFFFF},"start":${vec(c.reach.start)}}`);
            clips.push(`    {${f.join(",")}}`);
        }
    }
    return [
        `{`,
        `  "format": "wall-push-clips-2",`,
        `  "game": ${JSON.stringify(info.game)}, "map": ${JSON.stringify(info.map)}, "falling": ${info.falling}, ` +
            `"extendedOnly": ${info.extendedOnly}, "numPolygons": ${info.numPolygons},`,
        `  "forms": [`,
        [...forms].map(([name, m]) => `    {"form":${JSON.stringify(name)},"radius":${num(m.radius)},"checkHeight":${num(m.checkHeight)}}`).join(",\n"),
        `  ],`,
        `  "clips": [`,
        clips.join(",\n"),
        `  ]`,
        `}`,
        ``,
    ].join("\n");
}

function logGroups(groups) {
    const xyz = p => [p.x, p.y, p.z].map(v => f32Str(v).split(" ")[0]).join(", ");
    const rows = groups.map(g => {
        const c = g.clips.find(c => c.kind === g.kind);
        return {
            kind: g.kind,
            type: g.cross ? "crossing" : "standing",
            pusherPoly: g.pusher.id,
            clippedPoly: g.crossed.id,
            points: g.clips.length,
            example: xyz(c.from),
            end: xyz(c.end),
        };
    });
    console.log("Wall push clips:");
    console.table(rows);
}

////////////////////////////////////////
// UI
////////////////////////////////////////

export function setupWallPushClipUI(scene) {
    const container = document.getElementById("wallClipContainer");
    const radiusSel = document.getElementById("wallClipRadius");
    const button = document.getElementById("wallClipScan");
    const status = document.getElementById("wallClipStatus");
    const reachableChk = document.getElementById("wallClipReachable");
    const maxSpeedInput = document.getElementById("wallClipMaxSpeed");
    const extendedOnlyChk = document.getElementById("wallClipExtendedOnly");
    if (!container) return;

    // The in-browser scan and its options (the native tools/clipfinder is the
    // usual route: its results only need Import and the reachable filter)
    const advancedChk = document.getElementById("wallClipShowAdvanced");
    const advanced = document.getElementById("wallClipAdvanced");
    const showAdvanced = () => { advanced.style.display = advancedChk.checked ? "flex" : "none"; };
    advancedChk.addEventListener("change", showAdvanced);
    showAdvanced();

    // The last scan, drawn again when the reachable filter changes.
    let last = null;

    const defaultSpeed = () => {
        const opt = RADIUS_OPTIONS[game]?.[Number(radiusSel.value)];
        if (opt) maxSpeedInput.value = opt[2];
    };

    let optionsGame = null;
    const refresh = () => {
        const visible = game === "OOT" || game === "MM";
        container.style.display = visible ? "flex" : "none";
        if (visible && optionsGame !== game) {
            optionsGame = game;
            radiusSel.innerHTML = "";
            RADIUS_OPTIONS[game].forEach(([label], i) => {
                const o = document.createElement("option");
                o.value = i;
                o.textContent = label;
                radiusSel.appendChild(o);
            });
            defaultSpeed();
        }
    };
    document.getElementById("selected-game").addEventListener("change", refresh);
    radiusSel.addEventListener("change", defaultSpeed);
    document.getElementById("loadMap").addEventListener("click", () => {
        scanToken++; // abandon a scan of the previous map
        last = null;
        status.textContent = "";
        button.disabled = false;
        refresh();
    });
    refresh();

    // Reachability is worked out the first time the filter is switched on.
    let reachToken = 0;
    const computeReach = async () => {
        const token = ++reachToken;
        const clips = last.groups.flatMap(g => g.clips);
        let lastYield = performance.now();
        for (let i = 0; i < clips.length; i++) {
            clips[i].reach = reachability(clips[i].model ?? last.model, clips[i]);
            if (performance.now() - lastYield > 30) {
                status.textContent = `Checking reachability ${Math.floor((i + 1) / clips.length * 100)}%`;
                await nextTask();
                if (token !== reachToken || !last) return false;
                lastYield = performance.now();
            }
        }
        last.reachDone = true;
        return true;
    };

    const render = async () => {
        if (!last) return;
        if (reachableChk.checked && !last.reachDone) {
            reachableChk.disabled = true;
            const done = await computeReach();
            reachableChk.disabled = false;
            if (!done) return;
        }
        removeMarkerModels(scene);
        const maxSpeed = Number(maxSpeedInput.value);
        let shown = reachableChk.checked
            ? last.groups.map(g => ({ ...g, clips: g.clips.filter(c => c.reach && c.reach.speed <= maxSpeed) }))
                .filter(g => g.clips.length > 0)
            : last.groups;
        // Extended plane only also hides acute clips already found (a scan
        // without it, or imported results): every wall pair (per form) with
        // at least one acute point - or falling one that clips without the
        // extended planes - is an acute angle clip, all its points hidden
        if (extendedOnlyChk.checked) {
            const key = g => (g.form ?? "") + ":" + g.pusher.id + ":" + g.crossed.id;
            const acute = new Set(last.groups.filter(g => g.kind === "acute" || g.clips.some(c => c.strict)).map(key));
            shown = shown.filter(g => !acute.has(key(g)));
        }
        const byKind = {};
        for (const kind of Object.keys(MODEL_NAMES)) byKind[kind] = shown.filter(g => g.kind === kind);
        const colors = { acute: ACUTE_COLOR, extended: EXTENDED_COLOR, low: LOW_COLOR };
        // Several forms (imported): a row per kind and form, each drawn with
        // its own form's radius and check height
        const forms = last.forms && last.forms.length > 1 ? last.forms : [null];
        for (const kind of Object.keys(MODEL_NAMES)) {
            for (const f of forms) {
                const list = f ? byKind[kind].filter(g => g.form === f.form) : byKind[kind];
                if (list.length === 0) continue;
                const model = f ? f.model : last.model;
                const name = MODEL_NAMES[kind] + (f ? ` - ${f.form}` : "");
                const g = buildMarkerGroup(model, list, colors[kind], model.checkHeight);
                scene.add(g);
                loadedModels.push({ name, mesh: g, edges: null });
                markerNames.push(name);
                addModelCheckbox(scene, name, g, null, false, true, "#" + colors[kind].toString(16).padStart(6, "0"), false, primaryColorTarget(g));
            }
        }
        const points = kind => byKind[kind].reduce((n, g) => n + g.clips.length, 0);
        status.textContent = `${points("acute")} acute, ${points("extended")} extended-plane, ${points("low")} low (falling) ` +
            `clip points${reachableChk.checked ? ` reachable at speed ${maxSpeed}` : ""} (${last.note})`;
        window.wallPushClips = shown;
    };
    reachableChk.addEventListener("change", render);
    extendedOnlyChk.addEventListener("change", render);

    document.getElementById("wallClipExport").addEventListener("click", () => {
        if (!last || !window.wallPushClips) {
            status.textContent = "Scan first";
            return;
        }
        const map = document.getElementById("mapDropdown").value;
        // A scan made here: the form's name as the tester reads Link's form
        // ("Fierce Deity (27)" -> "FierceDeity", "Crawlspace (10, child)" -> "Crawlspace")
        const scanForm = (last.scanForm ?? "").replace(/\s*\(.*\)\s*$/, "").replace(/\s+/g, "");
        const text = exportJson(window.wallPushClips, {
            game, map, form: { form: scanForm, model: last.model },
            falling: last.model.lowDrop > 0, extendedOnly: extendedOnlyChk.checked,
            numPolygons: last.model.colCtx.colHeader.numPolygons,
        });
        const label = last.formLabel ?? scanForm;
        const a = document.createElement("a");
        a.href = URL.createObjectURL(new Blob([text], { type: "application/json" }));
        a.download = `${game}_${map}_${label}`.replace(/[^A-Za-z0-9_-]/g, "_") + ".json";
        a.click();
        URL.revokeObjectURL(a.href);
    });
    maxSpeedInput.addEventListener("change", render);

    button.addEventListener("click", async () => {
        const main = loadedModels.find(m => m.name === "Main Model");
        const colCtx = currentColCtx;
        if (!main || !main.mesh || !main.mesh.userData.triangles || !colCtx) {
            status.textContent = "Load a map first";
            return;
        }
        removeMarkerModels(scene);
        last = null;
        const [, radius, , checkHeight = WALL_CHECK_HEIGHT[game]] = RADIUS_OPTIONS[game][Number(radiusSel.value)];
        const falling = document.getElementById("wallClipFalling").checked;
        const model = new CollisionModel(colCtx, main.mesh.userData.triangles, radius, checkHeight, falling);
        model.extendedOnly = extendedOnlyChk.checked;
        button.disabled = true;
        const t0 = performance.now();
        const clips = await scanWallPushClips(model, (done, total, found) => {
            status.textContent = `Scanning ${Math.floor(done / total * 100)}% (${found} found)`;
        });
        button.disabled = false;
        if (!clips) return;

        const groups = groupClips(clips);
        last = {
            groups, model, checkHeight: model.checkHeight, note: ((performance.now() - t0) / 1000).toFixed(1) + "s",
            scanForm: radiusSel.selectedOptions[0]?.textContent ?? "",
        };
        render();
        logGroups(groups);
    });

    // Results from tools/clipfinder (the same scan, native): the points are
    // turned back into the viewer's own objects on the loaded map's collision,
    // so they behave exactly like a scan here (markers, click info, Reachable
    // only, test script export).
    const importInput = document.getElementById("wallClipImportFile");
    document.getElementById("wallClipImport").addEventListener("click", () => importInput.click());
    importInput.addEventListener("change", async () => {
        const file = importInput.files[0];
        importInput.value = "";
        if (!file) return;
        const main = loadedModels.find(m => m.name === "Main Model");
        const colCtx = currentColCtx;
        if (!main || !main.mesh || !main.mesh.userData.triangles || !colCtx) {
            status.textContent = "Load the map first";
            return;
        }
        let data;
        try {
            data = JSON.parse(await file.text());
        } catch (err) {
            status.textContent = `${file.name}: not JSON (${err.message})`;
            return;
        }
        const map = document.getElementById("mapDropdown").value;
        if (data.format !== "wall-push-clips-1" && data.format !== "wall-push-clips-2") {
            status.textContent = `${file.name}: not a clipfinder results file`;
            return;
        }
        if (data.game !== game || data.numPolygons !== colCtx.colHeader.numPolygons) {
            status.textContent = `${file.name} is for ${data.game} ${data.map}; load that map first`;
            return;
        }
        removeMarkerModels(scene);
        last = null;
        // Format 1: one form. Format 2: `forms` (name, radius, check height)
        // and each clip marked with its form, each form getting its own model.
        const forms = (data.forms ?? [{ form: data.form, radius: data.radius, checkHeight: data.checkHeight }]).map(f => ({
            form: f.form,
            model: new CollisionModel(colCtx, main.mesh.userData.triangles, f.radius, f.checkHeight, data.falling),
        }));
        const formOf = new Map(forms.map(f => [f.form, f]));
        const model = forms[0].model;
        const vec = a => ({ x: a[0], y: a[1], z: a[2] });
        const clips = [];
        for (const c of data.clips) {
            const f = formOf.get(c.form ?? forms[0].form);
            if (!f) continue;
            const pusher = f.model.polys.get(c.pusher), crossed = f.model.polys.get(c.crossed);
            if (!pusher || !crossed) continue;
            const end = vec(c.end);
            if (c.endNoFloor) end.noFloor = true;
            const clip = {
                kind: c.kind, cross: c.cross, drop: c.drop, pusher, crossed,
                from: vec(c.from), prev: vec(c.prev), res: vec(c.res), end,
                form: f.form, model: f.model,
            };
            if (c.next) clip.next = vec(c.next);
            if (c.floorY !== undefined) clip.floorY = c.floorY;
            if (c.yaws) clip.yaws = c.yaws;
            if (c.speed !== undefined) { clip.yaw = c.yaw; clip.speed = c.speed; }
            if (c.strict) clip.strict = true;
            // clipfinder --min-speed: the reachability already worked out
            if ("reach" in c) clip.reach = c.reach ? { speed: c.reach.speed, yaw: c.reach.yaw, start: vec(c.reach.start) } : null;
            clips.push(clip);
        }
        const groups = groupClips(clips);
        const formNames = forms.map(f => f.form).join(", ");
        last = { groups, model, forms, formLabel: formNames, checkHeight: model.checkHeight, note: `imported ${formNames}${data.falling ? ", falling" : ""}${data.extendedOnly ? ", extended plane only" : ""}` };
        if (clips.length > 0 && clips.every(c => c.reach !== undefined)) last.reachDone = true;
        if (data.map !== map) console.warn(`wall push clips: ${file.name} says map "${data.map}", "${map}" is loaded (same polygon count)`);
        render();
        logGroups(groups);
    });
}

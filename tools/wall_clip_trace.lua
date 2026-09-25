-- Wall clip frame trace (BizHawk, MM US)
--
-- Logs Link's movement / bg check state every emulated frame, to see exactly
-- what the game does on a clip frame and compare it with the viewer's model
-- (js/wall_push_clips.js).
--
-- APPLY = true: makes the same writes as the manual Laundry Pool setup (below)
-- once, on the frame the script starts, then logs FRAMES emulated frames.
-- APPLY = false: writes nothing; just logs. Pause, poke values yourself, run
-- the script and frame-advance: every emulated frame gets a line.
--
-- Output goes to the Lua console and wall_clip_trace.txt next to this script.

local APPLY = true
local FRAMES = 24
local SETUP = {
	pos = { 0xC4B31388, 0xC116E003, 0x43D1CF66 },  -- written to home.pos and world.pos (f32 bits)
	shapeRotY = 0x0484,
	speedXZ = 9.75,   -- 9.7 stops 0.005 short of TRI 27 (traced); the viewer predicts >= 9.75 clips
}

local PLAY = 0x3E6B20
local PLAYER = 0x3FFDB0
local O = {
	home = 0x08, pos = 0x24, rotY = 0x32, velocity = 0x64, speed = 0x70, gravity = 0x74,
	wallPoly = 0x7C, floorPoly = 0x80, wallYaw = 0x86, floorHeight = 0x88, bgCheckFlags = 0x90,
	displacement = 0xA0, shapeRotY = 0xBE, prevPos = 0x108, speedXZ = 0xAD0, yaw = 0xAD4,
}

local function rf(a) return mainmemory.readfloat(a, true) end
local function vec(a) return { rf(a), rf(a + 4), rf(a + 8) } end
local function fv(v) return string.format("%.4f, %.4f, %.4f", v[1], v[2], v[3]) end

-- static poly index of a CollisionPoly pointer, or "dyna"/"-"
local header = mainmemory.read_u32_be(PLAY + 0x830) - 0x80000000
local polyList = mainmemory.read_u32_be(header + 0x18) - 0x80000000
local numPolys = mainmemory.read_u16_be(header + 0x14)
local function polyId(off)
	local p = mainmemory.read_u32_be(PLAYER + off)
	if p == 0 then return "-" end
	local i = (p - 0x80000000 - polyList) / 0x10
	if i >= 0 and i < numPolys and i == math.floor(i) then return tostring(i) end
	return string.format("%08X", p)
end

local lines = {}
local function out(s)
	print(s)
	lines[#lines + 1] = s
end

local function snapshot(tag)
	local P = PLAYER
	out(string.format("%s frame %d (gameplayFrames %d)", tag, emu.framecount(), mainmemory.read_u32_be(PLAY + 0x18840)))
	out("  home    " .. fv(vec(P + O.home)))
	out("  prevPos " .. fv(vec(P + O.prevPos)))
	out("  pos     " .. fv(vec(P + O.pos)))
	out("  vel     " .. fv(vec(P + O.velocity)) .. "  disp " .. fv(vec(P + O.displacement)))
	out(string.format("  speedXZ %.4f  actor.speed %.4f  gravity %.3f  yaw %04X  rot.y %04X  shape.rot.y %04X",
		rf(P + O.speedXZ), rf(P + O.speed), rf(P + O.gravity), mainmemory.read_u16_be(P + O.yaw),
		mainmemory.read_u16_be(P + O.rotY), mainmemory.read_u16_be(P + O.shapeRotY)))
	out(string.format("  wallPoly %s  wallYaw %04X  floorPoly %s  floorY %.4f  bgCheckFlags %04X",
		polyId(O.wallPoly), mainmemory.read_u16_be(P + O.wallYaw), polyId(O.floorPoly),
		rf(P + O.floorHeight), mainmemory.read_u16_be(P + O.bgCheckFlags)))
end

snapshot("start")
if APPLY then
	for i = 0, 2 do
		mainmemory.write_u32_be(PLAYER + O.home + i * 4, SETUP.pos[i + 1])
		mainmemory.write_u32_be(PLAYER + O.pos + i * 4, SETUP.pos[i + 1])
	end
	mainmemory.write_u16_be(PLAYER + O.shapeRotY, SETUP.shapeRotY)
	mainmemory.writefloat(PLAYER + O.speedXZ, SETUP.speedXZ, true)
	snapshot("after writes")
end
for i = 1, FRAMES do
	emu.frameadvance()
	snapshot("+" .. i)
end

local dir = (debug.getinfo(1, "S").source:match("^@?(.*[/\\])")) or ""
local f = io.open(dir .. "wall_clip_trace.txt", "w")
if f then
	f:write(table.concat(lines, "\n"), "\n")
	f:close()
	print("Written to " .. dir .. "wall_clip_trace.txt")
end

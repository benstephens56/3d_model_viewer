-- Wall push clip tester (BizHawk, N64 OoT US 1.0 / MM US, Mupen64Plus core)
--
-- Tries, in the game, every clip point in a results JSON - from
-- tools/clipfinder, or the 3d_model_viewer's "Export JSON" of imported
-- results (e.g. just the reachable ones) - and writes a summary of the ones that worked.
--
-- How a test runs: the game's own wall check does the work. A callback on
-- Actor_UpdateBgCheckInfo (code segment, so its address is fixed) catches the
-- player's call, and just before it runs sets his prevPos and world.pos (his
-- position after this frame's movement) to the test's `prev` and `next`:
--   standing points: prev = next = the point (no movement, so no line check;
--                    only the wall pushes), falling ones start from the floor
--                    height so only y changes;
--   crossing points: prev = the start, next = just past the crossing (the line
--                    check stops him on the wall, then the pushes).
-- Then the game runs on for SETTLE_FRAMES with no input and the script looks
-- at where Link is. Every test starts from the same savestate.
--
-- Setup:
--   1. Get a results JSON: run tools/clipfinder, or scan the map in the viewer
--      and click "Export JSON". Set TESTS_FILE below to it (or save it as
--      wall_clip_tests.json next to this script).
--   2. In BizHawk (N64 core: Mupen64Plus - the callback needs it), load the
--      same map as the same form, with Link standing still anywhere and no
--      menus or text open. (Tests of several forms: the ones for the form
--      Link is in run - see FORM below.) Unthrottled / fast-forward makes it much quicker.
--   3. Run this script. Progress prints to the Lua console; the summary goes
--      to the console and to wall_clip_results.txt next to the tests file.
--      The game is put back to the starting savestate at the end.
--   Recording a video: set RECORD = true below, run BizHawk at normal speed
--   (not unthrottled) and start BizHawk's AVI/video recording before the
--   script. Each test then gets the camera behind Link and a pause before and
--   after it (RECORD_BUFFER).

---------------------------------------------------------------------------
-- Settings
---------------------------------------------------------------------------

-- The results JSON (tools/clipfinder or the viewer's "Export JSON"), e.g.
-- [[C:\...\results\OOT_Spot_01_-_Kakariko_Village_All.json]]; nil:
-- wall_clip_tests.json next to this script. Its clips are turned into tests,
-- with the walls read from RAM (load that map first). (A .lua test file from
-- an older viewer still works too.)
local TESTS_FILE = [[C:\Users\X\Documents\GitHub\3d_model_viewer\tools\clipfinder\results\OOT_Spot_16_-_Death_Mountain_Trail_Adult_falling_setup2_dyna.json]]
local RESULTS_FILE = nil          -- nil: wall_clip_results.txt next to the tests
local MAX_PER_GROUP = 0          -- points tried per wall pair (spread evenly); 0 = all
local SKIP_FALLING = false        -- true: leave out the falling clips (drop > 0, from --falling scans)
local SETTLE_FRAMES = 30          -- emulated frames to let run after the test frame (3 per game frame)
local HOOK_TIMEOUT = 60           -- emulated frames to wait for the player's bg check
local HOLD_FRAMES = 9             -- "move": emulated frames Link is held at the start first
local BEHIND_MIN = 1.0            -- units behind the clipped wall that count as through it
local FAST = true                 -- skip drawing while testing (client.invisibleemulation)
-- Recording a video of the tests: draws every frame (FAST is ignored), turns
-- the camera to behind Link before each test ("move" mode: he's already
-- facing the way he'll go, and Z is tapped - Z-targeting nothing swings the
-- camera behind him), and pauses RECORD_BUFFER emulated frames (60 a second)
-- before and after each one. Off by default.
local RECORD = false
local RECORD_BUFFER = 90
local RECORD_ONE_PER_PAIR = true  -- recording: once a wall pair's test works, skip the rest of that pair's
if RECORD then FAST = false end
-- How the test frame is set up:
--   "auto": "exec", falling back to "read", then "move" if a hook never fires
--   "exec": execute callback on Actor_UpdateBgCheckInfo (exact prevPos/posNext)
--   "read": read callback on Link's world.pos.y, taken when the PC is inside
--           Actor_UpdateBgCheckInfo (exact, for cores without exec callbacks)
--   "move": no callbacks: Link is put at the start and given the yaw and speed
--           (Player speedXZ) to move there himself - real movement, but his
--           action code can still change the speed on the frame
local MODE = "auto"
-- Tests exported with several forms (clipfinder --form All, imported into the
-- viewer): only the ones for this form run. nil = the form Link is in, read
-- from RAM (OoT: "Crawlspace" if he has the crawling flag, else "Adult" /
-- "Child"; MM: "Human", "Deku", "Zora", "Goron", "FierceDeity"), or set a name.
local FORM = nil
-- Checking clipfinder --yaw's CSV grids (<output>_<YAW>.csv, Yes / No per
-- x, z) in game: set TESTS_FILE to that run's JSON (the CSVs and their
-- _speeds.csv are found next to it, one per yaw in it) and CSV_TESTS = true.
-- Every cell is tried in "move" mode (the game moves Link from exactly that
-- x, z at the yaw): a Yes at its lowest speed (should clip), a No at the max
-- speed (shouldn't). Each grid's in-game result goes to <that CSV>_ingame.csv,
-- mismatches marked, and the summary lists them.
local CSV_TESTS = false
local CSV_CELLS = "all"           -- "all", or "border": only cells next to one with the other answer
local CSV_DRIFT = 0.0001          -- Link pushed further than this off a cell's start before the move: No

---------------------------------------------------------------------------
-- Game / memory
---------------------------------------------------------------------------

console.clear()

local GAME
local hash = gameinfo.getromhash()
if hash == 'AD69C91157F6705E8AB06C79FE08AAD47BB57BA7' then
	GAME = "OOT" -- OoT US 1.0
elseif hash == 'D6133ACE5AFAA0882CF214CF88DABA39E266C078' then
	GAME = "MM" -- MM US
else
	error("wall_clip_tester: needs OoT US 1.0 or MM US (rom hash " .. hash .. ")")
end

local function read_u16(addr) return mainmemory.read_u16_be(addr) end
local function read_s16(addr) return mainmemory.read_s16_be(addr) end
local function read_u32(addr) return mainmemory.read_u32_be(addr) end
local function readfloat(addr) return mainmemory.readfloat(addr, true) end
local function writefloat(addr, val) mainmemory.writefloat(addr, val, true) end

local K = {}
if GAME == "OOT" then
	K.play = 0x1C84A0                   -- globalContext
	K.gameplayFrames = 0x11DE4
	K.colCtx = 0x1C84A0 + 0x7C0         -- globalContext + 0x7C0
	K.player = 0x1DAA30                 -- Player actor (RDRAM offset)
	K.prevPos = 0x100                   -- Actor.prevPos
	K.velocity = 0x5C                   -- Actor.velocity
	K.bgCheckInfo = 0x8001DFB4          -- Actor_UpdateBgCheckInfo (oot-ntsc-1.0.map)
	K.bgCheckInfoEnd = 0x8001E2D4       -- (next function)
	-- Player's own fields: the decomp's offsets (include/player.h) are for
	-- the debug build, whose Actor has an extra 0x10 bytes (dbgPad), so on
	-- retail they're 0x10 lower. Actor fields before that are the same.
	K.speedXZ = 0x828                   -- Player.speedXZ
	K.yaw = 0x82C                       -- Player.yaw
	K.shapeRotY = 0xB6                  -- Actor.shape.rot.y
	K.actionFunc = 0x664                -- Player.actionFunc (for the move log)
	K.stateFlags1 = 0x66C
	K.skelAnime = 0x1A4
	K.rideActor = 0x430
	K.actorSpeed = 0x68                 -- Actor.speed
	K.stateFlags2 = 0x670
	K.crawling = 0x40000                -- PLAYER_STATE2_CRAWLING
	K.linkAge = 0x11A5D4                -- gSaveContext.linkAge (0 adult, 1 child)
else
	K.play = 0x3E6B20
	K.gameplayFrames = 0x18840
	K.colCtx = 0x3E6B20 + 0x830
	K.player = 0x3FFDB0
	K.prevPos = 0x108
	K.velocity = 0x64
	K.bgCheckInfo = 0x800AFE10          -- Actor_UpdateBgCheckInfo (mm-n64-us.map)
	K.bgCheckInfoEnd = 0x800B02B0
	K.speedXZ = 0xAD0                   -- Player.speedXZ (0x400880)
	K.yaw = 0xAD4
	K.shapeRotY = 0xBE
	K.actionFunc = 0x748
	K.stateFlags1 = 0xA6C
	K.skelAnime = 0x240
	K.rideActor = 0x390
	K.actorSpeed = 0x70
	K.transformation = 0x14B            -- Player.transformation (PlayerTransformation)
end
-- Player_UpdateCommon (both games) sets prevPos from home.pos at the start of
-- the frame (and home.pos = world.pos at its end), so home.pos is Link's real
-- "where he was last frame"
K.home = 0x08                           -- Actor.home.pos (both games)
K.rotY = 0x32                           -- Actor.world.rot.y (both games)
K.pos = 0x24                            -- Actor.world.pos (both games)

local function readVec(addr)
	return { readfloat(addr), readfloat(addr + 4), readfloat(addr + 8) }
end
local function writeVec(addr, v)
	writefloat(addr, v[1]); writefloat(addr + 4, v[2]); writefloat(addr + 8, v[3])
end

-- Static collision header of the loaded scene: polygon count and a reader for
-- one polygon's vertices, to check the tests belong to this map.
local function staticCollision()
	local header = read_u32(K.colCtx) - 0x80000000
	local numPolygons = read_u16(header + 0x14)
	local vtxList = read_u32(header + 0x10) - 0x80000000
	local polyList = read_u32(header + 0x18) - 0x80000000
	local function polyVerts(id)
		local poly = polyList + id * 0x10
		local out = {}
		for i, off in ipairs({ 0x2, 0x4, 0x6 }) do
			local vi = read_u16(poly + off) % 0x2000
			out[i] = { read_s16(vtxList + vi * 6), read_s16(vtxList + vi * 6 + 2), read_s16(vtxList + vi * 6 + 4) }
		end
		return out
	end
	-- (CollisionPoly normal at +0x8, dist at +0xE: the export's n and d)
	local function polyPlane(id)
		local poly = polyList + id * 0x10
		return { read_s16(poly + 0x8), read_s16(poly + 0xA), read_s16(poly + 0xC) }, read_s16(poly + 0xE)
	end
	return numPolygons, polyVerts, polyPlane
end

-- A small JSON reader (objects, arrays, strings, numbers, true/false/null),
-- for clipfinder's results files: BizHawk's Lua has none built in.
local function parseJson(text)
	local pos = 1
	local function fail(msg) error(string.format("bad JSON at character %d: %s", pos, msg)) end
	local function ws() pos = text:find("[^ \t\r\n]", pos) or #text + 1 end
	local value
	local function str()
		local out, i = {}, pos + 1
		while true do
			local c = text:sub(i, i)
			if c == "" then fail("unterminated string") end
			if c == '"' then pos = i + 1; return table.concat(out) end
			if c == "\\" then
				local e = text:sub(i + 1, i + 1)
				local map = { n = "\n", t = "\t", r = "\r", b = "\b", f = "\f" }
				if e == "u" then local cp = tonumber(text:sub(i + 2, i + 5), 16) or 63
				out[#out + 1] = (utf8 and utf8.char(cp)) or (cp < 256 and string.char(cp)) or "?"; i = i + 6
				else out[#out + 1] = map[e] or e; i = i + 2 end
			else
				out[#out + 1] = c
				i = i + 1
			end
		end
	end
	function value()
		ws()
		local c = text:sub(pos, pos)
		if c == "{" then
			local obj = {}
			pos = pos + 1; ws()
			if text:sub(pos, pos) == "}" then pos = pos + 1; return obj end
			while true do
				ws()
				if text:sub(pos, pos) ~= '"' then fail("expected a key") end
				local k = str()
				ws()
				if text:sub(pos, pos) ~= ":" then fail("expected ':'") end
				pos = pos + 1
				obj[k] = value()
				ws()
				local d = text:sub(pos, pos)
				pos = pos + 1
				if d == "}" then return obj end
				if d ~= "," then fail("expected ',' or '}'") end
			end
		elseif c == "[" then
			local arr = {}
			pos = pos + 1; ws()
			if text:sub(pos, pos) == "]" then pos = pos + 1; return arr end
			while true do
				arr[#arr + 1] = value()
				ws()
				local d = text:sub(pos, pos)
				pos = pos + 1
				if d == "]" then return arr end
				if d ~= "," then fail("expected ',' or ']'") end
			end
		elseif c == '"' then
			return str()
		elseif text:sub(pos, pos + 3) == "true" then pos = pos + 4; return true
		elseif text:sub(pos, pos + 4) == "false" then pos = pos + 5; return false
		elseif text:sub(pos, pos + 3) == "null" then pos = pos + 4; return nil
		else
			local num = text:match("^-?%d+%.?%d*[eE]?[-+]?%d*", pos)
			if not num or num == "" then fail("unexpected '" .. c .. "'") end
			pos = pos + #num
			return tonumber(num)
		end
	end
	local v = value()
	return v
end

-- A results JSON (wall-push-clips-1 / -2, from clipfinder or the viewer's
-- "Export JSON") as a tests table: every clip, grouped by form, wall pair,
-- crossing/standing and kind. For the frame being tested, Link's prevPos and
-- posNext are `prev` and `next`: standing points move nowhere (falling ones
-- from the floor height, so only y changes), crossing points and standing
-- points with a move go from their start to `next`. Walls are filled in from
-- RAM later (T.fromJson).
local function testsFromJson(path)
	local f = io.open(path, "rb")
	if not f then error("can't read " .. path) end
	local data = parseJson(f:read("*a"))
	f:close()
	if data.format ~= "wall-push-clips-1" and data.format ~= "wall-push-clips-2" then
		error(path .. " isn't a clipfinder results file")
	end
	local forms = data.forms or { { form = data.form, radius = data.radius, checkHeight = data.checkHeight } }
	local T = {
		game = data.game, map = data.map, numPolygons = data.numPolygons, fromJson = true,
		radius = forms[1].radius, checkHeight = forms[1].checkHeight, tests = {}, walls = {},
	}
	if data.forms then
		T.forms = {}
		local names = {}
		for _, fm in ipairs(data.forms) do
			T.forms[fm.form] = { radius = fm.radius, checkHeight = fm.checkHeight }
			names[#names + 1] = fm.form
		end
		T.form = table.concat(names, ", ")
	else
		T.form = data.form
	end
	local groupOf, nGroups, seen = {}, 0, {}
	local vec = function(a) return { a[1], a[2], a[3] } end
	-- CSV_TESTS: the cells of clipfinder --yaw's CSV grids instead of the
	-- clips. The JSON's clip for each yaw gives the wall pair, form and floor
	-- height; <json name>_<YAW>.csv the grid, <...>_speeds.csv each cell's speed.
	if CSV_TESTS then
		T.csvGrids = {}
		local skipped = {}  -- yaws in the JSON with no CSV next to it (moved away to test fewer)
		local base = path:gsub("%.[jJ][sS][oO][nN]$", "")
		local function readCsv(file)
			local f = io.open(file, "r")
			if not f then return nil end
			local rows = {}
			for l in f:lines() do
				l = l:gsub("\r$", "")
				if l ~= "" then
					local cells = {}
					for c in (l .. ","):gmatch("([^,]*),") do cells[#cells + 1] = c end
					rows[#rows + 1] = cells
				end
			end
			f:close()
			return rows
		end
		for _, c in ipairs(data.clips) do
			if c.yaw and c.speed then
				local yawName = string.format("%04X", c.yaw)
				-- (with several forms the file has the form in its name too)
				local names = {}
				for part in tostring(c.form or ""):gmatch("[^/]+") do
					names[#names + 1] = base .. "_" .. part:gsub("[^%w%-_]", "_") .. "_" .. yawName
				end
				names[#names + 1] = base .. "_" .. yawName
				local grid, speeds, name
				for _, n in ipairs(names) do
					grid, speeds = readCsv(n .. ".csv"), readCsv(n .. "_speeds.csv")
					if grid and speeds then name = n; break end
				end
				if not name then
					skipped[#skipped + 1] = "0x" .. yawName
				else
					local g = { name = name, yaw = c.yaw, form = c.form, header = grid[1], rows = {} }
					T.csvGrids[#T.csvGrids + 1] = g
					local expect = {}
					for zi = 2, #grid do
						expect[zi] = {}
						for xi = 2, #grid[zi] do expect[zi][xi] = grid[zi][xi] == "Yes" end
					end
					local function border(zi, xi)
						for _, d in ipairs({ { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }) do
							local e = expect[zi + d[1]] and expect[zi + d[1]][xi + d[2]]
							if e ~= nil and e ~= expect[zi][xi] then return true end
						end
						return false
					end
					local y = c.prev[2]
					local ang = c.yaw / 32768 * math.pi
					for zi = 2, #grid do
						g.rows[zi] = { label = grid[zi][1], cells = {} }
						local z = tonumber(grid[zi][1])
						for xi = 2, #grid[zi] do
							if CSV_CELLS ~= "border" or border(zi, xi) then
								local x, speed = tonumber(grid[1][xi]), tonumber(speeds[zi][xi])
								nGroups = nGroups + 1
								local t = {
									group = nGroups, form = data.forms and c.form or nil, kind = "csv 0x" .. yawName, type = "cell",
									pusher = c.pusher, crossed = c.crossed, prev = { x, y, z },
									-- (roughly: move mode lets the game work out the move itself)
									next = { x + speed * math.sin(ang) * 1.5, y - 7.5, z + speed * math.cos(ang) * 1.5 },
									yaw = c.yaw, speed = speed, csv = g, zi = zi, xi = xi, expectClip = expect[zi][xi],
								}
								T.tests[#T.tests + 1] = t
								g.rows[zi].cells[xi] = t
							end
						end
					end
					T.walls[c.pusher] = true
					T.walls[c.crossed] = true
				end
			end
		end
		if #skipped > 0 then
			print(string.format("CSV_TESTS: %d yaws have no CSV next to the JSON, skipped: %s", #skipped, table.concat(skipped, ", ")))
		end
		if #T.tests == 0 then error("CSV_TESTS: no CSV grids found next to " .. path .. " (run clipfinder --yaw with -o that JSON)") end
		return T
	end
	local key3 = function(v) return string.format("%.9g,%.9g,%.9g", v[1], v[2], v[3]) end
	for _, c in ipairs(data.clips) do
		local form = data.forms and c.form or nil
		-- falling clips: "low-acute" / "low-extended" (their wall pair's category),
		-- or "low" from files older than the per-pair categories
		local kind = c.kind
		if kind ~= "low" and (c.drop or 0) > 0 then kind = "low-" .. kind end
		local gk = table.concat({ form or "", c.pusher, c.crossed, c.cross and "cross" or "stand", kind }, ":")
		if not groupOf[gk] then nGroups = nGroups + 1; groupOf[gk] = nGroups end
		local prev, nxt
		if c.cross or c.speed then
			-- (a standing point Link walks onto: the same kind of move)
			prev, nxt = vec(c.prev), vec(c.next or c.from)
		else
			-- standing on the floor; the game's movement puts posNext below it
			nxt = vec(c.from)
			prev = { c.from[1], c.floorY or c.from[2], c.from[3] }
		end
		local k = (form or "") .. key3(prev) .. key3(nxt)
		if not seen[k] then
			seen[k] = true
			T.tests[#T.tests + 1] = {
				group = groupOf[gk], form = form, kind = kind, type = c.cross and "cross" or "stand",
				pusher = c.pusher, crossed = c.crossed, prev = prev, next = nxt,
				yaw = c.speed and c.yaw or nil, speed = c.speed, speed2 = c.speed2, expect = vec(c["end"]),
			}
			T.walls[c.pusher] = true
			T.walls[c.crossed] = true
		end
	end
	return T
end

---------------------------------------------------------------------------
-- Tests file
---------------------------------------------------------------------------

local scriptDir = (debug.getinfo(1, "S").source:match("^@?(.*[/\\])")) or ""
local testsPath = TESTS_FILE or (scriptDir .. "wall_clip_tests.json")
local T = testsPath:lower():match("%.json$") and testsFromJson(testsPath) or dofile(testsPath)
local resultsPath = RESULTS_FILE or (testsPath:match("^(.*[/\\])") or scriptDir) .. "wall_clip_results.txt"

if T.game ~= GAME then
	error(string.format("tests are for %s, the loaded game is %s", tostring(T.game), GAME))
end

local numPolygons, polyVerts, polyPlane = staticCollision()
if numPolygons ~= T.numPolygons then
	error(string.format("tests are for %s (%d static polys); the loaded scene has %d - load that map first",
		T.map, T.numPolygons, numPolygons))
end
if T.fromJson then
	-- (the JSON has only polygon ids: the walls come from the loaded map)
	for id in pairs(T.walls) do
		local n, d = polyPlane(id)
		T.walls[id] = { v = polyVerts(id), n = n, d = d }
	end
end
for id, w in pairs(T.walls) do
	local v = polyVerts(id)
	for i = 1, 3 do
		for j = 1, 3 do
			if v[i][j] ~= w.v[i][j] then
				error(string.format("TRI %d in RAM doesn't match the export - wrong map or version?", id))
			end
		end
	end
end

---------------------------------------------------------------------------
-- Hook: set prevPos / posNext right before the player's bg check
---------------------------------------------------------------------------

-- Registers are looked up once here in the emu.getregisters() table (looking
-- a name up there can't throw), since cores don't all name them the same.
-- No pcall around API calls anywhere in this script: an error thrown by one
-- inside pcall after the script has yielded makes BizHawk's NLua panic
-- ("unprotected error in call to Lua API").
local function findRegister(candidates)
	local regs = emu.getregisters()
	for _, name in ipairs(candidates) do
		if regs[name] ~= nil then return name end
	end
	return nil
end
local a1Reg = findRegister({ "a1_lo", "a1", "A1", "r5_lo", "r5", "R5", "gpr5", "GPR5" })
local pcReg = findRegister({ "pc", "PC", "pc_lo", "PC_lo" })
do
	local names = {}
	for k in pairs(emu.getregisters()) do names[#names + 1] = tostring(k) end
	table.sort(names)
	print("Registers: a1 = " .. tostring(a1Reg) .. ", pc = " .. tostring(pcReg) ..
		((a1Reg and pcReg) and "" or ("  (the core has: " .. table.concat(names, ", ") .. ")")))
end

local pending = nil   -- the test to apply on the player's next bg check
local fired = false
local calls, seenActors = 0, {}  -- callbacks seen, for diagnosing a hook that never catches Link

local function applyPending()
	writeVec(K.player + K.prevPos, pending.prev)
	writeVec(K.player + K.pos, pending.next)
	pending = nil
	fired = true
end

-- "exec": Actor_UpdateBgCheckInfo(play, actor, ...) is starting; a1 = actor.
local function onBgCheckExec()
	calls = calls + 1
	if not pending or not a1Reg then return end
	local actor = emu.getregister(a1Reg) % 0x1000000
	if #seenActors < 8 then seenActors[#seenActors + 1] = string.format("%06X", actor) end
	if actor == K.player then applyPending() end
end

-- "read": something is reading Link's world.pos.y; take it if that's
-- Actor_UpdateBgCheckInfo (its first statement reads it).
local function onPosRead()
	calls = calls + 1
	if not pending or not pcReg then return end
	local pc = emu.getregister(pcReg) % 0x20000000
	if #seenActors < 8 then seenActors[#seenActors + 1] = string.format("pc %06X", pc) end
	if pc >= K.bgCheckInfo % 0x20000000 and pc < K.bgCheckInfoEnd % 0x20000000 then applyPending() end
end

local hookIds = {}
local function unhook()
	for _, id in ipairs(hookIds) do event.unregisterbyid(id) end
	hookIds = {}
end
-- Each at the KSEG0 address and without the segment bits, in case the core
-- reports physical addresses.
local function hook(mode)
	unhook()
	calls, seenActors = 0, {}
	if mode == "exec" then
		hookIds[1] = event.onmemoryexecute(onBgCheckExec, K.bgCheckInfo, "wall_clip_exec")
		hookIds[2] = event.onmemoryexecute(onBgCheckExec, K.bgCheckInfo % 0x20000000, "wall_clip_exec_phys")
	elseif mode == "read" then
		local y = 0x80000000 + K.player + K.pos + 4
		hookIds[1] = event.onmemoryread(onPosRead, y, "wall_clip_read")
		hookIds[2] = event.onmemoryread(onPosRead, y % 0x20000000, "wall_clip_read_phys")
	end
end

---------------------------------------------------------------------------
-- Judging a result
---------------------------------------------------------------------------

-- Signed distance to a wall's plane (collision header normal / dist), at
-- Link's wall check height (the test's form's, with several forms).
local checkHeight = T.checkHeight
local function planeDist(w, p)
	local y = p[2] + checkHeight
	return (w.n[1] * p[1] + w.n[2] * y + w.n[3] * p[3]) / 32767 + w.d
end

local function dist3(a, b)
	return math.sqrt((a[1] - b[1]) ^ 2 + (a[2] - b[2]) ^ 2 + (a[3] - b[3]) ^ 2)
end

-- "clipped": behind the clipped wall at the end; "fell": dropped a long way
-- (out of bounds with no floor); "voided": moved far away (void out /
-- respawn); "no": still in front of it.
local function judge(test, after, final)
	local w = T.walls[test.crossed]
	if dist3(final, test.next) > 300 then return "voided" end
	if final[2] < test.next[2] - 150 then return "fell" end
	if planeDist(w, final) < -BEHIND_MIN and planeDist(w, after) < -BEHIND_MIN then return "clipped" end
	return "no"
end

---------------------------------------------------------------------------
-- Run
---------------------------------------------------------------------------

local function fmt(v) return v and string.format("%.9g, %.9g, %.9g", v[1], v[2], v[3]) or "-" end

-- The form Link is in now.
local function currentForm()
	if GAME == "OOT" then
		-- (arithmetic, not `&`: BizHawk's Lua doesn't parse the bitwise operators)
		if math.floor(read_u32(K.player + K.stateFlags2) / K.crawling) % 2 == 1 then return "Crawlspace" end
		return read_u32(K.linkAge) == 0 and "Adult" or "Child"
	end
	local names = { [0] = "FierceDeity", "Goron", "Zora", "Deku", "Human" }
	return names[mainmemory.read_u8(K.player + K.transformation)] or "?"
end

-- Tests with several forms: keep the ones for this form. A test's form can
-- name several ("Human/Deku": the same radius and check height, one scan).
local tests, runForm = T.tests, T.form
if T.forms then
	runForm = FORM or currentForm()
	local function formMatches(label)
		for part in tostring(label):gmatch("[^/]+") do
			if part:lower() == runForm:lower() then return true end
		end
		return false
	end
	tests = {}
	for _, t in ipairs(T.tests) do
		if formMatches(t.form) then tests[#tests + 1] = t end
	end
	local names = {}
	for label, f in pairs(T.forms) do
		names[#names + 1] = label
		if formMatches(label) then checkHeight = f.checkHeight end
	end
	table.sort(names)
	if #tests == 0 then
		error(string.format("no tests for %s (Link's form%s) - the tests have %s", runForm,
			FORM and ", from FORM" or " in RAM", table.concat(names, ", ")))
	end
	print(string.format("Form: %s (%d of %d tests; the file has %s)", runForm, #tests, #T.tests, table.concat(names, ", ")))
end

-- Falling crossings whose drop makes checkHeight + dy < 5 can't work: the
-- game's wall line test then runs from Link's feet with floors included and
-- stops him on the floor he starts from (older exports still have them).
do
	local kept = {}
	for _, t in ipairs(tests) do
		if not (t.type == "cross" and checkHeight + (t.next[2] - t.prev[2]) < 5) then kept[#kept + 1] = t end
	end
	if #kept < #tests then
		print(string.format("left out %d falling crossing tests with a drop over %g (can't clip)", #tests - #kept, checkHeight - 5))
	end
	tests = kept
	if #tests == 0 then error("no tests left: they were all falling crossings that can't clip") end
end

if SKIP_FALLING then
	local kept = {}
	for _, t in ipairs(tests) do
		if not t.kind:find("^low") then kept[#kept + 1] = t end
	end
	print(string.format("SKIP_FALLING: left out %d falling tests", #tests - #kept))
	tests = kept
	if #tests == 0 then error("no tests left after SKIP_FALLING (they were all falling ones)") end
end

-- Group the tests, and pick up to MAX_PER_GROUP spread over each group.
local groups, order = {}, {}
for _, t in ipairs(tests) do
	if not groups[t.group] then
		groups[t.group] = { tests = {}, first = t }
		order[#order + 1] = t.group
	end
	table.insert(groups[t.group].tests, t)
end
local queue = {}
for _, gi in ipairs(order) do
	local list = groups[gi].tests
	local n = #list
	local pick = (MAX_PER_GROUP > 0 and n > MAX_PER_GROUP) and MAX_PER_GROUP or n
	groups[gi].picked = {}
	for k = 1, pick do
		local idx = (pick == n) and k or (1 + math.floor((k - 1) * (n - 1) / (pick - 1) + 0.5))
		local t = list[idx]
		table.insert(groups[gi].picked, t)
		queue[#queue + 1] = t
	end
end

print(string.format("Wall clip tester: %s, %s, %s - %d tests (%d points exported)",
	GAME, T.map, runForm, #queue, #tests))

local base = memorysavestate.savecorestate()
print("Saved the starting state")

-- Put things back however the script ends (finished, error, or stopped).
local cleanedUp = false
local function cleanUp()
	if cleanedUp then return end
	cleanedUp = true
	pending = nil
	unhook()
	if FAST and client.invisibleemulation then client.invisibleemulation(false) end
	memorysavestate.loadcorestate(base)
	memorysavestate.removestate(base)
end
event.onexit(cleanUp)

if FAST and client.invisibleemulation then client.invisibleemulation(true) end

-- atan2(y, x) by hand: BizHawk's math.atan ignores a second argument (it
-- returned atan(dx), sending Link off at the wrong yaw).
local function atan2(y, x)
	if x > 0 then return math.atan(y / x) end
	if x < 0 then return math.atan(y / x) + (y >= 0 and math.pi or -math.pi) end
	if y > 0 then return math.pi / 2 end
	if y < 0 then return -math.pi / 2 end
	return 0
end

local function yawTo(dx, dz)
	local a = math.floor(atan2(dx, dz) / math.pi * 0x8000 + 0.5)
	return ((a + 0x8000) % 0x10000) - 0x8000
end

-- One test: sets up the frame, lets it run, and says where Link ended up.
local function runTest(t, mode)
	memorysavestate.loadcorestate(base)
	local r = { test = t }
	-- Slope clips (kind "slope", clipfinder slope.h) are always "move": the
	-- clip is the game's own floor check lifting Link after the move, and can
	-- take a second frame's move (speed2). (A hook set for the other tests
	-- does nothing: no test is pending.)
	if t.kind == "slope" then mode = "move" end
	if mode == "move" then
		-- The way a manual setup that works in-game does it:
		-- Link standing still at the start facing the test's yaw, then speedXZ
		-- written once, just before a game frame, and nothing else touched.
		local dx, dz = t.next[1] - t.prev[1], t.next[3] - t.prev[3]
		local dist = math.sqrt(dx * dx + dz * dz)
		-- (the export's exact s16 yaw and f32 speed when it has them: the game
		-- moves along the sine table, so the crossing is where the viewer
		-- worked it out only for that exact move)
		-- (a yaw with nowhere to move: exported by an older viewer, where
		-- `prev` was the point itself - tested standing on it instead)
		if t.yaw and dist < 0.01 then
			print("  old export (standing test with no start to walk from): re-export with the current viewer (Ctrl+F5 first)")
			t = { group = t.group, kind = t.kind, type = t.type, pusher = t.pusher, crossed = t.crossed,
				prev = t.prev, next = t.next, expect = t.expect }
			r.test = t
		end
		local yaw = t.yaw or (dist >= 0.01 and yawTo(dx, dz) or nil)
		if yaw and yaw >= 0x8000 then yaw = yaw - 0x10000 end
		-- Hold him at the start for a few game frames: world.pos, prevPos and
		-- (MM) home.pos every emulated frame, no speed. A game frame spans 3
		-- emulated frames and a write can land in the middle of one, so a
		-- single write isn't enough.
		-- (recording: longer, with Z tapped early on for the camera, then
		-- let go for the rest so he's back to standing normally)
		local hold = HOLD_FRAMES + (RECORD and RECORD_BUFFER or 0)
		for i = 1, hold do
			if RECORD and i >= 4 and i < 10 then joypad.set({ Z = true }, 1) end
			writeVec(K.player + K.pos, t.prev)
			writeVec(K.player + K.prevPos, t.prev)
			if K.home then writeVec(K.player + K.home, t.prev) end
			writefloat(K.player + K.speedXZ, 0)
			writefloat(K.player + K.actorSpeed, 0)
			if yaw then
				mainmemory.write_s16_be(K.player + K.yaw, yaw)
				mainmemory.write_s16_be(K.player + K.rotY, yaw)
				mainmemory.write_s16_be(K.player + K.shapeRotY, yaw)
			end
			emu.frameadvance()
		end
		-- Let him stand there on his own until a game frame has just run,
		-- then 2 more emulated frames: the next one runs the next game frame
		-- (the same timing as the trace).
		local frames = read_u32(K.play + K.gameplayFrames)
		for _ = 1, 6 do
			emu.frameadvance()
			if read_u32(K.play + K.gameplayFrames) ~= frames then break end
		end
		for _ = 1, 2 do emu.frameadvance() end
		-- (a standing point isn't somewhere Link stays: the pushes of the
		-- first game frame from it are the test, and have already run)
		if yaw then r.start = readVec(K.player + K.pos) end
		-- Just enough speed to reach `next` (Actor_UpdatePos moves 1.5x speed).
		local speed = t.speed or dist / 1.5
		if yaw then
			mainmemory.write_s16_be(K.player + K.yaw, yaw)
			mainmemory.write_s16_be(K.player + K.shapeRotY, yaw)
		end
		writefloat(K.player + K.speedXZ, speed)
		r.yaw, r.speed = yaw, speed
		-- Falling tests (kind "low..."): `next` is `drop` below the floor, not the
		-- usual GROUND_DROP, so give him the y velocity that gets there. Written
		-- where the last frame's floor check left it (-4 standing), before the
		-- frame's gravity (-1) and Actor_UpdatePos (x1.5): velocity.y ends up
		-- -drop / 1.5 (at most -20, the terminal velocity, for the 30 drop).
		local drop = t.prev[2] - t.next[2]
		if t.kind:find("^low") or drop > 7.5 + 0.01 then
			r.velY = -drop / 1.5 + 1
			writefloat(K.player + K.velocity + 4, r.velY)
		end
		r.log = {}
		local frames0 = read_u32(K.play + K.gameplayFrames)
		-- Link's state too: what he's doing (actionFunc, stateFlags1), whether
		-- his animation moves him itself (skelAnime.movementFlags) and whether
		-- he's riding something
		local function logLine(tag)
			r.log[#r.log + 1] = string.format(
				"%s gf+%d pos %s speedXZ %.3f speed %.3f velY %.3f yaw %04X action %08X flags1 %08X animMove %02X ride %08X",
				tag, read_u32(K.play + K.gameplayFrames) - frames0, fmt(readVec(K.player + K.pos)),
				readfloat(K.player + K.speedXZ), readfloat(K.player + K.actorSpeed), readfloat(K.player + K.velocity + 4),
				mainmemory.read_u16_be(K.player + K.yaw),
				read_u32(K.player + K.actionFunc), read_u32(K.player + K.stateFlags1),
				mainmemory.read_u8(K.player + K.skelAnime + 0x35), read_u32(K.player + K.rideActor))
		end
		logLine("write")
		for i = 1, 9 do
			emu.frameadvance()
			logLine("+" .. i)
			if read_u32(K.play + K.gameplayFrames) ~= frames0 then break end
		end
		if read_u32(K.play + K.gameplayFrames) == frames0 then
			r.status = "stuck"
			return r
		end
		-- (the game frame ran in that emulated frame: `after` is right after it)
		r.after = readVec(K.player + K.pos)
		-- Slope clips with a second frame: its speed (the same yaw), written
		-- now, before the next game frame, the same way as the first; `after`
		-- is then after that frame (after the first, the wall is still
		-- about to push him back out).
		if t.speed2 then
			if yaw then
				mainmemory.write_s16_be(K.player + K.yaw, yaw)
				mainmemory.write_s16_be(K.player + K.shapeRotY, yaw)
			end
			writefloat(K.player + K.speedXZ, t.speed2)
			logLine("write speed2")
			local frames1 = read_u32(K.play + K.gameplayFrames)
			for i = 1, 9 do
				emu.frameadvance()
				logLine("2nd +" .. i)
				if read_u32(K.play + K.gameplayFrames) ~= frames1 then break end
			end
			r.after = readVec(K.player + K.pos)
		end
		for _ = 1, 3 do emu.frameadvance() end
		local later = readVec(K.player + K.pos)
		logLine("+1 game frame")
		-- Given speed but didn't move at all in two game frames: Link's state
		-- ignores speedXZ (riding Epona, or an animation that moves him itself
		-- - ANIM_FLAG_OVERRIDE_MOVEMENT). The savestate needs him standing
		-- still on foot.
		if yaw and r.start and dist3(later, r.start) < 0.01 then
			r.status = "no move"
			r.log[#r.log + 1] = "Link didn't move with speedXZ set: use a savestate with him standing still on foot"
			return r
		end
	else
		pending = t
		fired = false
		local waited = 0
		while not fired and waited < HOOK_TIMEOUT do
			emu.frameadvance()
			waited = waited + 1
		end
		if not fired then
			pending = nil
			r.status = "hook"
			return r
		end
	end
	if mode ~= "move" then
		-- the rest of that game frame
		for _ = 1, 3 do emu.frameadvance() end
		r.after = readVec(K.player + K.pos)
	end
	for _ = 1, SETTLE_FRAMES do emu.frameadvance() end
	r.final = readVec(K.player + K.pos)
	-- (recording: let the end show before the next test resets everything)
	if RECORD then for _ = 1, RECORD_BUFFER do emu.frameadvance() end end
	-- The test frame has to have started from `prev`: the frame's movement,
	-- line check and pushes move Link a few tens of units at most.
	local moveDist = math.sqrt((t.next[1] - t.prev[1]) ^ 2 + (t.next[3] - t.prev[3]) ^ 2)
	if (r.start and dist3(r.start, t.prev) > 1) or
		math.sqrt((r.after[1] - t.prev[1]) ^ 2 + (r.after[3] - t.prev[3]) ^ 2) > moveDist + 60 then
		r.status = "setup"
		-- (where he actually was when the move started: not standing still
		-- at `prev` means the viewer's resting spot is off)
		if r.start then
			r.log = r.log or {}
			table.insert(r.log, 1, "start (should be prev) " .. fmt(r.start))
		end
		return r
	end
	r.status = judge(t, r.after, r.final)
	return r
end

-- Pick the mode on the first test: a hook that never catches Link's bg check
-- falls through to the next one.
-- (CSV_TESTS: always "move" - the game works out the move from exactly
-- that start, yaw and speed, which is what the grid is about)
local mode = T.csvGrids and "move" or MODE
local first
-- (the mode is tried on the first test that isn't a slope clip: those always
-- run in "move" mode, so they'd pass any hook; all slope clips: "move")
local probe = 1
while queue[probe] and queue[probe].kind == "slope" do probe = probe + 1 end
if not queue[probe] then probe = 1; mode = "move" end
if mode == "auto" then
	for _, m in ipairs({ "exec", "read", "move" }) do
		if (m == "exec" and not a1Reg) or (m == "read" and not pcReg) then
			print("  " .. m .. ": skipped (register not found)")
		else
			hook(m)
			first = runTest(queue[probe], m)
			if first.status ~= "hook" then
				mode = m
				break
			end
			print(string.format("  %s: callback ran %d times, never for Link's bg check%s", m, calls,
				#seenActors > 0 and (" (seen: " .. table.concat(seenActors, ", ") .. ")") or ""))
		end
	end
	unhook()
	if mode ~= "move" then hook(mode) end
else
	hook(mode)
	first = runTest(queue[probe], mode)
	if first.status == "hook" then
		cleanUp()
		error(string.format("the %s hook never caught Link's bg check (callback ran %d times)", mode, calls))
	end
end
print("Mode: " .. mode)

local results = {}
local pairDone = {}  -- recording: wall pairs (pusher:crossed) that already have a clip on video
local skipped = 0
for i, t in ipairs(queue) do
	local pairKey = t.pusher .. ":" .. t.crossed
	-- (not the CSV cells: they're all the one wall pair)
	if RECORD and RECORD_ONE_PER_PAIR and pairDone[pairKey] and not t.csv then
		skipped = skipped + 1
	else
		local r = i == probe and first or runTest(t, mode)
		results[#results + 1] = r
		if t.csv then
			-- (Link pushed off the start before the move frame: that start isn't
			-- one he can stand at, which the summary counts as No - said here too)
			local drift = r.start and math.sqrt((r.start[1] - t.prev[1]) ^ 2 + (r.start[3] - t.prev[3]) ^ 2) or 0
			local status = drift > CSV_DRIFT and string.format("No (pushed %.6f off the start first)", drift) or r.status
			print(string.format("  %d / %d: %s x %s z %s speed %s (expect %s): %s", i, #queue, t.kind,
				t.csv.header[t.xi], t.csv.rows[t.zi].label, t.speed, t.expectClip and "Yes" or "No", status))
		else
			print(string.format("  %d / %d: %s %s TRI %d -> %d: %s", i, #queue, t.kind, t.type, t.pusher, t.crossed, r.status))
		end
		if r.status == "clipped" or r.status == "fell" or r.status == "voided" then pairDone[pairKey] = true end
	end
end
if skipped > 0 then
	print(string.format("  (recording: skipped %d tests of wall pairs that had already clipped)", skipped))
end

cleanUp()

---------------------------------------------------------------------------
-- Summary
---------------------------------------------------------------------------

local out = {}
local function line(s) out[#out + 1] = s end

line(string.format("Wall push clip test results: %s, %s, %s (mode: %s)", GAME, T.map, runForm, mode))
line(string.format("%d tests; worked = Link ended up behind the clipped wall (clipped), fell out of the map (fell)", #results))
line("or was moved far away (voided), " .. SETTLE_FRAMES .. " frames after the test frame.")
line("")

local byGroup = {}
for _, r in ipairs(results) do
	local g = r.test.group
	byGroup[g] = byGroup[g] or { worked = 0, tried = 0, hits = {}, statuses = {} }
	local b = byGroup[g]
	b.tried = b.tried + 1
	b.statuses[r.status] = (b.statuses[r.status] or 0) + 1
	if r.status == "clipped" or r.status == "fell" or r.status == "voided" then
		b.worked = b.worked + 1
		b.hits[#b.hits + 1] = r
	end
end

-- How Link got there: where he stood, the yaw he faced and the speedXZ given.
-- Move mode: what was actually used (the start read back from RAM). Hook modes
-- write the frame's positions straight into the bg check, so it's the export's
-- start, yaw and speed (the move the viewer found).
local function setupStr(r)
	local t = r.test
	local yaw, speed = r.yaw or t.yaw, r.speed or t.speed
	if not yaw then
		local dx, dz = t.next[1] - t.prev[1], t.next[3] - t.prev[3]
		if dx * dx + dz * dz >= 0.0001 then
			yaw = yawTo(dx, dz)
			speed = speed or math.sqrt(dx * dx + dz * dz) / 1.5
		end
	end
	return string.format("start %s  angle %s  speedXZ %s%s", fmt(r.start or t.prev),
		yaw and string.format("0x%04X", yaw % 0x10000) or "-", speed and string.format("%.9g", speed) or "-",
		t.speed2 and string.format(" then %.9g", t.speed2) or "")
end

-- CSV_TESTS: per grid, how many cells came out as the CSV says, each
-- mismatch, and the grid as it went in game (<CSV>_ingame.csv): Yes / No,
-- "Yes (expected No)" / "No (expected Yes)" where it differs, "?" + the
-- status where the test didn't run properly, "-" where not tried (border).
-- "setup", or Link pushed off the cell's start before the move frame (by more
-- than CSV_DRIFT: that exact start isn't somewhere he can stand) are No, even
-- if he then clips from where the push left him: the clip can't be done from
-- there. The list of mismatches says when that's why.
local function csvDrift(r)
	if not r.start then return 0 end
	return math.sqrt((r.start[1] - r.test.prev[1]) ^ 2 + (r.start[3] - r.test.prev[3]) ^ 2)
end
local function csvResult(r)
	if csvDrift(r) > CSV_DRIFT then return false, "moved" end
	if r.status == "clipped" or r.status == "fell" or r.status == "voided" then return true end
	if r.status == "no" or r.status == "setup" then return false end
	return nil
end
if T.csvGrids then
	local byTest = {}
	for _, r in ipairs(results) do byTest[r.test] = r end
	local allMatched, allTried = 0, 0
	for _, g in ipairs(T.csvGrids) do
		local matched, tried, moved, bad = 0, 0, 0, {}
		local rowsOut = { table.concat(g.header, ",") }
		for zi = 2, #g.rows + 1 do
			local row = g.rows[zi]
			if row then
				local cells = { row.label }
				for xi = 2, #g.header do
					local t = row.cells[xi]
					local r = t and byTest[t]
					local v = "-"
					if r then
						local got, why = csvResult(r)
						tried = tried + 1
						if why == "moved" then moved = moved + 1 end
						if got == nil then
							v = "? " .. r.status
							bad[#bad + 1] = string.format("    x %s z %s: expected %s, test %s", g.header[xi], row.label, t.expectClip and "Yes" or "No", r.status)
						elseif got == t.expectClip then
							v = got and "Yes" or "No"
							matched = matched + 1
						else
							v = string.format("%s (expected %s)", got and "Yes" or "No", t.expectClip and "Yes" or "No")
							if why == "moved" then
								bad[#bad + 1] = string.format("    x %s z %s: expected %s, got No: the game pushed Link %.6f off the start (to %s) before the move",
									g.header[xi], row.label, t.expectClip and "Yes" or "No", csvDrift(r), fmt(r.start))
							else
								bad[#bad + 1] = string.format("    x %s z %s speed %s: expected %s, got %s (%s)  start read back %s, after %s, final %s",
									g.header[xi], row.label, t.speed, t.expectClip and "Yes" or "No", got and "Yes" or "No", r.status,
									fmt(r.start), fmt(r.after), fmt(r.final))
							end
						end
					end
					cells[#cells + 1] = v
				end
				rowsOut[#rowsOut + 1] = table.concat(cells, ",")
			end
		end
		allMatched, allTried = allMatched + matched, allTried + tried
		local outPath = g.name .. "_ingame.csv"
		local f = io.open(outPath, "w")
		if f then f:write(table.concat(rowsOut, "\n"), "\n"); f:close() end
		line(string.format("CSV 0x%04X (%s): %d of %d cells as the CSV says%s%s", g.yaw, g.name .. ".csv", matched, tried,
			moved > 0 and string.format(" (%d No where the game pushed Link off the start)", moved) or "",
			f and ("; in game: " .. outPath) or ("; couldn't write " .. outPath)))
		for _, b in ipairs(bad) do line(b) end
	end
	line("")
	line(string.format("%d of %d cells as the CSVs say", allMatched, allTried))
else
	local totalWorked, groupsWorked = 0, 0
	line("WORKED")
	for _, gi in ipairs(order) do
		local b = byGroup[gi]
		if b and b.worked > 0 then
			local t = groups[gi].first
			groupsWorked = groupsWorked + 1
			totalWorked = totalWorked + b.worked
			line(string.format("  %s %s: TRI %d through TRI %d - %d of %d tried (%d points in the group)",
				t.kind, t.type, t.pusher, t.crossed, b.worked, b.tried, #groups[gi].tests))
			for _, r in ipairs(b.hits) do
				line(string.format("    [%s] prev %s -> next %s  => after %s, final %s",
					r.status, fmt(r.test.prev), fmt(r.test.next), fmt(r.after), fmt(r.final)))
				line("        " .. setupStr(r))
			end
			-- and the ones in the group that didn't
			for _, r in ipairs(results) do
				if r.test.group == gi and r.after and r.status ~= "clipped" and r.status ~= "fell" and r.status ~= "voided" then
					line(string.format("    [%s] prev %s -> next %s  => after %s, final %s (expected %s)",
						r.status, fmt(r.test.prev), fmt(r.test.next), fmt(r.after), fmt(r.final), fmt(r.test.expect)))
					for _, l in ipairs(r.log or {}) do line("        " .. l) end
				end
			end
		end
	end
	if groupsWorked == 0 then line("  (none)") end
	line("")

	line("DIDN'T WORK")
	for _, gi in ipairs(order) do
		local b = byGroup[gi]
		if b and b.worked == 0 then
			local t = groups[gi].first
			local st = {}
			for k, v in pairs(b.statuses) do st[#st + 1] = k .. " " .. v end
			line(string.format("  %s %s: TRI %d through TRI %d - 0 of %d (%s)",
				t.kind, t.type, t.pusher, t.crossed, b.tried, table.concat(st, ", ")))
			for _, r in ipairs(results) do
				if r.test.group == gi and r.after then
					line(string.format("    [%s] prev %s -> next %s  => after %s, final %s (expected %s)",
						r.status, fmt(r.test.prev), fmt(r.test.next), fmt(r.after), fmt(r.final), fmt(r.test.expect)))
					for _, l in ipairs(r.log or {}) do line("        " .. l) end
				end
			end
		end
	end
	-- (recording: groups never run because their wall pair had already clipped)
	local skippedGroups = {}
	for _, gi in ipairs(order) do
		if not byGroup[gi] then
			local t = groups[gi].first
			skippedGroups[#skippedGroups + 1] = string.format("  %s %s: TRI %d through TRI %d - skipped (the pair already clipped)",
				t.kind, t.type, t.pusher, t.crossed)
		end
	end
	if #skippedGroups > 0 then
		line("")
		line("SKIPPED")
		for _, l in ipairs(skippedGroups) do line(l) end
	end
	line("")
	-- Wall pairs (pushing wall, clipped wall), not groups: a pair can have both
	-- standing and crossing points.
	local pairsAll, pairsWorked, nAll, nWorked = {}, {}, 0, 0
	for _, gi in ipairs(order) do
		local t = groups[gi].first
		local k = t.pusher .. ":" .. t.crossed
		if not pairsAll[k] then pairsAll[k] = true; nAll = nAll + 1 end
		local b = byGroup[gi]
		if b and b.worked > 0 and not pairsWorked[k] then pairsWorked[k] = true; nWorked = nWorked + 1 end
	end
	line(string.format("%d of %d wall pairs had a point that worked (%d points)", nWorked, nAll, totalWorked))
end

local text = table.concat(out, "\n")
print(text)
local f = io.open(resultsPath, "w")
if f then
	f:write(text, "\n")
	f:close()
	print("Written to " .. resultsPath)
else
	print("Couldn't write " .. resultsPath)
end

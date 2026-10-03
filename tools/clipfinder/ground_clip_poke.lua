-- Ground clip poke (BizHawk 3DS core, OoT3D US Rev 1 / MM3D US): press K and
-- Link gets speedXZ (linear velocity) SPEED and velocity.y VEL_Y for one game
-- frame, moving the way he faces. For finding ground clips by hand: stand
-- against a wall, face it, press K.
--
-- The values are written every emulated frame until Link moves (the game frame
-- that uses them has run), at most MAX_WRITES emulated frames, so they land on
-- exactly one game frame whichever of its 2 emulated frames K is pressed on.

local SPEED = 25
local VEL_Y = -18
local KEY = "K"         -- keyboard key (input.get() name)
local MAX_WRITES = 4

local GAME = gameinfo.getromhash() == '8AEB0679FC5F77D35B8A58954CE98236' and "MM3D" or "OOT3D"

-- Player actor, and his fields (see wall_clip_tester.lua's K)
local function playerAddr()
	if GAME == "OOT3D" then return 0x06FF4010 end
	local p = mainmemory.read_u32_le(0x0752FD6C)
	if p > 0x24EE000 then p = p - 0x24EE000 end
	return p
end
local F = GAME == "OOT3D"
	and { pos = 0x28, velY = 0x64, speedXZ = 0x221C, yaw = 0x2220 }
	or { pos = 0x24, velY = 0x68, speedXZ = 0x11E30, yaw = 0x11E34 }

local function readPos(p)
	return mainmemory.readfloat(p + F.pos, false), mainmemory.readfloat(p + F.pos + 4, false), mainmemory.readfloat(p + F.pos + 8, false)
end

console.clear()
print(string.format("%s ground clip poke: press %s for speed %g, y velocity %g for a game frame", GAME, KEY, SPEED, VEL_Y))

local wasDown = false
local writes = 0          -- emulated frames left to write on (0: idle)
local startX, startY, startZ
local player

while true do
	local down = input.get()[KEY] == true
	if down and not wasDown then
		player = playerAddr()
		startX, startY, startZ = readPos(player)
		writes = MAX_WRITES
		print(string.format("start %.9g, %.9g, %.9g  yaw 0x%04X", startX, startY, startZ, mainmemory.read_u16_le(player + F.yaw)))
	end
	wasDown = down
	if writes > 0 then
		local x, y, z = readPos(player)
		if x ~= startX or y ~= startY or z ~= startZ then
			-- moved: that game frame used the values
			print(string.format("  after %.9g, %.9g, %.9g", x, y, z))
			writes = 0
		else
			mainmemory.writefloat(player + F.speedXZ, SPEED, false)
			mainmemory.writefloat(player + F.velY, VEL_Y, false)
			writes = writes - 1
			if writes == 0 then print("  Link didn't move (standing still on foot?)") end
		end
	end
	emu.frameadvance()
end

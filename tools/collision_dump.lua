console.clear()
--print(gameinfo.getromhash())


function setPosition(x, y, z)
	writefloat(Player.xPos, x, true)
	writefloat(Player.yPos, y, true)
	writefloat(Player.zPos, z, true)
	
	writefloat(Player.xPosCol, x, true)
	writefloat(Player.yPosCol, y, true)
	writefloat(Player.zPosCol, z, true)

	print(string.format("Position set to:  X= %.4f  Y= %.4f  Z= %.4f", x, y, z))
end

function getPosition()
	x = readfloat(Player.xPos, true)
	y = readfloat(Player.yPos, true)
	z = readfloat(Player.zPos, true)

	print(string.format("Position is:  X= %.4f  Y= %.4f  Z= %.4f", x, y, z))
end

if gameinfo.getromhash() == 'AD69C91157F6705E8AB06C79FE08AAD47BB57BA7' then -- OoT U 1.0
	GAME = "OoT"
elseif gameinfo.getromhash() == 'D6133ACE5AFAA0882CF214CF88DABA39E266C078' then -- MM U
	GAME = "MM"
elseif gameinfo.getromhash() == '8AEB0679FC5F77D35B8A58954CE98236' then -- MM3D U
	GAME = "MM3D"
else
	GAME = "OOT3D"
end

function read_u16(addr)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.read_u16_be(addr)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.read_u16_le(addr)
	end
end

function write_u16(addr, val)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.write_u16_be(addr, val)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.write_u16_le(addr, val)
	end
end

function read_s16(addr)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.read_s16_be(addr)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.read_s16_le(addr)
	end
end

function write_s16(addr, val)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.write_s16_be(addr, val)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.write_s16_le(addr, val)
	end
end

function read_u32(addr)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.read_u32_be(addr)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.read_u32_le(addr)
	end
end

function read_s32(addr)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.read_s32_be(addr)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.read_s32_le(addr)
	end
end

function readfloat(addr)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.readfloat(addr, true)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.readfloat(addr, false)
	end
end

function writefloat(addr, val)
	if GAME == "OoT" or GAME == "MM" then
		return mainmemory.writefloat(addr, val, true)
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		return mainmemory.writefloat(addr, val, false)
	end
end

function bit_band(first, second)
	if _VERSION == "Lua 5.4" then
		--return first & second --comment this out for bizhawk 2.7
	else
		return bit.band(first, second)
	end
end

function bit_rshift(first, second)
	if _VERSION == "Lua 5.4" then
		--return first >> second --comment this out for bizhawk 2.7
	else
		return bit.rshift(first, second)
	end
end
					
function round(num, numDecimalPlaces)
	local mult = 10^(numDecimalPlaces or 0)
	return math.floor(num * mult + 0.5) / mult
end

Player = {}
if GAME == "OoT" then
	offset_addr = 0x80000000
	
	addr_globalContext = 0x1C84A0
	addr_exitList = addr_globalContext + 0x11E04
	addr_colCtx = addr_globalContext + 0x7C0
	addr_dynaColCtx = addr_colCtx + 0x50
	addr_scene_segment = addr_globalContext + 0xB0
	addr_warp_trigger = 0x1DA2B5
	
	Player.xPos = 0x1DAA54 -- float
	Player.yPos = 0x1DAA58 -- float
	Player.zPos = 0x1DAA5C -- float
	Player.xPosCol = 0x1DAA38 -- float
	Player.yPosCol = 0x1DAA3C -- float
	Player.zPosCol = 0x1DAA40 -- float
	Player.wallPoly = 0x1DAAA4 -- u32
	Player.floorPoly = 0x1DAAA8 -- u32
	Player.xPosPrev = 0x1DAB30 -- float
	Player.yPosPrev = 0x1DAB34 -- float
	Player.zPosPrev = 0x1DAB38 -- float
	
	POLY_SIZE = 0x10
	DYNA_POLY_SIZE = 0x10
	BG_ACTOR_SIZE = 0x64
elseif GAME == "MM" then
	offset_addr = 0x80000000
	
	addr_globalContext = 0x3E6B20
	addr_exitList = addr_globalContext + 0x18860
	addr_colCtx = addr_globalContext + 0x830
	addr_dynaColCtx = addr_colCtx + 0x50
	addr_scene_segment = addr_globalContext + 0xB0
	addr_warp_trigger = 0x3FF395
	
	Player.xPos = 0x3FFDD4 -- float
	Player.yPos = 0x3FFDD8 -- float
	Player.zPos = 0x3FFDDC -- float
	Player.xPosCol = 0x3FFDB8 -- float
	Player.yPosCol = 0x3FFDBC -- float
	Player.zPosCol = 0x3FFDC0 -- float
	Player.wallPoly = 0x3FFE2C -- u32
	Player.floorPoly = 0x3FFE30 -- u32
	
	POLY_SIZE = 0x10
	DYNA_POLY_SIZE = 0x10
	BG_ACTOR_SIZE = 0x64
end

function update_addresses()
	if GAME == "OOT3D" then
		offset_addr = 0x2900000
		
		addr_saveContext = 0x077C6958 -- probably wrong, but its where entrance is
		
		addr_globalContext = 0x05E1E840
		
		addr_exitList = addr_globalContext + 0x5C1C
		addr_colCtx = addr_globalContext + 0xA98
		addr_dynaColCtx = addr_colCtx + 0x50
		addr_scene_segment = addr_globalContext + 0x110 -- no idea if this is correct
		addr_warp_trigger = addr_globalContext + 0x5C2D
		addr_next_entrance = addr_globalContext + 0x5C32
		
		addr_player = 0x06FF4010
		
		Player.xPos = addr_player + 0x28 -- float
		Player.yPos = addr_player + 0x2C -- float
		Player.zPos = addr_player + 0x30 -- float
		Player.xPosCol = addr_player + 0x08 -- float
		Player.yPosCol = addr_player + 0x0C -- float
		Player.zPosCol = addr_player + 0x10 -- float
		Player.wallPoly = addr_player + 0x78 -- u32
		Player.floorPoly = addr_player + 0x7C -- u32
	
		POLY_SIZE = 0x14
		DYNA_POLY_SIZE = 0x20
		BG_ACTOR_SIZE = 0x6C
	
	elseif GAME == "MM3D" then
		offset_addr = 0x24EE000
		
		globalContextPtr = 0x0754D890
		
		addr_saveContext = 0x0765B1B0
		
		addr_globalContext = mainmemory.read_u32_le(globalContextPtr) - offset_addr
		--print("globalContext: "..string.format("0x%x", addr_globalContext)) -- WORKS
		addr_exitList = addr_globalContext + 0xC4E8
		addr_colCtx = addr_globalContext + 0xAB0
		addr_dynaColCtx = addr_colCtx + 0x50
		addr_scene_segment = addr_globalContext + 0x154 -- no idea if this is correct
		addr_warp_trigger = addr_globalContext + 0xC529
		addr_next_entrance = addr_globalContext + 0xC52E
		
		playerPtr = 0x0752FD6C -- or try 0x0752FFA8
		addr_player = mainmemory.read_u32_le(playerPtr)
		if addr_player > offset_addr then
			addr_player = addr_player - offset_addr
		end
		
		--print("player: "..string.format("0x%x", addr_player)) -- WORKS
		
		Player.xPos = addr_player + 0x24 -- float
		Player.yPos = addr_player + 0x28 -- float
		Player.zPos = addr_player + 0x2C -- float
		Player.xPosCol = addr_player + 0x08 -- float
		Player.yPosCol = addr_player + 0x0C -- float
		Player.zPosCol = addr_player + 0x10 -- float
		Player.wallPoly = addr_player + 0x7C -- u32
		Player.floorPoly = addr_player + 0x80 -- u32
	
		POLY_SIZE = 0x14
		DYNA_POLY_SIZE = 0x20
		BG_ACTOR_SIZE = 0x6C
	end
end

update_addresses()
addr_colHeader = read_u32(addr_colCtx + 0x0) - offset_addr
exitList = read_u32(addr_exitList)
if exitList ~= 0x00000000 then
	exitList = exitList - offset_addr
end

local flat_ground_clip_y_table = {
	515, 519, 523, 527, 531, 535, 539, 543, 547, 551, 555, 559, 563, 567, 571, 575, 579, 583, 587, 591, 595, 599, 603, 607, 611, 615, 619, 623, 627, 631, 635, 639, 643, 647, 651, 655, 659, 663, 667, 671, 675, 679, 683, 687, 691, 695, 699, 703, 707, 711, 715, 719, 723, 727, 731, 735, 739, 743, 747, 751, 755, 759, 763, 767, 771, 775, 779, 783, 787, 791, 795, 799, 803, 807, 811, 815, 819, 823, 827, 831, 835, 839, 843, 847, 851, 855, 859, 863, 867, 871, 875, 879, 883, 887, 891, 895, 899, 903, 907, 911, 915, 919, 923, 927, 931, 935, 939, 943, 947, 951, 955, 959, 963, 967, 971, 975, 979, 983, 987, 991, 995, 999, 1003, 1007, 1011, 1015, 1019, 1023, 1030, 1038, 1046, 1054, 1062, 1070, 1078, 1086, 1094, 1102, 1110, 1118, 1126, 1134, 1142, 1150, 1158, 1166, 1174, 1182, 1190, 1198, 1206, 1214, 1222, 1230, 1238, 1246, 1254, 1262, 1270, 1278, 1286, 1294, 1302, 1310, 1318, 1326, 1334, 1342, 1350, 1358, 1366, 1374, 1382, 1390, 1398, 1406, 1414, 1422, 1430, 1438, 1446, 1454, 1462, 1470, 1478, 1486, 1494, 1502, 1510, 1518, 1526, 1534, 1542, 1550, 1558, 1566, 1574, 1582, 1590, 1598, 1606, 1614, 1622, 1630, 1638, 1646, 1654, 1662, 1670, 1678, 1686, 1694, 1702, 1710, 1718, 1726, 1734, 1742, 1750, 1758, 1766, 1774, 1782, 1790, 1798, 1806, 1814, 1822, 1830, 1838, 1846, 1854, 1862, 1870, 1878, 1886, 1894, 1902, 1910, 1918, 1926, 1934, 1942, 1950, 1958, 1966, 1974, 1982, 1990, 1998, 2006, 2014, 2022, 2030, 2038, 2046, 2060, 2076, 2092, 2108, 2124, 2140, 2156, 2172, 2188, 2204, 2220, 2236, 2252, 2268, 2284, 2300, 2316, 2332, 2348, 2364, 2380, 2396, 2412, 2428, 2444, 2460, 2476, 2492, 2508, 2524, 2540, 2556, 2572, 2588, 2604, 2620, 2636, 2652, 2668, 2684, 2700, 2716, 2732, 2748, 2764, 2780, 2796, 2812, 2828, 2844, 2860, 2876, 2892, 2908, 2924, 2940, 2956, 2972, 2988, 3004, 3020, 3036, 3052, 3068, 3084, 3100, 3116, 3132, 3148, 3164, 3180, 3196, 3212, 3228, 3244, 3260, 3276, 3292, 3308, 3324, 3340, 3356, 3372, 3388, 3404, 3420, 3436, 3452, 3468, 3484, 3500, 3516, 3532, 3548, 3564, 3580, 3596, 3612, 3628, 3644, 3660, 3676, 3692, 3708, 3724, 3740, 3756, 3772, 3788, 3804, 3820, 3836, 3852, 3868, 3884, 3900, 3916, 3932, 3948, 3964, 3980, 3996, 4012, 4028, 4044, 4060, 4076, 4092, 4120, 4152, 4184, 4216, 4248, 4280, 4312, 4344, 4376, 4408, 4440, 4472, 4504, 4536, 4568, 4600, 4632, 4664, 4696, 4728, 4760, 4792, 4824, 4856, 4888, 4920, 4952, 4984
}

colCtx = {}
colHeader = {}
dynaColCtx = {}

function update_collision_addresses()
	update_addresses()

	exitList = read_u32(addr_exitList)
	if exitList ~= 0x00000000 then
		exitList = exitList - offset_addr
	end
	addr_colHeader = read_u32(addr_colCtx + 0x0)
	if addr_colHeader == 0x00000000 then
		return
	end
	addr_colHeader = addr_colHeader - offset_addr
	
	colCtx.colHeader_ptr = read_u32(addr_colCtx + 0x0) - offset_addr
	colCtx.minBoundsX = readfloat(addr_colCtx + 0x4)
	colCtx.minBoundsY = readfloat(addr_colCtx + 0x8)
	colCtx.minBoundsZ = readfloat(addr_colCtx + 0xC)
	colCtx.maxBoundsX = readfloat(addr_colCtx + 0x10)
	colCtx.maxBoundsY = readfloat(addr_colCtx + 0x14)
	colCtx.maxBoundsZ = readfloat(addr_colCtx + 0x18)
	colCtx.subdivAmountX = read_u32(addr_colCtx + 0x1C)
	colCtx.subdivAmountY = read_u32(addr_colCtx + 0x20)
	colCtx.subdivAmountZ = read_u32(addr_colCtx + 0x24)
	colCtx.total_subdivisions = colCtx.subdivAmountX * colCtx.subdivAmountY * colCtx.subdivAmountZ
	colCtx.subdivLengthX = readfloat(addr_colCtx + 0x28)
	colCtx.subdivLengthY = readfloat(addr_colCtx + 0x2C)
	colCtx.subdivLengthZ = readfloat(addr_colCtx + 0x30)
	colCtx.subdivLengthInvX = readfloat(addr_colCtx + 0x34)
	colCtx.subdivLengthInvY = readfloat(addr_colCtx + 0x38)
	colCtx.subdivLengthInvZ = readfloat(addr_colCtx + 0x3C)
	colCtx.StaticLookup_lookupTbl_ptr = read_u32(addr_colCtx + 0x40) - offset_addr
	colCtx.SSNodeList_polyNodes_max = read_u16(addr_colCtx + 0x44)
	colCtx.SSNodeList_polyNodes_count = read_u16(addr_colCtx + 0x46)
	colCtx.SSNodeList_polyNodes_SSNode_tbl_ptr = read_u32(addr_colCtx + 0x48) - offset_addr
	colCtx.SSNodeList_polyNodes_u8_polyCheckTbl_ptr = read_u32(addr_colCtx + 0x4C) - offset_addr
	if GAME == "OoT" then
		colCtx.memSize = read_u32(addr_colCtx + 0x1460)
	elseif GAME == "MM" then
		colCtx.memSize = read_u32(addr_colCtx + 0x1468)
		colCtx.flags = read_u32(addr_colCtx + 0x146C)
	elseif GAME == "MM3D" then
		colCtx.memSize = 0 --read_u32(addr_colCtx + 0x1468) -- TODO
		colCtx.flags = 0 --read_u32(addr_colCtx + 0x146C) --TODO
	elseif GAME == "OOT3D" then
		colCtx.memSize = 0 -- TODO
	end
	
	if GAME == "OoT" or GAME == "MM" then
		colHeader.minBoundsX = read_s16(addr_colHeader + 0x00)
		colHeader.minBoundsY = read_s16(addr_colHeader + 0x02)
		colHeader.minBoundsZ = read_s16(addr_colHeader + 0x04)
		colHeader.maxBoundsX = read_s16(addr_colHeader + 0x06)
		colHeader.maxBoundsY = read_s16(addr_colHeader + 0x08)
		colHeader.maxBoundsZ = read_s16(addr_colHeader + 0x0A)
		colHeader.numVertices = read_u16(addr_colHeader + 0xC)
		colHeader.vtxList_ptr = read_u32(addr_colHeader + 0x10) - offset_addr
		colHeader.numPolygons = read_u16(addr_colHeader + 0x14)
		colHeader.polyList_ptr = read_u32(addr_colHeader + 0x18) - offset_addr
		colHeader.surfaceTypeList_ptr = read_u32(addr_colHeader + 0x1C) - offset_addr
		colHeader.bgCamList_ptr = read_u32(addr_colHeader + 0x20) - offset_addr
		colHeader.numWaterBoxes = read_u16(addr_colHeader + 0x24)
		colHeader.waterBoxes_ptr = read_u32(addr_colHeader + 0x28)
		if colHeader.waterBoxes_ptr ~= 0x0 then
			colHeader.waterBoxes_ptr = colHeader.waterBoxes_ptr - offset_addr
		end
	elseif GAME == "OOT3D" then
		-- idk what 0x00 is
		colHeader.minBoundsX = read_s16(addr_colHeader + 0x00)
		colHeader.minBoundsY = read_s16(addr_colHeader + 0x02)
		colHeader.minBoundsZ = read_s16(addr_colHeader + 0x04)
		colHeader.maxBoundsX = read_s16(addr_colHeader + 0x06)
		colHeader.maxBoundsY = read_s16(addr_colHeader + 0x08)
		colHeader.maxBoundsZ = read_s16(addr_colHeader + 0x0A)
		colHeader.numVertices = read_u16(addr_colHeader + 0xC)
		colHeader.numSurfaceTypes = read_u16(addr_colHeader + 0x10) -- unused
		colHeader.numPolygons = read_u16(addr_colHeader + 0xE)
		colHeader.numWaterBoxes = read_u16(addr_colHeader + 0x14)
		colHeader.numBgCams = read_u16(addr_colHeader + 0x12) -- unused?
		colHeader.vtxList_ptr = read_u32(addr_colHeader + 0x18) - offset_addr
		colHeader.polyList_ptr = read_u32(addr_colHeader + 0x1C) - offset_addr
		colHeader.surfaceTypeList_ptr = read_u32(addr_colHeader + 0x20) - offset_addr
		colHeader.bgCamList_ptr = read_u32(addr_colHeader + 0x24) - offset_addr
		colHeader.waterBoxes_ptr = read_u32(addr_colHeader + 0x28) - offset_addr
	elseif GAME == "MM3D" then
		-- idk what 0x00 is
		colHeader.minBoundsX = read_s16(addr_colHeader + 0x02)
		colHeader.minBoundsY = read_s16(addr_colHeader + 0x04)
		colHeader.minBoundsZ = read_s16(addr_colHeader + 0x06)
		colHeader.maxBoundsX = read_s16(addr_colHeader + 0x08)
		colHeader.maxBoundsY = read_s16(addr_colHeader + 0x0A)
		colHeader.maxBoundsZ = read_s16(addr_colHeader + 0x0C)
		colHeader.numVertices = read_u16(addr_colHeader + 0xE)
		colHeader.numSurfaceTypes = read_u16(addr_colHeader + 0x12) -- unused
		colHeader.numPolygons = read_u16(addr_colHeader + 0x10)
		colHeader.numWaterBoxes = read_u16(addr_colHeader + 0x16)
		colHeader.numBgCams = read_u16(addr_colHeader + 0x14) -- unused?
		colHeader.vtxList_ptr = read_u32(addr_colHeader + 0x18) - offset_addr
		colHeader.polyList_ptr = read_u32(addr_colHeader + 0x1C) - offset_addr
		colHeader.surfaceTypeList_ptr = read_u32(addr_colHeader + 0x20) - offset_addr
		colHeader.bgCamList_ptr = read_u32(addr_colHeader + 0x24) - offset_addr
		colHeader.waterBoxes_ptr = read_u32(addr_colHeader + 0x28) - offset_addr
	end
	colHeader.highest_type = 0	

	dynaColCtx.bitFlag = mainmemory.read_u8(addr_dynaColCtx + 0x0)
	dynaColCtx.bgActors_list_start = addr_dynaColCtx + 0x4 -- list of 50 items of size 0x64 in OoT and MM, but size 0x6C in MM3D and OOT3D
	
	if GAME == "OoT" or GAME == "MM" then
		dynaColCtx.bgActorFlags_list_start = addr_dynaColCtx + 0x138C -- list of 50 u16 items
		dynaColCtx.polyList_ptr = read_u32(addr_dynaColCtx + 0x13F0) - offset_addr
		dynaColCtx.vtxList_ptr = read_u32(addr_dynaColCtx + 0x13F4) - offset_addr
		
		if GAME == "OoT" then
			dynaColCtx.waterBoxList_unk0 = 0 -- does not exist in oot
			dynaColCtx.waterBoxList_WaterBox_boxes_ptr = 0 -- does not exist in oot
			dynaColCtx.DynaSSNodeList_polyNodes_SSNode_tbl_ptr = read_u32(addr_dynaColCtx + 0x13F8) - offset_addr
			dynaColCtx.DynaSSNodeList_polyNodes_count = read_u32(addr_dynaColCtx + 0x13FC)
			dynaColCtx.DynaSSNodeList_polyNodes_maxNodes = read_s32(addr_dynaColCtx + 0x1400)
			dynaColCtx.polyNodesMax = read_s32(addr_dynaColCtx + 0x1404)
			dynaColCtx.polyListMax = read_s32(addr_dynaColCtx + 0x1408)
			dynaColCtx.vtxListMax = read_s32(addr_dynaColCtx + 0x140C)
		elseif GAME == "MM" then
			dynaColCtx.waterBoxList_unk0 = read_s32(addr_dynaColCtx + 0x13F8)
			dynaColCtx.waterBoxList_WaterBox_boxes_ptr = read_u32(addr_dynaColCtx + 0x13FC) - offset_addr
			dynaColCtx.DynaSSNodeList_polyNodes_SSNode_tbl_ptr = read_u32(addr_dynaColCtx + 0x1400) - offset_addr
			dynaColCtx.DynaSSNodeList_polyNodes_count = read_u32(addr_dynaColCtx + 0x1404)
			dynaColCtx.DynaSSNodeList_polyNodes_maxNodes = read_s32(addr_dynaColCtx + 0x1408)
			dynaColCtx.polyNodesMax = read_s32(addr_dynaColCtx + 0x140C)
			dynaColCtx.polyListMax = read_s32(addr_dynaColCtx + 0x1410)
			dynaColCtx.vtxListMax = read_s32(addr_dynaColCtx + 0x1414)
		end
		
	elseif GAME == "MM3D" or GAME == "OOT3D" then
		dynaColCtx.bgActorFlags_list_start = addr_dynaColCtx + 0x151C -- list of 50 u16 items
		dynaColCtx.polyList_ptr = read_u32(addr_dynaColCtx + 0x1580) - offset_addr
		dynaColCtx.vtxList_ptr = read_u32(addr_dynaColCtx + 0x1584) - offset_addr
		
		if GAME == "OOT3D" then
			dynaColCtx.waterBoxList_unk0 = 0 -- does not exist in oot
			dynaColCtx.waterBoxList_WaterBox_boxes_ptr = 0 -- does not exist in oot
			dynaColCtx.DynaSSNodeList_polyNodes_SSNode_tbl_ptr = read_u32(addr_dynaColCtx + 0x1588) - offset_addr
			dynaColCtx.DynaSSNodeList_polyNodes_count = read_u32(addr_dynaColCtx + 0x158C)
			dynaColCtx.DynaSSNodeList_polyNodes_maxNodes = read_s32(addr_dynaColCtx + 0x1590)
			dynaColCtx.polyNodesMax = read_s32(addr_dynaColCtx + 0x1594)
			dynaColCtx.polyListMax = read_s32(addr_dynaColCtx + 0x1598)
			dynaColCtx.vtxListMax = read_s32(addr_dynaColCtx + 0x159C)
		elseif GAME == "MM3D" then
			dynaColCtx.waterBoxList_unk0 = read_s32(addr_dynaColCtx + 0x1588)
			dynaColCtx.waterBoxList_WaterBox_boxes_ptr = read_u32(addr_dynaColCtx + 0x158C) - offset_addr
			dynaColCtx.DynaSSNodeList_polyNodes_SSNode_tbl_ptr = read_u32(addr_dynaColCtx + 0x1590) - offset_addr
			dynaColCtx.DynaSSNodeList_polyNodes_count = read_u32(addr_dynaColCtx + 0x1594)
			dynaColCtx.DynaSSNodeList_polyNodes_maxNodes = read_s32(addr_dynaColCtx + 0x1598)
			dynaColCtx.polyNodesMax = read_s32(addr_dynaColCtx + 0x159C)
			dynaColCtx.polyListMax = read_s32(addr_dynaColCtx + 0x15A0)
			dynaColCtx.vtxListMax = read_s32(addr_dynaColCtx + 0x15A4)
		end
	end
end

update_collision_addresses()


function round(num, numDecimalPlaces)
  local mult = 10^(numDecimalPlaces or 0)
  return math.floor(num * mult + 0.5) / mult
end

function print_collision_context_info()
	print("-Collision Context- "..string.format("0x%x", addr_colCtx))
	print("colHeader: "..string.format("0x%x", colCtx.colHeader_ptr))
	print("minBounds: ("..string.format("%.1f", colCtx.minBoundsX)..", "..string.format("%.1f", colCtx.minBoundsY)..", "..string.format("%.1f", colCtx.minBoundsZ)..")")
	print("maxBounds: ("..string.format("%.1f", colCtx.maxBoundsX)..", "..string.format("%.1f", colCtx.maxBoundsY)..", "..string.format("%.1f", colCtx.maxBoundsZ)..")")
	print("subdivAmount: ("..colCtx.subdivAmountX..", "..colCtx.subdivAmountY..", "..colCtx.subdivAmountZ..")   total: "..colCtx.total_subdivisions)
	print("subdivLength: ("..string.format("%.1f", colCtx.subdivLengthX)..", "..string.format("%.1f", colCtx.subdivLengthY)..", "..string.format("%.1f", colCtx.subdivLengthZ)..")")
	print("lookupTbl_ptr: "..string.format("0x%x", colCtx.StaticLookup_lookupTbl_ptr))
	print("polyNodes_max: "..colCtx.SSNodeList_polyNodes_max)
	print("polyNodes_count: "..colCtx.SSNodeList_polyNodes_count)
	print("polyNodes_SSNode_tbl_ptr: "..string.format("0x%x", colCtx.SSNodeList_polyNodes_SSNode_tbl_ptr))
	print("polyNodes_u8_polyCheckTbl_ptr: "..string.format("0x%x", colCtx.SSNodeList_polyNodes_u8_polyCheckTbl_ptr))
	print("memSize: "..string.format("0x%x", colCtx.memSize))
	if GAME == "MM" or GAME == "MM3D" then
		print("flags: "..string.format("0x%x", colCtx.flags))
	end
	print("")
end

function print_collision_header_info()
	print("-Collision Header-")
	scene_segment_ptr = read_u32(addr_scene_segment) - offset_addr
	print("Scene Segment: "..string.format("0x%x", scene_segment_ptr))
	print("minBounds: ("..colHeader.minBoundsX..", "..colHeader.minBoundsY..", "..colHeader.minBoundsZ..")")
	print("maxBounds: ("..colHeader.maxBoundsX..", "..colHeader.maxBoundsY..", "..colHeader.maxBoundsZ..")")
	print("numVertices: "..colHeader.numVertices)
	print("vtxList_ptr: "..string.format("0x%x", colHeader.vtxList_ptr))
	print("numPolygons: "..colHeader.numPolygons)
	print("polyList_ptr: "..string.format("0x%x", colHeader.polyList_ptr))
	print("surfaceTypeList_ptr: "..string.format("0x%x", colHeader.surfaceTypeList_ptr))
	print("bgCamList_ptr: "..string.format("0x%x", colHeader.bgCamList_ptr))
	print("numWaterBoxes: "..colHeader.numWaterBoxes)
	print("waterBoxes_ptr: "..string.format("0x%x", colHeader.waterBoxes_ptr))
	print("")
end

function print_dyna_collision_context_info()
	print("-Dyna Collision Context- "..string.format("0x%x", addr_dynaColCtx))
	print("bitFlag: "..dynaColCtx.bitFlag)
	print("bgActors_list_start: "..string.format("0x%x", dynaColCtx.bgActors_list_start))
	print("bgActorFlags_list_start: "..string.format("0x%x", dynaColCtx.bgActorFlags_list_start))
	print("polyNodesMax: "..dynaColCtx.polyNodesMax)
	print("polyListMax: "..dynaColCtx.polyListMax)
	print("vtxListMax: "..dynaColCtx.vtxListMax)
	print("DynaSSNodeList_polyNodes_SSNode_tbl_ptr: "..string.format("0x%x", dynaColCtx.DynaSSNodeList_polyNodes_SSNode_tbl_ptr))
	print("polyNodes_count: "..dynaColCtx.DynaSSNodeList_polyNodes_count)
	print("polyNodes_maxNodes: "..dynaColCtx.DynaSSNodeList_polyNodes_maxNodes)
	print("polyList_ptr: "..string.format("0x%x", dynaColCtx.polyList_ptr))
	print("vtxList_ptr: "..string.format("0x%x", dynaColCtx.vtxList_ptr))
	
	print("")
end

function print_static_vertices()
	s = "-Static Vertices-\n"
	i = 0
	while i < colHeader.numVertices do
		vtx_addr = colHeader.vtxList_ptr + (i * 0x6)
		vtxX = read_s16(colHeader.vtxList_ptr + (i * 0x6) + 0x0)
		vtxY = read_s16(colHeader.vtxList_ptr + (i * 0x6) + 0x2)
		vtxZ = read_s16(colHeader.vtxList_ptr + (i * 0x6) + 0x4)
		s = s..i..". "..string.format("%06X", vtx_addr).." ("..vtxX..", "..vtxY..", "..vtxZ..")\n"
		
		i = i + 1
	end
	
	print(s)
end



OOT_Scene_Entrances = {
	--[[{0x0000, "Inside the Deku Tree"},
	{0x0004, "Dodongo's Cavern"},
	{0x0028, "Inside Jabu-Jabu's Belly"},
	{0x0169, "Forest Temple"},
	{0x0165, "Fire Temple"},
	{0x0010, "Water Temple"},
	{0x0082, "Spirit Temple"},
	{0x0037, "Shadow Temple"},
	{0x0098, "Bottom of the Well"},
	{0x0088, "Ice Cavern"},
	{0x041B, "Ganon's Tower"},
	{0x0008, "Gerudo Training Ground"},
	{0x0486, "Thieves' Hideout"},
	{0x0467, "Inside Ganon's Castle"},
	{0x0134, "Ganon's Tower (Collapsing)"},
	{0x056C, "Inside Ganon's Castle (Collapsing)"},
	{0x0063, "Treasure Box Shop"},
	{0x040F, "Gohma's Lair"},
	{0x040B, "King Dodongo's Lair"},
	{0x0301, "Barinade's Lair"},
	{0x000C, "Phantom Ganon's Lair"},
	{0x0305, "Volvagia's Lair"},
	{0x0417, "Morpha's Lair"},
	{0x008D, "Twinrova's Lair & Nabooru's Mini-Boss Room"},
	{0x0413, "Bongo Bongo's Lair"},
	{0x041F, "Ganondorf's Lair"},
	{0x01C9, "Ganon's Tower Exterior (Collapsing)"},
	{0x0033, "Market Entrance (Child - Day)"},
	{0x0034, "Market Entrance (Child - Night)"},
	{0x0067, "Back Alley (Child - Day)"},
	{0x0068, "Back Alley (Child - Night)"},
	{0x01CD, "Market (Child - Day)"},
	{0x01CE, "Market (Child - Night)"},
	{0x0171, "Temple of Time Exterior (Child - Day)"},
	{0x0172, "Temple of Time Exterior (Child - Night)"},
	{0x007A, "Castle Hedge Maze (Day)"},
	{0x007B, "Castle Hedge Maze (Night)"},
	{0x00C9, "Know-It-All Brothers' House"},
	{0x009C, "House of Twins"},
	{0x0433, "Mido's House"},
	{0x0437, "Saria's House"},
	{0x02FD, "Kakariko Tenement"},
	{0x043B, "Back Alley House (Man in Green)"},
	{0x00B7, "Bazaar"},
	{0x00C1, "Kokiri Shop"},
	{0x037C, "Goron Shop"},
	{0x0380, "Zora Shop"},
	{0x0384, "Kakariko Potion Shop"},
	{0x0388, "Market Potion Shop"},
	{0x0390, "Bombchu Shop"},
	{0x0530, "Happy Mask Shop"},
	{0x00BB, "Link's House"},
	{0x0398, "Back Alley House (Dog Lady)"},
	{0x02F9, "Stable"},
	{0x039C, "Impa's House"},
	{0x0043, "Lakeside Laboratory"},
	{0x03A0, "Carpenters' Tent"},
	{0x030D, "Gravekeeper's Hut"},
	{0x0315, "Great Fairy's Fountain (Upgrades)"},
	{0x036D, "Fairy's Fountain"},
	{0x0578, "Great Fairy's Fountain (Spells)"},
	{0x003F, "Grottos"},
	{0x031C, "Grave (Redead)"},
	{0x004B, "Grave (Fairy's Fountain)"},
	{0x002D, "Royal Family's Tomb"},
	{0x003B, "Shooting Gallery"},
	{0x0053, "Temple of Time"},
	{0x006B, "Chamber of the Sages"},
	{0x00A0, "Cutscene Map"},
	{0x044F, "Dampé's Grave & Windmill"},
	{0x045F, "Fishing Pond"},
	{0x05F0, "Castle Courtyard"},
	{0x0507, "Bombchu Bowling Alley"},
	{0x004F, "Ranch House & Silo"},
	{0x007E, "Guard House"},
	{0x0072, "Granny's Potion Shop"},
	{0x0517, "Ganon's Tower Collapse & Battle Arena"},
	{0x0550, "House of Skulltula"},
	{0x00CD, "Spot 00 - Hyrule Field"},
	{0x00DB, "Spot 01 - Kakariko Village"},
	{0x00E4, "Spot 02 - Graveyard"},
	{0x00EA, "Spot 03 - Zora's River"},
	{0x00EE, "Spot 04 - Kokiri Forest"},
	{0x00FC, "Spot 05 - Sacred Forest Meadow"},
	{0x0102, "Spot 06 - Lake Hylia"},
	{0x0108, "Spot 07 - Zora's Domain"},
	{0x010E, "Spot 08 - Zora's Fountain"},
	{0x0117, "Spot 09 - Gerudo Valley"},
	{0x011E, "Spot 10 - Lost Woods"},
	{0x0123, "Spot 11 - Desert Colossus"},
	{0x0129, "Spot 12 - Gerudo's Fortress"},
	{0x0138, "Spot 15 - Hyrule Castle"},
	{0x013D, "Spot 16 - Death Mountain Trail"},
	{0x0147, "Spot 17 - Death Mountain Crater"},
	{0x014D, "Spot 18 - Goron City"},
	{0x0157, "Spot 20 - Lon Lon Ranch"},
	{0x0130, "Spot 13 - Haunted Wasteland"},]]--
	
	
	{0x0035-2, "Market Entrance (Ruins)"},
	{0x01CF-2, "Market (Ruins)"},
	{0x0173-2, "Temple of Time Exterior (Ruins)"},
	{0x013A, "Ganon's Castle Exterior"}
}

--[[
MM_Scene_Entrances = {
	{0x0000, "Mayor's Residence"},
	{0x0200, "Majora's Lair"},
	{0x0400, "Magic Hags' Potion Shop"},
	{0x0600, "Barn / Ranch House"},
	{0x0800, "Honey & Darling's Shop"},
	{0x0A00, "Beneath the Graveyard"},
	{0x0C00, "Southern Swamp (Clean)"},
	{0x0E00, "Curiosity Shop / Kafei's Hideout"},
	{0x1000, "Test Map"},
	{0x1400, "Grottos"},
	{0x1C00, "Cutscene Map"},
	{0x2000, "Ikana Canyon / Spring Water Cave"},
	{0x2200, "Pirates' Fortress (Outside buildings)"},
	{0x2400, "Milk Bar"},
	{0x2600, "Stone Tower Temple"},
	{0x2800, "Treasure Chest Shop"},
	{0x2A00, "Inverted Stone Tower Temple"},
	{0x2C00, "Clock Tower Rooftop"},
	{0x2E00, "Before the Portal to Termina"},
	{0x3000, "Woodfall Temple"},
	{0x3200, "Path to Mountain Village"},
	{0x3400, "Ancient Castle of Ikana"},
	{0x3600, "Deku Scrub Playground"},
	{0x3800, "Odolwa's Lair"},
	{0x3A00, "Town Shooting Gallery"},
	{0x3C00, "Snowhead Temple"},
	{0x3E00, "Milk Road"},
	{0x4000, "Pirates' Fortress (Inside buildings)"},
	{0x4200, "Swamp Shooting Gallery"},
	{0x4400, "Pinnacle Rock"},
	{0x4600, "Fairy's Fountain"},
	{0x4800, "Swamp Spider House"},
	{0x4A00, "Oceanside Spider House"},
	{0x4C00, "Astral Observatory (Bomber's Hideout)"},
	{0x4E00, "The Moon - Deku Trial"},
	{0x5000, "Deku Palace"},
	{0x5200, "Mountain Smithy"},
	{0x5400, "Termina Field"},
	{0x5600, "Post Office"},
	{0x5800, "Marine Research Lab"},
	{0x5A00, "Dampe's House"},
	{0x5E00, "Goron Shrine"},
	{0x6000, "Zora Hall"},
	{0x6200, "Trading Post"},
	{0x6400, "Romani Ranch"},
	{0x6600, "Twinmold's Lair"},
	{0x6800, "Great Bay Coast"},
	{0x6A00, "Zora Cape"},
	{0x6C00, "Lottery Shop"},
	{0x7000, "Pirates' Fortress (Water area)"},
	{0x7200, "Fisherman's Hut"},
	{0x7400, "Goron Shop"},
	{0x7600, "Deku King's Chamber"},
	{0x7800, "The Moon - Goron Trial"},
	{0x7A00, "Path to Southern Swamp"},
	{0x7C00, "Doggy Racetrack"},
	{0x7E00, "Cucco Shack"},
	{0x8000, "Ikana Graveyard"},
	{0x8200, "Goht's Lair"},
	{0x8400, "Southern Swamp (Poisoned)"},
	{0x8600, "Woodfall"},
	{0x8800, "The Moon - Zora Trial"},
	{0x8A00, "Goron Village (Spring)"},
	{0x8C00, "Great Bay Temple"},
	{0x8E00, "Waterfall Rapids"},
	{0x9000, "Beneath the Well"},
	{0x9200, "Zora Hall Rooms"},
	{0x9400, "Goron Village (Winter)"},
	{0x9600, "Goron Graveyard"},
	{0x9800, "Sakon's Hideout"},
	{0x9A00, "Mountain Village (Winter)"},
	{0x9C00, "Ghost Hut"},
	{0x9E00, "Deku Shrine"},
	{0xA000, "Path to Ikana"},
	{0xA200, "Swordsman's School"},
	{0xA400, "Music Box House"},
	{0xA600, "Igos du Ikana's Lair"},
	{0xA800, "Tourist Information"},
	{0xAA00, "Stone Tower"},
	{0xAC00, "Stone Tower (Inverted)"},
	{0xAE00, "Mountain Village (Spring)"},
	{0xB000, "Path to Snowhead"},
	{0xB200, "Snowhead"},
	{0xB400, "Path to Goron Village (Winter)"},
	{0xB600, "Path to Goron Village (Spring)"},
	{0xB800, "Gyorg's Lair"},
	{0xBA00, "Secret Shrine"},
	{0xBC00, "Stock Pot Inn"},
	{0xBE00, "Great Bay (Cutscene)"},
	{0xC000, "Clock Tower Interior"},
	{0xC200, "Woods of Mystery"},
	{0xC400, "Lost Woods"},
	{0xC600, "The Moon - Link Trial"},
	{0xC800, "The Moon"},
	{0xCA00, "Bomb Shop"},
	{0xCC00, "Giants' Chamber"},
	{0xCE00, "Gorman Track"},
	{0xD000, "Goron Racetrack"},
	{0xD200, "East Clock Town"},
	{0xD400, "West Clock Town"},
	{0xD600, "North Clock Town"},
	{0xD800, "South Clock Town"},
	{0xDA00, "Laundry Pool"},
	{0xDC00, "Unused Southern Swamp (healed)"}
}
]]--

local function writeLine(path, text)
    local f = io.open(path, "a")
    if not f then
        error("could not open file")
    end
    f:write(text, "\n")
    f:close()
end


function check_invisible_seam(verts, normal, dist)
	--print(verts[1][1]..", "..verts[1][2]..", "..verts[1][3])
	--print(verts[2][1]..", "..verts[2][2]..", "..verts[2][3])
	--print(verts[3][1]..", "..verts[3][2]..", "..verts[3][3])
	--print("")

	if normal[2] < 0.00008 or normal[2] > 0.1 then
		return
	end
	
	ymax = math.max(verts[1][2], verts[2][2], verts[3][2])
	q = 1
	while q <= 3 do
		if verts[q][2] == ymax then
			--seamX, seamY, seamZ = highestPoint(verts[q][1], verts[q][2], verts[q][3], normal[1], normal[2], normal[3], dist)
			--print("("..verts[q][1]..", "..verts[q][2]..", "..verts[q][3].."): ("..seamX..", "..seamY..", "..seamZ..")")
			--print(verts[q][1]..", "..verts[q][2]..", "..verts[q][3]..normal[1]..", "..normal[2]..", "..normal[3]..", "..dist)
		end
		q = q + 1
	end
	
end

OUT_PATH = "C:\\Users\\X\\Downloads\\BizHawk-2.10-win-x64\\Lua\\N64\\"

function print_static_polys()

	colHeader.highest_type = 0
	
	s = "-Static Polys-\n"
	i = 0
	while i < colHeader.numPolygons do
		polyAddr = colHeader.polyList_ptr + (i * POLY_SIZE)
		polySurfaceTypeId = read_u16(polyAddr + 0x0)
		vtxData1 = bit_band(read_u16(polyAddr + 0x2), 0x1FFF)
		vtxData2 = bit_band(read_u16(polyAddr + 0x4), 0x1FFF)
		vtxData3 = read_u16(polyAddr + 0x6)
		
		
		if GAME == "OOT3D" then
			polyNormalX = read_s16(polyAddr + 10)
			polyNormalY = read_s16(polyAddr + 12)
			polyNormalZ = read_s16(polyAddr + 14)
		else
			polyNormalX = read_s16(polyAddr + 8)
			polyNormalY = read_s16(polyAddr + 10)
			polyNormalZ = read_s16(polyAddr + 12)
		end
		
		
		dist = read_s16(polyAddr + 14)
		if GAME == "MM3D" or GAME == "OOT3D" then
			dist = readfloat(polyAddr + 0x10)
		end
		
		surfaceExitIndex = bit_band(mainmemory.readbyte(colHeader.surfaceTypeList_ptr + 8*polySurfaceTypeId + 2), 0x1F)
		
		exitValue = ""
		if (surfaceExitIndex ~= 0 and exitList ~= 0) then
			exitValue = read_u16(exitList + 2*(surfaceExitIndex-1))
			
			exitValue = string.format("EXIT %04X, ", exitValue)
		end
		
		vtx1X = read_s16(colHeader.vtxList_ptr + (vtxData1 * 0x6) + 0x0)
		vtx1Y = read_s16(colHeader.vtxList_ptr + (vtxData1 * 0x6) + 0x2)
		vtx1Z = read_s16(colHeader.vtxList_ptr + (vtxData1 * 0x6) + 0x4)
		
		vtx2X = read_s16(colHeader.vtxList_ptr + (vtxData2 * 0x6) + 0x0)
		vtx2Y = read_s16(colHeader.vtxList_ptr + (vtxData2 * 0x6) + 0x2)
		vtx2Z = read_s16(colHeader.vtxList_ptr + (vtxData2 * 0x6) + 0x4)
		
		vtx3X = read_s16(colHeader.vtxList_ptr + (vtxData3 * 0x6) + 0x0)
		vtx3Y = read_s16(colHeader.vtxList_ptr + (vtxData3 * 0x6) + 0x2)
		vtx3Z = read_s16(colHeader.vtxList_ptr + (vtxData3 * 0x6) + 0x4)
		
		if polySurfaceTypeId > colHeader.highest_type then
			colHeader.highest_type = polySurfaceTypeId
		end
		
		centerX = round((vtx1X+vtx2X+vtx3X)/3, 2)
		centerY = round((vtx1Y+vtx2Y+vtx3Y)/3, 2)
		centerZ = round((vtx1Z+vtx2Z+vtx3Z)/3, 2)
		
		--check for invisible seam
		--COLPOLY_NORMAL_FRAC = 1.0 / 32767.0
		--verts = {{vtx1X, vtx1Y, vtx1Z}, {vtx2X, vtx2Y, vtx2Z}, {vtx3X, vtx3Y, vtx3Z}}
		--normal = {polyNormalX, polyNormalY, polyNormalZ}
		--print(verts[1][1]..", "..verts[1][2]..", "..verts[1][3]..", "..verts[2][1]..", "..verts[2][2]..", "..verts[2][3]..", "..verts[3][1]..", "..verts[3][2]..", "..verts[3][3]..", "..normal[1]..", "..normal[2]..", "..normal[3]..", "..dist)
		
		--writeLine(OUT_PATH.."test.txt", verts[1][1]..", "..verts[1][2]..", "..verts[1][3]..", "..verts[2][1]..", "..verts[2][2]..", "..verts[2][3]..", "..verts[3][1]..", "..verts[3][2]..", "..verts[3][3]..", "..normal[1]..", "..normal[2]..", "..normal[3]..", "..dist)
		--normal = {polyNormalX*COLPOLY_NORMAL_FRAC, polyNormalY*COLPOLY_NORMAL_FRAC, polyNormalZ*COLPOLY_NORMAL_FRAC}
		--check_invisible_seam(verts, normal, dist)
		
		
		
		--local lookup = {}
		--for _, v in ipairs(flat_ground_clip_y_table) do
		--	lookup[v] = true
		--end

		--if vtx1Y == vtx2Y and vtx2Y == vtx3Y and polyNormalY == 32767 and lookup[vtx1Y] then
			s = s..i..". "..string.format("%06X", polyAddr)..": "..exitValue.."surfaceType: "..polySurfaceTypeId..", ("..vtx1X..", "..vtx1Y..", "..vtx1Z.."), ("..vtx2X..", "..vtx2Y..", "..vtx2Z.."), ("..vtx3X..", "..vtx3Y..", "..vtx3Z.."), normal("..polyNormalX..", "..polyNormalY..", "..polyNormalZ.."), dist: "..round(dist, 6)
			s = s..", center: ("..centerX..", "..centerY..", "..centerZ.."), vtxIds: ("..vtxData1..", "..vtxData2..", "..vtxData3..")"
			s = s.."\n"
		--end
		
		i = i + 1
	end
	print(s)
	print("highest surface type is: "..colHeader.highest_type.."\n")
end

function print_subdivisions()
	s = "-Subdivisions- (total: "..colCtx.total_subdivisions..")\n"
	
	for zi=0,colCtx.subdivAmountZ-1 do
		for yi=0,colCtx.subdivAmountY-1 do
			for xi=0,colCtx.subdivAmountX-1 do
				
				sector_min_x = colCtx.minBoundsX + colCtx.subdivLengthX*xi
				sector_min_y = colCtx.minBoundsY + colCtx.subdivLengthY*yi
				sector_min_z = colCtx.minBoundsZ + colCtx.subdivLengthZ*zi
				sector_max_x = colCtx.minBoundsX + colCtx.subdivLengthX*(xi+1)
				sector_max_y = colCtx.minBoundsY + colCtx.subdivLengthY*(yi+1)
				sector_max_z = colCtx.minBoundsZ + colCtx.subdivLengthZ*(zi+1)
				
				s = s..xi..", "..yi..", "..zi..": minPos: ("..sector_min_x..", "..sector_min_y..", "..sector_min_z.."), maxPos: ("..sector_max_x..", "..sector_max_y..", "..sector_max_z..")\n"
				
			end
		end
	end
	
	print(s)
end

function print_static_polys_by_subdivision()
	s = "-Static Polys by Subdivision-\n"
	
	fwc = {"floor  ", "wall   ", "ceiling"} -- floor, wall, ceiling
	
	i = 0
	poly_counter = 0
	for zi=0,colCtx.subdivAmountZ-1 do
		for yi=0,colCtx.subdivAmountY-1 do
			for xi=0,colCtx.subdivAmountX-1 do
				
				local sector_min_x = colCtx.minBoundsX + colCtx.subdivLengthX * xi
                local sector_min_y = colCtx.minBoundsY + colCtx.subdivLengthY * yi
                local sector_min_z = colCtx.minBoundsZ + colCtx.subdivLengthZ * zi
                local sector_max_x = colCtx.minBoundsX + colCtx.subdivLengthX * (xi + 1)
                local sector_max_y = colCtx.minBoundsY + colCtx.subdivLengthY * (yi + 1)
                local sector_max_z = colCtx.minBoundsZ + colCtx.subdivLengthZ * (zi + 1)
				
				local foundANode = false
				
				for j=0,2 do -- floors, walls, and ceilings
				
					nodeIndex = read_u16(colCtx.StaticLookup_lookupTbl_ptr + 6*i + 2*j)
					StaticLookupAddr = colCtx.StaticLookup_lookupTbl_ptr + 6*i + 2*j
					while nodeIndex ~= 0xFFFF do
						
						if foundANode == false then
							--if xi ~= 13 or yi ~= 0 or zi ~= 9 then
							--	break
							--end
							
							s = s..string.format("sector [%s,%s],[%s,%s],[%s,%s], num: (%s, %s, %s), i: %s, StaticLookupAddr: %s\n", sector_min_x,sector_max_x, sector_min_y,sector_max_y, sector_min_z,sector_max_z, xi, yi, zi, i, string.format("%06X", StaticLookupAddr))
						end
						foundANode = true
						
						local polyIdAddr = colCtx.SSNodeList_polyNodes_SSNode_tbl_ptr + 4 * nodeIndex
                        local polyId = read_u16(polyIdAddr)
                        local base = colHeader.polyList_ptr + POLY_SIZE * polyId

                        local polySurfaceTypeId = read_u16(base)
                        local surfaceExitIndex = bit_band(
                            mainmemory.readbyte(colHeader.surfaceTypeList_ptr + 8 * polySurfaceTypeId + 2), 0x1F
                        )
						
						local exitValue = ""
                        if surfaceExitIndex ~= 0 then
                            local exitVal = read_u16(exitList + 2 * (surfaceExitIndex - 1))
                            exitValue = string.format("- exit %04X", exitVal)
                        end
						
						local polyVertIdA = bit_band(read_u16(base + 2), 0x1FFF)
                        local polyVertIdB = bit_band(read_u16(base + 4), 0x1FFF)
                        local polyVertIdC = read_u16(base + 6)

                        local polyNormalX = read_s16(base + 8)
                        local polyNormalY = read_s16(base + 10)
                        local polyNormalZ = read_s16(base + 12)
						
						local dist = read_s16(base + 14)
						if GAME == "MM3D" or GAME == "OOT3D" then
							dist = readfloat(base + 0x10)
						end
						
						local function readVert(id)
                            local addr = colHeader.vtxList_ptr + 6 * id
                            return read_s16(addr + 0),
                                   read_s16(addr + 2),
                                   read_s16(addr + 4)
                        end

                        local vertAX, vertAY, vertAZ = readVert(polyVertIdA)
                        local vertBX, vertBY, vertBZ = readVert(polyVertIdB)
                        local vertCX, vertCY, vertCZ = readVert(polyVertIdC)
						
						local tri_min_x = math.min(vertAX, vertBX, vertCX)
                        local tri_min_y = math.min(vertAY, vertBY, vertCY)
                        local tri_min_z = math.min(vertAZ, vertBZ, vertCX)
                        local tri_max_x = math.max(vertAX, vertBX, vertCX)
                        local tri_max_y = math.max(vertAY, vertBY, vertCY)
                        local tri_max_z = math.max(vertAZ, vertBZ, vertCX)
						--s = s..nodeIndex..": "..j.."\n"
						
						
						s=s..string.format("   %s %s - nodeIndex %s polyId %d (%s) surface %d - vtx (%d,%d,%d), (%d,%d,%d), (%d,%d,%d), vertIds (%s,%s,%s), normal(%s,%s,%s), dist %s\n", fwc[j+1], exitValue, string.format("%04X", nodeIndex), polyId, string.format("%06X", polyIdAddr), polySurfaceTypeId, vertAX,vertAY,vertAZ, vertBX,vertBY,vertBZ, vertCX,vertCY,vertCZ, polyVertIdA,polyVertIdB,polyVertIdC,polyNormalX,polyNormalY,polyNormalZ,round(dist, 6))
						-- %d. poly_counter, 
						poly_counter = poly_counter + 1
							
						nodeIndex = read_u16(colCtx.SSNodeList_polyNodes_SSNode_tbl_ptr + 4*nodeIndex + 2)
					end
				end
				i = i + 1
			end
		end
	end
	
	print(s)
end

function print_scene_surfaceTypes()
	MATERIALS_MM = {"dirt", "sand", "stone", "dirt_shallow", "water_shallow", "water_deep", "tall_grass", "lava", "grass", "bridge", "wood", "dirt_soft", "ice", "carpet", "snow"}
	MATERIALS_OOT = {"dirt", "sand", "stone", "jabu", "water_shallow", "water_deep", "tall_grass", "lava", "grass", "bridge", "wood", "dirt_soft", "ice", "carpet", "unk"}
	CAMERA_TYPES = {"NONE", "NORMAL0", "NORMAL3", "PIVOT_DIVING", "HORSE", "ZORA_DIVING", "PREREND_FIXED", "PREREND_PIVOT", "DOORC", "DEMO0", "FREE0", "BIRDS_EYE_VIEW_0", "NORMAL1", "NANAME", "CIRCLE0", "FIXED0", "SPIRAL_DOOR", "DUNGEON0", "ITEM0", "ITEM1", "ITEM2", "ITEM3", "NAVI", "WARP_PAD_MOON", "DEATH", "REBIRTH", "LONG_CHEST_OPENING", "MASK_TRANSFORMATION", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)", "(todo)"} -- CameraSettingType enum in decomp
	
	s = "-Scene Surface Type List-\n"
	i = 0
	while i < colHeader.highest_type + 1 do
		word1 = read_u32(colHeader.surfaceTypeList_ptr + (i * 0x8) + 0x0)
		word2 = read_u32(colHeader.surfaceTypeList_ptr + (i * 0x8) + 0x4)
		
		horseBlocked = bit_rshift(bit_band(word1, 0x80000000), 31)
		isSoft = bit_rshift(bit_band(word1, 0x40000000), 30)
		floorProperty = bit_rshift(bit_band(word1, 0x3C000000), 26)
		wallType = bit_rshift(bit_band(word1, 0x03E00000), 21)
		unk18 = bit_rshift(bit_band(word1, 0x001C0000), 18) -- unused in OoT. idk if used in MM
		floorType = bit_rshift(bit_band(word1, 0x0003E000), 13)
		surfaceExitIndex = bit_rshift(bit_band(word1, 0x00001F00), 8)
		bgCamIndex = bit_rshift(bit_band(word1, 0x000000FF), 0)
		
		wallDamage = bit_rshift(bit_band(word2, 0x08000000), 27)
		conveyorDirection = bit_rshift(bit_band(word2, 0x07E00000), 21)
		conveyorSpeed = bit_rshift(bit_band(word2, 0x001C0000), 18)
		canHookshot = bit_rshift(bit_band(word2, 0x00020000), 17)
		echo = bit_rshift(bit_band(word2, 0x0001F800), 11)
		lightSetting = bit_rshift(bit_band(word2, 0x000007C0), 6)
		floorEffect = bit_rshift(bit_band(word2, 0x00000030), 4)
		material = bit_rshift(bit_band(word2, 0x0000000F), 0)
		
		s = s..i..". "
		if surfaceExitIndex ~= 0 then
			exitValue = read_u16(exitList + 2*(surfaceExitIndex-1))
			s = s.."exitIndex: "..string.format("0x%x", exitValue)..", "
		end
		if GAME == "OoT" or GAME == "OOT3D" then
			s = s.."material: "..MATERIALS_OOT[material+1]..", lightSetting: "..string.format("0x%x", lightSetting)
		elseif GAME == "MM" or GAME == "MM3D" then
			s = s.."material: "..MATERIALS_MM[material+1]..", lightSetting: "..string.format("0x%x", lightSetting)
		end
		--..string.format("0x%x", word1)..", "..string.format("0x%x", word2)..
		
		if bgCamIndex ~= 0 then
			s = s..", bgCamIndex: "..CAMERA_TYPES[bgCamIndex+1]
		end
		
		if horseBlocked ~= 0 then
			s = s..", horseBlocked"
		end
		if isSoft ~= 0 then
			s = s..", isSoft"
		end
		if floorProperty ~= 0 then
			s = s..", floorProperty: "..string.format("0x%x", floorProperty)
		end
		if wallType ~= 0 then
			s = s..", wallType: "..string.format("0x%x", wallType)
		end
		if unk18 ~= 0 then
			s = s..", unk18: "..string.format("0x%x", unk18)
		end
		if floorType ~= 0 then
			s = s..", floorType: "..string.format("0x%x", floorType)
		end
		
		if wallDamage ~= 0 then
			s = s..", wallDamage"
		end
		if conveyorDirection ~= 0 then
			s = s..", conveyorDirection: "..string.format("0x%x", conveyorDirection)
		end
		if conveyorSpeed ~= 0 then
			s = s..", conveyorSpeed: "..string.format("0x%x", conveyorSpeed)
		end
		if canHookshot ~= 0 then
			s = s..", canHookshot"
		end
		if echo ~= 0 then
			s = s..", echo: "..echo
		end
		if floorEffect ~= 0 then
			s = s..", floorEffect: "..string.format("0x%x", floorEffect)
		end
		
		s = s.."\n"
		
		i = i + 1
	end
	
	print(s)
end

function print_static_waterBoxes()
	s = "-Static Water Boxes-\n"
	i = 0
	while i < colHeader.numWaterBoxes do
		minPosX = read_s16(colHeader.waterBoxes_ptr + (i * POLY_SIZE) + 0x0)
		minPosY = read_s16(colHeader.waterBoxes_ptr + (i * POLY_SIZE) + 0x2)
		minPosZ = read_s16(colHeader.waterBoxes_ptr + (i * POLY_SIZE) + 0x4)
		xLength = read_s16(colHeader.waterBoxes_ptr + (i * POLY_SIZE) + 0x6)
		zLength = read_s16(colHeader.waterBoxes_ptr + (i * POLY_SIZE) + 0x8)
		properties = read_u32(colHeader.waterBoxes_ptr + (i * POLY_SIZE) + 0xC)
		
		s = s..i..". properties: "..string.format("0x%x", properties)..", minPos: ("..minPosX..", "..minPosY..", "..minPosZ.."), xLength: "..xLength..", zLength: "..zLength.."\n"
		
		i = i + 1
	end
	
	print(s)
end

function print_bg_actors()
	s = "-Bg Actors and Dyna Polys-\n"
	
	i = 0
	while i < 50 do
		actor_ptr = read_u32(dynaColCtx.bgActors_list_start + (i * BG_ACTOR_SIZE) + 0x0)
		dynaLookup_polyStartIndex = read_s16(dynaColCtx.bgActors_list_start + (i * BG_ACTOR_SIZE) + 0x8)
		dynaLookup_ceiling = read_u16(dynaColCtx.bgActors_list_start + (i * BG_ACTOR_SIZE) + 0xA)
		dynaLookup_wall = read_u16(dynaColCtx.bgActors_list_start + (i * BG_ACTOR_SIZE) + 0xC)
		dynaLookup_floor = read_u16(dynaColCtx.bgActors_list_start + (i * BG_ACTOR_SIZE) + 0xE)
		vtxStartIndex = read_u16(dynaColCtx.bgActors_list_start + (i * BG_ACTOR_SIZE) + 0x10)
		flags = read_u16(dynaColCtx.bgActorFlags_list_start + (i * 0x2) + 0x0)
		flags_string = ""
		if bit_band(flags, 0x2) == 0x2 then
			flags_string = flags_string.."unk_flag, "
		end
		if bit_band(flags, 0x4) == 0x4 then
			flags_string = flags_string.."COLLISION_DISABLED, "
		end
		if bit_band(flags, 0x8) == 0x8 then
			flags_string = flags_string.."CEILING_COLLISION_DISABLED, "
		end
		if bit_band(flags, 0x20) == 0x20 then
			flags_string = flags_string.."FLOOR_COLLISION_DISABLED, "
		end
		
		if actor_ptr ~= 0x00000000 then
			actorX = readfloat(actor_ptr - offset_addr + 0x24)
			actorY = readfloat(actor_ptr - offset_addr + 0x28)
			actorZ = readfloat(actor_ptr - offset_addr + 0x2C)
			
			actor_id = read_u16(actor_ptr - offset_addr + 0x0)
			s = s..i..". "..string.format("0x%x", actor_id)..": "..string.format("0x%x", actor_ptr)..", w: "..string.format("0x%x", dynaLookup_wall)..", f: "..string.format("0x%x", dynaLookup_floor)..", c: "..string.format("0x%x", dynaLookup_ceiling)..", pos("..actorX..", "..actorY..", "..actorZ.."), "..flags_string.."\n"
			
			cwf_list1 = {dynaLookup_wall, dynaLookup_floor, dynaLookup_ceiling}
			cwf_list2 = {"wall   ", "floor  ", "ceiling"}
			
			j = 1
			while j < 4 do
				nodeIndex = cwf_list1[j]
				while nodeIndex ~= 0xFFFF do
					polyId = read_u16(dynaColCtx.DynaSSNodeList_polyNodes_SSNode_tbl_ptr + 4*nodeIndex)
					
					if GAME == "MM3D" or GAME == "OOT3D" then
						polyAddr = dynaColCtx.polyList_ptr + DYNA_POLY_SIZE*polyId
					else
						polyAddr = dynaColCtx.polyList_ptr + POLY_SIZE*polyId
					
					end
					polySurfaceTypeId = read_u16(polyAddr)
					
					exitValue = ""
					--[[surfaceExitIndex = bit_band(mainmemory.readbyte(colHeader.surfaceTypeList_ptr + 8*polySurfaceTypeId + 2), 0x1F)
					
					exitValue = ""
					if (surfaceExitIndex ~= 0) then
						exitValue = read_u16(exitList + 2*(surfaceExitIndex-1))
						
						exitValue = string.format("- EXIT %04X - ", exitValue)
					end]]--
					
					polyVertIdA = bit_band(read_u16(polyAddr + 2), 0x1FFF)
					polyVertIdB = bit_band(read_u16(polyAddr + 4), 0x1FFF)
					polyVertIdC = read_u16(polyAddr + 6)
					
					if GAME == "OoT" or GAME == "MM" then
						vertAX = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdA + 0), 3)
						vertAY = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdA + 2), 3)
						vertAZ = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdA + 4), 3)
						vertBX = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdB + 0), 3)
						vertBY = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdB + 2), 3)
						vertBZ = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdB + 4), 3)
						vertCX = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdC + 0), 3)
						vertCY = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdC + 2), 3)
						vertCZ = round(read_s16(dynaColCtx.vtxList_ptr + 6*polyVertIdC + 4), 3)
					elseif GAME == "MM3D" or GAME == "OOT3D" then
						vertAX = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdA + 0), 3)
						vertAY = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdA + 4), 3)
						vertAZ = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdA + 8), 3)
						vertBX = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdB + 0), 3)
						vertBY = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdB + 4), 3)
						vertBZ = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdB + 8), 3)
						vertCX = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdC + 0), 3)
						vertCY = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdC + 4), 3)
						vertCZ = round(readfloat(dynaColCtx.vtxList_ptr + 12*polyVertIdC + 8), 3)
					end
					
					local polyNormalX = read_s16(polyAddr + 8)
					local polyNormalY = read_s16(polyAddr + 10)
					local polyNormalZ = read_s16(polyAddr + 12)
					local dist = read_s16(polyAddr + 14)
					if GAME == "MM3D" or GAME == "OOT3D" then
						dist = readfloat(polyAddr + 0x10)
					end
					
					--s=s..string.format("   %06X %s nodeIndex %d polyId %d surfaceType %d - poly (%d,%d,%d), (%d,%d,%d), (%d,%d,%d) (center (%s,%s,%s))\n", polyAddr, cwf_list2[j], nodeIndex, polyId, polySurfaceTypeId, vertAX,vertAY,vertAZ, vertBX,vertBY,vertBZ, vertCX,vertCY,vertCZ, round((vertAX+vertBX+vertCX)/3,3),round((vertAY+vertBY+vertCY)/3,3),round((vertAZ+vertBZ+vertCZ)/3,3)) -- also print vertices
					--s=s..string.format("   %06X %s nodeIndex %d polyId %d surfaceType %d - poly (center (%s,%s,%s))\n", polyAddr, cwf_list2[j], nodeIndex, polyId, polySurfaceTypeId, round((vertAX+vertBX+vertCX)/3,3),round((vertAY+vertBY+vertCY)/3,3),round((vertAZ+vertBZ+vertCZ)/3,3))
					
					--s=s..string.format("%d.   %06X %s surfaceType %d - poly (%d,%d,%d), (%d,%d,%d), (%d,%d,%d)\n", polyAddr, cwf_list2[j], nodeIndex, polyId, polySurfaceTypeId, vertAX,vertAY,vertAZ, vertBX,vertBY,vertBZ, vertCX,vertCY,vertCZ) -- also print vertices
					
					s = s.."   "..cwf_list2[j].."   "..polyId..". "..string.format("%06X", polyAddr)..": "..exitValue.."surfaceType: "..polySurfaceTypeId..", ("..vertAX..", "..vertAY..", "..vertAZ.."), ("..vertBX..", "..vertBY..", "..vertBZ.."), ("..vertCX..", "..vertCY..", "..vertCZ.."), normal("..polyNormalX..", "..polyNormalY..", "..polyNormalZ.."), vtxIds: ("..polyVertIdA..", "..polyVertIdB..", "..polyVertIdC.."), dist: "..round(dist, 6).."\n"
					
					nodeIndex = read_u16(dynaColCtx.DynaSSNodeList_polyNodes_SSNode_tbl_ptr + 4*nodeIndex + 2)
				end
				j = j + 1
			end
		end
		
		i = i + 1
	end
	
	print(s)
end

function print_dyna_vertices()
	s = "-Dyna Vertices-\n"
	
	--works but have no way of knowing how many there are without the ssnode lookup stuff
	
	i = 0
	while i < dynaColCtx.vtxListMax do
		vtxX = read_s16(dynaColCtx.vtxList_ptr + (i * 0x6) + 0x0)
		vtxY = read_s16(dynaColCtx.vtxList_ptr + (i * 0x6) + 0x2)
		vtxZ = read_s16(dynaColCtx.vtxList_ptr + (i * 0x6) + 0x4)
		s = s..i..". ("..vtxX..", "..vtxY..", "..vtxZ..")\n"
		
		i = i + 1
	end
	
	print(s)
end

function print_dyna_polys()
	dynaColCtx.highest_type = 0
	
	s = "-Dyna Polys-\n"
	i = 0
	while i < dynaColCtx.polyListMax do
		polyType = read_u16(dynaColCtx.polyList_ptr + (i * POLY_SIZE) + 0x0)
		vtxData1 = bit_band(read_u16(dynaColCtx.polyList_ptr + (i * POLY_SIZE) + 0x2), 0x1FFF)
		vtxData2 = bit_band(read_u16(dynaColCtx.polyList_ptr + (i * POLY_SIZE) + 0x4), 0x1FFF)
		vtxData3 = read_u16(dynaColCtx.polyList_ptr + (i * POLY_SIZE) + 0x6)
		
		vtx1X = read_s16(dynaColCtx.vtxList_ptr + (vtxData1 * 0x6) + 0x0)
		vtx1Y = read_s16(dynaColCtx.vtxList_ptr + (vtxData1 * 0x6) + 0x2)
		vtx1Z = read_s16(dynaColCtx.vtxList_ptr + (vtxData1 * 0x6) + 0x4)
		
		vtx2X = read_s16(dynaColCtx.vtxList_ptr + (vtxData2 * 0x6) + 0x0)
		vtx2Y = read_s16(dynaColCtx.vtxList_ptr + (vtxData2 * 0x6) + 0x2)
		vtx2Z = read_s16(dynaColCtx.vtxList_ptr + (vtxData2 * 0x6) + 0x4)
		
		vtx3X = read_s16(dynaColCtx.vtxList_ptr + (vtxData3 * 0x6) + 0x0)
		vtx3Y = read_s16(dynaColCtx.vtxList_ptr + (vtxData3 * 0x6) + 0x2)
		vtx3Z = read_s16(dynaColCtx.vtxList_ptr + (vtxData3 * 0x6) + 0x4)
		
		if polyType > dynaColCtx.highest_type then
			dynaColCtx.highest_type = polyType
		end
		
		centerX = round((vtx1X+vtx2X+vtx3X)/3, 2)
		centerY = round((vtx1Y+vtx2Y+vtx3Y)/3, 2)
		centerZ = round((vtx1Z+vtx2Z+vtx3Z)/3, 2)
		
		s = s..i..". surfaceType: "..polyType..", ("..vtx1X..", "..vtx1Y..", "..vtx1Z.."), ("..vtx2X..", "..vtx2Y..", "..vtx2Z.."), ("..vtx3X..", "..vtx3Y..", "..vtx3Z..")"
		s = s..", center: ("..centerX..", "..centerY..", "..centerZ..")".."\n"
		
		i = i + 1
	end
	print(s)
	print("highest surface type is: "..dynaColCtx.highest_type.."\n")
end

function print_dyna_polys_by_subdivision()
	
end

function print_dyna_waterBoxes()
	
end

go_to_poly_flag = false
go_to_vtx1 = {0,0,0}
go_to_vtx2 = {0,0,0}
go_to_vtx3 = {0,0,0}

function setup_go_to_poly()
	update_collision_addresses()
	polyId = forms.gettext(go_to_poly_textbox);
	
	polyType = read_u16(colHeader.polyList_ptr + (polyId * POLY_SIZE) + 0x0)
	vtxData1 = bit_band(read_u16(colHeader.polyList_ptr + (polyId * POLY_SIZE) + 0x2), 0x1FFF)
	vtxData2 = bit_band(read_u16(colHeader.polyList_ptr + (polyId * POLY_SIZE) + 0x4), 0x1FFF)
	vtxData3 = read_u16(colHeader.polyList_ptr + (polyId * POLY_SIZE) + 0x6)
	
	vtx1X = read_s16(colHeader.vtxList_ptr + (vtxData1 * 0x6) + 0x0)
	vtx1Y = read_s16(colHeader.vtxList_ptr + (vtxData1 * 0x6) + 0x2)
	vtx1Z = read_s16(colHeader.vtxList_ptr + (vtxData1 * 0x6) + 0x4)
	
	vtx2X = read_s16(colHeader.vtxList_ptr + (vtxData2 * 0x6) + 0x0)
	vtx2Y = read_s16(colHeader.vtxList_ptr + (vtxData2 * 0x6) + 0x2)
	vtx2Z = read_s16(colHeader.vtxList_ptr + (vtxData2 * 0x6) + 0x4)
	
	vtx3X = read_s16(colHeader.vtxList_ptr + (vtxData3 * 0x6) + 0x0)
	vtx3Y = read_s16(colHeader.vtxList_ptr + (vtxData3 * 0x6) + 0x2)
	vtx3Z = read_s16(colHeader.vtxList_ptr + (vtxData3 * 0x6) + 0x4)
	
	centerX = round((vtx1X+vtx2X+vtx3X)/3, 2)
	centerY = round((vtx1Y+vtx2Y+vtx3Y)/3, 2)
	centerZ = round((vtx1Z+vtx2Z+vtx3Z)/3, 2)
	
	go_to_vtx1 = {vtx1X, vtx1Y, vtx1Z}
	go_to_vtx2 = {vtx2X, vtx2Y, vtx2Z}
	go_to_vtx3 = {vtx3X, vtx3Y, vtx3Z}
	go_to_poly_flag = true
end

function setup_go_to_dyna_poly()
	update_collision_addresses()
	polyId = forms.gettext(go_to_dyna_poly_textbox);
	
	polyType = read_u16(dynaColCtx.polyList_ptr + (polyId * POLY_SIZE) + 0x0)
	vtxData1 = bit_band(read_u16(dynaColCtx.polyList_ptr + (polyId * POLY_SIZE) + 0x2), 0x1FFF)
	vtxData2 = bit_band(read_u16(dynaColCtx.polyList_ptr + (polyId * POLY_SIZE) + 0x4), 0x1FFF)
	vtxData3 = read_u16(dynaColCtx.polyList_ptr + (polyId * POLY_SIZE) + 0x6)
	
	vtx1X = read_s16(dynaColCtx.vtxList_ptr + (vtxData1 * 0x6) + 0x0)
	vtx1Y = read_s16(dynaColCtx.vtxList_ptr + (vtxData1 * 0x6) + 0x2)
	vtx1Z = read_s16(dynaColCtx.vtxList_ptr + (vtxData1 * 0x6) + 0x4)
	
	vtx2X = read_s16(dynaColCtx.vtxList_ptr + (vtxData2 * 0x6) + 0x0)
	vtx2Y = read_s16(dynaColCtx.vtxList_ptr + (vtxData2 * 0x6) + 0x2)
	vtx2Z = read_s16(dynaColCtx.vtxList_ptr + (vtxData2 * 0x6) + 0x4)
	
	vtx3X = read_s16(dynaColCtx.vtxList_ptr + (vtxData3 * 0x6) + 0x0)
	vtx3Y = read_s16(dynaColCtx.vtxList_ptr + (vtxData3 * 0x6) + 0x2)
	vtx3Z = read_s16(dynaColCtx.vtxList_ptr + (vtxData3 * 0x6) + 0x4)
	
	centerX = round((vtx1X+vtx2X+vtx3X)/3, 2)
	centerY = round((vtx1Y+vtx2Y+vtx3Y)/3, 2)
	centerZ = round((vtx1Z+vtx2Z+vtx3Z)/3, 2)
	
	go_to_vtx1 = {vtx1X, vtx1Y, vtx1Z}
	go_to_vtx2 = {vtx2X, vtx2Y, vtx2Z}
	go_to_vtx3 = {vtx3X, vtx3Y, vtx3Z}
	go_to_poly_flag = true
end

function display_floor_poly_id()
	floor_poly = read_u32(Player.floorPoly)
	--print(string.format("floor: 0x%x", floor_poly))
	scene_segment_ptr = read_u32(addr_scene_segment)
	--print(string.format("scene: 0x%x", scene_segment_ptr))
	
	--print(floor_poly > scene_segment_ptr)
	
	if floor_poly == 0x00000000 then
		floor_poly_id = "N/A"
		forms.settext(floor_poly_id_textbox, floor_poly_id);
		forms.settext(floor_dyna_poly_id_textbox, floor_poly_id);
	--elseif floor_poly > scene_segment_ptr then
	elseif (colHeader.polyList_ptr > dynaColCtx.polyList_ptr and floor_poly - offset_addr >= colHeader.polyList_ptr) or (colHeader.polyList_ptr < dynaColCtx.polyList_ptr and floor_poly - offset_addr < dynaColCtx.polyList_ptr)then
		floor_poly_id = math.floor((floor_poly - offset_addr - colHeader.polyList_ptr) / POLY_SIZE)
		forms.settext(floor_poly_id_textbox, floor_poly_id);
		--forms.settext(floor_poly_id_textbox, string.format("%x", floor_poly - offset_addr));
		forms.settext(floor_dyna_poly_id_textbox, "N/A");
		
		--[[
		-- attempt at determining ground clips, doesnt work
		
		polyNormalX = read_s16(floor_poly - offset_addr + 8)
		polyNormalY = read_s16(floor_poly - offset_addr + 10)
		polyNormalZ = read_s16(floor_poly - offset_addr + 12)
		originDist = read_s16(floor_poly - offset_addr + 14)
		
		--print(polyNormalX..", "..polyNormalY..", "..polyNormalZ..", dist: "..originDist)
		
		prevX = readfloat(Player.xPosPrev)
		prevY = readfloat(Player.yPosPrev)
		prevZ = readfloat(Player.zPosPrev)
		
		COLPOLY_NORMAL_FRAC = 1.0 / 32767.0
		planeDistA =
			(polyNormalX * prevX + polyNormalY * prevY + polyNormalZ * prevZ) * COLPOLY_NORMAL_FRAC +
			originDist;
		planeDistB =
			(polyNormalX * prevX + polyNormalY * prevY-10.0 + polyNormalZ * prevZ) * COLPOLY_NORMAL_FRAC +
			originDist;

		planeDistDelta = planeDistA - planeDistB;
		
		if ((planeDistA >= 0.0 and planeDistB >= 0.0) or (planeDistA < 0.0 and planeDistB < 0.0) or
			(true and planeDistA < 0.0 and planeDistB > 0.0) or math.abs(planeDistDelta) < 0.008) then
			print("ground clip")
		end
		]]--
	else
		floor_poly_id = math.floor((floor_poly - offset_addr - dynaColCtx.polyList_ptr) / DYNA_POLY_SIZE)
		forms.settext(floor_dyna_poly_id_textbox, floor_poly_id);
		forms.settext(floor_poly_id_textbox, "N/A");
	end
	
end

function display_wall_poly_id()
	wall_poly = read_u32(Player.wallPoly)
	scene_segment_ptr = read_u32(addr_scene_segment)
	
	if wall_poly == 0x00000000 then
		wall_poly_id = "N/A"
		forms.settext(wall_dyna_poly_id_textbox, wall_poly_id);
		forms.settext(wall_poly_id_textbox, wall_poly_id);
	--elseif  wall_poly > scene_segment_ptr then
	elseif (colHeader.polyList_ptr > dynaColCtx.polyList_ptr and wall_poly - offset_addr >= colHeader.polyList_ptr) or (colHeader.polyList_ptr < dynaColCtx.polyList_ptr and wall_poly - offset_addr < dynaColCtx.polyList_ptr)then
		wall_poly_id = math.floor((wall_poly - offset_addr - colHeader.polyList_ptr) / POLY_SIZE)
		forms.settext(wall_poly_id_textbox, wall_poly_id);
		forms.settext(wall_dyna_poly_id_textbox, "N/A");
	else
		wall_poly_id = math.floor((wall_poly - offset_addr - dynaColCtx.polyList_ptr) / DYNA_POLY_SIZE)
		forms.settext(wall_dyna_poly_id_textbox, wall_poly_id);
		forms.settext(wall_poly_id_textbox, "N/A");
	end
	
end

function display_current_subdivision()
	
end

function refresh()
	console.clear()
	
	--[[w = 1
	while w <= # (MM_Scene_Entrances) do
		
		write_u16(addr_next_entrance, MM_Scene_Entrances[w][1])
		mainmemory.write_u8(addr_warp_trigger, 20)
		
		warp_state = mainmemory.read_u8(addr_warp_trigger)
		col_header = read_u32(addr_colCtx + 0x0)
		if warp_state == 20 or warp_state == 222 or warp_state == 236 then
			repeat
				warp_state = mainmemory.read_u8(addr_warp_trigger)
				emu.frameadvance()
				emu.frameadvance()
				emu.frameadvance()
			until warp_state == 0
		elseif colCtx.colHeader_ptr == 0x00000000 then
			repeat
				col_header = read_u32(addr_colCtx + 0x0)
				emu.frameadvance()
				emu.frameadvance()
				emu.frameadvance()
			until col_header ~= 0x00000000
		end
		
		print(MM_Scene_Entrances[w][2])
		w = w + 1
	end]]--
	
	if colCtx.colHeader_ptr == 0x00000000 then
		return
	end
	
	update_collision_addresses()
	
	print_collision_context_info() -- temp comment out
	print_collision_header_info() -- temp comment out
	print_dyna_collision_context_info() -- temp comment out
	print_bg_actors() -- temp comment out

	--print_static_vertices()
	--print_subdivisions()
	
	print_static_polys() -- temp comment out
	print_scene_surfaceTypes() -- temp comment out
	print_static_waterBoxes() -- temp comment out
	print_static_polys_by_subdivision() -- temp comment out

	--print_dyna_vertices() -- no way to know when it ends
	--print_dyna_polys() -- no way to know when it ends
	--print_dyna_polys_by_subdivision()
	--print_dyna_waterBoxes()
end

options_form = forms.newform(300, 300, "Collision Options");
refresh_button = forms.button(options_form, "Refresh", refresh, 6, 6, 50, 20)
go_to_poly_button = forms.button(options_form, "Go static", setup_go_to_poly, 80, 6, 50, 20)
go_to_poly_textbox = forms.textbox(options_form, nil, 40, 30, nil, 140, 6, false, true)
forms.label(options_form, "Static wall", 80, 34, 60, 20)
wall_poly_id_textbox = forms.textbox(options_form, nil, 40, 30, nil, 140, 31, false, true)
forms.label(options_form, "Static floor", 80, 59, 60, 20)
floor_poly_id_textbox = forms.textbox(options_form, nil, 40, 30, nil, 140, 56, false, true)

go_to_poly_button = forms.button(options_form, "Go dyna", setup_go_to_dyna_poly, 80, 81, 50, 20)
go_to_dyna_poly_textbox = forms.textbox(options_form, nil, 40, 30, nil, 140, 81, false, true)
forms.label(options_form, "Dyna wall", 80, 109, 60, 20)
wall_dyna_poly_id_textbox = forms.textbox(options_form, nil, 40, 30, nil, 140, 106, false, true)
forms.label(options_form, "Dyna floor", 80, 134, 60, 20)
floor_dyna_poly_id_textbox = forms.textbox(options_form, nil, 40, 30, nil, 140, 131, false, true)


event.onexit(function()
	forms.destroy(options_form)
end)

function process_go_to_poly()
	-- set player coords to poly coords
	
	origX = readfloat(Player.xPos)
	origXCol = readfloat(Player.xPosCol)
	origY = readfloat(Player.yPos)
	origYCol = readfloat(Player.yPosCol)
	origZ = readfloat(Player.zPos)
	origZCol = readfloat(Player.zPosCol)
	
	q = 0
	while q < 20 do
		writefloat(Player.xPos, go_to_vtx1[1])
		writefloat(Player.xPosCol, go_to_vtx1[1])
		writefloat(Player.yPos, go_to_vtx1[2])
		writefloat(Player.yPosCol, go_to_vtx1[2])
		writefloat(Player.zPos, go_to_vtx1[3])
		writefloat(Player.zPosCol, go_to_vtx1[3])
		
		emu.frameadvance()
		emu.frameadvance()
		emu.frameadvance()
		q = q + 1
	end
	
	q = 0
	while q < 20 do
		writefloat(Player.xPos, go_to_vtx2[1])
		writefloat(Player.xPosCol, go_to_vtx2[1])
		writefloat(Player.yPos, go_to_vtx2[2])
		writefloat(Player.yPosCol, go_to_vtx2[2])
		writefloat(Player.zPos, go_to_vtx2[3])
		writefloat(Player.zPosCol, go_to_vtx2[3])
		
		emu.frameadvance()
		emu.frameadvance()
		emu.frameadvance()
		q = q + 1
	end
	
	q = 0
	while q < 20 do
		writefloat(Player.xPos, go_to_vtx3[1])
		writefloat(Player.xPosCol, go_to_vtx3[1])
		writefloat(Player.yPos, go_to_vtx3[2])
		writefloat(Player.yPosCol, go_to_vtx3[2])
		writefloat(Player.zPos, go_to_vtx3[3])
		writefloat(Player.zPosCol, go_to_vtx3[3])
		
		emu.frameadvance()
		emu.frameadvance()
		emu.frameadvance()
		q = q + 1
	end
	
	q = 0
	while q < 20 do
		writefloat(Player.xPos, origX)
		writefloat(Player.xPosCol, origXCol)
		writefloat(Player.yPos, origY)
		writefloat(Player.yPosCol, origYCol)
		writefloat(Player.zPos, origZ)
		writefloat(Player.zPosCol, origZCol)
		
		emu.frameadvance()
		emu.frameadvance()
		emu.frameadvance()
		q = q + 1
	end
	
	go_to_poly_flag = false
end

-- Main loop--
while(true) do
	-- stop execution while in loading zone
	warp_state = mainmemory.read_u8(addr_warp_trigger)
	col_header = read_u32(addr_colCtx + 0x0)
	
	
	--[[console.clear()
	
	w = 1
	while w <= # (OOT_Scene_Entrances) do
		
		write_u16(addr_next_entrance, OOT_Scene_Entrances[w][1])
		mainmemory.write_u8(addr_warp_trigger, 20)
		
		warp_state = mainmemory.read_u8(addr_warp_trigger)
		col_header = read_u32(addr_colCtx + 0x0)
		if warp_state == 20 or warp_state == 222 or warp_state == 236 then
			repeat
				warp_state = mainmemory.read_u8(addr_warp_trigger)
				emu.frameadvance()
				emu.frameadvance()
				emu.frameadvance()
			until warp_state == 0
		elseif colCtx.colHeader_ptr == 0x00000000 then
			repeat
				col_header = read_u32(addr_colCtx + 0x0)
				emu.frameadvance()
				emu.frameadvance()
				emu.frameadvance()
			until col_header ~= 0x00000000
		end
		
		
		timer = 0
		repeat
			emu.frameadvance()
			emu.frameadvance()
			emu.frameadvance()
			timer = timer + 1
		until timer == 50
		
		update_collision_addresses()
		writeLine(OUT_PATH.."test.txt", OOT_Scene_Entrances[w][2])
		print_static_polys()
		writeLine(OUT_PATH.."test.txt", "")
		
		print(OOT_Scene_Entrances[w][2])
		w = w + 1
	end]]--
	
	if warp_state == 20 or warp_state == 222 then
		repeat
			warp_state = mainmemory.read_u8(addr_warp_trigger)
			emu.frameadvance()
			emu.frameadvance()
			emu.frameadvance()
		until warp_state == 236
	elseif colCtx.colHeader_ptr == 0x00000000 then
		repeat
			col_header = read_u32(addr_colCtx + 0x0)
			emu.frameadvance()
			emu.frameadvance()
			emu.frameadvance()
		until col_header ~= 0x00000000
	end
	
	update_collision_addresses()
	if go_to_poly_flag == true then
		process_go_to_poly()
	end
	
	display_floor_poly_id()
	display_wall_poly_id()
	
	emu.yield()
end

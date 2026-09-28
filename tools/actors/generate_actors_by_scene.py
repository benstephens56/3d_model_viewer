"""Regenerate models/<GAME>/actors/<GAME>_actors_by_scene.json from the scene
and room files in models/<GAME>/, every setup (scene layer) included.

The JSON is keyed by scene file name; each value is a list indexed by setup:
setup 0 is the scene's main header, setup N its alternate header N-1 (None
where that entry of the alternate header list is null, as in the game, which
then carries on with the main header). Each setup:
  rooms[i].actors    the actor list room i loads in that setup: the room's own
                     alternate header N-1, or its main header when it has no
                     such entry (Scene_CommandAlternateHeaderList)
  rooms[i].objects   that header's object list
  transitionActors   the scene header's transition actor list
  specialObject      the scene header's special files command's keep object

The format is the one heapsims' ROM-based actor_scene_data_generator wrote,
which only read setup 0 for MM (and setups 0-3 for OoT). This reads the same
data from the files the viewer already has, and gives MM its alternate setups.

    python tools/actors/generate_actors_by_scene.py MM [--check]

--check compares with the existing JSON instead of writing it: OoT setups 0-3
and MM setup 0 come out identical to the ROM generator's.
"""
import json
import os
import struct
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")

CMD_ACTOR_LIST = 0x01
CMD_ROOM_LIST = 0x04
CMD_SPECIAL_FILES = 0x07
CMD_TRANSITION_ACTORS = 0x0E
CMD_OBJECT_LIST = 0x0B
CMD_END = 0x14
CMD_ALTERNATE_HEADERS = 0x18


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def commands(b, off):
    """The header's commands at `off`, as {cmd: (data1, data2)} (first of each)."""
    out = {}
    for _ in range(64):
        if off + 8 > len(b):
            break
        cmd, d1 = b[off], b[off + 1]
        d2 = u32(b, off + 4)
        if cmd == CMD_END:
            break
        out.setdefault(cmd, (d1, d2))
        off += 8
    return out


def seg_off(addr, seg, size):
    if addr >> 24 != seg or (addr & 0xFFFFFF) >= size:
        return None
    return addr & 0xFFFFFF


def alternate_headers(b, cmds, seg):
    """The alternate header list: offsets, None for null entries. Its length
    isn't stored: entries run until one that is neither null nor a pointer
    into this file, or the list reaches the first header it points to.
    Trailing nulls are kept (OoT's lists are always 3 long)."""
    if CMD_ALTERNATE_HEADERS not in cmds:
        return []
    start = seg_off(cmds[CMD_ALTERNATE_HEADERS][1], seg, len(b))
    if start is None:
        return []
    out = []
    end = len(b)
    o = start
    while o + 4 <= min(end, len(b)):
        v = u32(b, o)
        if v == 0:
            out.append(None)
        else:
            p = seg_off(v, seg, len(b))
            if p is None or p % 8 or b[p] > 0x20:
                break
            out.append(p)
            end = min(end, p)
        o += 4
    # (trailing nulls - the list's own, or padding before the first header -
    # are setups the game skips: None)
    return out


def object_list(b, cmds, seg):
    if CMD_OBJECT_LIST not in cmds:
        return []
    n, addr = cmds[CMD_OBJECT_LIST]
    o = seg_off(addr, seg, len(b))
    if o is None:
        return []
    return [struct.unpack_from(">H", b, o + 2 * i)[0] for i in range(n) if o + 2 * i + 2 <= len(b)]


def actor_list(b, cmds, seg, game):
    if CMD_ACTOR_LIST not in cmds:
        return []
    n, addr = cmds[CMD_ACTOR_LIST]
    o = seg_off(addr, seg, len(b))
    if o is None:
        return []
    out = []
    for i in range(n):
        e = o + i * 16
        if e + 16 > len(b):
            break
        aid, x, y, z, rx, ry, rz, params = struct.unpack_from(">HhhhHHHH", b, e)
        # (MM: the id without its flag bits, like the ROM generator this
        # replaces wrote; render_actors.js has the flags per actor)
        if game == "MM":
            aid &= 0xFFF
        out.append({"actorId": aid, "actorParams": params, "position": [x, y, z], "rotation": [rx, ry, rz]})
    return out


def transition_actors(b, cmds):
    if CMD_TRANSITION_ACTORS not in cmds:
        return []
    n, addr = cmds[CMD_TRANSITION_ACTORS]
    o = seg_off(addr, 2, len(b))
    if o is None:
        return []
    out = []
    for i in range(n):
        e = o + i * 16
        if e + 16 > len(b):
            break
        front, _fc, back, _bc, aid = struct.unpack_from(">BBBBH", b, e)
        params = struct.unpack_from(">H", b, e + 14)[0]
        out.append({"frontRoom": front, "backRoom": back, "actorId": aid, "actorParams": params})
    return out


def room_file(game, scene, i):
    if game == "MM":
        return f"{scene}_room_{i:02d}"
    return f"{scene[:-6] if scene.endswith('_scene') else scene}_room_{i}"


def read(path):
    with open(path, "rb") as f:
        return f.read()


def scene_setups(game, scene):
    models = os.path.join(ROOT, "models", game)
    sb = read(os.path.join(models, scene))
    main = commands(sb, 0)
    alts = alternate_headers(sb, main, 2)
    num_rooms = main.get(CMD_ROOM_LIST, (0, 0))[0]
    rooms = []
    for i in range(num_rooms):
        p = os.path.join(models, room_file(game, scene, i))
        if not os.path.exists(p):
            rooms.append(None)
            continue
        rb = read(p)
        rmain = commands(rb, 0)
        rooms.append((rb, rmain, alternate_headers(rb, rmain, 3)))

    setups = []
    for s in range(len(alts) + 1):
        if s > 0 and alts[s - 1] is None:
            setups.append(None)
            continue
        cmds = main if s == 0 else commands(sb, alts[s - 1])
        room_actors = []
        for r in rooms:
            if r is None:
                room_actors.append({"actors": [], "objects": []})
                continue
            rb, rmain, ralts = r
            rc = rmain
            if s > 0 and s - 1 < len(ralts) and ralts[s - 1] is not None:
                rc = commands(rb, ralts[s - 1])
            room_actors.append({"actors": actor_list(rb, rc, 3, game), "objects": object_list(rb, rc, 3)})
        special = cmds.get(CMD_SPECIAL_FILES, main.get(CMD_SPECIAL_FILES, (0, 0)))[1]
        setups.append({
            "rooms": room_actors,
            "transitionActors": transition_actors(sb, cmds if CMD_TRANSITION_ACTORS in cmds else main),
            "specialObject": special,
        })
    # OoT: at least setups 0-3 (child/adult day/night), null where the scene
    # has none, like the ROM generator's
    if game == "OOT":
        setups += [None] * (4 - len(setups))
    return setups


def main():
    game = sys.argv[1].upper()
    check = "--check" in sys.argv
    path = os.path.join(ROOT, "models", game, "actors", f"{game}_actors_by_scene.json")
    old = json.load(open(path))
    new = {}
    same = diff = 0
    for scene, value in old.items():
        if not scene or not os.path.exists(os.path.join(ROOT, "models", game, scene)):
            new[scene] = value
            continue
        new[scene] = scene_setups(game, scene)
        if check:
            n = len(new[scene])
            if new[scene] == value:
                same += 1
            else:
                diff += 1
                for s in range(max(n, len(value))):
                    a = new[scene][s] if s < n else "missing"
                    b = value[s] if s < len(value) else "missing"
                    if a != b:
                        what = "rooms" if isinstance(a, dict) and isinstance(b, dict) and a["rooms"] != b["rooms"] else \
                            "transitions" if isinstance(a, dict) and isinstance(b, dict) and a["transitionActors"] != b["transitionActors"] else \
                            "special" if isinstance(a, dict) and isinstance(b, dict) else f"{type(a).__name__} vs {type(b).__name__}"
                        print(f"{scene} setup {s}: {what}")
    if check:
        print(f"{same} scenes the same, {diff} different")
        return
    with open(path, "w") as f:
        json.dump(new, f, indent="\t")  # (the original's layout)
    counts = [sum(1 for s in v if s) for k, v in new.items() if k]
    print(f"wrote {path}: {len(counts)} scenes, {sum(counts)} setups")


if __name__ == "__main__":
    main()

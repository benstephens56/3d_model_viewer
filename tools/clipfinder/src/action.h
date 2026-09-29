// clipfinder: action clips - a sword lunge's own movement doing the clip.
#pragma once

#include "search.h"

// A melee attack's lunge moves Link by its animation's root motion
// (z_player.c func_80837948: Player_StartAnimMovement with
// ANIM_FLAG_UPDATE_XZ | ANIM_FLAG_ENABLE_MOVEMENT, speedXZ zeroed). That's
// added to world.pos by AnimTask_ActorMovement after Player's update, so
// after the frame's bg check, and the next frame's prevPos is home.pos from
// before it (Player_UpdateCommon: prevPos = home.pos). The next frame's bg
// check then sweeps prevPos -> world.pos with the root motion in it, exactly
// like a walking frame's move (velocity.y is still Link's ground -4 -> -5, so
// posNext is GROUND_DROP below). OoT and MM are the same here.
//
// The lunge (the stick held forward: the stab Z-targeting, else the forward
// slash; a Deku stick always does the forward slash) also sets speedXZ 15 on
// the attack's first action frame, which Math_StepToF(speed, 0, 5) takes to
// 10 at once: UpdatePos moves Link 15 forward the next frame, then 7.5.
//
// Each game frame of an action, exactly as the game does it (bit-exact):
// the root motion the movement task added at the end of the previous frame
// (SkelAnime_UpdateTranslation: the animation's root translation `j`, minus
// prevTransl `p`, rotated by the facing; x 0.01 scale, MM x the form's
// unk_08), then speedXZ `speed` along the facing (Actor_UpdatePos, x 1.5).
// OoT rotates j and p separately and subtracts, MM subtracts first. The
// table comes from the decomps' animation data (link_animetion): see
// action.cpp.
// swing: the sword swing is active at this frame's collision (meleeWeaponState
// != 0): if Link leaves the ground, func_8083AA10 (MM func_8083827C) puts him
// back at prevPos and zeroes speedXZ - no lunging off a ledge.
struct ActionFrame { int jx, jz, px, pz; double speed; bool swing; };
struct Action {
	string key;       // --actions name, e.g. "1h-slash"
	string name;      // e.g. "lunge 1h slash"
	string game;      // "OOT" / "MM"
	vector<string> forms;  // upper case, the forms it's for
	vector<ActionFrame> frames;
};
extern const vector<Action> ACTIONS;

// Where frame f of action a moves Link from pos (posNext's x / z; y is
// pos.y - GROUND_DROP), facing `facing`.
// noSpeed: speedXZ was zeroed (a swing frame put Link back): root motion only.
V3 actionStep(const Action& a, const ActionFrame& f, const V3& pos, int facing, bool noSpeed = false);
// Frame f's move as a speed (move / 1.5) and angle from the facing, for
// display; the angle is the s16 yaw at facing 0.
void actionFrameMove(const Action& a, const ActionFrame& f, double& speed, int& angle);

// The actions of `game` named in `list` ("all", or keys separated by commas).
// err: an unknown name.
vector<int> parseActions(const string& game, const string& list, string& err);
// Whether action a is done by the form (upper case: "ADULT", "HUMAN", ...)
bool actionForForm(const Action& a, const string& formUpper);

// Link standing still at `start` facing `facing` does action a: its frames
// (line test, pushes, floor check each), then two frames standing still. A
// clip (kind 0 / 1: a wall push, still to be categorised; 2: the floor check
// lifted him behind the wall, as slope.h), or none.
std::optional<Clip> actionClip(const Model& m, Scratch& s, const V3& start, int facing, int action);

// Every action in `actions` aimed at every walking and slope clip point of
// `targets` (the ordinary scan's): from many facings, the start the lunge
// takes to that point's posNext (for each of its frames), where Link rests
// there. At most one clip per target point and action. Wall pairs get one
// category per action, as the scan's.
vector<Clip> actionScan(const Model& m, const vector<Clip>& targets, const vector<int>& actions, int threads);

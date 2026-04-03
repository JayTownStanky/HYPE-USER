// Struct-relative offsets — stable across patches. Global/base offsets are in config.json.
#pragma once
#include <cstdint>

// ============================================================
// Health / Shield
// ============================================================
constexpr uint64_t OFF_XP               = 0x3824;
constexpr uint64_t OFF_HEALTH           = 0x0324;
constexpr uint64_t OFF_MAX_HEALTH       = 0x0468;
constexpr uint64_t OFF_SHIELD           = 0x01a0;
constexpr uint64_t OFF_MAX_SHIELD       = 0x01a4;
constexpr uint64_t OFF_ARMOR_TYPE       = 0x4814;

// ============================================================
// Skeleton / Animation
// ============================================================
constexpr uint64_t OFF_CAMERA_ORIGIN    = 0x1fc4;
constexpr uint64_t OFF_CUSTOM_SPIRIT    = 0x15d4;
constexpr uint64_t OFF_STUDIO_HDR       = 0x1000;
constexpr uint64_t OFF_BONES            = 0x0E00;

// ============================================================
// Position / Movement
// ============================================================
constexpr uint64_t OFF_LOCAL_ORIGIN     = 0x0004;
constexpr uint64_t OFF_ABS_ORIGIN       = 0x017c;
constexpr uint64_t OFF_ENTITY_ROTATION  = 0x0164;
constexpr uint64_t OFF_ABS_VELOCITY     = 0x0170;
constexpr uint64_t OFF_COLLISION        = 0x03B0;
constexpr uint64_t OFF_COLLISION_MINS   = 0x0010;
constexpr uint64_t OFF_COLLISION_MAXS   = 0x001C;
constexpr uint64_t OFF_PROP_SCRIPT_NETWORK_FLAG = 0x02d4;

// ============================================================
// Player State
// ============================================================
constexpr uint64_t OFF_ZOOMING          = 0x1cb1;
constexpr uint64_t OFF_TEAM_NUMBER      = 0x0334;
constexpr uint64_t OFF_TEAM_SQUAD_ID    = 0x0340;
constexpr uint64_t OFF_NAME             = 0x0479;
constexpr uint64_t OFF_NAME_INDEX       = 0x0038;
constexpr uint64_t OFF_LIFE_STATE       = 0x0690;
constexpr uint64_t OFF_BLEEDOUT_STATE   = 0x27c0;
constexpr uint64_t OFF_LAST_VISIBLE_TIME  = 0x1a64;
constexpr uint64_t OFF_LAST_AIMED_AT_TIME = 0x1a68;
constexpr uint64_t OFF_FLAGS            = 0x00c8;
constexpr uint64_t OFF_MOVE_SPEED_SCALE = 0x3338;
constexpr uint64_t OFF_DEATH_TIME       = 0x36d8;

// ============================================================
// View Angles
// ============================================================
constexpr uint64_t OFF_VIEW_ANGLES      = 0x2600;
constexpr uint64_t OFF_BREATH_ANGLES    = 0x25F0;
constexpr uint64_t OFF_PUNCH_ANGLES     = 0x2518;
constexpr uint64_t OFF_YAW              = 0x2314;

// ============================================================
// Weapons
// ============================================================
constexpr uint64_t OFF_WEAPON_HANDLE    = 0x19c4;
constexpr uint64_t OFF_WEAPON_INDEX     = 0x1818;
constexpr uint64_t OFF_PROJECTILE_SPEED = 0x1FCC;
constexpr uint64_t OFF_PROJECTILE_SCALE = 0x1FD4;
constexpr uint64_t OFF_OFFHAND_WEAPON   = 0x19d4;
constexpr uint64_t OFF_CURRENT_ZOOM_FOV = 0x1720;
constexpr uint64_t OFF_TARGET_ZOOM_FOV  = 0x1724;
constexpr uint64_t OFF_WEAPON_AMMO      = 0x1610;
constexpr uint64_t OFF_RELOADING        = 0x162a;
constexpr uint64_t OFF_ACTIVE_WEAPON    = 0x1A1C;
constexpr uint64_t OFF_WEAPON_BITFIELD  = 0x17b8;
constexpr uint64_t OFF_LAST_FIRED_TIME  = 0x1930;
constexpr uint64_t OFF_NEXT_PRIMARY_ATTACK = 0x15ec;
constexpr uint64_t OFF_NEXT_READY_TIME  = 0x15e8;

// ============================================================
// Glow (entity-relative)
// ============================================================
constexpr uint64_t OFF_GLOW_ENABLE            = 0x028C;
constexpr uint64_t OFF_GLOW_THROUGH_WALL      = 0x026C;
constexpr uint64_t OFF_GLOW_FIX               = 0x0278;
constexpr uint64_t OFF_GLOW_HIGHLIGHT_ID      = 0x029C;
constexpr uint64_t OFF_GLOW_HIGHLIGHT_TYPE_SIZE = 0x0034;
constexpr uint64_t OFF_GLOW_DISTANCE          = 0x0264;
constexpr uint64_t OFF_ITEM_GLOW              = 0x02f0;
constexpr uint64_t OFF_GLOW_T1                = 0x0292;
constexpr uint64_t OFF_GLOW_T2                = 0x030c;

// ============================================================
// Identity
// ============================================================
constexpr uint64_t OFF_MODEL_NAME             = 0x0030;
constexpr uint64_t OFF_SIGNIFIER_NAME         = 0x0470;
constexpr uint64_t OFF_UID                    = 0x2708;
constexpr uint64_t OFF_NUCLEUS_ID             = 0x2710;
constexpr uint64_t OFF_VIEW_MODEL             = 0x2f60;

// ============================================================
// Traversal
// ============================================================
constexpr uint64_t OFF_TIME_BASE              = 0x2168;
constexpr uint64_t OFF_TRAVERSAL_START_TIME   = 0x2d3c;
constexpr uint64_t OFF_TRAVERSAL_PROGRESS     = 0x2d34;
constexpr uint64_t OFF_WALL_RUN_START_TIME    = 0x370c;
constexpr uint64_t OFF_WALL_RUN_CLEAR_TIME    = 0x3710;

// ============================================================
// Movement
// ============================================================
constexpr uint64_t OFF_SKYDIVE_STATE          = 0x49d4;
constexpr uint64_t OFF_DUCK_STATE             = 0x2c24;

// ============================================================
// Grapple
// ============================================================
constexpr uint64_t OFF_GRAPPLE                = 0x2e88;
constexpr uint64_t OFF_GRAPPLE_ACTIVE         = 0x2f10;
constexpr uint64_t OFF_GRAPPLE_ATTACHED       = 0x0048;

// ============================================================
// Network (entity-relative)
// ============================================================
constexpr uint64_t OFF_NETVAR_GLOBAL          = 0x4968;
constexpr uint64_t OFF_NETVAR_RANGES          = 0x0b72;
constexpr uint64_t OFF_NETVAR_INT32S          = 0x0bc0;

// ============================================================
// Observer (entity-relative)
// ============================================================
constexpr uint64_t OFF_OBSERVER_ARRAY         = 0x0954;

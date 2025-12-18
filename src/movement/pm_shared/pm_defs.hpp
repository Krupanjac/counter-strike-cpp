#pragma once

/**
 * @file pm_defs.hpp
 * @brief Player Movement Definitions and Constants
 * 
 * Contains all the constants, flags, and structures used by the movement system.
 * These values are ported from the GoldSrc SDK to ensure identical behavior.
 */

#include "core/types.hpp"
#include "core/math/math.hpp"

namespace cscpp::movement {

// ============================================================================
// Player Flags (FL_*)
// ============================================================================

enum PlayerFlags : i32 {
    FL_ONGROUND     = (1 << 0),   ///< At rest / on ground
    FL_DUCKING      = (1 << 1),   ///< Player is ducked
    FL_WATERJUMP    = (1 << 2),   ///< Player jumping out of water
    FL_ONTRAIN      = (1 << 3),   ///< Player is on a func_train
    FL_INRAIN       = (1 << 4),   ///< Player is in rain zone
    FL_FROZEN       = (1 << 5),   ///< Player is frozen for look around
    FL_ATCONTROLS   = (1 << 6),   ///< Player is controlling a func_tank
    FL_CLIENT       = (1 << 7),   ///< Is a player
    FL_FAKECLIENT   = (1 << 8),   ///< Fake client (bot)
    FL_INWATER      = (1 << 9),   ///< Player is in water
};

// ============================================================================
// Input Buttons (IN_*)
// ============================================================================

enum InputButtons : u16 {
    IN_ATTACK       = (1 << 0),   ///< Primary attack
    IN_JUMP         = (1 << 1),   ///< Jump
    IN_DUCK         = (1 << 2),   ///< Duck/crouch
    IN_FORWARD      = (1 << 3),   ///< Move forward
    IN_BACK         = (1 << 4),   ///< Move backward
    IN_USE          = (1 << 5),   ///< Use/interact
    IN_MOVELEFT     = (1 << 6),   ///< Strafe left
    IN_MOVERIGHT    = (1 << 7),   ///< Strafe right
    IN_ATTACK2      = (1 << 8),   ///< Secondary attack
    IN_RELOAD       = (1 << 9),   ///< Reload weapon
    IN_SPEED        = (1 << 10),  ///< Walk modifier
    IN_SCORE        = (1 << 11),  ///< Show scoreboard
};

// ============================================================================
// Hull Types
// ============================================================================

enum HullType : i32 {
    HULL_STANDING = 0,    ///< Standing player hull
    HULL_DUCKED = 1,      ///< Ducked player hull
    HULL_POINT = 2,       ///< Point hull (for traces)
    HULL_LARGE = 3,       ///< Large hull (monsters)
};

// ============================================================================
// Water Levels
// ============================================================================

enum WaterLevel : i32 {
    WL_NOT_IN_WATER = 0,  ///< Not in water
    WL_FEET = 1,          ///< Feet in water
    WL_WAIST = 2,         ///< Waist deep
    WL_HEAD = 3,          ///< Head under water
};

// ============================================================================
// Contents (collision types)
// ============================================================================

enum Contents : i32 {
    CONTENTS_EMPTY = -1,
    CONTENTS_SOLID = -2,
    CONTENTS_WATER = -3,
    CONTENTS_SLIME = -4,
    CONTENTS_LAVA = -5,
    CONTENTS_SKY = -6,
    CONTENTS_LADDER = -16,
};

// ============================================================================
// Movement Variables
// ============================================================================

/**
 * @brief Server-controlled movement variables
 * 
 * These values define the feel of player movement. Changing them
 * will significantly affect gameplay (e.g., sv_airaccelerate controls
 * how easy bunnyhopping is).
 */
struct MoveVars {
    f32 gravity         = 800.0f;   ///< sv_gravity
    f32 stopSpeed       = 100.0f;   ///< sv_stopspeed
    f32 maxSpeed        = 320.0f;   ///< sv_maxspeed
    f32 spectatorMaxSpeed = 500.0f; ///< sv_spectatormaxspeed
    f32 accelerate      = 10.0f;    ///< sv_accelerate
    f32 airAccelerate   = 10.0f;    ///< sv_airaccelerate (100 for classic bhop)
    f32 waterAccelerate = 10.0f;    ///< sv_wateraccelerate
    f32 friction        = 4.0f;     ///< sv_friction
    f32 edgeFriction    = 2.0f;     ///< sv_edgefriction
    f32 waterFriction   = 1.0f;     ///< sv_waterfriction
    f32 entGravity      = 1.0f;     ///< Entity gravity modifier
    f32 bounce          = 1.0f;     ///< sv_bounce
    f32 stepSize        = 18.0f;    ///< sv_stepsize
    f32 maxVelocity     = 2000.0f;  ///< sv_maxvelocity
    f32 zMax            = 4096.0f;  ///< Maximum height
    f32 waveHeight      = 0.0f;     ///< Water wave height
    bool footsteps      = true;     ///< Enable footstep sounds
    f32 rollAngle       = 0.0f;     ///< View roll angle
    f32 rollSpeed       = 0.0f;     ///< View roll speed
    
    // CS 1.6 specific
    f32 jumpSpeed       = 268.3281572999748f;  ///< Jump velocity (sqrt(2 * 800 * 45))
    f32 airSpeedCap     = 30.0f;    ///< Max air speed for strafe input
};

// ============================================================================
// Trace Result
// ============================================================================

/**
 * @brief Result of a collision trace
 */
struct TraceResult {
    bool allSolid = false;     ///< Trace started and ended in solid
    bool startSolid = false;   ///< Trace started in solid
    bool inOpen = false;       ///< Trace ended in open (non-solid)
    bool inWater = false;      ///< Trace ended in water
    
    f32 fraction = 1.0f;       ///< 0-1, how far the trace went before hitting
    Vec3 endPos{0.0f};         ///< Final position of trace
    
    struct {
        Vec3 normal{0.0f};     ///< Surface normal at hit point
        f32 dist = 0.0f;       ///< Distance to plane
    } plane;
    
    i32 entity = -1;           ///< Entity hit (-1 for world)
    i32 hitgroup = 0;          ///< Hit group (for damage calculation)
};

// ============================================================================
// Default Hull Sizes
// ============================================================================

namespace hull {

// Standing player (32x32x72)
constexpr Vec3 STANDING_MINS{-16.0f, -16.0f, -36.0f};
constexpr Vec3 STANDING_MAXS{ 16.0f,  16.0f,  36.0f};

// Ducked player (32x32x36)
constexpr Vec3 DUCKED_MINS{-16.0f, -16.0f, -18.0f};
constexpr Vec3 DUCKED_MAXS{ 16.0f,  16.0f,  18.0f};

// Point hull (for traces)
constexpr Vec3 POINT_MINS{0.0f, 0.0f, 0.0f};
constexpr Vec3 POINT_MAXS{0.0f, 0.0f, 0.0f};

// View height when standing
constexpr f32 STANDING_VIEW_HEIGHT = 28.0f;

// View height when ducked
constexpr f32 DUCKED_VIEW_HEIGHT = 12.0f;

// Duck transition time
constexpr f32 DUCK_TIME = 0.4f;

} // namespace hull

// ============================================================================
// Movement Constants
// ============================================================================

namespace pmove {

// Maximum number of clip planes for FlyMove
constexpr i32 MAX_CLIP_PLANES = 5;

// Number of iterations for FlyMove
constexpr i32 MAX_BUMPS = 4;

// Ground check distance
constexpr f32 GROUND_CHECK_DIST = 2.0f;

// Minimum speed before considered stopped
constexpr f32 STOP_EPSILON = 0.1f;

// Slope angle limit (above this is a wall, not floor)
constexpr f32 MAX_FLOOR_NORMAL = 0.7f;

// Fall damage thresholds
constexpr f32 FALL_PUNCH_THRESHOLD = 350.0f;
constexpr f32 FALL_DAMAGE_THRESHOLD = 580.0f;

// Dead player velocity
constexpr f32 DEAD_MAXSPEED = 1.0f;

// Ladder speeds
constexpr f32 LADDER_SPEED = 200.0f;

// Water depths for different water levels
constexpr f32 WATER_DEPTH_FEET = 1.0f;
constexpr f32 WATER_DEPTH_WAIST = 0.5f;

} // namespace pmove

// ============================================================================
// Utility Macros
// ============================================================================

/// Check if a button is pressed this frame but wasn't last frame
inline bool PM_ButtonPressed(u16 buttons, u16 oldButtons, InputButtons button) {
    return (buttons & button) && !(oldButtons & button);
}

/// Check if a button is held
inline bool PM_ButtonHeld(u16 buttons, InputButtons button) {
    return (buttons & button) != 0;
}

/// Check if on ground
inline bool PM_OnGround(i32 flags) {
    return (flags & FL_ONGROUND) != 0;
}

/// Check if ducking
inline bool PM_Ducking(i32 flags) {
    return (flags & FL_DUCKING) != 0;
}

} // namespace cscpp::movement


#pragma once

/**
 * @file pm_shared.hpp
 * @brief GoldSrc Player Movement Port
 * 
 * This is a faithful port of Valve's pm_shared.c from the Half-Life SDK.
 * The original code can be found at:
 * https://github.com/ValveSoftware/halflife/blob/master/pm_shared/pm_shared.c
 * 
 * The movement code is critical for authentic Counter-Strike gameplay feel,
 * including bunnyhopping, air strafing, and other movement mechanics.
 * 
 * Key concepts:
 * - Wish velocity: The direction and speed the player wants to move
 * - Acceleration: How quickly velocity changes toward wish velocity  
 * - Friction: Slows the player when on ground
 * - Air acceleration: Enables strafe jumping and bunnyhopping
 */

#include "core/types.hpp"
#include "core/math/math.hpp"
#include "movement/pm_shared/pm_defs.hpp"

namespace cscpp::movement {

// ============================================================================
// Forward Declarations
// ============================================================================

struct PlayerMove;
struct TraceResult;

// ============================================================================
// Trace Function Type
// ============================================================================

/// Function signature for collision traces
using TraceFunc = TraceResult(*)(
    const PlayerMove* pm,
    Vec3 start,
    Vec3 end,
    i32 hullType
);

// ============================================================================
// Player Movement Structure
// ============================================================================

/**
 * @brief Main player movement state
 * 
 * This structure contains all the state needed for player movement simulation.
 * It's passed to PM_PlayerMove and modified in place.
 * 
 * This is equivalent to the playermove_t structure in GoldSrc.
 */
struct PlayerMove {
    // ========================================================================
    // Position & Velocity
    // ========================================================================
    
    Vec3 origin{0.0f};          ///< Player position
    Vec3 velocity{0.0f};        ///< Player velocity
    Vec3 baseVelocity{0.0f};    ///< Push velocity from conveyor belts, etc.
    Vec3 viewAngles{0.0f};      ///< View angles (pitch, yaw, roll)
    Vec3 punchAngle{0.0f};      ///< Weapon recoil/punch angle
    
    // ========================================================================
    // Movement Input
    // ========================================================================
    
    f32 forwardMove = 0.0f;     ///< Forward/back input (-1 to 1)
    f32 sideMove = 0.0f;        ///< Left/right strafe input (-1 to 1)
    f32 upMove = 0.0f;          ///< Up/down input (ladder, swim)
    
    u16 buttons = 0;            ///< Current button state (IN_JUMP, IN_DUCK, etc.)
    u16 oldButtons = 0;         ///< Previous frame button state
    
    // ========================================================================
    // Timing
    // ========================================================================
    
    f32 frameTime = 0.0f;       ///< Time for this movement tick
    f32 time = 0.0f;            ///< Total game time
    
    // ========================================================================
    // State Flags
    // ========================================================================
    
    i32 flags = 0;              ///< FL_ONGROUND, FL_DUCKING, etc.
    i32 oldFlags = 0;           ///< Previous frame flags
    
    i32 onGround = -1;          ///< Ground entity index (-1 if in air)
    i32 waterLevel = 0;         ///< 0-3 water immersion level
    i32 waterType = 0;          ///< Water content type
    
    bool dead = false;          ///< Is player dead
    
    // ========================================================================
    // Hull & Collision
    // ========================================================================
    
    i32 useHull = 0;            ///< 0=standing, 1=ducked, 2=point
    
    Vec3 playerMins[4];         ///< Hull mins for each hull type
    Vec3 playerMaxs[4];         ///< Hull maxs for each hull type
    
    // ========================================================================
    // Duck State
    // ========================================================================
    
    f32 duckTime = 0.0f;        ///< Time spent ducking/unducking
    bool inDuck = false;        ///< Currently in ducking transition
    i32 timeStepSound = 0;      ///< Next footstep sound time
    i32 stepLeft = 0;           ///< Left foot for footsteps
    
    // ========================================================================
    // Fall Damage
    // ========================================================================
    
    f32 fallVelocity = 0.0f;    ///< Velocity when player started falling
    
    // ========================================================================
    // Ladder State
    // ========================================================================
    
    Vec3 ladderNormal{0.0f};    ///< Normal of ladder player is on
    bool onLadder = false;      ///< Currently on a ladder
    
    // ========================================================================
    // Movement Variables
    // ========================================================================
    
    const MoveVars* moveVars = nullptr;  ///< Shared movement variables
    
    // ========================================================================
    // Collision Interface
    // ========================================================================
    
    TraceFunc traceFunc = nullptr;       ///< Trace function for collision
    void* traceUserData = nullptr;       ///< User data for trace function
    
    // ========================================================================
    // Player Info
    // ========================================================================
    
    i32 playerIndex = 0;        ///< Player entity index
    f32 maxSpeed = 320.0f;      ///< Max movement speed
    f32 clientMaxSpeed = 320.0f;///< Client-set max speed
    
    // ========================================================================
    // Output (set by movement code)
    // ========================================================================
    
    Vec3 forward{0.0f};         ///< Calculated forward vector
    Vec3 right{0.0f};           ///< Calculated right vector
    Vec3 up{0.0f};              ///< Calculated up vector
    
    // ========================================================================
    // Methods
    // ========================================================================
    
    /// Initialize hull sizes to default values
    void initHulls();
    
    /// Get mins for current hull
    Vec3 getMins() const { return playerMins[useHull]; }
    
    /// Get maxs for current hull
    Vec3 getMaxs() const { return playerMaxs[useHull]; }
};

// ============================================================================
// Main Movement Functions
// ============================================================================

/**
 * @brief Main player movement entry point
 * 
 * Call this function each tick to simulate player movement.
 * The PlayerMove structure is modified in place with the new
 * position, velocity, and state.
 * 
 * @param pm Player movement state (modified in place)
 */
void PM_PlayerMove(PlayerMove* pm);

// ============================================================================
// Movement Mode Functions
// ============================================================================

/**
 * @brief Ground movement (walking, running)
 * 
 * Handles movement when the player is on the ground.
 * Applies friction and acceleration.
 */
void PM_WalkMove(PlayerMove* pm);

/**
 * @brief Air movement (jumping, falling)
 * 
 * Handles movement when the player is in the air.
 * This is where the air acceleration math enables bunnyhopping.
 */
void PM_AirMove(PlayerMove* pm);

/**
 * @brief Ladder movement
 * 
 * Handles movement when the player is on a ladder.
 */
void PM_LadderMove(PlayerMove* pm);

/**
 * @brief Water movement (swimming)
 * 
 * Handles movement when the player is in water.
 */
void PM_WaterMove(PlayerMove* pm);

// ============================================================================
// Physics Functions
// ============================================================================

/**
 * @brief Apply acceleration toward wish velocity
 * 
 * This is THE critical function for movement feel.
 * The math here is what enables strafe jumping and bunnyhopping.
 * 
 * @param pm Player movement state
 * @param wishDir Desired movement direction (normalized)
 * @param wishSpeed Desired movement speed
 * @param accel Acceleration rate
 */
void PM_Accelerate(PlayerMove* pm, Vec3 wishDir, f32 wishSpeed, f32 accel);

/**
 * @brief Apply air acceleration
 * 
 * Similar to PM_Accelerate but with different clamping for air movement.
 */
void PM_AirAccelerate(PlayerMove* pm, Vec3 wishDir, f32 wishSpeed, f32 accel);

/**
 * @brief Apply ground friction
 * 
 * Slows the player when on ground.
 */
void PM_Friction(PlayerMove* pm);

/**
 * @brief Add gravity to velocity
 */
void PM_AddGravity(PlayerMove* pm);

/**
 * @brief Move and slide along obstacles
 * 
 * Attempts to move along velocity, sliding along any surfaces hit.
 * 
 * @return Bitfield of blocked directions
 */
i32 PM_FlyMove(PlayerMove* pm);

/**
 * @brief Try to step up stairs
 */
void PM_StepMove(PlayerMove* pm, Vec3 dest, TraceResult& trace);

// ============================================================================
// State Functions
// ============================================================================

/**
 * @brief Check and categorize player's position
 * 
 * Determines if player is on ground, in water, etc.
 */
void PM_CategorizePosition(PlayerMove* pm);

/**
 * @brief Handle jump
 */
void PM_Jump(PlayerMove* pm);

/**
 * @brief Handle duck/crouch
 */
void PM_Duck(PlayerMove* pm);

/**
 * @brief Check for fall damage
 */
void PM_CheckFalling(PlayerMove* pm);

/**
 * @brief Clamp velocity to max values
 */
void PM_CheckVelocity(PlayerMove* pm);

/**
 * @brief Check if player is on a ladder
 */
bool PM_CheckLadder(PlayerMove* pm);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Perform a player trace
 * 
 * @param pm Player movement state
 * @param start Start position
 * @param end End position
 * @return Trace result
 */
TraceResult PM_PlayerTrace(const PlayerMove* pm, Vec3 start, Vec3 end);

/**
 * @brief Clip velocity to a surface
 * 
 * @param in Input velocity
 * @param normal Surface normal
 * @param out Output velocity
 * @param overbounce Bounce factor (usually 1.0)
 */
void PM_ClipVelocity(Vec3 in, Vec3 normal, Vec3& out, f32 overbounce);

/**
 * @brief Play a step sound
 */
void PM_PlayStepSound(PlayerMove* pm, i32 step, f32 volume);

/**
 * @brief Calculate view angles to direction vectors
 */
void PM_AngleVectors(const PlayerMove* pm);

} // namespace cscpp::movement


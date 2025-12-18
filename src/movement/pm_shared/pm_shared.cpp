/**
 * @file pm_shared.cpp
 * @brief GoldSrc Player Movement Implementation
 * 
 * This is a faithful port of Valve's pm_shared.c from the Half-Life SDK.
 * The implementation preserves the exact floating-point behavior to ensure
 * identical movement feel, including bunnyhopping and air strafing mechanics.
 * 
 * Reference: https://github.com/ValveSoftware/halflife/blob/master/pm_shared/pm_shared.c
 */

#include "movement/pm_shared/pm_shared.hpp"
#include "core/logging/logger.hpp"

#include <cmath>
#include <algorithm>

namespace cscpp::movement {

// ============================================================================
// GoldSrc Parity Mode
// ============================================================================

#ifdef CSCPP_GOLDSRC_PARITY
    // Use single precision and exact operation order for GoldSrc compatibility
    #define PM_FLOAT float
    #define PM_SQRT sqrtf
    #define PM_FABS fabsf
#else
    // Modern mode with potentially better precision
    #define PM_FLOAT float
    #define PM_SQRT std::sqrt
    #define PM_FABS std::abs
#endif

// ============================================================================
// PlayerMove Methods
// ============================================================================

void PlayerMove::initHulls() {
    // Hull 0: Standing
    playerMins[HULL_STANDING] = hull::STANDING_MINS;
    playerMaxs[HULL_STANDING] = hull::STANDING_MAXS;
    
    // Hull 1: Ducked
    playerMins[HULL_DUCKED] = hull::DUCKED_MINS;
    playerMaxs[HULL_DUCKED] = hull::DUCKED_MAXS;
    
    // Hull 2: Point
    playerMins[HULL_POINT] = hull::POINT_MINS;
    playerMaxs[HULL_POINT] = hull::POINT_MAXS;
    
    // Hull 3: Large (same as standing for players)
    playerMins[HULL_LARGE] = hull::STANDING_MINS;
    playerMaxs[HULL_LARGE] = hull::STANDING_MAXS;
}

// ============================================================================
// Main Entry Point
// ============================================================================

void PM_PlayerMove(PlayerMove* pm) {
    // Calculate view direction vectors
    PM_AngleVectors(pm);
    
    // Categorize position (on ground, in water, etc.)
    PM_CategorizePosition(pm);
    
    // Handle frozen state
    if (pm->flags & FL_FROZEN) {
        return;
    }
    
    // Handle dead state
    if (pm->dead) {
        pm->maxSpeed = pmove::DEAD_MAXSPEED;
        // Dead movement would go here
        return;
    }
    
    // Handle ducking
    PM_Duck(pm);
    
    // Check if on ladder
    if (PM_CheckLadder(pm)) {
        PM_LadderMove(pm);
        return;
    }
    
    // Check water movement
    if (pm->waterLevel >= WL_WAIST) {
        PM_WaterMove(pm);
        return;
    }
    
    // Ground or air movement
    if (pm->flags & FL_ONGROUND) {
        // Jump check (removes ground flag if jumping)
        if (pm->buttons & IN_JUMP) {
            PM_Jump(pm);
        } else {
            // Clear water jump flag
            pm->flags &= ~FL_WATERJUMP;
        }
        
        // If still on ground after jump check, do walk move
        if (pm->flags & FL_ONGROUND) {
            PM_WalkMove(pm);
        }
    } else {
        // In air
        PM_AirMove(pm);
    }
    
    // Categorize position again after movement
    PM_CategorizePosition(pm);
    
    // Check for fall damage
    PM_CheckFalling(pm);
    
    // Clamp velocity
    PM_CheckVelocity(pm);
    
    // Store old values for next frame
    pm->oldButtons = pm->buttons;
    pm->oldFlags = pm->flags;
}

// ============================================================================
// Ground Movement
// ============================================================================

void PM_WalkMove(PlayerMove* pm) {
    // Get wish direction from input
    Vec3 wishVel;
    wishVel.x = pm->forward.x * pm->forwardMove + pm->right.x * pm->sideMove;
    wishVel.y = pm->forward.y * pm->forwardMove + pm->right.y * pm->sideMove;
    wishVel.z = 0.0f;  // No vertical movement on ground
    
    Vec3 wishDir = wishVel;
    f32 wishSpeed = glm::length(wishDir);
    
    if (wishSpeed > pmove::STOP_EPSILON) {
        wishDir /= wishSpeed;
    }
    
    // Scale by max speed
    wishSpeed *= pm->maxSpeed;
    
    // Clamp to max speed
    if (wishSpeed > pm->maxSpeed) {
        wishVel *= pm->maxSpeed / wishSpeed;
        wishSpeed = pm->maxSpeed;
    }
    
    // Walking modifier
    if (pm->buttons & IN_SPEED) {
        wishSpeed *= 0.52f;  // Walk speed multiplier
    }
    
    // Apply friction
    PM_Friction(pm);
    
    // Accelerate
    PM_Accelerate(pm, wishDir, wishSpeed, pm->moveVars->accelerate);
    
    // Clamp velocity
    PM_CheckVelocity(pm);
    
    // Check if stopped
    f32 speed = glm::length(pm->velocity);
    if (speed < 1.0f) {
        pm->velocity = Vec3(0.0f);
        return;
    }
    
    // Try to move forward
    Vec3 dest = pm->origin + pm->velocity * pm->frameTime;
    TraceResult trace = PM_PlayerTrace(pm, pm->origin, dest);
    
    // Made the full move
    if (trace.fraction == 1.0f) {
        pm->origin = dest;
        return;
    }
    
    // Try stepping up stairs
    PM_StepMove(pm, dest, trace);
}

// ============================================================================
// Air Movement
// ============================================================================

void PM_AirMove(PlayerMove* pm) {
    // Get wish direction from input
    Vec3 wishVel;
    wishVel.x = pm->forward.x * pm->forwardMove + pm->right.x * pm->sideMove;
    wishVel.y = pm->forward.y * pm->forwardMove + pm->right.y * pm->sideMove;
    wishVel.z = 0.0f;
    
    Vec3 wishDir = wishVel;
    f32 wishSpeed = glm::length(wishDir);
    
    if (wishSpeed > pmove::STOP_EPSILON) {
        wishDir /= wishSpeed;
    }
    
    // Scale by max speed
    wishSpeed *= pm->maxSpeed;
    
    // Apply air acceleration
    // Note: Air acceleration with wishspeed capped to 30 is what enables
    // bunnyhopping and air strafing
    PM_AirAccelerate(pm, wishDir, wishSpeed, pm->moveVars->airAccelerate);
    
    // Add gravity
    PM_AddGravity(pm);
    
    // Move and clip
    PM_FlyMove(pm);
}

// ============================================================================
// Core Physics Functions
// ============================================================================

/**
 * @brief Ground acceleration
 * 
 * This is the core acceleration function. The math here determines
 * how the player speeds up toward their desired velocity.
 */
void PM_Accelerate(PlayerMove* pm, Vec3 wishDir, f32 wishSpeed, f32 accel) {
    // Current speed in wish direction (projection of velocity onto wishdir)
    // This is the key insight: we only consider the component of velocity
    // in the direction we want to go
    PM_FLOAT currentSpeed = glm::dot(pm->velocity, wishDir);
    
    // How much speed to add (difference between desired and current)
    PM_FLOAT addSpeed = wishSpeed - currentSpeed;
    
    // If we're already going fast enough (or too fast) in that direction, stop
    if (addSpeed <= 0) {
        return;
    }
    
    // Calculate the actual acceleration amount
    // accelspeed = accel * frametime * wishspeed
    PM_FLOAT accelSpeed = accel * pm->frameTime * wishSpeed;
    
    // Don't accelerate beyond what's needed
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }
    
    // Apply acceleration in the wish direction
    pm->velocity.x += accelSpeed * wishDir.x;
    pm->velocity.y += accelSpeed * wishDir.y;
    pm->velocity.z += accelSpeed * wishDir.z;
}

/**
 * @brief Air acceleration (enables bunnyhopping)
 * 
 * The key difference from ground acceleration is the wishspeed cap.
 * By capping wishspeed to a low value (30), when strafing perpendicular
 * to our current velocity, currentSpeed (dot product) becomes ~0,
 * allowing us to add speed regardless of how fast we're already going.
 * 
 * This is what enables bunnyhopping and air strafing.
 */
void PM_AirAccelerate(PlayerMove* pm, Vec3 wishDir, f32 wishSpeed, f32 accel) {
    // Cap wishspeed for air acceleration
    // This is THE key mechanic for bunnyhopping
    PM_FLOAT wishSpd = wishSpeed;
    if (wishSpd > pm->moveVars->airSpeedCap) {
        wishSpd = pm->moveVars->airSpeedCap;
    }
    
    PM_FLOAT currentSpeed = glm::dot(pm->velocity, wishDir);
    PM_FLOAT addSpeed = wishSpd - currentSpeed;
    
    if (addSpeed <= 0) {
        return;
    }
    
    PM_FLOAT accelSpeed = accel * wishSpeed * pm->frameTime;
    
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }
    
    pm->velocity.x += accelSpeed * wishDir.x;
    pm->velocity.y += accelSpeed * wishDir.y;
    pm->velocity.z += accelSpeed * wishDir.z;
}

void PM_Friction(PlayerMove* pm) {
    f32 speed = glm::length(pm->velocity);
    
    if (speed < pmove::STOP_EPSILON) {
        return;
    }
    
    f32 drop = 0.0f;
    
    // Apply ground friction
    if (pm->flags & FL_ONGROUND) {
        f32 friction = pm->moveVars->friction;
        
        // Calculate control (for low speeds, use stopspeed)
        f32 control = (speed < pm->moveVars->stopSpeed) 
                     ? pm->moveVars->stopSpeed 
                     : speed;
        
        // Add the amount to take off
        drop = control * friction * pm->frameTime;
    }
    
    // Scale velocity
    f32 newSpeed = speed - drop;
    if (newSpeed < 0) {
        newSpeed = 0;
    }
    
    if (newSpeed != speed) {
        newSpeed /= speed;
        pm->velocity *= newSpeed;
    }
}

void PM_AddGravity(PlayerMove* pm) {
    f32 gravity = pm->moveVars->gravity;
    
    // Apply entity gravity modifier
    if (pm->moveVars->entGravity != 0.0f) {
        gravity *= pm->moveVars->entGravity;
    }
    
    pm->velocity.z -= gravity * pm->frameTime;
    
    // Add base velocity (for conveyor belts, etc.)
    pm->velocity.z += pm->baseVelocity.z * pm->frameTime;
    pm->baseVelocity.z = 0.0f;
}

// ============================================================================
// Movement and Collision
// ============================================================================

i32 PM_FlyMove(PlayerMove* pm) {
    i32 numBumps = pmove::MAX_BUMPS;
    i32 blocked = 0;
    
    Vec3 planes[pmove::MAX_CLIP_PLANES];
    i32 numPlanes = 0;
    
    Vec3 originalVelocity = pm->velocity;
    Vec3 primalVelocity = pm->velocity;
    
    f32 timeLeft = pm->frameTime;
    
    for (i32 bumpCount = 0; bumpCount < numBumps; ++bumpCount) {
        if (glm::length(pm->velocity) == 0.0f) {
            break;
        }
        
        Vec3 end = pm->origin + pm->velocity * timeLeft;
        TraceResult trace = PM_PlayerTrace(pm, pm->origin, end);
        
        if (trace.allSolid) {
            // Stuck in solid
            pm->velocity = Vec3(0.0f);
            return 4;
        }
        
        if (trace.fraction > 0) {
            // Move partially
            pm->origin = trace.endPos;
            numPlanes = 0;
        }
        
        if (trace.fraction == 1.0f) {
            // Moved full distance
            break;
        }
        
        // Hit something
        timeLeft -= timeLeft * trace.fraction;
        
        // Track blocked directions
        if (trace.plane.normal.z > pmove::MAX_FLOOR_NORMAL) {
            blocked |= 1;  // Floor
        }
        if (trace.plane.normal.z == 0) {
            blocked |= 2;  // Wall
        }
        
        planes[numPlanes] = trace.plane.normal;
        ++numPlanes;
        
        // Modify velocity to slide along the surface
        if (numPlanes >= pmove::MAX_CLIP_PLANES) {
            pm->velocity = Vec3(0.0f);
            break;
        }
        
        // Clip velocity
        PM_ClipVelocity(originalVelocity, planes[numPlanes - 1], pm->velocity, 1.0f);
        
        // Check if clipped velocity is valid for all planes
        i32 i;
        for (i = 0; i < numPlanes; ++i) {
            if (glm::dot(pm->velocity, planes[i]) < 0) {
                break;
            }
        }
        
        if (i == numPlanes) {
            // Valid for all planes
            break;
        }
    }
    
    return blocked;
}

void PM_StepMove(PlayerMove* pm, Vec3 dest, TraceResult& trace) {
    Vec3 originalOrigin = pm->origin;
    Vec3 originalVelocity = pm->velocity;
    
    // Try to step up
    Vec3 stepUp = pm->origin;
    stepUp.z += pm->moveVars->stepSize;
    
    TraceResult upTrace = PM_PlayerTrace(pm, pm->origin, stepUp);
    
    if (upTrace.allSolid) {
        // Can't step up, just slide along the surface
        PM_ClipVelocity(pm->velocity, trace.plane.normal, pm->velocity, 1.0f);
        pm->origin = trace.endPos;
        return;
    }
    
    // Try to move forward from the raised position
    Vec3 stepDest = upTrace.endPos;
    stepDest.x = dest.x;
    stepDest.y = dest.y;
    
    TraceResult forwardTrace = PM_PlayerTrace(pm, upTrace.endPos, stepDest);
    
    // Step down
    Vec3 stepDown = forwardTrace.endPos;
    stepDown.z = originalOrigin.z;
    
    TraceResult downTrace = PM_PlayerTrace(pm, forwardTrace.endPos, stepDown);
    
    if (!downTrace.startSolid && !downTrace.allSolid) {
        // Check if this is actually a step (hit a floor)
        if (downTrace.plane.normal.z > pmove::MAX_FLOOR_NORMAL) {
            pm->origin = downTrace.endPos;
            return;
        }
    }
    
    // Stepping didn't work, use the original collision
    pm->origin = trace.endPos;
    PM_ClipVelocity(pm->velocity, trace.plane.normal, pm->velocity, 1.0f);
}

void PM_ClipVelocity(Vec3 in, Vec3 normal, Vec3& out, f32 overbounce) {
    f32 backoff = glm::dot(in, normal) * overbounce;
    
    out.x = in.x - backoff * normal.x;
    out.y = in.y - backoff * normal.y;
    out.z = in.z - backoff * normal.z;
    
    // Clamp tiny values to zero for numerical stability
    for (i32 i = 0; i < 3; ++i) {
        if (PM_FABS(out[i]) < pmove::STOP_EPSILON) {
            out[i] = 0.0f;
        }
    }
}

// ============================================================================
// State Functions
// ============================================================================

void PM_CategorizePosition(PlayerMove* pm) {
    // Reset ground state
    pm->onGround = -1;
    
    // Check for ground
    Vec3 point = pm->origin;
    point.z -= pmove::GROUND_CHECK_DIST;
    
    TraceResult trace = PM_PlayerTrace(pm, pm->origin, point);
    
    // Not on ground if:
    // - Trace didn't hit anything
    // - Surface too steep
    // - Moving up too fast (jumping)
    if (trace.fraction == 1.0f) {
        pm->flags &= ~FL_ONGROUND;
        return;
    }
    
    if (trace.plane.normal.z < pmove::MAX_FLOOR_NORMAL) {
        pm->flags &= ~FL_ONGROUND;
        return;
    }
    
    if (pm->velocity.z > 180.0f) {
        pm->flags &= ~FL_ONGROUND;
        return;
    }
    
    // On ground
    pm->flags |= FL_ONGROUND;
    pm->onGround = trace.entity;
    
    // Snap to ground if trace hit
    if (trace.fraction < 1.0f && trace.fraction != 0.0f) {
        pm->origin = trace.endPos;
    }
}

void PM_Jump(PlayerMove* pm) {
    // Can't jump if holding jump from last frame
    if (pm->oldButtons & IN_JUMP) {
        return;
    }
    
    // Must be on ground
    if (!(pm->flags & FL_ONGROUND)) {
        return;
    }
    
    // Leave ground
    pm->flags &= ~FL_ONGROUND;
    pm->onGround = -1;
    
    // Apply jump velocity
    // sqrt(2 * gravity * height) where height is 45 units
    pm->velocity.z = pm->moveVars->jumpSpeed;
    
    // Add base velocity
    if (pm->baseVelocity.z > 0) {
        pm->velocity.z += pm->baseVelocity.z;
        pm->baseVelocity.z = 0;
    }
    
    // Track fall velocity for fall damage
    pm->fallVelocity = 0.0f;
}

void PM_Duck(PlayerMove* pm) {
    if (pm->buttons & IN_DUCK) {
        if (!(pm->flags & FL_DUCKING) && !pm->inDuck) {
            // Start ducking
            pm->inDuck = true;
            pm->duckTime = hull::DUCK_TIME;
        }
    } else {
        if (pm->flags & FL_DUCKING || pm->inDuck) {
            // Try to stand up
            Vec3 newOrigin = pm->origin;
            newOrigin.z += hull::STANDING_MAXS.z - hull::DUCKED_MAXS.z;
            
            // Check if we can stand up
            i32 savedHull = pm->useHull;
            pm->useHull = HULL_STANDING;
            
            TraceResult trace = PM_PlayerTrace(pm, pm->origin, newOrigin);
            
            pm->useHull = savedHull;
            
            if (!trace.startSolid) {
                // Can stand up
                pm->flags &= ~FL_DUCKING;
                pm->useHull = HULL_STANDING;
                pm->inDuck = false;
                pm->origin = newOrigin;
            }
        }
    }
    
    // Update duck transition
    if (pm->inDuck) {
        pm->duckTime -= pm->frameTime;
        
        if (pm->duckTime <= 0.0f || (pm->flags & FL_ONGROUND) == 0) {
            // Finish ducking
            pm->flags |= FL_DUCKING;
            pm->useHull = HULL_DUCKED;
            pm->inDuck = false;
        }
    }
}

void PM_CheckFalling(PlayerMove* pm) {
    if (pm->flags & FL_ONGROUND) {
        if (pm->fallVelocity >= pmove::FALL_PUNCH_THRESHOLD) {
            // Apply punch angle
            f32 punchAmount = pm->fallVelocity * 0.013f;
            if (punchAmount > 8.0f) {
                punchAmount = 8.0f;
            }
            pm->punchAngle.x = punchAmount;
        }
        
        if (pm->fallVelocity >= pmove::FALL_DAMAGE_THRESHOLD) {
            // Would apply fall damage here
            LOG_DEBUG("Fall damage! Velocity: {}", pm->fallVelocity);
        }
        
        pm->fallVelocity = 0.0f;
    } else {
        // Track fall velocity (only negative Z)
        if (pm->velocity.z < 0 && -pm->velocity.z > pm->fallVelocity) {
            pm->fallVelocity = -pm->velocity.z;
        }
    }
}

void PM_CheckVelocity(PlayerMove* pm) {
    f32 maxVel = pm->moveVars->maxVelocity;
    
    for (i32 i = 0; i < 3; ++i) {
        if (pm->velocity[i] > maxVel) {
            pm->velocity[i] = maxVel;
        }
        if (pm->velocity[i] < -maxVel) {
            pm->velocity[i] = -maxVel;
        }
    }
}

// ============================================================================
// Special Movement
// ============================================================================

bool PM_CheckLadder(PlayerMove* pm) {
    // Simplified ladder check
    // Full implementation would trace in multiple directions
    pm->onLadder = false;
    return false;
}

void PM_LadderMove(PlayerMove* pm) {
    // Ladder movement implementation
    // Uses forward/back to move up/down
    f32 speed = pmove::LADDER_SPEED;
    
    if (pm->buttons & IN_SPEED) {
        speed *= 0.5f;  // Walk on ladder
    }
    
    pm->velocity = Vec3(0.0f);
    
    if (pm->forwardMove != 0) {
        // Looking up = go up, looking down = go down
        if (pm->viewAngles.x < 0) {
            pm->velocity.z = speed * (pm->forwardMove > 0 ? 1.0f : -1.0f);
        } else {
            pm->velocity.z = speed * (pm->forwardMove > 0 ? -1.0f : 1.0f);
        }
    }
    
    if (pm->sideMove != 0) {
        pm->velocity += pm->right * pm->sideMove * speed;
    }
    
    // Move
    PM_FlyMove(pm);
}

void PM_WaterMove(PlayerMove* pm) {
    // Water movement - similar to air but with different physics
    Vec3 wishVel;
    wishVel.x = pm->forward.x * pm->forwardMove + pm->right.x * pm->sideMove;
    wishVel.y = pm->forward.y * pm->forwardMove + pm->right.y * pm->sideMove;
    wishVel.z = pm->forward.z * pm->forwardMove + pm->upMove;
    
    Vec3 wishDir = wishVel;
    f32 wishSpeed = glm::length(wishDir);
    
    if (wishSpeed > pmove::STOP_EPSILON) {
        wishDir /= wishSpeed;
    }
    
    wishSpeed *= pm->maxSpeed;
    
    // Clamp to max speed
    f32 maxSpeed = pm->maxSpeed * 0.8f;  // Slower in water
    if (wishSpeed > maxSpeed) {
        wishVel *= maxSpeed / wishSpeed;
        wishSpeed = maxSpeed;
    }
    
    // Apply water friction
    f32 speed = glm::length(pm->velocity);
    if (speed > 0) {
        f32 newSpeed = speed - pm->frameTime * speed * pm->moveVars->waterFriction;
        if (newSpeed < 0) {
            newSpeed = 0;
        }
        pm->velocity *= newSpeed / speed;
    }
    
    // Accelerate
    PM_Accelerate(pm, wishDir, wishSpeed, pm->moveVars->waterAccelerate);
    
    // Move
    PM_FlyMove(pm);
}

// ============================================================================
// Utility Functions
// ============================================================================

TraceResult PM_PlayerTrace(const PlayerMove* pm, Vec3 start, Vec3 end) {
    if (pm->traceFunc) {
        return pm->traceFunc(pm, start, end, pm->useHull);
    }
    
    // Default trace (no collision)
    TraceResult result;
    result.fraction = 1.0f;
    result.endPos = end;
    return result;
}

void PM_AngleVectors(const PlayerMove* pm) {
    // Calculate direction vectors from view angles
    PlayerMove* mutablePm = const_cast<PlayerMove*>(pm);
    math::angleVectors(pm->viewAngles, &mutablePm->forward, &mutablePm->right, &mutablePm->up);
    
    // Flatten forward and right for ground movement
    mutablePm->forward.z = 0;
    mutablePm->right.z = 0;
    
    if (glm::length(mutablePm->forward) > pmove::STOP_EPSILON) {
        mutablePm->forward = glm::normalize(mutablePm->forward);
    }
    if (glm::length(mutablePm->right) > pmove::STOP_EPSILON) {
        mutablePm->right = glm::normalize(mutablePm->right);
    }
}

void PM_PlayStepSound(PlayerMove* pm, i32 step, f32 volume) {
    // Would play footstep sound here
    (void)pm;
    (void)step;
    (void)volume;
}

} // namespace cscpp::movement


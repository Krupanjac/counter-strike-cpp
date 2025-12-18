# Movement System Documentation

## Overview

The movement system is a faithful port of GoldSrc's `pm_shared.c`, preserving the exact physics that enable bunnyhopping, air strafing, and other movement techniques that define Counter-Strike gameplay.

## GoldSrc Movement Theory

### Why Bunnyhopping Works

Bunnyhopping and air strafing emerge from the interaction of several movement mechanics:

```
PM_AirAccelerate:
  1. Calculate current speed in wish direction
  2. Calculate how much speed to add
  3. Add velocity in wish direction

The key insight: when strafing in air, the wish direction is perpendicular
to current velocity. This means currentspeed (dot product) is ~0, allowing
full acceleration to be applied regardless of current speed.
```

### Mathematical Analysis

From Adrian Biagioli's analysis (adrianb.io):

```cpp
// Core acceleration formula
void PM_Accelerate(float wishspeed, glm::vec3 wishdir, float accel) {
    float currentspeed = glm::dot(velocity, wishdir);
    float addspeed = wishspeed - currentspeed;
    
    if (addspeed <= 0) return;
    
    float accelspeed = accel * frametime * wishspeed;
    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }
    
    velocity += accelspeed * wishdir;
}
```

When air strafing:
- `wishdir` is perpendicular to `velocity`
- `currentspeed = dot(velocity, wishdir) ≈ 0`
- `addspeed = wishspeed - 0 = wishspeed`
- Full acceleration is applied perpendicular to current velocity
- This rotates the velocity vector without reducing speed
- Combined with proper mouse movement, speed increases

## Implementation

### Core Structures

```cpp
// Player movement state (equivalent to pmove_t)
struct PlayerMove {
    // Input
    glm::vec3 origin;              // Current position
    glm::vec3 velocity;            // Current velocity
    glm::vec3 viewangles;          // View angles (pitch, yaw, roll)
    glm::vec3 basevelocity;        // Push velocity (conveyor belts, etc.)
    
    // Timing
    float frametime;               // Time for this tick
    
    // Movement intent (from user command)
    float forwardmove;             // Forward/back input (-1 to 1)
    float sidemove;                // Left/right input (-1 to 1)
    float upmove;                  // Up/down input (jump, duck)
    
    // State flags
    int flags;                     // FL_ONGROUND, FL_DUCKING, etc.
    int oldbuttons;                // Previous frame buttons
    int waterlevel;                // 0-3 water immersion
    int watertype;                 // Water content type
    
    // Hull selection
    int usehull;                   // 0=standing, 1=ducked, 2=point
    
    // Output (set by movement code)
    int onground;                  // Ground entity index (-1 if airborne)
    
    // Physics tuning
    MoveVars* movevars;            // Shared movement variables
    
    // Trace interface
    TraceFunc traceFunc;           // Collision trace function
    
    // Player info
    int playerindex;
    bool dead;
    float maxspeed;
    float clientmaxspeed;
};

// Movement variables (server-controlled)
struct MoveVars {
    float gravity       = 800.0f;     // sv_gravity
    float stopspeed     = 100.0f;     // sv_stopspeed
    float maxspeed      = 320.0f;     // sv_maxspeed
    float accelerate    = 10.0f;      // sv_accelerate
    float airaccelerate = 10.0f;      // sv_airaccelerate (100 for classic bhop)
    float wateraccelerate = 10.0f;    // sv_wateraccelerate
    float friction      = 4.0f;       // sv_friction
    float edgefriction  = 2.0f;       // sv_edgefriction
    float waterfriction = 1.0f;       // sv_waterfriction
    float stepsize      = 18.0f;      // sv_stepsize
    float maxvelocity   = 2000.0f;    // sv_maxvelocity
    float bounce        = 1.0f;       // sv_bounce
    float entgravity    = 1.0f;       // Entity gravity modifier
};

// Movement flags
enum PMFlags : int {
    FL_ONGROUND     = (1 << 0),
    FL_DUCKING      = (1 << 1),
    FL_WATERJUMP    = (1 << 2),
    FL_ONTRAIN      = (1 << 3),
    FL_INRAIN       = (1 << 4),
    FL_FROZEN       = (1 << 5),
    FL_ATCONTROLS   = (1 << 6),
    FL_CLIENT       = (1 << 7),
    FL_FAKECLIENT   = (1 << 8),
    FL_INWATER      = (1 << 9),
};

// Button flags (from user command)
enum PMButtons : int {
    IN_ATTACK       = (1 << 0),
    IN_JUMP         = (1 << 1),
    IN_DUCK         = (1 << 2),
    IN_FORWARD      = (1 << 3),
    IN_BACK         = (1 << 4),
    IN_USE          = (1 << 5),
    IN_MOVELEFT     = (1 << 6),
    IN_MOVERIGHT    = (1 << 7),
    IN_ATTACK2      = (1 << 8),
    IN_RELOAD       = (1 << 9),
    IN_SPEED        = (1 << 10),  // Walk modifier
};
```

### Main Entry Point

```cpp
void PM_PlayerMove(PlayerMove* pmove) {
    // Reduce timers
    PM_ReduceTimers(pmove);
    
    // Calculate view direction vectors
    glm::vec3 forward, right, up;
    AngleVectors(pmove->viewangles, &forward, &right, &up);
    
    // Categorize position (on ground, in water, etc.)
    PM_CategorizePosition(pmove);
    
    // Handle special cases
    if (pmove->flags & FL_FROZEN) {
        return;
    }
    
    if (pmove->dead) {
        PM_DeadMove(pmove);
        return;
    }
    
    // Check if on ladder
    if (PM_CheckLadder(pmove)) {
        PM_LadderMove(pmove);
        return;
    }
    
    // Handle water movement
    if (pmove->waterlevel >= 2) {
        PM_WaterMove(pmove);
        return;
    }
    
    // Normal movement
    if (pmove->flags & FL_ONGROUND) {
        // Handle jump first (leaves ground)
        if (pmove->buttons & IN_JUMP) {
            PM_Jump(pmove);
        } else {
            pmove->flags &= ~FL_WATERJUMP;
        }
        
        // If still on ground, do walk move
        if (pmove->flags & FL_ONGROUND) {
            PM_WalkMove(pmove);
        }
    } else {
        // Air movement
        PM_AirMove(pmove);
    }
    
    // Categorize again after movement
    PM_CategorizePosition(pmove);
    
    // Check for fall damage
    PM_CheckFalling(pmove);
    
    // Cap velocity
    PM_CheckVelocity(pmove);
}
```

### Ground Movement (PM_WalkMove)

```cpp
void PM_WalkMove(PlayerMove* pmove) {
    // Get wish direction from input
    glm::vec3 forward, right, up;
    AngleVectors(pmove->viewangles, &forward, &right, &up);
    
    // Flatten to ground plane
    forward.z = 0;
    right.z = 0;
    forward = glm::normalize(forward);
    right = glm::normalize(right);
    
    // Calculate wish velocity
    glm::vec3 wishvel;
    wishvel.x = forward.x * pmove->forwardmove + right.x * pmove->sidemove;
    wishvel.y = forward.y * pmove->forwardmove + right.y * pmove->sidemove;
    wishvel.z = 0;
    
    glm::vec3 wishdir = wishvel;
    float wishspeed = glm::length(wishdir);
    
    if (wishspeed > 0.0001f) {
        wishdir /= wishspeed;
    }
    
    // Clamp to max speed
    if (wishspeed > pmove->maxspeed) {
        wishvel *= pmove->maxspeed / wishspeed;
        wishspeed = pmove->maxspeed;
    }
    
    // Apply friction
    PM_Friction(pmove);
    
    // Accelerate
    PM_Accelerate(pmove, wishdir, wishspeed, pmove->movevars->accelerate);
    
    // Cap velocity
    PM_CheckVelocity(pmove);
    
    // Try to move
    float spd = glm::length(pmove->velocity);
    if (spd < 1.0f) {
        pmove->velocity = glm::vec3(0);
        return;
    }
    
    // First try a straight move
    glm::vec3 dest = pmove->origin + pmove->velocity * pmove->frametime;
    TraceResult trace = pmove->traceFunc(pmove->origin, dest, pmove->usehull);
    
    if (trace.fraction == 1.0f) {
        pmove->origin = dest;
        return;
    }
    
    // Try stair stepping
    PM_StepMove(pmove, dest, trace);
}
```

### Air Movement (PM_AirMove)

```cpp
void PM_AirMove(PlayerMove* pmove) {
    // Get wish direction from input
    glm::vec3 forward, right, up;
    AngleVectors(pmove->viewangles, &forward, &right, &up);
    
    // Use full 3D vectors for air movement
    glm::vec3 wishvel;
    wishvel.x = forward.x * pmove->forwardmove + right.x * pmove->sidemove;
    wishvel.y = forward.y * pmove->forwardmove + right.y * pmove->sidemove;
    wishvel.z = 0;  // Vertical component handled separately
    
    glm::vec3 wishdir = wishvel;
    float wishspeed = glm::length(wishdir);
    
    if (wishspeed > 0.0001f) {
        wishdir /= wishspeed;
    }
    
    // Clamp to max air speed (usually 30)
    float maxairspeed = 30.0f;
    if (wishspeed > maxairspeed) {
        wishvel *= maxairspeed / wishspeed;
        wishspeed = maxairspeed;
    }
    
    // Apply air acceleration
    PM_AirAccelerate(pmove, wishdir, wishspeed, pmove->movevars->airaccelerate);
    
    // Add gravity
    PM_AddGravity(pmove);
    
    // Move
    PM_FlyMove(pmove);
}
```

### Core Acceleration Function

```cpp
// This is THE critical function for movement feel
void PM_Accelerate(PlayerMove* pmove, glm::vec3 wishdir, 
                   float wishspeed, float accel) {
    // Current speed in wish direction
    float currentspeed = glm::dot(pmove->velocity, wishdir);
    
    // How much to add
    float addspeed = wishspeed - currentspeed;
    
    // Nothing to add
    if (addspeed <= 0) {
        return;
    }
    
    // Determine acceleration
    float accelspeed = accel * pmove->frametime * wishspeed;
    
    // Cap it
    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }
    
    // Apply
    pmove->velocity += accelspeed * wishdir;
}

// Air acceleration (allows values > maxspeed due to perpendicular strafing)
void PM_AirAccelerate(PlayerMove* pmove, glm::vec3 wishdir,
                      float wishspeed, float accel) {
    // Cap wishspeed to 30 for air
    float wishspd = wishspeed;
    if (wishspd > 30.0f) {
        wishspd = 30.0f;
    }
    
    float currentspeed = glm::dot(pmove->velocity, wishdir);
    float addspeed = wishspd - currentspeed;
    
    if (addspeed <= 0) {
        return;
    }
    
    float accelspeed = accel * wishspeed * pmove->frametime;
    
    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }
    
    pmove->velocity += accelspeed * wishdir;
}
```

### Friction

```cpp
void PM_Friction(PlayerMove* pmove) {
    float speed = glm::length(pmove->velocity);
    
    if (speed < 0.1f) {
        return;
    }
    
    float drop = 0.0f;
    
    // Apply ground friction
    if (pmove->flags & FL_ONGROUND) {
        float friction = pmove->movevars->friction;
        
        // Apply edge friction if near ledge
        // (simplified - full impl does trace)
        float control = (speed < pmove->movevars->stopspeed) 
                        ? pmove->movevars->stopspeed : speed;
        
        drop = control * friction * pmove->frametime;
    }
    
    // Scale velocity
    float newspeed = speed - drop;
    if (newspeed < 0) {
        newspeed = 0;
    }
    
    if (newspeed != speed) {
        newspeed /= speed;
        pmove->velocity *= newspeed;
    }
}
```

### Jump

```cpp
void PM_Jump(PlayerMove* pmove) {
    // Can't jump again if already jumped
    if (pmove->oldbuttons & IN_JUMP) {
        return;  // Holding jump - wait for release
    }
    
    // Not on ground
    if (!(pmove->flags & FL_ONGROUND)) {
        return;
    }
    
    // Leave ground
    pmove->flags &= ~FL_ONGROUND;
    
    // Standard jump velocity
    pmove->velocity.z = sqrtf(2.0f * pmove->movevars->gravity * 45.0f);
    
    // Account for slopes
    if (pmove->basevelocity.z > 0) {
        pmove->velocity.z += pmove->basevelocity.z;
        pmove->basevelocity.z = 0;
    }
    
    // Play jump sound
    PM_PlaySound(CHAN_BODY, "player/plyrjmp8.wav", 0.5f);
}
```

### Collision (PM_FlyMove)

```cpp
int PM_FlyMove(PlayerMove* pmove) {
    int numbumps = 4;
    int blocked = 0;
    glm::vec3 planes[5];
    int numplanes = 0;
    
    glm::vec3 original_velocity = pmove->velocity;
    glm::vec3 primal_velocity = pmove->velocity;
    
    float time_left = pmove->frametime;
    
    for (int bumpcount = 0; bumpcount < numbumps; bumpcount++) {
        if (glm::length(pmove->velocity) == 0.0f) {
            break;
        }
        
        glm::vec3 end = pmove->origin + pmove->velocity * time_left;
        
        TraceResult trace = pmove->traceFunc(pmove->origin, end, pmove->usehull);
        
        if (trace.allsolid) {
            // Stuck in solid
            pmove->velocity = glm::vec3(0);
            return 4;
        }
        
        if (trace.fraction > 0) {
            // Move partway
            pmove->origin = trace.endpos;
            numplanes = 0;
        }
        
        if (trace.fraction == 1.0f) {
            // Moved the whole way
            break;
        }
        
        // Hit something
        time_left -= time_left * trace.fraction;
        
        // Clip velocity to plane
        PM_ClipVelocity(pmove->velocity, trace.plane.normal, 
                        pmove->velocity, 1.0f);
        
        // Track blocked directions
        if (trace.plane.normal.z > 0.7f) {
            blocked |= 1;  // Floor
        }
        if (trace.plane.normal.z == 0) {
            blocked |= 2;  // Wall
        }
        
        planes[numplanes++] = trace.plane.normal;
        
        // Multi-plane clipping
        if (numplanes > 1) {
            // Find exit velocity
            for (int i = 0; i < numplanes; i++) {
                PM_ClipVelocity(original_velocity, planes[i], 
                               pmove->velocity, 1.0f);
                
                // Check if this velocity is good for all planes
                bool valid = true;
                for (int j = 0; j < numplanes; j++) {
                    if (j != i && glm::dot(pmove->velocity, planes[j]) < 0) {
                        valid = false;
                        break;
                    }
                }
                
                if (valid) break;
            }
        }
    }
    
    return blocked;
}

void PM_ClipVelocity(glm::vec3 in, glm::vec3 normal, 
                     glm::vec3& out, float overbounce) {
    float backoff = glm::dot(in, normal) * overbounce;
    
    out = in - backoff * normal;
    
    // Tiny tolerance for numerical stability
    for (int i = 0; i < 3; i++) {
        if (fabsf(out[i]) < 0.001f) {
            out[i] = 0;
        }
    }
}
```

### Ground Check (PM_CategorizePosition)

```cpp
void PM_CategorizePosition(PlayerMove* pmove) {
    // Reset
    pmove->onground = -1;
    pmove->flags &= ~FL_ONGROUND;
    
    // Trace down
    glm::vec3 point = pmove->origin;
    point.z -= 2.0f;  // Small distance
    
    TraceResult trace = pmove->traceFunc(pmove->origin, point, pmove->usehull);
    
    // Not on ground if:
    // - No hit
    // - Too steep (normal.z < 0.7)
    // - Moving up fast (jumping)
    if (trace.fraction == 1.0f) {
        return;
    }
    
    if (trace.plane.normal.z < 0.7f) {
        return;
    }
    
    if (pmove->velocity.z > 180.0f) {
        return;
    }
    
    // On ground
    pmove->flags |= FL_ONGROUND;
    pmove->onground = trace.ent;
    
    // Snap to ground
    if (trace.fraction < 1.0f && trace.fraction != 0.0f) {
        pmove->origin = trace.endpos;
    }
}
```

## GoldSrc Parity Mode

When `CSCPP_GOLDSRC_PARITY` is defined, the movement code uses exact GoldSrc floating-point behavior:

```cpp
#ifdef CSCPP_GOLDSRC_PARITY
    // Disable compiler FP optimizations that reorder operations
    #pragma float_control(precise, on)
    
    // Use single precision throughout
    using pmfloat = float;
    
    // Match exact operation order from pm_shared.c
    // Example: dot product must be computed exactly as in original
    inline float PM_DotProduct(const vec3_t a, const vec3_t b) {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];  // Exact order
    }
#else
    // Modern mode: allow optimizations, use glm
    using pmfloat = float;
    
    inline float PM_DotProduct(glm::vec3 a, glm::vec3 b) {
        return glm::dot(a, b);
    }
#endif
```

## Testing

### Determinism Test

```cpp
void testMovementDeterminism() {
    PlayerMove pm1, pm2;
    
    // Same initial state
    pm1.origin = pm2.origin = {0, 0, 100};
    pm1.velocity = pm2.velocity = {0, 0, 0};
    pm1.frametime = pm2.frametime = 1.0f / 128.0f;
    
    // Same inputs
    std::vector<UserCmd> inputs = loadTestInputs("bhop_test.dat");
    
    for (auto& cmd : inputs) {
        pm1.forwardmove = pm2.forwardmove = cmd.forward;
        pm1.sidemove = pm2.sidemove = cmd.side;
        pm1.viewangles = pm2.viewangles = cmd.angles;
        pm1.buttons = pm2.buttons = cmd.buttons;
        
        PM_PlayerMove(&pm1);
        PM_PlayerMove(&pm2);
        
        // Must be exactly equal
        ASSERT_EQ(pm1.origin, pm2.origin);
        ASSERT_EQ(pm1.velocity, pm2.velocity);
    }
}
```

### Reference Comparison

```cpp
void testAgainstReference() {
    // Load reference data captured from original engine
    auto refData = loadReferenceData("goldsrc_movement_capture.dat");
    
    PlayerMove pm;
    pm.origin = refData.initialOrigin;
    pm.velocity = refData.initialVelocity;
    
    for (size_t i = 0; i < refData.frames.size(); i++) {
        auto& frame = refData.frames[i];
        
        pm.forwardmove = frame.input.forward;
        pm.sidemove = frame.input.side;
        pm.viewangles = frame.input.angles;
        pm.buttons = frame.input.buttons;
        pm.frametime = frame.frametime;
        
        PM_PlayerMove(&pm);
        
        // Allow tiny tolerance for float differences
        ASSERT_NEAR(pm.origin, frame.expectedOrigin, 0.001f);
        ASSERT_NEAR(pm.velocity, frame.expectedVelocity, 0.01f);
    }
}
```

## Useful CVars

| CVar | Default | Description |
|------|---------|-------------|
| `sv_gravity` | 800 | World gravity |
| `sv_stopspeed` | 100 | Speed below which friction increases |
| `sv_maxspeed` | 320 | Maximum ground speed |
| `sv_accelerate` | 10 | Ground acceleration |
| `sv_airaccelerate` | 10 | Air acceleration (100 for classic bhop) |
| `sv_friction` | 4 | Ground friction |
| `sv_edgefriction` | 2 | Extra friction near edges |
| `sv_stepsize` | 18 | Max stair step height |
| `sv_maxvelocity` | 2000 | Absolute velocity cap |

## Known Differences from GoldSrc

1. **Tickrate Independence**: Original GoldSrc had some tick-dependent behavior. Our implementation uses consistent tick intervals.

2. **Floating Point**: Modern compilers may reorder FP operations. Use `CSCPP_GOLDSRC_PARITY` for exact behavior.

3. **Hull Sizes**: Must match original hull dimensions exactly:
   - Standing: mins(-16, -16, -36), maxs(16, 16, 36)
   - Ducked: mins(-16, -16, -18), maxs(16, 16, 18)
   - Point: mins(0, 0, 0), maxs(0, 0, 0)


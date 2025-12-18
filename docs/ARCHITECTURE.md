# Architecture Documentation

## Overview

This document describes the high-level architecture of the Counter-Strike C++ Engine, a modern reimplementation focusing on gameplay authenticity with GoldSrc-style movement and networking.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              APPLICATION LAYER                               │
│   ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐            │
│   │  Game Client    │  │ Dedicated Server│  │   Dev Tools     │            │
│   │  (cscpp_client) │  │ (cscpp_server)  │  │ (replay, asset) │            │
│   └────────┬────────┘  └────────┬────────┘  └────────┬────────┘            │
└────────────┼────────────────────┼────────────────────┼──────────────────────┘
             │                    │                    │
┌────────────┼────────────────────┼────────────────────┼──────────────────────┐
│            │         GAMEPLAY LAYER                  │                      │
│   ┌────────▼────────────────────▼────────────────────▼────────┐            │
│   │                    ECS World (entt)                        │            │
│   │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │            │
│   │  │Components│ │Components│ │Components│ │Components│     │            │
│   │  │(Transform│ │ (Health, │ │(Weapon,  │ │(Network, │     │            │
│   │  │ Physics) │ │  Armor)  │ │  Ammo)   │ │  Input)  │     │            │
│   │  └──────────┘ └──────────┘ └──────────┘ └──────────┘     │            │
│   └───────────────────────┬───────────────────────────────────┘            │
│                           │                                                 │
│   ┌───────────────────────▼───────────────────────────────────┐            │
│   │                    System Pipeline                         │            │
│   │  Input → Movement → Physics → Weapons → Damage → Network  │            │
│   └───────────────────────────────────────────────────────────┘            │
│                                                                             │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│   │   Weapons   │  │  Game Mode  │  │   Player    │  │    Audio    │      │
│   │   System    │  │   Logic     │  │   State     │  │   System    │      │
│   └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘      │
└─────────────────────────────────────────────────────────────────────────────┘
             │                    │                    │
┌────────────┼────────────────────┼────────────────────┼──────────────────────┐
│            │        ENGINE SERVICES LAYER            │                      │
│   ┌────────▼────────┐  ┌────────▼────────┐  ┌───────▼─────────┐           │
│   │    Movement     │  │    Networking   │  │    Renderer     │           │
│   │   (pm_shared)   │  │  (Server/Client)│  │  (Deferred PBR) │           │
│   │                 │  │                 │  │                 │           │
│   │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────┐ │           │
│   │ │ PM_WalkMove │ │  │ │   Server    │ │  │ │  G-Buffer   │ │           │
│   │ │ PM_AirMove  │ │  │ │  Tick Loop  │ │  │ │    Pass     │ │           │
│   │ │ PM_Friction │ │  │ ├─────────────┤ │  │ ├─────────────┤ │           │
│   │ ├─────────────┤ │  │ │ Prediction  │ │  │ │  Lighting   │ │           │
│   │ │  Collision  │ │  │ │ Reconcile   │ │  │ │    Pass     │ │           │
│   │ │   Traces    │ │  │ ├─────────────┤ │  │ ├─────────────┤ │           │
│   │ └─────────────┘ │  │ │ Lag Comp    │ │  │ │ Post-FX     │ │           │
│   └─────────────────┘  │ └─────────────┘ │  │ └─────────────┘ │           │
│                        └─────────────────┘  └─────────────────┘           │
│                                                                             │
│   ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐           │
│   │     Assets      │  │       UI        │  │    Physics      │           │
│   │ (glTF, BSP)     │  │ (ImGui + HUD)   │  │   (Collision)   │           │
│   └─────────────────┘  └─────────────────┘  └─────────────────┘           │
└─────────────────────────────────────────────────────────────────────────────┘
             │                    │                    │
┌────────────┼────────────────────┼────────────────────┼──────────────────────┐
│            │           CORE LAYER                    │                      │
│   ┌────────▼────────┐  ┌────────▼────────┐  ┌───────▼─────────┐           │
│   │    Platform     │  │   Job System    │  │    Resources    │           │
│   │ (Window, Input) │  │ (Thread Pool)   │  │    (Manager)    │           │
│   └─────────────────┘  └─────────────────┘  └─────────────────┘           │
│                                                                             │
│   ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐           │
│   │     Memory      │  │      Math       │  │     Logging     │           │
│   │  (Allocators)   │  │   (glm + SIMD)  │  │    (spdlog)     │           │
│   └─────────────────┘  └─────────────────┘  └─────────────────┘           │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Module Dependencies

```
                    ┌─────────┐
                    │  Core   │
                    └────┬────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
    ┌────▼────┐    ┌─────▼─────┐   ┌─────▼─────┐
    │   ECS   │    │  Movement │   │  Network  │
    └────┬────┘    └─────┬─────┘   └─────┬─────┘
         │               │               │
         │          ┌────┴────┐          │
         │          │         │          │
         │     ┌────▼───┐ ┌───▼────┐     │
         │     │Renderer│ │ Assets │     │
         │     └────┬───┘ └───┬────┘     │
         │          │         │          │
         └──────────┼─────────┼──────────┘
                    │         │
              ┌─────▼─────────▼─────┐
              │      Gameplay       │
              └─────────┬───────────┘
                        │
         ┌──────────────┼──────────────┐
         │              │              │
    ┌────▼────┐   ┌─────▼─────┐  ┌─────▼─────┐
    │  Audio  │   │    UI     │  │  Weapons  │
    └─────────┘   └───────────┘  └───────────┘
```

## Core Module

### Purpose
Provides foundational services used by all other modules.

### Components

#### Application (`core/application/`)
- Entry point and main loop orchestration
- Frame timing and fixed timestep management
- Subsystem initialization order

```cpp
class Application {
public:
    void run();
    
private:
    void initialize();
    void shutdown();
    void tick(float deltaTime);
    void fixedTick(float fixedDelta);  // Physics/network tick
    void render(float interpolation);
};
```

#### Platform (`core/platform/`)
- Window management (SDL2)
- Input handling (keyboard, mouse, gamepad)
- High-resolution timer

```cpp
class Window {
public:
    bool create(const WindowConfig& config);
    void pollEvents();
    void swapBuffers();
    glm::ivec2 getSize() const;
};

class Input {
public:
    void update();
    bool isKeyDown(Key key) const;
    bool isKeyPressed(Key key) const;  // Just this frame
    glm::vec2 getMouseDelta() const;
    UserCmd buildUserCmd() const;
};
```

#### Memory (`core/memory/`)
- Linear/stack allocator for per-frame allocations
- Pool allocator for fixed-size objects
- Arena allocator for hierarchical allocations

```cpp
class StackAllocator {
public:
    void* allocate(size_t size, size_t alignment = 16);
    void reset();  // Called each frame
};

class PoolAllocator {
public:
    template<typename T>
    T* allocate();
    void deallocate(void* ptr);
};
```

#### Jobs (`core/jobs/`)
- Work-stealing thread pool
- Job dependencies and continuations
- Parallel-for patterns

```cpp
class JobSystem {
public:
    JobHandle schedule(Job&& job);
    JobHandle scheduleParallel(ParallelJob&& job, uint32_t count);
    void wait(JobHandle handle);
    void waitAll();
};
```

#### Resources (`core/resources/`)
- Reference-counted resource handles
- Async loading with callbacks
- Hot-reloading support (debug builds)

```cpp
template<typename T>
class ResourceHandle {
    std::shared_ptr<T> m_resource;
    ResourceId m_id;
};

class ResourceManager {
public:
    template<typename T>
    ResourceHandle<T> load(std::string_view path);
    
    template<typename T>
    void loadAsync(std::string_view path, std::function<void(ResourceHandle<T>)> callback);
};
```

## ECS Module

### Purpose
Entity-Component-System architecture using entt for cache-friendly, data-oriented design.

### Design Philosophy
- Components are **pure data** (POD structs)
- Systems contain **logic** and operate on component queries
- Avoid inheritance; prefer composition
- Use entt groups for frequently co-accessed components

### Key Components

```cpp
// Transform & Physics
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct VelocityComponent {
    glm::vec3 linear{0.0f};
    glm::vec3 angular{0.0f};
};

struct ColliderComponent {
    ColliderType type;  // Box, Capsule, Hull
    glm::vec3 halfExtents;
    uint32_t collisionMask;
};

// Player
struct PlayerComponent {
    ClientId clientId;
    Team team;
    bool isAlive;
};

struct HealthComponent {
    float current = 100.0f;
    float max = 100.0f;
    float armor = 0.0f;
    float armorType = 0.0f;  // 0=none, 1=kevlar, 2=kevlar+helmet
};

struct InputComponent {
    std::deque<UserCmd> pendingCmds;
    Tick lastProcessedTick;
};

// Network
struct NetworkIdComponent {
    uint32_t networkId;  // Replicated entity ID
};

struct InterpolationComponent {
    std::array<EntityState, 3> history;
    float interpTime;
};

// Rendering
struct RenderableComponent {
    MeshHandle mesh;
    MaterialHandle material;
    uint32_t renderFlags;
};

struct AnimationComponent {
    AnimationHandle currentAnim;
    float time;
    float speed;
};
```

### System Pipeline

Systems execute in a defined order each tick:

```cpp
enum class SystemPhase {
    PrePhysics,    // Input processing
    Physics,       // Movement, collision
    PostPhysics,   // Weapon logic, damage
    PreRender,     // Animation, visibility
    Render,        // Draw calls
    PostRender,    // Debug, UI
    Network        // Send/receive
};

// Example system registration
world.registerSystem<InputSystem>(SystemPhase::PrePhysics);
world.registerSystem<MovementSystem>(SystemPhase::Physics);
world.registerSystem<WeaponSystem>(SystemPhase::PostPhysics);
world.registerSystem<RenderSystem>(SystemPhase::Render);
world.registerSystem<NetworkSyncSystem>(SystemPhase::Network);
```

## Movement Module

### Purpose
Faithful port of GoldSrc `pm_shared.c` for authentic CS 1.6 movement feel.

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    PlayerMove Struct                     │
│  (origin, velocity, angles, flags, hull, frametime)     │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                     PM_PlayerMove()                      │
│  Main entry point - categorizes move type               │
└────────────────────────┬────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
         ▼               ▼               ▼
┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│ PM_WalkMove │  │ PM_AirMove  │  │ PM_LadderMo │
│  (ground)   │  │   (air)     │  │   (ladder)  │
└──────┬──────┘  └──────┬──────┘  └─────────────┘
       │                │
       └────────┬───────┘
                │
                ▼
┌─────────────────────────────────────────────────────────┐
│                   PM_Accelerate()                        │
│  Core acceleration math (enables strafe/bhop)           │
│                                                         │
│  currentspeed = dot(velocity, wishdir)                  │
│  addspeed = wishspeed - currentspeed                    │
│  accelspeed = accel * frametime * wishspeed             │
│  velocity += clamp(accelspeed, 0, addspeed) * wishdir   │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    PM_FlyMove()                          │
│  Applies velocity, handles collisions via traces        │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                   Trace Functions                        │
│  PM_PlayerTrace, PM_HullTrace - collision detection     │
└─────────────────────────────────────────────────────────┘
```

### Key Functions (ported from pm_shared.c)

```cpp
namespace cscpp::movement {

// Main entry point
void PM_PlayerMove(PlayerMove* pmove);

// Movement modes
void PM_WalkMove(PlayerMove* pmove);      // Ground movement
void PM_AirMove(PlayerMove* pmove);       // Air movement
void PM_LadderMove(PlayerMove* pmove);    // Ladder climbing
void PM_WaterMove(PlayerMove* pmove);     // Swimming

// Core physics
void PM_Accelerate(PlayerMove* pmove, glm::vec3 wishdir, 
                   float wishspeed, float accel);
void PM_AirAccelerate(PlayerMove* pmove, glm::vec3 wishdir,
                      float wishspeed, float accel);
void PM_Friction(PlayerMove* pmove);
void PM_AddGravity(PlayerMove* pmove);

// Collision
TraceResult PM_PlayerTrace(PlayerMove* pmove, glm::vec3 start, glm::vec3 end);
void PM_CategorizePosition(PlayerMove* pmove);  // On ground check
int PM_FlyMove(PlayerMove* pmove);              // Move + slide

// State
void PM_Duck(PlayerMove* pmove);
void PM_Jump(PlayerMove* pmove);
void PM_CheckFalling(PlayerMove* pmove);

}
```

### Movement Variables

```cpp
struct MoveVars {
    float gravity = 800.0f;
    float stopspeed = 100.0f;
    float maxspeed = 320.0f;       // Player max speed
    float spectatormaxspeed = 500.0f;
    float accelerate = 10.0f;
    float airaccelerate = 10.0f;   // 100 for classic CS bhop
    float wateraccelerate = 10.0f;
    float friction = 4.0f;
    float edgefriction = 2.0f;
    float waterfriction = 1.0f;
    float entgravity = 1.0f;       // Entity gravity scale
    float bounce = 1.0f;
    float stepsize = 18.0f;
    float maxvelocity = 2000.0f;
    float zmax = 4096.0f;
    float waveHeight = 0.0f;
    bool footsteps = true;
    float rollangle = 0.0f;
    float rollspeed = 0.0f;
};
```

### GoldSrc Parity Mode

When `CSCPP_GOLDSRC_PARITY` is defined:
- Float operations match exact order from original code
- No compiler optimizations that reorder FP math
- Single precision throughout (no double)
- Matches original tick timing behavior

## Network Module

### Purpose
Authoritative server with client prediction, reconciliation, and lag compensation.

### Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                            SERVER                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                      Tick Loop                               │   │
│  │  1. Receive client inputs                                    │   │
│  │  2. Apply inputs to player entities                          │   │
│  │  3. Run world simulation (physics, weapons, etc.)            │   │
│  │  4. Build & send snapshots to clients                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│  ┌───────────────────────────▼─────────────────────────────────┐   │
│  │                  Lag Compensation                            │   │
│  │  - Store hitbox history per tick                             │   │
│  │  - Rewind to client's view time for hit detection            │   │
│  │  - Apply damage at rewound positions                         │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              │ UDP (ENet)
                              │
┌─────────────────────────────────────────────────────────────────────┐
│                            CLIENT                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                     Prediction                               │   │
│  │  - Run same movement code locally                            │   │
│  │  - Store predicted states per input tick                     │   │
│  │  - Apply inputs immediately (responsive feel)                │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│  ┌───────────────────────────▼─────────────────────────────────┐   │
│  │                   Reconciliation                             │   │
│  │  - Receive authoritative state from server                   │   │
│  │  - Compare with predicted state at that tick                 │   │
│  │  - If mismatch: rewind, correct, re-predict forward          │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│  ┌───────────────────────────▼─────────────────────────────────┐   │
│  │                   Interpolation                              │   │
│  │  - Buffer multiple snapshots for other entities              │   │
│  │  - Interpolate between snapshots for smooth visuals          │   │
│  │  - Extrapolate if packets lost                               │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### Message Types

```cpp
// Client → Server
struct UserCmdMsg {
    static constexpr uint8_t ID = 0x01;
    Tick clientTick;
    SequenceNumber seq;
    std::vector<UserCmd> cmds;  // Redundant for reliability
};

struct ClientAckMsg {
    static constexpr uint8_t ID = 0x02;
    Tick lastReceivedServerTick;
    uint32_t snapshotAckBits;  // Bitmask of received snapshots
};

// Server → Client
struct SnapshotMsg {
    static constexpr uint8_t ID = 0x10;
    Tick serverTick;
    Tick clientTickAck;  // Last client tick processed
    SequenceNumber seq;
    uint32_t baseline;   // Reference snapshot for delta
    std::vector<EntityDelta> entities;
};

struct GameEventMsg {
    static constexpr uint8_t ID = 0x11;
    EventType type;
    std::vector<uint8_t> data;
};
```

### Snapshot Delta Compression

```cpp
struct EntityState {
    uint32_t networkId;
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 angles;
    uint16_t flags;
    // ... other replicated state
};

struct EntityDelta {
    uint32_t networkId;
    uint32_t changedFields;  // Bitmask
    // Only changed fields follow (variable size)
};

class SnapshotManager {
public:
    void storeSnapshot(Tick tick, const Snapshot& snapshot);
    Snapshot* getBaseline(uint32_t baselineId);
    
    std::vector<uint8_t> deltaEncode(const Snapshot& current, 
                                      const Snapshot& baseline);
    Snapshot deltaDecode(const std::vector<uint8_t>& delta,
                         const Snapshot& baseline);
};
```

### Lag Compensation

```cpp
class LagCompensation {
public:
    // Called each server tick to store state
    void recordTick(Tick tick, const EntityRegistry& registry);
    
    // Rewind world to approximate client view time
    void beginRewind(ClientId shooter, Tick viewTick);
    void endRewind();
    
    // Check if a shot hit at the rewound time
    HitResult traceShot(glm::vec3 origin, glm::vec3 direction,
                        float maxDistance, Entity shooter);
    
private:
    static constexpr int HISTORY_TICKS = 128;  // ~1 second at 128 tick
    std::array<TickSnapshot, HISTORY_TICKS> m_history;
    
    struct TickSnapshot {
        Tick tick;
        std::vector<HitboxState> hitboxes;
    };
};
```

## Renderer Module

### Purpose
Modern OpenGL 4.5+ deferred PBR renderer with IBL and post-processing.

### Pipeline Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Render Frame                                 │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    1. Shadow Pass (Optional)                         │
│  - Render depth from light POV                                      │
│  - Cascaded shadow maps for directional light                       │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    2. G-Buffer Pass (Deferred)                       │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌────────────┐ │
│  │    Albedo    │ │    Normal    │ │  Roughness   │ │   Depth    │ │
│  │    (RGB)     │ │ (Octahedral) │ │   Metallic   │ │ (Reversed) │ │
│  └──────────────┘ └──────────────┘ └──────────────┘ └────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    3. Lighting Pass                                  │
│  - Screen-space quad                                                │
│  - Sample G-buffer                                                  │
│  - PBR BRDF (Cook-Torrance)                                        │
│  - IBL (diffuse irradiance + specular prefiltered)                 │
│  - Point/spot lights (tiled or clustered culling)                  │
│  Output: HDR lighting buffer                                        │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    4. Forward Pass (Transparent)                     │
│  - Sorted back-to-front                                             │
│  - Alpha blending                                                   │
│  - Particles, glass, effects                                        │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    5. Post-Processing                                │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐      │
│  │  SSAO   │→│  Bloom  │→│   SSR   │→│Tonemap  │→│  FXAA   │      │
│  │(optional│ │         │ │(optional│ │ (ACES)  │ │         │      │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘      │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    6. UI Pass                                        │
│  - ImGui debug overlay                                              │
│  - Game HUD (custom)                                                │
└─────────────────────────────────────────────────────────────────────┘
```

### G-Buffer Layout

```cpp
enum class GBufferAttachment {
    Albedo,      // RGB8 - Base color
    Normal,      // RG16F - Octahedral encoded world normal
    Material,    // RGBA8 - R=roughness, G=metallic, B=AO, A=flags
    Emission,    // RGB16F - Emissive color (HDR)
    Depth        // D32F - Reversed-Z depth
};

struct GBuffer {
    GLuint framebuffer;
    GLuint albedoTexture;
    GLuint normalTexture;
    GLuint materialTexture;
    GLuint emissionTexture;
    GLuint depthTexture;
    
    void create(int width, int height);
    void bind();
    void bindTextures(int startUnit);
};
```

### PBR Material

```cpp
struct PBRMaterial {
    // Textures (bindless handles if supported)
    TextureHandle albedoMap;
    TextureHandle normalMap;
    TextureHandle metallicRoughnessMap;  // G=roughness, B=metallic (glTF)
    TextureHandle aoMap;
    TextureHandle emissiveMap;
    
    // Factors (multiplied with textures)
    glm::vec4 baseColorFactor{1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    glm::vec3 emissiveFactor{0.0f};
    
    // Rendering state
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};
```

### Shader Structure

```
assets/shaders/
├── common/
│   ├── uniforms.glsl      # Shared uniform blocks
│   ├── math.glsl          # Math utilities
│   ├── pbr.glsl           # BRDF functions
│   └── tonemapping.glsl   # Tone mapping operators
├── gbuffer/
│   ├── gbuffer.vert
│   └── gbuffer.frag
├── lighting/
│   ├── deferred.vert      # Fullscreen quad
│   ├── deferred.frag      # Main lighting
│   └── ibl.glsl           # IBL sampling
├── forward/
│   ├── forward.vert
│   └── forward.frag
├── postprocess/
│   ├── bloom.comp
│   ├── ssao.frag
│   ├── tonemap.frag
│   └── fxaa.frag
└── debug/
    ├── wireframe.vert
    └── wireframe.frag
```

## Assets Module

### Purpose
Load and manage game assets with streaming and LOD support.

### glTF Pipeline

```
Blender → glTF 2.0 Export → Asset Compiler → Runtime Loading
                               │
                               ▼
                    ┌──────────────────────┐
                    │   Asset Compiler     │
                    │  - Validate glTF     │
                    │  - Compress textures │
                    │  - Generate LODs     │
                    │  - Build runtime fmt │
                    └──────────────────────┘
```

### Asset Types

```cpp
struct MeshAsset {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    AABB boundingBox;
    std::vector<MeshLOD> lods;
};

struct TextureAsset {
    uint32_t width, height;
    TextureFormat format;
    uint32_t mipLevels;
    std::vector<uint8_t> data;  // Or KTX2 compressed
};

struct MaterialAsset {
    std::string name;
    PBRMaterial material;
};

struct ModelAsset {
    std::vector<MeshAsset> meshes;
    std::vector<MaterialAsset> materials;
    std::vector<AnimationAsset> animations;
    Skeleton skeleton;
};
```

### BSP Support

For loading original GoldSrc maps:

```cpp
class BSPLoader {
public:
    bool load(std::string_view path);
    
    // Geometry
    std::vector<BSPFace> faces;
    std::vector<glm::vec3> vertices;
    
    // Collision
    std::vector<BSPNode> nodes;
    std::vector<BSPLeaf> leaves;
    std::vector<BSPPlane> planes;
    BSPClipNode* clipnodes;
    
    // Visibility
    std::vector<uint8_t> visData;
    
    // Entities
    std::string entityLump;
};

// Collision trace through BSP
TraceResult BSP_TraceLine(const BSPLoader& bsp, 
                          glm::vec3 start, glm::vec3 end,
                          int hullIndex);
```

## Data Flow Examples

### Player Movement (Single Tick)

```
Client:
  1. Input System reads keyboard/mouse
  2. Build UserCmd (buttons, angles, move)
  3. Prediction: run PM_PlayerMove locally
  4. Store predicted state for tick N
  5. Send UserCmd to server

Server:
  1. Receive UserCmd for tick N
  2. Apply to player entity
  3. PM_PlayerMove (authoritative)
  4. Store result in snapshot
  5. Send snapshot to client

Client (on snapshot receive):
  1. Find predicted state for server's ack tick
  2. Compare with authoritative state
  3. If mismatch > threshold:
     a. Set position to server state
     b. Re-run prediction for ticks N+1 to current
  4. Interpolate other entities from snapshot history
```

### Weapon Fire (Hitscan)

```
Client:
  1. Player presses fire
  2. Play fire animation/sound (prediction)
  3. Send fire command to server

Server:
  1. Receive fire command with client timestamp
  2. Validate ammo, fire rate, etc.
  3. Lag compensation: rewind hitboxes to client view time
  4. Trace ray from player eye position
  5. If hit: calculate damage, apply to target
  6. Restore current hitbox positions
  7. Send hit confirmation/effects to clients
```

## Threading Model

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Main Thread                                   │
│  - Window events                                                    │
│  - Input handling                                                   │
│  - Game logic tick                                                  │
│  - Render submission                                                │
│  - ImGui                                                            │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              │ Job dispatch
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       Worker Threads (N)                             │
│  - Parallel entity updates                                          │
│  - Visibility culling                                               │
│  - Animation updates                                                │
│  - Asset loading (async)                                            │
│  - Physics broadphase                                               │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                       Network Thread                                 │
│  - Packet send/receive (ENet)                                       │
│  - Message serialization                                            │
│  - Connection management                                            │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                        Audio Thread                                  │
│  - OpenAL source updates                                            │
│  - Streaming audio decode                                           │
└─────────────────────────────────────────────────────────────────────┘
```

## Configuration System

```cpp
// ConVar system similar to Source engine
class ConVar {
public:
    ConVar(std::string_view name, float defaultValue, 
           uint32_t flags, std::string_view help);
    
    float getFloat() const;
    int getInt() const;
    bool getBool() const;
    void setValue(float value);
    
    static ConVar* find(std::string_view name);
};

// Usage
static ConVar sv_gravity("sv_gravity", 800.0f, 
                         FCVAR_SERVER | FCVAR_REPLICATED,
                         "World gravity");

static ConVar cl_interp("cl_interp", 0.03f,
                        FCVAR_CLIENT,
                        "Interpolation delay");
```

## Error Handling Strategy

- Use `std::expected<T, E>` (C++23) or custom Result type for recoverable errors
- Use exceptions only for truly exceptional cases (init failures)
- Validate at system boundaries (network input, file loading)
- Trust internal code paths; avoid defensive programming internally
- Log errors with context; use structured logging

```cpp
template<typename T, typename E = std::string>
using Result = std::expected<T, E>;

Result<MeshAsset> loadMesh(std::string_view path) {
    auto file = readFile(path);
    if (!file) {
        return std::unexpected(fmt::format("Failed to read: {}", path));
    }
    // ...
}
```


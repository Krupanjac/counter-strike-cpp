# Networking Protocol Specification

## Overview

This document specifies the network protocol for the Counter-Strike C++ Engine. The design follows Source/GoldSrc networking principles: authoritative server, client prediction, and lag compensation.

## Transport Layer

### ENet Configuration

```cpp
// Server
ENetHost* server = enet_host_create(
    &address,
    MAX_CLIENTS,      // 32 typical
    2,                // 2 channels (reliable, unreliable)
    0,                // No incoming bandwidth limit
    0                 // No outgoing bandwidth limit
);

// Client
ENetHost* client = enet_host_create(
    nullptr,          // Client doesn't bind
    1,                // 1 outgoing connection
    2,                // 2 channels
    0, 0
);
```

### Channels

| Channel | Type | Usage |
|---------|------|-------|
| 0 | Reliable Ordered | Connect, disconnect, game events, reliable state |
| 1 | Unreliable Sequenced | Snapshots, user commands |

## Message Format

### Header

All messages share a common header:

```
┌────────────────┬────────────────┬─────────────────────────────────┐
│  Message ID    │    Flags       │         Payload Size            │
│   (1 byte)     │   (1 byte)     │          (2 bytes)              │
├────────────────┴────────────────┴─────────────────────────────────┤
│                          Payload                                   │
│                       (variable size)                              │
└───────────────────────────────────────────────────────────────────┘
```

### Message Flags

```cpp
enum MessageFlags : uint8_t {
    MSG_NONE        = 0x00,
    MSG_COMPRESSED  = 0x01,  // Payload is zstd compressed
    MSG_FRAGMENTED  = 0x02,  // Part of larger message
    MSG_ENCRYPTED   = 0x04,  // Reserved for future
};
```

## Message Types

### Connection Messages (Reliable)

#### ClientConnect (0x01)
Client → Server: Initial connection request

```cpp
struct ClientConnectMsg {
    uint8_t id = 0x01;
    uint32_t protocolVersion;
    char playerName[32];
    uint8_t steamIdHash[20];  // For auth
};
```

#### ServerAccept (0x02)
Server → Client: Connection accepted

```cpp
struct ServerAcceptMsg {
    uint8_t id = 0x02;
    ClientId assignedClientId;
    Tick serverTick;
    uint32_t tickRate;
    uint32_t snapshotRate;
    // Server info (map, mode, etc.)
    char mapName[64];
    uint8_t gameMode;
};
```

#### ServerReject (0x03)
Server → Client: Connection rejected

```cpp
struct ServerRejectMsg {
    uint8_t id = 0x03;
    uint8_t reason;  // See RejectReason enum
    char message[128];
};

enum RejectReason : uint8_t {
    REJECT_SERVER_FULL = 0,
    REJECT_BANNED = 1,
    REJECT_VERSION_MISMATCH = 2,
    REJECT_AUTH_FAILED = 3,
};
```

#### ClientDisconnect (0x04)
Either direction: Clean disconnect

```cpp
struct ClientDisconnectMsg {
    uint8_t id = 0x04;
    uint8_t reason;
};
```

### Input Messages (Unreliable)

#### UserCmdMessage (0x10)
Client → Server: Player input

```cpp
struct UserCmdMsg {
    uint8_t id = 0x10;
    Tick clientTick;           // Client's current tick
    Tick lastReceivedTick;     // Last server tick received
    uint8_t cmdCount;          // Number of commands (redundancy)
    UserCmd cmds[cmdCount];    // Most recent first
};

struct UserCmd {
    Tick tick;
    uint16_t buttons;          // Bitmask (see InputButtons)
    int16_t viewAngleX;        // Pitch * 182.04 (fixed point)
    int16_t viewAngleY;        // Yaw * 182.04
    int8_t forwardMove;        // -128 to 127
    int8_t sideMove;
    int8_t upMove;
    uint8_t impulse;           // Weapon switch, etc.
    uint8_t weaponSelect;
    uint8_t seed;              // For spread/recoil RNG
};

enum InputButtons : uint16_t {
    IN_ATTACK       = 1 << 0,
    IN_JUMP         = 1 << 1,
    IN_DUCK         = 1 << 2,
    IN_FORWARD      = 1 << 3,
    IN_BACK         = 1 << 4,
    IN_USE          = 1 << 5,
    IN_MOVELEFT     = 1 << 6,
    IN_MOVERIGHT    = 1 << 7,
    IN_ATTACK2      = 1 << 8,
    IN_RELOAD       = 1 << 9,
    IN_SPEED        = 1 << 10,  // Walk
    IN_SCORE        = 1 << 11,
};
```

### Snapshot Messages (Unreliable)

#### SnapshotMessage (0x20)
Server → Client: World state update

```cpp
struct SnapshotMsg {
    uint8_t id = 0x20;
    Tick serverTick;
    Tick clientTickAck;        // Last processed client tick
    uint32_t baselineId;       // Delta reference (0 = full)
    uint16_t entityCount;
    EntityDelta entities[entityCount];
    // Player-specific data follows
    PlayerStateData playerState;
};
```

#### EntityDelta Format

```cpp
struct EntityDelta {
    uint16_t networkId;
    uint8_t updateType;        // See EntityUpdateType
    
    // If updateType == UPDATE_DELTA:
    uint32_t changedFields;    // Bitmask of changed fields
    // Variable-length encoded changed values follow
    
    // If updateType == UPDATE_FULL:
    EntityState fullState;
    
    // If updateType == UPDATE_REMOVE:
    // No additional data
};

enum EntityUpdateType : uint8_t {
    UPDATE_NONE = 0,
    UPDATE_DELTA = 1,
    UPDATE_FULL = 2,
    UPDATE_REMOVE = 3,
};
```

#### EntityState Fields

```cpp
struct EntityState {
    // Position (12 bytes)
    float posX, posY, posZ;
    
    // Velocity (6 bytes, half precision)
    uint16_t velX, velY, velZ;
    
    // Angles (4 bytes, compressed)
    uint16_t pitch;            // 0-65535 maps to 0-360
    uint16_t yaw;
    
    // Flags (2 bytes)
    uint16_t flags;
    
    // Animation (4 bytes)
    uint16_t sequence;
    uint16_t frame;
    
    // Health (1 byte, for players)
    uint8_t health;
    
    // Weapon (1 byte)
    uint8_t weaponId;
};

// Field bitmask for delta encoding
enum EntityField : uint32_t {
    FIELD_POSITION    = 1 << 0,
    FIELD_VELOCITY    = 1 << 1,
    FIELD_ANGLES      = 1 << 2,
    FIELD_FLAGS       = 1 << 3,
    FIELD_ANIMATION   = 1 << 4,
    FIELD_HEALTH      = 1 << 5,
    FIELD_WEAPON      = 1 << 6,
    // ... up to 32 fields
};
```

### Game Event Messages (Reliable)

#### GameEvent (0x30)
Server → Client: Game state changes

```cpp
struct GameEventMsg {
    uint8_t id = 0x30;
    uint16_t eventType;
    Tick eventTick;
    uint16_t dataSize;
    uint8_t data[dataSize];
};

enum GameEventType : uint16_t {
    EVENT_PLAYER_SPAWN = 1,
    EVENT_PLAYER_DEATH = 2,
    EVENT_ROUND_START = 3,
    EVENT_ROUND_END = 4,
    EVENT_BOMB_PLANTED = 5,
    EVENT_BOMB_DEFUSED = 6,
    EVENT_WEAPON_FIRE = 7,
    EVENT_PLAYER_HURT = 8,
    // ...
};
```

### Voice Messages (Unreliable)

#### VoiceData (0x40)
Either direction: Voice chat

```cpp
struct VoiceDataMsg {
    uint8_t id = 0x40;
    ClientId speakerId;
    uint16_t dataSize;
    uint8_t compressedAudio[dataSize];  // Opus encoded
};
```

## Delta Compression

### Baseline System

```
Server maintains:
- Baseline 0: Default entity state (all zeros/defaults)
- Baseline N: Confirmed client snapshot

Client acknowledges:
- Sends lastReceivedTick in UserCmdMsg
- Server uses acknowledged tick as delta baseline

Delta encoding:
1. Compare current state with baseline
2. Build bitmask of changed fields
3. Write only changed values
4. Use variable-length encoding for values
```

### Compression Strategies

```cpp
// Position: Full precision (12 bytes)
void writePosition(BitWriter& w, glm::vec3 pos) {
    w.writeFloat(pos.x);
    w.writeFloat(pos.y);
    w.writeFloat(pos.z);
}

// Velocity: Half precision (6 bytes)
void writeVelocity(BitWriter& w, glm::vec3 vel) {
    w.writeFloat16(vel.x);
    w.writeFloat16(vel.y);
    w.writeFloat16(vel.z);
}

// Angles: 16-bit normalized (4 bytes)
void writeAngles(BitWriter& w, glm::vec2 angles) {
    w.writeUint16(uint16_t(angles.x / 360.0f * 65535.0f));
    w.writeUint16(uint16_t(angles.y / 360.0f * 65535.0f));
}

// Small integers: Variable length
void writeVarInt(BitWriter& w, uint32_t value) {
    while (value >= 0x80) {
        w.writeByte((value & 0x7F) | 0x80);
        value >>= 7;
    }
    w.writeByte(value);
}
```

## Client Prediction

### Prediction Buffer

```cpp
class PredictionBuffer {
    static constexpr int MAX_HISTORY = 128;
    
    struct PredictedState {
        Tick tick;
        glm::vec3 position;
        glm::vec3 velocity;
        int flags;
        UserCmd cmd;
    };
    
    std::array<PredictedState, MAX_HISTORY> m_history;
    int m_head = 0;
    
public:
    void store(Tick tick, const PredictedState& state);
    const PredictedState* get(Tick tick) const;
    void clearBefore(Tick tick);
};
```

### Reconciliation Algorithm

```cpp
void Client::reconcile(Tick serverTick, const EntityState& serverState) {
    // Find our predicted state for this tick
    auto* predicted = m_predictionBuffer.get(serverTick);
    if (!predicted) return;  // Too old, skip
    
    // Calculate error
    float posError = glm::distance(predicted->position, serverState.position);
    float velError = glm::distance(predicted->velocity, serverState.velocity);
    
    // Threshold check (small errors are acceptable)
    if (posError < 0.1f && velError < 1.0f) {
        return;  // Close enough, no correction needed
    }
    
    // Correction needed
    LOG_DEBUG("Misprediction at tick {}: pos error={}, vel error={}", 
              serverTick, posError, velError);
    
    // Start from server's authoritative state
    PlayerMove pm;
    pm.origin = serverState.position;
    pm.velocity = serverState.velocity;
    pm.flags = serverState.flags;
    
    // Re-predict forward from serverTick to current tick
    for (Tick t = serverTick + 1; t <= m_currentTick; ++t) {
        auto* cmd = m_predictionBuffer.get(t);
        if (!cmd) break;
        
        pm.cmd = cmd->cmd;
        pm.frametime = TICK_INTERVAL;
        PM_PlayerMove(&pm);
        
        // Update stored prediction
        m_predictionBuffer.store(t, {t, pm.origin, pm.velocity, pm.flags, cmd->cmd});
    }
    
    // Update local player state
    m_localPlayer.position = pm.origin;
    m_localPlayer.velocity = pm.velocity;
}
```

## Entity Interpolation

### Snapshot Buffer

```cpp
class InterpolationBuffer {
    static constexpr int BUFFER_SIZE = 3;
    static constexpr float INTERP_DELAY = 0.1f;  // 100ms
    
    struct BufferedSnapshot {
        Tick tick;
        float serverTime;
        std::unordered_map<uint16_t, EntityState> entities;
    };
    
    std::array<BufferedSnapshot, BUFFER_SIZE> m_buffer;
    
public:
    void addSnapshot(const SnapshotMsg& snapshot);
    
    EntityState interpolate(uint16_t entityId, float renderTime) {
        // Find two snapshots that bracket renderTime
        auto* from = findSnapshotBefore(renderTime);
        auto* to = findSnapshotAfter(renderTime);
        
        if (!from || !to) {
            // Extrapolate or use last known
            return extrapolate(entityId, renderTime);
        }
        
        float t = (renderTime - from->serverTime) / 
                  (to->serverTime - from->serverTime);
        
        return lerpEntityState(from->entities[entityId],
                               to->entities[entityId], t);
    }
};
```

## Lag Compensation

### Server-Side Rewind

```cpp
class LagCompensation {
    static constexpr int HISTORY_SIZE = 128;  // ~1 second at 128 tick
    static constexpr float MAX_UNLAG = 1.0f;  // Max rewind time
    
    struct HitboxSnapshot {
        std::vector<HitboxData> hitboxes;
    };
    
    struct TickRecord {
        Tick tick;
        float time;
        std::unordered_map<Entity, HitboxSnapshot> entityHitboxes;
    };
    
    std::array<TickRecord, HISTORY_SIZE> m_history;
    
public:
    void recordTick(Tick tick, float time, const EntityRegistry& reg) {
        auto& record = m_history[tick % HISTORY_SIZE];
        record.tick = tick;
        record.time = time;
        record.entityHitboxes.clear();
        
        auto view = reg.view<TransformComponent, HitboxComponent>();
        for (auto [entity, transform, hitbox] : view.each()) {
            record.entityHitboxes[entity] = computeHitboxes(transform, hitbox);
        }
    }
    
    HitResult traceWithRewind(
        Entity shooter,
        glm::vec3 origin,
        glm::vec3 direction,
        float clientTime
    ) {
        // Clamp rewind time
        float currentTime = getCurrentServerTime();
        float rewindTime = std::min(clientTime, currentTime - MAX_UNLAG);
        
        // Find closest tick
        Tick targetTick = timeToTick(rewindTime);
        auto* record = getRecord(targetTick);
        
        if (!record) {
            // Fallback to current hitboxes
            return traceCurrent(shooter, origin, direction);
        }
        
        // Trace against historical hitboxes
        HitResult best;
        best.fraction = 1.0f;
        
        for (auto& [entity, snapshot] : record->entityHitboxes) {
            if (entity == shooter) continue;  // Don't hit self
            
            for (auto& hitbox : snapshot.hitboxes) {
                float fraction = traceRayBox(origin, direction, hitbox);
                if (fraction < best.fraction) {
                    best.fraction = fraction;
                    best.entity = entity;
                    best.hitbox = hitbox.group;
                    best.position = origin + direction * fraction;
                }
            }
        }
        
        return best;
    }
};
```

### Conditional Lag Compensation

To prevent abuse by high-ping players:

```cpp
HitResult validateShot(
    Entity shooter,
    const UserCmd& cmd,
    float clientLatency
) {
    // Apply diminishing returns for high latency
    float effectiveLatency = clientLatency;
    if (clientLatency > 0.1f) {  // 100ms threshold
        float excess = clientLatency - 0.1f;
        effectiveLatency = 0.1f + excess * 0.5f;  // 50% penalty
    }
    
    // Clamp to max unlag
    effectiveLatency = std::min(effectiveLatency, MAX_UNLAG);
    
    float rewindTime = m_currentTime - effectiveLatency;
    return traceWithRewind(shooter, cmd.origin, cmd.direction, rewindTime);
}
```

## Bandwidth Considerations

### Typical Message Sizes

| Message | Size (bytes) | Frequency |
|---------|--------------|-----------|
| UserCmdMsg (1 cmd) | ~20 | Every tick (client) |
| UserCmdMsg (3 cmds) | ~50 | With redundancy |
| SnapshotMsg (10 players) | ~200-500 | 64-128 Hz |
| SnapshotMsg (delta) | ~50-150 | Typical delta |
| GameEvent | 20-100 | Variable |

### Bandwidth Targets

- Client upload: ~50 KB/s (input + voice)
- Client download: ~100-200 KB/s (snapshots + events)
- Server per-client: ~100-200 KB/s

### Rate Limiting

```cpp
struct ClientRateLimit {
    int maxBytesPerSecond = 128 * 1024;  // 128 KB/s
    int maxPacketsPerSecond = 200;
    
    int bytesSent = 0;
    int packetsSent = 0;
    float lastResetTime = 0;
    
    bool canSend(int bytes) {
        float now = getCurrentTime();
        if (now - lastResetTime >= 1.0f) {
            bytesSent = 0;
            packetsSent = 0;
            lastResetTime = now;
        }
        
        return bytesSent + bytes <= maxBytesPerSecond &&
               packetsSent < maxPacketsPerSecond;
    }
};
```

## Security Considerations

### Input Validation

```cpp
bool validateUserCmd(const UserCmd& cmd, const PlayerState& player) {
    // Check view angles are reasonable
    if (cmd.viewAngleX < -89 || cmd.viewAngleX > 89) return false;
    if (cmd.viewAngleY < 0 || cmd.viewAngleY >= 360) return false;
    
    // Check movement values are in range
    if (cmd.forwardMove < -128 || cmd.forwardMove > 127) return false;
    if (cmd.sideMove < -128 || cmd.sideMove > 127) return false;
    
    // Check tick is reasonable (not too far in future)
    if (cmd.tick > m_serverTick + 10) return false;
    
    // Check buttons are valid
    if (cmd.buttons & ~ALL_VALID_BUTTONS) return false;
    
    return true;
}
```

### Movement Validation

```cpp
void validateMovement(ClientId client, const EntityState& claimed) {
    auto& expected = m_serverStates[client];
    
    // Position should be close to server simulation
    float distance = glm::distance(claimed.position, expected.position);
    if (distance > MAX_POSITION_ERROR) {
        LOG_WARN("Client {} position mismatch: {} units", client, distance);
        // Force correction
        sendCorrection(client, expected);
    }
    
    // Speed should be within limits
    float speed = glm::length(claimed.velocity);
    if (speed > MAX_ALLOWED_SPEED * 1.1f) {  // 10% tolerance
        LOG_WARN("Client {} exceeds max speed: {}", client, speed);
        // Potential speedhack, log for review
        flagForReview(client, "speed_anomaly");
    }
}
```

## Protocol Versioning

```cpp
constexpr uint32_t PROTOCOL_VERSION = 1;

// Version check on connect
if (clientVersion != PROTOCOL_VERSION) {
    sendReject(REJECT_VERSION_MISMATCH,
               fmt::format("Expected version {}, got {}", 
                          PROTOCOL_VERSION, clientVersion));
}
```


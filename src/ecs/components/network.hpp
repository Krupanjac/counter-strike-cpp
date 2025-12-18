#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"
#include <array>

namespace cscpp::ecs {

/**
 * @brief Network identity component
 * 
 * Identifies entities for network replication.
 */
struct NetworkIdComponent {
    NetworkId networkId = INVALID_NETWORK_ID;
    bool isReplicated = true;
    
    /// Entity owner (for client-authoritative entities like projectiles)
    ClientId owner = INVALID_CLIENT_ID;
};

/**
 * @brief Entity state for network transmission
 */
struct EntityState {
    NetworkId networkId = INVALID_NETWORK_ID;
    Vec3 position{0.0f};
    Vec3 velocity{0.0f};
    Vec3 angles{0.0f};  // Pitch, yaw, roll
    u16 flags = 0;
    u8 health = 100;
    u8 weaponId = 0;
    u16 sequence = 0;   // Animation sequence
    f32 frame = 0.0f;   // Animation frame
};

/**
 * @brief Client-side interpolation state
 */
struct InterpolationComponent {
    static constexpr size_t HISTORY_SIZE = 3;
    
    struct Snapshot {
        Tick tick = 0;
        f32 time = 0.0f;
        Vec3 position{0.0f};
        Vec3 velocity{0.0f};
        Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };
    
    std::array<Snapshot, HISTORY_SIZE> history;
    size_t historyHead = 0;
    f32 interpTime = 0.0f;
    
    void addSnapshot(Tick tick, f32 time, Vec3 pos, Vec3 vel, Quat rot) {
        historyHead = (historyHead + 1) % HISTORY_SIZE;
        history[historyHead] = {tick, time, pos, vel, rot};
    }
    
    /// Get interpolated position at given time
    Vec3 getInterpolatedPosition(f32 time) const;
    Quat getInterpolatedRotation(f32 time) const;
};

/**
 * @brief Client-side prediction state
 */
struct PredictionComponent {
    static constexpr size_t BUFFER_SIZE = 128;
    
    struct PredictedState {
        Tick tick = 0;
        Vec3 position{0.0f};
        Vec3 velocity{0.0f};
        i32 flags = 0;
        UserCmd cmd;
    };
    
    std::array<PredictedState, BUFFER_SIZE> buffer;
    Tick oldestTick = 0;
    Tick newestTick = 0;
    
    /// Store predicted state for reconciliation
    void store(Tick tick, const PredictedState& state) {
        size_t index = tick % BUFFER_SIZE;
        buffer[index] = state;
        buffer[index].tick = tick;
        
        if (tick > newestTick) newestTick = tick;
        if (oldestTick == 0 || tick < oldestTick) oldestTick = tick;
    }
    
    /// Get predicted state at tick
    const PredictedState* get(Tick tick) const {
        if (tick < oldestTick || tick > newestTick) return nullptr;
        size_t index = tick % BUFFER_SIZE;
        if (buffer[index].tick != tick) return nullptr;
        return &buffer[index];
    }
    
    /// Clear predictions before tick
    void clearBefore(Tick tick) {
        oldestTick = tick;
    }
};

/**
 * @brief Server-side hitbox history for lag compensation
 */
struct HitboxHistoryComponent {
    static constexpr size_t HISTORY_SIZE = 128;  // ~1 second at 128 tick
    
    struct HistoryEntry {
        Tick tick = 0;
        f32 time = 0.0f;
        Vec3 position{0.0f};
        Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        // Could include full hitbox transforms for accurate rewind
    };
    
    std::array<HistoryEntry, HISTORY_SIZE> history;
    size_t historyHead = 0;
    
    void record(Tick tick, f32 time, Vec3 pos, Quat rot) {
        historyHead = (historyHead + 1) % HISTORY_SIZE;
        history[historyHead] = {tick, time, pos, rot};
    }
    
    /// Get historical state at tick (for lag compensation)
    const HistoryEntry* getAtTick(Tick tick) const {
        for (const auto& entry : history) {
            if (entry.tick == tick) {
                return &entry;
            }
        }
        return nullptr;
    }
    
    /// Get historical state closest to time
    const HistoryEntry* getAtTime(f32 time) const;
};

/**
 * @brief Network statistics component
 */
struct NetworkStatsComponent {
    f32 ping = 0.0f;              // Round-trip time in seconds
    f32 jitter = 0.0f;            // Ping variation
    f32 packetLoss = 0.0f;        // 0-1 packet loss ratio
    u32 packetsReceived = 0;
    u32 packetsSent = 0;
    f32 incomingBandwidth = 0.0f; // Bytes/sec
    f32 outgoingBandwidth = 0.0f;
};

/**
 * @brief Local player marker component
 * 
 * Attached to the entity controlled by the local client.
 */
struct LocalPlayerComponent {
    // This is just a tag component
    // Presence indicates this is the local player
};

/**
 * @brief Remote player marker component
 */
struct RemotePlayerComponent {
    ClientId clientId = INVALID_CLIENT_ID;
};

} // namespace cscpp::ecs


#pragma once

/**
 * @file messages.hpp
 * @brief Network message definitions
 * 
 * Defines all message types used for client-server communication.
 */

#include "core/types.hpp"
#include "core/math/math.hpp"
#include "core/platform/input.hpp"
#include <vector>

namespace cscpp::network {

// ============================================================================
// Message IDs
// ============================================================================

enum class MessageId : u8 {
    // Connection (0x00 - 0x0F)
    ClientConnect       = 0x01,
    ServerAccept        = 0x02,
    ServerReject        = 0x03,
    ClientDisconnect    = 0x04,
    ServerDisconnect    = 0x05,
    Heartbeat           = 0x06,
    
    // Input (0x10 - 0x1F)
    UserCmd             = 0x10,
    ClientAck           = 0x11,
    
    // Snapshots (0x20 - 0x2F)
    Snapshot            = 0x20,
    FullSnapshot        = 0x21,
    DeltaSnapshot       = 0x22,
    
    // Game Events (0x30 - 0x3F)
    GameEvent           = 0x30,
    ChatMessage         = 0x31,
    ServerInfo          = 0x32,
    
    // Voice (0x40 - 0x4F)
    VoiceData           = 0x40,
};

// ============================================================================
// Message Flags
// ============================================================================

enum class MessageFlags : u8 {
    None        = 0x00,
    Compressed  = 0x01,
    Fragmented  = 0x02,
    Encrypted   = 0x04,
    Reliable    = 0x08,
};

inline MessageFlags operator|(MessageFlags a, MessageFlags b) {
    return static_cast<MessageFlags>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline MessageFlags operator&(MessageFlags a, MessageFlags b) {
    return static_cast<MessageFlags>(static_cast<u8>(a) & static_cast<u8>(b));
}

// ============================================================================
// Message Header
// ============================================================================

struct MessageHeader {
    MessageId id;
    MessageFlags flags;
    u16 payloadSize;
};

// ============================================================================
// Connection Messages
// ============================================================================

struct ClientConnectMsg {
    static constexpr MessageId ID = MessageId::ClientConnect;
    
    u32 protocolVersion;
    char playerName[32];
    u8 passwordHash[32];  // For server password
};

struct ServerAcceptMsg {
    static constexpr MessageId ID = MessageId::ServerAccept;
    
    ClientId clientId;
    Tick serverTick;
    u32 tickRate;
    u32 snapshotRate;
    char mapName[64];
    u8 gameMode;
};

struct ServerRejectMsg {
    static constexpr MessageId ID = MessageId::ServerReject;
    
    enum class Reason : u8 {
        ServerFull = 0,
        Banned = 1,
        VersionMismatch = 2,
        BadPassword = 3,
        AuthFailed = 4,
    };
    
    Reason reason;
    char message[128];
};

struct DisconnectMsg {
    static constexpr MessageId ID = MessageId::ClientDisconnect;
    
    enum class Reason : u8 {
        UserQuit = 0,
        Kicked = 1,
        Banned = 2,
        Timeout = 3,
        ServerShutdown = 4,
    };
    
    Reason reason;
    char message[64];
};

// ============================================================================
// Input Messages
// ============================================================================

struct UserCmdMsg {
    static constexpr MessageId ID = MessageId::UserCmd;
    
    Tick clientTick;
    Tick lastReceivedServerTick;
    u8 cmdCount;
    std::vector<UserCmd> cmds;
};

struct ClientAckMsg {
    static constexpr MessageId ID = MessageId::ClientAck;
    
    Tick lastReceivedServerTick;
    u32 snapshotAckBits;  // Bitmask of received snapshots
};

// ============================================================================
// Snapshot Messages
// ============================================================================

struct EntityState {
    NetworkId networkId;
    Vec3 position;
    Vec3 velocity;
    Vec3 angles;
    u16 flags;
    u8 health;
    u8 weaponId;
    u16 animSequence;
    f32 animFrame;
};

struct EntityDelta {
    NetworkId networkId;
    
    enum class UpdateType : u8 {
        None = 0,
        Delta = 1,
        Full = 2,
        Remove = 3,
    };
    
    UpdateType updateType;
    u32 changedFields;
    std::vector<u8> data;  // Serialized changed fields
};

struct SnapshotMsg {
    static constexpr MessageId ID = MessageId::Snapshot;
    
    Tick serverTick;
    Tick clientTickAck;
    SequenceNumber sequenceNumber;
    u32 baselineId;
    std::vector<EntityDelta> entities;
};

// ============================================================================
// Game Event Messages
// ============================================================================

struct GameEventMsg {
    static constexpr MessageId ID = MessageId::GameEvent;
    
    enum class EventType : u16 {
        PlayerSpawn = 1,
        PlayerDeath = 2,
        PlayerHurt = 3,
        WeaponFire = 4,
        RoundStart = 5,
        RoundEnd = 6,
        BombPlanted = 7,
        BombDefused = 8,
        BombExploded = 9,
        HostageRescued = 10,
        BuyZoneEnter = 11,
        BuyZoneLeave = 12,
    };
    
    EventType eventType;
    Tick eventTick;
    std::vector<u8> eventData;
};

struct ChatMsg {
    static constexpr MessageId ID = MessageId::ChatMessage;
    
    ClientId senderId;
    bool teamOnly;
    char message[256];
};

// ============================================================================
// Voice Messages
// ============================================================================

struct VoiceDataMsg {
    static constexpr MessageId ID = MessageId::VoiceData;
    
    ClientId speakerId;
    u16 dataSize;
    std::vector<u8> compressedAudio;  // Opus encoded
};

// ============================================================================
// Entity Field Flags
// ============================================================================

enum class EntityField : u32 {
    Position    = 1 << 0,
    Velocity    = 1 << 1,
    Angles      = 1 << 2,
    Flags       = 1 << 3,
    Health      = 1 << 4,
    Weapon      = 1 << 5,
    Animation   = 1 << 6,
    // Add more as needed
};

// ============================================================================
// Protocol Constants
// ============================================================================

constexpr u32 PROTOCOL_VERSION = 1;
constexpr u32 MAX_PACKET_SIZE = 1400;  // MTU safe size
constexpr u32 MAX_SNAPSHOT_ENTITIES = 256;
constexpr u32 MAX_PENDING_COMMANDS = 64;

} // namespace cscpp::network


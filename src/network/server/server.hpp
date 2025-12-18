#pragma once

/**
 * @file server.hpp
 * @brief Authoritative game server
 */

#include "core/types.hpp"
#include "network/protocol/messages.hpp"
#include <unordered_map>
#include <memory>

namespace cscpp::network {

// Forward declarations
class ClientConnection;

/**
 * @brief Server configuration
 */
struct ServerConfig {
    u16 port = 27015;
    i32 maxClients = 32;
    i32 tickRate = 128;
    std::string serverName = "CS C++ Server";
};

/**
 * @brief Authoritative game server
 */
class Server {
public:
    Server() = default;
    ~Server();
    
    /// Start the server
    Result<void> start(const ServerConfig& config);
    
    /// Stop the server
    void stop();
    
    /// Process network events
    void update(f32 deltaTime);
    
    /// Get server tick
    Tick getCurrentTick() const { return m_currentTick; }
    
    /// Get tick rate
    i32 getTickRate() const { return m_config.tickRate; }
    
    /// Get connected client count
    i32 getClientCount() const { return static_cast<i32>(m_clients.size()); }
    
    /// Kick a client
    void kickClient(ClientId clientId, const std::string& reason);
    
    /// Broadcast message to all clients
    void broadcast(const void* data, size_t size, bool reliable);
    
    /// Send message to specific client
    void sendTo(ClientId clientId, const void* data, size_t size, bool reliable);
    
private:
    void onClientConnect(/* ENet peer */);
    void onClientDisconnect(ClientId clientId);
    void onReceive(ClientId clientId, const void* data, size_t size);
    
    void processUserCmd(ClientId clientId, const UserCmdMsg& msg);
    void sendSnapshots();
    
    ServerConfig m_config;
    std::unordered_map<ClientId, std::unique_ptr<ClientConnection>> m_clients;
    
    Tick m_currentTick = 0;
    ClientId m_nextClientId = 0;
    
    bool m_running = false;
    
    // ENet host would be here
    // ENetHost* m_host = nullptr;
};

/**
 * @brief Represents a connected client
 */
class ClientConnection {
public:
    ClientConnection(ClientId id);
    
    ClientId getId() const { return m_clientId; }
    
    /// Get client ping (RTT)
    f32 getPing() const { return m_ping; }
    
    /// Get last acknowledged tick
    Tick getLastAckedTick() const { return m_lastAckedTick; }
    
    /// Add user command
    void addUserCmd(const UserCmd& cmd);
    
    /// Get pending user commands
    const std::vector<UserCmd>& getPendingCmds() const { return m_pendingCmds; }
    
    /// Clear processed commands
    void clearProcessedCmds(Tick upToTick);
    
private:
    ClientId m_clientId;
    f32 m_ping = 0.0f;
    Tick m_lastAckedTick = 0;
    std::vector<UserCmd> m_pendingCmds;
    
    // ENet peer would be here
    // ENetPeer* m_peer = nullptr;
};

} // namespace cscpp::network


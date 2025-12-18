/**
 * @file server_main.cpp
 * @brief Dedicated server entry point
 * 
 * This is the main entry point for the Counter-Strike C++ dedicated server.
 * The server is headless (no rendering) and runs the authoritative game simulation.
 */

#include "core/core.hpp"
#include "core/logging/logger.hpp"
#include "core/math/math.hpp"
#include "ecs/ecs.hpp"
#include "movement/pm_shared/pm_shared.hpp"

#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

using namespace cscpp;

// Global shutdown flag
std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_INFO("Shutdown signal received");
        g_shutdown = true;
    }
}

/**
 * @brief Server configuration
 */
struct ServerConfig {
    std::string mapName = "de_dust2";
    i32 maxPlayers = 32;
    i32 tickRate = 128;
    u16 port = 27015;
    std::string serverName = "Counter-Strike C++ Server";
    std::string rconPassword = "";
};

/**
 * @brief Dedicated server application
 */
class ServerApplication {
public:
    bool initialize(const ServerConfig& config) {
        m_config = config;
        
        // Initialize logging
        Logger::initialize("cscpp_server.log", LogLevel::Info, LogLevel::Debug);
        LOG_INFO("Counter-Strike C++ Dedicated Server");
        LOG_INFO("Version: {}", VERSION_STRING);
        
        // Initialize ECS world
        m_world = std::make_unique<ecs::World>();
        
        // Initialize movement variables
        m_moveVars.gravity = 800.0f;
        m_moveVars.stopSpeed = 100.0f;
        m_moveVars.maxSpeed = 320.0f;
        m_moveVars.accelerate = 10.0f;
        m_moveVars.airAccelerate = 10.0f;  // Set to 100 for classic bhop
        m_moveVars.friction = 4.0f;
        m_moveVars.stepSize = 18.0f;
        m_moveVars.maxVelocity = 2000.0f;
        
        // Calculate tick interval
        m_tickInterval = 1.0f / static_cast<f32>(config.tickRate);
        
        LOG_INFO("Server initialized");
        LOG_INFO("  Map: {}", config.mapName);
        LOG_INFO("  Max players: {}", config.maxPlayers);
        LOG_INFO("  Tick rate: {} Hz ({:.4f}s interval)", config.tickRate, m_tickInterval);
        LOG_INFO("  Port: {}", config.port);
        
        return true;
    }
    
    void shutdown() {
        LOG_INFO("Server shutting down...");
        
        m_world.reset();
        
        Logger::shutdown();
    }
    
    void run() {
        LOG_INFO("Starting server tick loop");
        
        auto lastTime = Clock::now();
        f32 accumulator = 0.0f;
        
        Tick tick = 0;
        
        while (!g_shutdown) {
            auto currentTime = Clock::now();
            f32 deltaTime = std::chrono::duration<f32>(currentTime - lastTime).count();
            lastTime = currentTime;
            
            // Cap delta to prevent spiral of death
            if (deltaTime > 0.25f) {
                deltaTime = 0.25f;
            }
            
            accumulator += deltaTime;
            
            // Process ticks
            while (accumulator >= m_tickInterval) {
                processTick(tick);
                ++tick;
                accumulator -= m_tickInterval;
            }
            
            // Sleep to avoid busy waiting
            f32 sleepTime = m_tickInterval - accumulator;
            if (sleepTime > 0.001f) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(static_cast<i64>(sleepTime * 1000000.0f))
                );
            }
        }
        
        LOG_INFO("Server stopped after {} ticks", tick);
    }
    
private:
    void processTick(Tick tick) {
        // 1. Receive and queue client inputs
        receiveClientInputs();
        
        // 2. Process inputs and run movement for each player
        processPlayerMovement(tick);
        
        // 3. Run world simulation (projectiles, game logic)
        simulateWorld();
        
        // 4. Build and send snapshots to clients
        sendSnapshots(tick);
        
        // Update world tick
        m_world->setCurrentTick(tick);
    }
    
    void receiveClientInputs() {
        // Network receive would go here
        // Parse incoming UserCmd packets from clients
    }
    
    void processPlayerMovement(Tick tick) {
        auto& registry = m_world->getRegistry();
        
        // Get all player entities with movement components
        auto view = registry.view<
            ecs::TransformComponent,
            ecs::VelocityComponent,
            ecs::MovementComponent,
            ecs::InputComponent,
            ecs::PlayerComponent
        >();
        
        for (auto [entity, transform, velocity, movement, input, player] : view.each()) {
            // Skip dead players
            if (!player.isAlive) continue;
            
            // Get the command for this tick
            UserCmd* cmd = input.getCmd(tick);
            if (!cmd) continue;
            
            // Build PlayerMove structure
            movement::PlayerMove pm;
            pm.initHulls();
            
            // Set position and velocity
            pm.origin = transform.position;
            pm.velocity = velocity.linear;
            pm.baseVelocity = movement.baseVelocity;
            pm.viewAngles = cmd->viewAngles;
            
            // Set input
            pm.forwardMove = cmd->forwardMove * 400.0f;  // Scale to units
            pm.sideMove = cmd->sideMove * 400.0f;
            pm.buttons = cmd->buttons;
            pm.oldButtons = input.latestCmd.buttons;
            
            // Set state
            pm.flags = movement.flags;
            pm.waterLevel = movement.waterLevel;
            pm.useHull = movement.useHull;
            pm.duckTime = movement.duckTime;
            pm.inDuck = movement.inDuck;
            pm.fallVelocity = movement.fallVelocity;
            pm.maxSpeed = movement.maxSpeed;
            pm.dead = !player.isAlive;
            
            // Set timing
            pm.frameTime = m_tickInterval;
            
            // Set movement variables
            pm.moveVars = &m_moveVars;
            
            // Set trace function (collision detection)
            // pm.traceFunc = &worldTraceFunction;
            
            // Run movement simulation
            movement::PM_PlayerMove(&pm);
            
            // Update entity state from movement result
            transform.position = pm.origin;
            velocity.linear = pm.velocity;
            movement.baseVelocity = pm.baseVelocity;
            movement.flags = pm.flags;
            movement.useHull = pm.useHull;
            movement.duckTime = pm.duckTime;
            movement.inDuck = pm.inDuck;
            movement.fallVelocity = pm.fallVelocity;
            
            // Update processed tick
            input.lastProcessedTick = tick;
        }
    }
    
    void simulateWorld() {
        // Projectile simulation, game logic, etc.
    }
    
    void sendSnapshots(Tick tick) {
        (void)tick;
        // Build delta-compressed snapshots and send to clients
    }
    
    ServerConfig m_config;
    std::unique_ptr<ecs::World> m_world;
    movement::MoveVars m_moveVars;
    f32 m_tickInterval = 1.0f / 128.0f;
};

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    ServerConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-port" && i + 1 < argc) {
            config.port = static_cast<u16>(std::stoi(argv[++i]));
        } else if (arg == "-maxplayers" && i + 1 < argc) {
            config.maxPlayers = std::stoi(argv[++i]);
        } else if (arg == "-tickrate" && i + 1 < argc) {
            config.tickRate = std::stoi(argv[++i]);
        } else if (arg == "-map" && i + 1 < argc) {
            config.mapName = argv[++i];
        }
    }
    
    ServerApplication server;
    
    if (!server.initialize(config)) {
        return 1;
    }
    
    server.run();
    server.shutdown();
    
    return 0;
}


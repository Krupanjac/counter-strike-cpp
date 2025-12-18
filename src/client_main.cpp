/**
 * @file client_main.cpp
 * @brief Game client entry point
 * 
 * This is the main entry point for the Counter-Strike C++ game client.
 * It initializes all subsystems and runs the main game loop.
 */

#include "core/core.hpp"
#include "ecs/ecs.hpp"
#include "renderer/simple_renderer.hpp"
#include "assets/bsp/simple_bsp_loader.hpp"
#include "assets/gltf/simple_gltf_loader.hpp"

#include <SDL2/SDL.h>
#include <glad/glad.h>

using namespace cscpp;

/**
 * @brief Client application class
 */
class ClientApplication {
public:
    bool initialize() {
        // Initialize logging
        Logger::initialize("cscpp_client.log", LogLevel::Debug);
        LOG_INFO("Counter-Strike C++ Client v{}", VERSION_STRING);
        
        // Create window
        WindowConfig windowConfig;
        windowConfig.title = "Counter-Strike C++";
        windowConfig.width = 1920;
        windowConfig.height = 1080;
        windowConfig.vsync = true;
        
        auto result = m_window.create(windowConfig);
        if (!result) {
            LOG_CRITICAL("Failed to create window: {}", result.error().message);
            return false;
        }
        
        // Initialize ECS world
        m_world = std::make_unique<ecs::World>();
        
        // Initialize renderer
        auto renderResult = m_renderer.initialize();
        if (!renderResult) {
            LOG_CRITICAL("Failed to initialize renderer: {}", renderResult.error().message);
            return false;
        }
        
        // Set viewport
        auto fbSize = m_window.getFramebufferSize();
        m_renderer.setViewport(fbSize.x, fbSize.y);
        
        // Load map
        assets::SimpleBSPLoader bspLoader;
        auto mapResult = bspLoader.load("assets/maps/de_dust2.bsp");
        if (mapResult) {
            m_mapMesh = std::move(*mapResult);  // Move instead of copy
            LOG_INFO("Map loaded successfully");
            LOG_INFO("Map has {} textures loaded", m_mapMesh.textureMap.size());
            if (!m_mapMesh.textureMap.empty()) {
                for (const auto& [index, texID] : m_mapMesh.textureMap) {
                    LOG_INFO("  Texture index {} -> OpenGL texture ID {}", index, texID);
                }
            }
        } else {
            LOG_WARN("Failed to load map, using test mesh: {}", mapResult.error().message);
            m_mapMesh = std::move(bspLoader.createTestMesh());  // Move instead of copy
        }
        
        // Load weapon
        assets::SimpleGLTFLoader gltfLoader;
        auto weaponResult = gltfLoader.load("assets/weapons/ak-47.gltf");
        if (weaponResult) {
            m_weaponModel = std::move(*weaponResult);  // Move instead of copy
            LOG_INFO("Weapon loaded successfully");
        } else {
            LOG_WARN("Failed to load weapon, using test mesh: {}", weaponResult.error().message);
            m_weaponModel = std::move(gltfLoader.createTestWeaponMesh());  // Move instead of copy
        }
        
        // Set up camera inside the map (de_dust2 spawn area)
        // Map is converted to OpenGL coordinates: X=left/right, Y=up, Z=forward
        // After BSP conversion: GoldSrc (X,Y,Z) -> OpenGL (Z,Y,X)
        // So OpenGL: +X = left (was GoldSrc +Z), +Y = up, +Z = forward (was GoldSrc +X)
        m_cameraPosition = Vec3(0.0f, 64.0f, 0.0f);  // Inside the map, at player height
        // Initial yaw: -90 degrees to rotate map 90 degrees right from spawn
        // forwardH = (sin(-90), 0, cos(-90)) = (-1, 0, 0) = -X = looking left
        // Actually, if map spawns to left, we need to rotate right 90 degrees
        // So initial yaw = -90 means we start looking left, then rotate right to see map
        // But user wants to see map correctly from start, so set to 0 after rotation = -90 initial
        m_cameraYaw = -90.0f;  // Rotate 90 degrees right from default to see map correctly
        m_cameraPitch = 0.0f;  // Looking horizontal
        
        // Initialize input
        m_window.setCursorCaptured(true);
        m_input.setCursorCaptured(true);
        
        LOG_INFO("Client initialized successfully");
        return true;
    }
    
    void shutdown() {
        LOG_INFO("Client shutting down...");
        
        m_world.reset();
        m_window.destroy();
        
        Logger::shutdown();
    }
    
    void run() {
        LOG_INFO("Starting main loop");
        
        auto lastTime = Clock::now();
        f32 accumulator = 0.0f;
        constexpr f32 FIXED_TIMESTEP = 1.0f / 128.0f;  // 128 tick
        
        while (m_running) {
            // Calculate delta time
            auto currentTime = Clock::now();
            f32 deltaTime = std::chrono::duration<f32>(currentTime - lastTime).count();
            lastTime = currentTime;
            
            // Cap delta time to prevent spiral of death
            if (deltaTime > 0.25f) {
                deltaTime = 0.25f;
            }
            
            // Process input (handles all SDL events including window events)
            processInput();
            
            // Check for escape to quit
            if (m_input.isKeyPressed(Key::Escape)) {
                m_running = false;
                break;
            }
            
            // Fixed timestep for physics
            accumulator += deltaTime;
            while (accumulator >= FIXED_TIMESTEP) {
                fixedUpdate(FIXED_TIMESTEP);
                accumulator -= FIXED_TIMESTEP;
            }
            
            // Variable timestep update
            update(deltaTime);
            
            // Render with interpolation
            f32 alpha = accumulator / FIXED_TIMESTEP;
            render(alpha);
            
            // Swap buffers
            m_window.swapBuffers();
        }
    }
    
private:
    void processInput() {
        m_input.update();
        
        // Process ALL SDL events (input + window events)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    m_running = false;
                    return;
                    
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_CLOSE:
                            m_running = false;
                            return;
                        case SDL_WINDOWEVENT_FOCUS_LOST:
                            // Window lost focus - could pause game
                            break;
                    }
                    break;
                    
                case SDL_KEYDOWN:
                    m_input.onKeyDown(Input::getKeyFromSDLScancode(event.key.keysym.scancode));
                    break;
                case SDL_KEYUP:
                    m_input.onKeyUp(Input::getKeyFromSDLScancode(event.key.keysym.scancode));
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    m_input.onMouseButtonDown(static_cast<MouseButton>(event.button.button - 1));
                    break;
                case SDL_MOUSEBUTTONUP:
                    m_input.onMouseButtonUp(static_cast<MouseButton>(event.button.button - 1));
                    break;
                case SDL_MOUSEMOTION:
                    if (m_window.isCursorCaptured()) {
                        m_input.onMouseMove(
                            static_cast<f32>(event.motion.xrel),
                            static_cast<f32>(event.motion.yrel)
                        );
                    } else {
                        m_input.onMouseMove(
                            static_cast<f32>(event.motion.x),
                            static_cast<f32>(event.motion.y)
                        );
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    m_input.onMouseWheel(static_cast<f32>(event.wheel.y));
                    break;
            }
        }
    }
    
    void fixedUpdate(f32 dt) {
        // Physics tick
        m_world->fixedUpdate(dt);
    }
    
    void update(f32 dt) {
        // Frame update
        m_world->update(dt);
        
        // Update camera from input
        updateCamera(dt);
    }
    
    void render(f32 interpolation) {
        (void)interpolation;
        
        auto fbSize = m_window.getFramebufferSize();
        m_renderer.setViewport(fbSize.x, fbSize.y);
        
        // Calculate view and projection matrices
        // STANDARD FPS CAMERA: Camera stays at fixed position, view rotates around it
        // Coordinate system: X=right, Y=up, Z=forward (matches converted BSP map)
        f32 yawRad = math::radians(m_cameraYaw);
        f32 pitchRad = math::radians(m_cameraPitch);
        
        // Forward vector in horizontal plane (XZ plane)
        // When yaw=0: forward = +Z (looking along positive Z = forward)
        // Coordinate system: +X = Left in map, so we use cross product for right vector
        Vec3 forwardH(std::sin(yawRad), 0.0f, std::cos(yawRad));
        
        // FIX: Calculate Right vector using Cross Product for consistency
        // Cross Product of (Forward) x (Up=Y) gives the correct perpendicular "Right" vector
        // This ensures consistency with the coordinate system where +X = Left
        Vec3 rightH = glm::normalize(glm::cross(forwardH, Vec3(0.0f, 1.0f, 0.0f)));
        
        // Apply pitch: rotate forwardH around rightH
        // forward = forwardH * cos(pitch) + up * sin(pitch)
        Vec3 forward = forwardH * std::cos(pitchRad) + Vec3(0.0f, 1.0f, 0.0f) * std::sin(pitchRad);
        forward = glm::normalize(forward);
        
        // Calculate camera up vector: rotate world up around rightH by pitch
        // up = worldUp * cos(pitch) - forwardH * sin(pitch)
        Vec3 up = Vec3(0.0f, 1.0f, 0.0f) * std::cos(pitchRad) - forwardH * std::sin(pitchRad);
        up = glm::normalize(up);
        
        // Create view matrix: camera at fixed position, looking along forward direction
        // The map stays fixed in world space, camera rotates around its position
        Vec3 target = m_cameraPosition + forward;
        Mat4 view = math::lookAt(m_cameraPosition, target, up);
        
        f32 aspect = static_cast<f32>(fbSize.x) / static_cast<f32>(fbSize.y);
        Mat4 projection = math::perspective(math::radians(90.0f), aspect, 0.1f, 10000.0f);
        
        m_renderer.setCamera(view, projection);
        
        // Clear with a visible color to test
        m_renderer.clear(Vec3(0.2f, 0.2f, 0.3f));
        
        // Render map
        static bool firstRender = true;
        static u32 renderCount = 0;
        renderCount++;
        
        if (firstRender) {
            LOG_INFO("First render - Camera pos: ({}, {}, {}), Yaw: {}, Pitch: {}", 
                     m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z,
                     m_cameraYaw, m_cameraPitch);
            LOG_INFO("Map mesh loaded: {}, groups: {}", m_mapMesh.loaded, m_mapMesh.groups.size());
            LOG_INFO("Weapon mesh loaded: {}, valid: {}", m_weaponModel.loaded, m_weaponModel.mesh.isValid());
            LOG_INFO("Shader valid: {}", m_renderer.getShader().isValid());
            firstRender = false;
        }
        
        // Always try to render - even if there are issues, we should see something
        if (m_mapMesh.loaded && !m_mapMesh.groups.empty()) {
            Mat4 mapModel = glm::mat4(1.0f);
            
            // Render each mesh group with its corresponding texture
            u32 renderedGroups = 0;
            u32 renderedWithTexture = 0;
            u32 renderedWithoutTexture = 0;
            for (const auto& group : m_mapMesh.groups) {
                if (!group.mesh.isValid()) continue;
                
                if (group.textureID != 0) {
                    // Render with texture
                    m_renderer.drawMeshWithTexture(group.mesh, mapModel, group.textureID, Vec3(1.0f, 1.0f, 1.0f));
                    renderedWithTexture++;
                } else {
                    // Render without texture (checkerboard pattern)
                    m_renderer.drawMesh(group.mesh, mapModel, Vec3(0.8f, 0.8f, 0.8f));
                    renderedWithoutTexture++;
                }
                renderedGroups++;
            }
            
            // Debug: log texture usage on first render
            static bool firstTextureLog = true;
            if (firstTextureLog) {
                LOG_INFO("Rendering {} groups: {} with textures, {} without textures (need WAD files)", 
                         renderedGroups, renderedWithTexture, renderedWithoutTexture);
                firstTextureLog = false;
            }
            
            // Debug: log camera and map info occasionally
            if (renderCount % 300 == 0) {
                u32 totalVertices = 0;
                u32 totalIndices = 0;
                for (const auto& group : m_mapMesh.groups) {
                    totalVertices += static_cast<u32>(group.vertices.size());
                    totalIndices += static_cast<u32>(group.indices.size());
                }
                LOG_INFO("Rendering map - Camera: ({:.1f}, {:.1f}, {:.1f}), {} groups, {} vertices, {} indices", 
                         m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z,
                         renderedGroups, totalVertices, totalIndices);
            }
        } else if (renderCount == 60) {
            // Log every second (assuming 60 FPS) if meshes aren't valid
            LOG_WARN("Map mesh not rendering - loaded: {}, groups: {}", m_mapMesh.loaded, m_mapMesh.groups.size());
        }
        
        // Don't render weapon for now - focus on map
        // if (m_weaponModel.loaded && m_weaponModel.mesh.isValid()) {
        //     Mat4 weaponModel = glm::mat4(1.0f);
        //     weaponModel = glm::translate(weaponModel, Vec3(0.0f, 100.0f, -100.0f));
        //     weaponModel = glm::scale(weaponModel, Vec3(100.0f));
        //     m_renderer.drawMesh(m_weaponModel.mesh, weaponModel, Vec3(1.0f, 0.0f, 0.0f));
        // }
    }
    
    void updateCamera(f32 dt) {
        static u32 frameCount = 0;
        frameCount++;
        
        // Get mouse delta for camera rotation
        Vec2 mouseDelta = m_input.getMouseDelta();
        
        // Debug logging every 300 frames (5 seconds at 60 FPS)
        if (frameCount % 300 == 0) {
            LOG_INFO("Camera pos: ({:.2f}, {:.2f}, {:.2f}), yaw: {:.2f}, pitch: {:.2f}, mouseDelta: ({:.2f}, {:.2f})",
                     m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z,
                     m_cameraYaw, m_cameraPitch, mouseDelta.x, mouseDelta.y);
        }
        
        // STANDARD FPS CAMERA ROTATION - EXACTLY LIKE COUNTER-STRIKE
        // Mouse X (horizontal) = yaw rotation (left/right look)
        // Mouse Y (vertical) = pitch rotation (up/down look)
        // In SDL relative mode: positive X = right, positive Y = down
        const f32 mouseSensitivity = 0.1f;
        
        // Only apply rotation if there's actual mouse movement (avoid floating point noise)
        // FIX: Invert Yaw control because +X is Left in our map coordinate system
        // Since +X is Left in the map, increasing Yaw (rotating to +X) means looking Left.
        // We want Mouse Right to look Right, so we must DECREASE Yaw.
        if (std::abs(mouseDelta.x) > 0.001f || std::abs(mouseDelta.y) > 0.001f) {
            m_cameraYaw -= mouseDelta.x * mouseSensitivity;      // Inverted: Mouse right = decrease yaw (turn right)
            m_cameraPitch -= mouseDelta.y * mouseSensitivity;      // Y controls pitch (vertical rotation)
        }
        
        // Clamp pitch to prevent gimbal lock
        m_cameraPitch = math::clamp(m_cameraPitch, -89.0f, 89.0f);
        
        // Normalize yaw to 0-360 range
        m_cameraYaw = math::normalizeAngle360(m_cameraYaw);
        
        // Calculate camera basis vectors for movement
        // Movement uses horizontal plane only (ignores pitch)
        f32 yawRad = math::radians(m_cameraYaw);
        
        // Forward vector in horizontal plane
        // Coordinate system: X=left/right (where +X = Left in map), Y=up, Z=forward
        // When yaw=0: forward = +Z
        Vec3 forwardH(std::sin(yawRad), 0.0f, std::cos(yawRad));
        
        // FIX: Calculate Right vector using Cross Product
        // Manual calculation was: Vec3(std::cos(yawRad), 0.0f, -std::sin(yawRad))
        // That formula resulted in +X, but +X is Left in our map coordinate system.
        // Cross Product of (Forward) x (Up=Y) gives the correct perpendicular "Right" vector.
        Vec3 rightH = glm::normalize(glm::cross(forwardH, Vec3(0.0f, 1.0f, 0.0f)));
        
        // Movement - STANDARD FPS CONTROLS (now with correct vectors)
        // W = forward (along forwardH direction)
        if (m_input.isKeyDown(Key::W)) {
            m_cameraPosition += forwardH * 500.0f * dt;
        }
        // S = backward (opposite forwardH)
        if (m_input.isKeyDown(Key::S)) {
            m_cameraPosition -= forwardH * 500.0f * dt;
        }
        // A = strafe left (opposite rightH)
        if (m_input.isKeyDown(Key::A)) {
            m_cameraPosition -= rightH * 500.0f * dt;
        }
        // D = strafe right (along rightH)
        if (m_input.isKeyDown(Key::D)) {
            m_cameraPosition += rightH * 500.0f * dt;
        }
        
        // Vertical movement (along Y axis)
        // Space = up (along +Y)
        if (m_input.isKeyDown(Key::Space)) {
            m_cameraPosition.y += 500.0f * dt;
        }
        // Ctrl = down (along -Y)
        if (m_input.isKeyDown(Key::LeftCtrl)) {
            m_cameraPosition.y -= 500.0f * dt;
        }
        
        // Clamp position to map bounds
        if (m_mapMesh.loaded && m_mapMesh.bounds.isValid()) {
            const f32 margin = 10.0f;
            m_cameraPosition.x = math::clamp(m_cameraPosition.x, m_mapMesh.bounds.min.x + margin, m_mapMesh.bounds.max.x - margin);
            m_cameraPosition.z = math::clamp(m_cameraPosition.z, m_mapMesh.bounds.min.z + margin, m_mapMesh.bounds.max.z - margin);
            m_cameraPosition.y = math::clamp(m_cameraPosition.y, m_mapMesh.bounds.min.y - 100.0f, m_mapMesh.bounds.max.y + 500.0f);
        }
    }
    
    Window m_window;
    Input m_input;
    std::unique_ptr<ecs::World> m_world;
    renderer::SimpleRenderer m_renderer;
    
    // Map and weapon
    assets::SimpleBSPMesh m_mapMesh;
    assets::SimpleModel m_weaponModel;
    
    // Camera
    Vec3 m_cameraPosition{0.0f, 50.0f, 0.0f};
    f32 m_cameraYaw = 0.0f;
    f32 m_cameraPitch = 0.0f;
    
    bool m_running = true;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    ClientApplication app;
    
    if (!app.initialize()) {
        return 1;
    }
    
    app.run();
    app.shutdown();
    
    return 0;
}


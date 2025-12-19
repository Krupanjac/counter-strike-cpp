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
        auto weaponResult = gltfLoader.load("assets/weapons/ak-47/scene.gltf");
        if (weaponResult) {
            m_weaponModel = std::move(*weaponResult);  // Move instead of copy
            LOG_INFO("Weapon loaded successfully");
        } else {
            LOG_WARN("Failed to load weapon, using test mesh: {}", weaponResult.error().message);
            m_weaponModel = std::move(gltfLoader.createTestWeaponMesh());  // Move instead of copy
        }
        
        // Set up camera inside the map (de_dust2 spawn area)
        // Map coordinate system: After BSP load + render transformation:
        // - Render applies: 90° Z rotation, 180° Y rotation, Y scale -1
        // - Final coordinate system: Standard OpenGL (X=right, Y=up, Z=forward)
        m_cameraPosition = Vec3(0.0f, 64.0f, 0.0f);  // Inside the map, at player height
        // Initial yaw: 0 degrees means looking along +Z (forward)
        m_cameraYaw = 0.0f;   // Looking along +Z axis (forward)
        m_cameraPitch = 0.0f; // Looking horizontal
        
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
        
        // 1. CALCULATE VECTORS
        // Must match updateCamera logic exactly
        f32 yawRad   = math::radians(m_cameraYaw);
        f32 pitchRad = math::radians(m_cameraPitch);
        
        // Forward Horizontal (Yaw only)
        // Matches: x = -sin, z = cos
        Vec3 forwardH(-std::sin(yawRad), 0.0f, std::cos(yawRad));
        
        // Full Forward Vector (Yaw + Pitch)
        // forward = forwardH * cos(pitch) + Up * sin(pitch)
        Vec3 forward = forwardH * std::cos(pitchRad) + Vec3(0.0f, 1.0f, 0.0f) * std::sin(pitchRad);
        forward = glm::normalize(forward);
        
        // 2. VIEW MATRIX
        // Up vector must be recalculated based on pitch to allow looking straight up/down comfortably
        // Right vector is Cross(Forward, WorldUp)
        Vec3 right = glm::normalize(glm::cross(forward, Vec3(0.0f, 1.0f, 0.0f)));
        // Camera Up is Cross(Right, Forward)
        Vec3 up = glm::normalize(glm::cross(right, forward));
        
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
            // WORKING TRANSFORMATION (discovered through testing):
            // This transformation correctly orients the GoldSrc BSP map to OpenGL coordinate system
            Mat4 mapModel = glm::mat4(1.0f);
            // 1. Rotate 90° around Z axis
            mapModel = glm::rotate(mapModel, math::radians(90.0f), Vec3(0.0f, 0.0f, 1.0f));

            //flip the map around the X axis
            mapModel = glm::scale(mapModel, Vec3(1.0f, -1.0f, 1.0f));
            // 2. Rotate 180° around Y axis
            
            
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
        
         if (m_weaponModel.loaded && m_weaponModel.mesh.isValid()) {
             // Position weapon in first-person view position
             // Use camera's orientation vectors to position weapon relative to view
             
             // Weapon offset in camera space: right, down, forward
             f32 rightOffset = 0.3f;   // Slightly to the right
             f32 downOffset = -0.2f;   // Slightly down
             f32 forwardOffset = -0.5f; // In front of camera (negative = forward in camera space)
             
             // Calculate weapon position using camera's orientation vectors
             Vec3 weaponPosition = m_cameraPosition 
                 + right * rightOffset
                 + up * downOffset
                 + forward * forwardOffset;
             
             // Build transformation matrix
             Mat4 weaponModel = glm::mat4(1.0f);
             
             // First-person weapon position: right side, slightly down, in front
             Vec3 weaponOffset = Vec3(0.3f, -0.2f, 0.0f);
             weaponModel = glm::translate(weaponModel, m_cameraPosition + weaponOffset);
             
             // Rotate to match camera orientation using the correct axes
             weaponModel = glm::rotate(weaponModel, glm::radians(-m_cameraYaw), Vec3(0.0f, 1.0f, 0.0f));
             weaponModel = glm::rotate(weaponModel, glm::radians(-m_cameraPitch), Vec3(1.0f, 0.0f, 0.0f));
             
             // Scale: glTF models are typically in meters, scale to appropriate weapon size
             weaponModel = glm::scale(weaponModel, Vec3(2.5f)); // Scale down from meters to reasonable size
             
             // Render with texture if available, otherwise use white color
             if (m_weaponModel.textureID != 0) {
                 m_renderer.drawMeshWithTexture(m_weaponModel.mesh, weaponModel, m_weaponModel.textureID, Vec3(1.0f, 1.0f, 1.0f));
             } else {
                 m_renderer.drawMesh(m_weaponModel.mesh, weaponModel, Vec3(1.0f, 1.0f, 1.0f));
             }
         }
    }
    
    void updateCamera(f32 dt) {
        // 1. INPUT
        // ---------------------------------------------------------
        // Use STANDARD Mouse Input
        // Mouse Right (+X) -> Increases Yaw (Turn Right)
        // Mouse Up (-Y)    -> Increases Pitch (Look Up) - SDL Y is inverted
        Vec2 mouseDelta = m_input.getMouseDelta();
        
        const f32 mouseSensitivity = 0.1f;
        const f32 moveSpeed = 500.0f;

        if (std::abs(mouseDelta.x) > 0.001f || std::abs(mouseDelta.y) > 0.001f) {
            m_cameraYaw   += mouseDelta.x * mouseSensitivity; // Standard +=
            m_cameraPitch -= mouseDelta.y * mouseSensitivity; // Standard -= (invert SDL Y)
        }

        // Clamp Pitch
        m_cameraPitch = math::clamp(m_cameraPitch, -89.0f, 89.0f);
        m_cameraYaw = math::normalizeAngle360(m_cameraYaw);

        // 2. VECTOR MATH (The Fix)
        // ---------------------------------------------------------
        // We need to map Yaw/Pitch to your specific Coordinate System:
        // Map: +X = Left, +Y = Up, +Z = Forward
        
        // We want Yaw 0 to be Forward (+Z).
        // We want Positive Yaw (Mouse Right) to look Right (-X).
        
        f32 yawRad   = math::radians(m_cameraYaw);
        
        // Use -sin(yaw) for X. 
        // If Yaw increases (Right), sin increases, so -sin becomes more negative (-X = Right).
        Vec3 forwardH(-std::sin(yawRad), 0.0f, std::cos(yawRad)); 
        
        // Calculate Right Vector
        // Cross Product: Forward x Up = Right
        // (+Z) x (+Y) = (-X). Since +X is Left, -X is Right. Perfect.
        Vec3 rightH = glm::normalize(glm::cross(forwardH, Vec3(0.0f, 1.0f, 0.0f)));

        // 3. MOVEMENT
        // ---------------------------------------------------------
        // Now "Forward" points +Z and "Right" points -X.
        // The keys will now move you in the direction you are visually facing.
        
        if (m_input.isKeyDown(Key::W)) m_cameraPosition += forwardH * moveSpeed * dt;
        if (m_input.isKeyDown(Key::S)) m_cameraPosition -= forwardH * moveSpeed * dt;
        if (m_input.isKeyDown(Key::A)) m_cameraPosition -= rightH * moveSpeed * dt;
        if (m_input.isKeyDown(Key::D)) m_cameraPosition += rightH * moveSpeed * dt;
        
        // Vertical
        if (m_input.isKeyDown(Key::Space))    m_cameraPosition.y += moveSpeed * dt;
        if (m_input.isKeyDown(Key::LeftCtrl)) m_cameraPosition.y -= moveSpeed * dt;

        // Bounds check
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


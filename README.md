# Counter-Strike C++ Engine

A modern C++20/23 game engine inspired by GoldSrc, featuring ECS architecture, deterministic authoritative networking, and a modern PBR renderer.

## 🎯 Project Goals

- **Faithful GoldSrc Movement**: Port `pm_shared.c` player physics for authentic CS 1.6 feel
- **Modern Architecture**: Data-oriented ECS design using entt
- **Authoritative Networking**: Server-authoritative simulation with client prediction & reconciliation
- **Modern Visuals**: OpenGL 4.5+ deferred/PBR renderer with glTF 2.0 assets

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Application                              │
├─────────────────────────────────────────────────────────────────┤
│  Gameplay   │  Weapons  │   UI/HUD   │   Audio   │   Scripting  │
├─────────────────────────────────────────────────────────────────┤
│           ECS (entt) - Components & Systems                      │
├──────────────────┬──────────────────┬───────────────────────────┤
│    Movement      │    Networking    │       Renderer            │
│   (pm_shared)    │  (Server/Client) │   (OpenGL Deferred)       │
├──────────────────┴──────────────────┴───────────────────────────┤
│                    Core / Platform Layer                         │
│         (Window, Input, Jobs, Resources, Math, Memory)           │
└─────────────────────────────────────────────────────────────────┘
```

## 📁 Project Structure

```
counter-strike-cpp/
├── .cursor/                    # AI assistant configuration
│   └── rules.md               # Copilot/AI reference document
├── assets/                     # Runtime assets
│   ├── models/                # glTF models
│   ├── textures/              # KTX2 textures
│   ├── shaders/               # GLSL shaders
│   ├── maps/                  # Map files (BSP or glTF scenes)
│   └── audio/                 # Sound files
├── cmake/                      # CMake modules and toolchains
├── docs/                       # Documentation
│   ├── ARCHITECTURE.md        # Detailed architecture
│   ├── NETWORKING.md          # Network protocol specification
│   ├── MOVEMENT.md            # Movement system documentation
│   ├── RENDERER.md            # Rendering pipeline docs
│   └── ASSETS.md              # Asset pipeline documentation
├── external/                   # Third-party dependencies (vcpkg)
├── src/
│   ├── core/                  # Core engine systems
│   │   ├── application/       # App entry, main loop
│   │   ├── platform/          # Platform abstraction (window, input)
│   │   ├── memory/            # Memory allocators, pools
│   │   ├── jobs/              # Job system / task scheduler
│   │   ├── resources/         # Resource manager
│   │   ├── math/              # Math library (glm wrapper + SIMD)
│   │   └── logging/           # Logging system
│   ├── ecs/                   # ECS integration layer
│   │   ├── components/        # All component definitions
│   │   ├── systems/           # All system implementations
│   │   └── world/             # World/scene management
│   ├── movement/              # GoldSrc movement port
│   │   ├── pm_shared/         # Direct pm_shared.c port
│   │   ├── collision/         # Collision detection (BSP, swept)
│   │   └── physics/           # Physics utilities
│   ├── network/               # Networking layer
│   │   ├── protocol/          # Message definitions
│   │   ├── server/            # Authoritative server
│   │   ├── client/            # Client prediction & interpolation
│   │   └── lagcomp/           # Lag compensation system
│   ├── renderer/              # Rendering system
│   │   ├── backend/           # OpenGL backend
│   │   ├── pipeline/          # Deferred/Forward+ pipeline
│   │   ├── materials/         # PBR material system
│   │   ├── lighting/          # Light management, IBL
│   │   └── effects/           # Post-processing effects
│   ├── assets/                # Asset loading & streaming
│   │   ├── gltf/              # glTF loader (tinygltf)
│   │   ├── bsp/               # BSP map loader
│   │   ├── textures/          # Texture loading (KTX2)
│   │   └── streaming/         # Asset streaming system
│   ├── gameplay/              # Game logic
│   │   ├── weapons/           # Weapon systems
│   │   ├── gamemode/          # Game modes (defuse, hostage)
│   │   └── player/            # Player state management
│   ├── audio/                 # Audio system
│   ├── ui/                    # UI/HUD system
│   └── tools/                 # Dev tools & debug
├── tests/                      # Unit and integration tests
│   ├── movement/              # Movement determinism tests
│   ├── network/               # Network tests
│   └── renderer/              # Renderer tests
├── tools/                      # Build & asset tools
│   ├── asset_compiler/        # Asset compilation pipeline
│   └── replay_tool/           # Deterministic replay testing
├── CMakeLists.txt             # Root CMake configuration
├── vcpkg.json                 # vcpkg manifest
└── README.md                  # This file
```

## 🛠️ Tech Stack

| Category | Technology |
|----------|------------|
| Language | C++20 (C++23 selective) |
| Build | CMake 3.25+ |
| Package Manager | vcpkg |
| ECS | entt |
| Math | glm (+ custom SIMD) |
| Networking | ENet |
| Window/Input | SDL2 |
| Graphics | OpenGL 4.5+ Core |
| Asset Loading | tinygltf, KTX-Software |
| Audio | OpenAL Soft |
| Debug UI | Dear ImGui |
| Profiling | RenderDoc, Tracy |

## 🚀 Building

### Prerequisites

- CMake 3.25+
- vcpkg (see setup below)
- C++20 compatible compiler (MSVC 2022, GCC 12+, Clang 15+)
- OpenGL 4.5+ capable GPU
- Git (for vcpkg installation)

### vcpkg Setup

**Option 1: Automatic Setup (Recommended)**

Run the setup script:
```bash
setup_vcpkg.bat
```

This will:
- Find existing vcpkg installation, or
- Install vcpkg to `C:\vcpkg` if not found
- Install all required dependencies

**Option 2: Manual Setup**

1. Install vcpkg:
```bash
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

2. Install dependencies:
```bash
.\vcpkg install sdl2 glm entt enet imgui spdlog openal-soft tinygltf stb glad gtest --triplet x64-windows
```

3. Note your vcpkg path (usually `C:\vcpkg`)

### Build Steps

```bash
# Configure with vcpkg
# Replace C:\vcpkg with your actual vcpkg path
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build --config Release
```

**Finding Your vcpkg Path:**

If you're not sure where vcpkg is installed, check these common locations:
- `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`
- `%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake`
- `%LOCALAPPDATA%\vcpkg\scripts\buildsystems\vcpkg.cmake`

Or use the setup script which will find it automatically.

## 📋 Development Phases

### Phase A - Foundations ⬜
- [ ] Project structure & CMake
- [ ] Core systems (logging, memory, jobs)
- [ ] Platform layer (SDL2 window/input)
- [ ] ECS integration with entt

### Phase B - Movement Proof ⬜
- [ ] Port pm_shared.c movement code
- [ ] Implement collision detection
- [ ] Unit test harness for determinism
- [ ] Verify behavioral parity

### Phase C - Networking Prototype ⬜
- [ ] Authoritative server tick loop
- [ ] Client prediction & reconciliation
- [ ] Snapshot delta compression
- [ ] Lag compensation system

### Phase D - Renderer & Assets ⬜
- [ ] OpenGL 4.5 backend
- [ ] Deferred rendering pipeline
- [ ] PBR material system
- [ ] glTF asset loading

### Phase E - Feature Parity ⬜
- [ ] Weapon systems
- [ ] Game modes
- [ ] Audio integration
- [ ] UI/HUD

### Phase F - Optimization ⬜
- [ ] GPU culling
- [ ] Asset streaming
- [ ] Performance profiling
- [ ] Server scaling

## 📚 Key References

- [Valve Half-Life SDK (pm_shared.c)](https://github.com/ValveSoftware/halflife/blob/master/pm_shared/pm_shared.c)
- [Valve Developer Community - Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking)
- [GoldSrc Engine Overview](https://developer.valvesoftware.com/wiki/GoldSrc)
- [Khronos glTF 2.0 Spec](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [LearnOpenGL PBR](https://learnopengl.com/PBR/Theory)
- [Air Strafing Math](https://adrianb.io/2015/02/14/bunnyhop.html)

## 📄 License

MIT License - See LICENSE file for details.


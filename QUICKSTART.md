# Quick Start Guide

## vcpkg Setup

### If you already have vcpkg installed:

Find your vcpkg path and configure CMake:
```powershell
# Common locations to check:
# C:\vcpkg\scripts\buildsystems\vcpkg.cmake
# $env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake

# Configure with your vcpkg path:
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### If you need to install vcpkg:

**Option 1: Install to C:\vcpkg (Recommended)**
```powershell
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install sdl2 glm entt enet imgui spdlog openal-soft tinygltf stb glad gtest --triplet x64-windows
```

Then configure:
```powershell
cd "C:\Users\da170706d\Documents\1 PROJECTS\counter-strike-cpp"
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

**Option 2: Install to user directory**
```powershell
cd $env:USERPROFILE
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install sdl2 glm entt enet imgui spdlog openal-soft tinygltf stb glad gtest --triplet x64-windows
```

Then configure:
```powershell
cd "C:\Users\da170706d\Documents\1 PROJECTS\counter-strike-cpp"
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake
```

## Build

After configuring with vcpkg:
```powershell
cmake --build build --config Release
```

## Run

```powershell
.\build\bin\Release\cscpp_client.exe
.\build\bin\Release\cscpp_server.exe
```

## Troubleshooting

**Error: "Could not find toolchain file"**
- Make sure you've provided the correct path to `vcpkg.cmake`
- The path should be: `<vcpkg-root>\scripts\buildsystems\vcpkg.cmake`
- Use forward slashes or escaped backslashes: `C:/vcpkg/...` or `C:\\vcpkg\\...`

**Error: "Package not found"**
- Make sure you've installed the dependencies: `vcpkg install sdl2 glm entt ...`
- Check that you're using the correct triplet (x64-windows for 64-bit Windows)

**Error: "Project file does not exist"**
- Make sure CMake configuration succeeded first
- Run `cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=...` before building


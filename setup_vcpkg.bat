@echo off
REM Setup script to find or install vcpkg

echo Counter-Strike C++ - vcpkg Setup
echo =================================
echo.

REM Check common vcpkg locations
set VCPKG_ROOT=

if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set VCPKG_ROOT=C:\vcpkg
    goto :found
)

if exist "%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set VCPKG_ROOT=%USERPROFILE%\vcpkg
    goto :found
)

if exist "%LOCALAPPDATA%\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set VCPKG_ROOT=%LOCALAPPDATA%\vcpkg
    goto :found
)

:notfound
echo vcpkg not found in common locations.
echo.
echo Would you like to install vcpkg? (Y/N)
set /p INSTALL_VCPKG=

if /i "%INSTALL_VCPKG%"=="Y" (
    echo.
    echo Installing vcpkg to C:\vcpkg...
    echo.
    
    cd C:\
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    call bootstrap-vcpkg.bat
    
    set VCPKG_ROOT=C:\vcpkg
    goto :found
) else (
    echo.
    echo Please install vcpkg manually or provide the path when running cmake.
    echo Example: cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
    exit /b 1
)

:found
echo.
echo Found vcpkg at: %VCPKG_ROOT%
echo.
echo Installing dependencies...
echo.

cd %VCPKG_ROOT%
call vcpkg install sdl2 glm entt enet imgui spdlog openal-soft tinygltf stb glad gtest --triplet x64-windows

echo.
echo =================================
echo Setup complete!
echo.
echo To configure the project, run:
echo   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
echo.
pause


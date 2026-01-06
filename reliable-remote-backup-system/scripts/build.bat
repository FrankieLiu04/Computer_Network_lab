@echo off
REM =============================================================================
REM Quick Start Script for Reliable Remote Backup System (Windows)
REM =============================================================================
REM Usage: build.bat [Release|Debug] [--clean]

setlocal enabledelayedexpansion

set BUILD_DIR=build
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo.
echo =============================================
echo Reliable Remote Backup System - Quick Start
echo =============================================
echo.

REM Check prerequisites
echo [INFO] Checking prerequisites...
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found. Please install CMake.
    exit /b 1
)

where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] git not found. Please install Git.
    exit /b 1
)
echo [SUCCESS] Prerequisites OK
echo.

REM Clean old build if --clean
if "%2"=="--clean" (
    echo [INFO] Cleaning old build...
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
    )
)

REM Create build directory
if not exist "%BUILD_DIR%" (
    echo [INFO] Creating build directory...
    mkdir "%BUILD_DIR%"
)

REM Configure
echo [INFO] Configuring CMake (%BUILD_TYPE%)...
cd /d "%BUILD_DIR%"
cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% .. >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    cd /d ..
    exit /b 1
)
cd /d ..
echo [SUCCESS] CMake configured
echo.

REM Build
echo [INFO] Building project...
cd /d "%BUILD_DIR%"
cmake --build . --config %BUILD_TYPE% --parallel 4 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Build failed!
    cd /d ..
    exit /b 1
)
cd /d ..
echo [SUCCESS] Build completed
echo.

REM Test (optional)
if not "%2"=="--no-test" (
    echo [INFO] Running unit tests...
    cd /d "%BUILD_DIR%"
    ctest -C %BUILD_TYPE% >nul 2>&1
    cd /d ..
)

REM Summary
echo.
echo =============================================
echo ✅ Build successful!
echo =============================================
echo.
echo To run the server:
echo   cd build
echo   backup-server.exe -port 35887 -http 8080
echo.
echo Then visit: http://localhost:8080
echo.
echo To run unit tests:
echo   cd build
echo   ctest -C %BUILD_TYPE%
echo.
echo For more details, see: docs\LOCAL_RUN_GUIDE.md
echo =============================================
echo.

endlocal

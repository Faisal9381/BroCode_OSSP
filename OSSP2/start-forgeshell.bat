@echo off
setlocal enabledelayedexpansion

title ForgeShell Launcher

echo ===================================================
echo               ForgeShell Launcher
echo ===================================================
echo.

rem Get project directory in Windows and WSL format
set "PROJ_DIR=%~dp0"
if "%PROJ_DIR:~-1%"=="\" set "PROJ_DIR=%PROJ_DIR:~0,-1%"

echo [1/5] Checking WSL environment...
where.exe wsl.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] wsl.exe was not found in your PATH.
    echo Please make sure Windows Subsystem for Linux is installed.
    pause
    exit /b 1
)

rem Translate project directory to WSL path
for /f "usebackq delims=" %%i in (`wsl.exe -- bash -lc "wslpath '%PROJ_DIR%'"`) do set "WSL_PROJ_DIR=%%i"

if "%WSL_PROJ_DIR%"=="" (
    echo [ERROR] Could not resolve WSL path for project directory.
    pause
    exit /b 1
)

echo Project Path: %PROJ_DIR%
echo WSL Path:     %WSL_PROJ_DIR%
echo.

rem Step 1: Check if ForgeShell is already running
echo [2/5] Checking if ForgeShell is already running...
curl.exe -s --max-time 2 http://localhost:3000/api/health 2>nul | findstr /i "ForgeShell" >nul 2>&1
if not errorlevel 1 (
    echo [OK] ForgeShell server is already running and healthy.
    goto LAUNCH_BROWSER
)

rem Step 2: Check and build missing artifacts
echo [3/5] Verifying build artifacts...

if not exist "%PROJ_DIR%\shell\forgeshell" (
    echo Building C shell binary: shell/forgeshell...
    wsl.exe -- bash -lc "cd '%WSL_PROJ_DIR%/shell' && make"
    if errorlevel 1 (
        echo [ERROR] Failed to compile shell/forgeshell.
        pause
        exit /b 1
    )
) else (
    echo  - C shell binary: OK
)

if not exist "%PROJ_DIR%\backend\node_modules" (
    echo Installing backend dependencies...
    wsl.exe -- bash -lc "cd '%WSL_PROJ_DIR%/backend' && npm install"
    if errorlevel 1 (
        echo [ERROR] Failed to install backend dependencies.
        pause
        exit /b 1
    )
) else (
    echo  - Backend dependencies: OK
)

if not exist "%PROJ_DIR%\frontend\dist" (
    echo Building frontend distribution...
    wsl.exe -- bash -lc "cd '%WSL_PROJ_DIR%/frontend' && [ -d node_modules ] || npm install && npm run build"
    if errorlevel 1 (
        echo [ERROR] Failed to build frontend.
        pause
        exit /b 1
    )
) else (
    echo  - Frontend bundle: OK
)

rem Step 3: Launch detached server in WSL using setsid
echo.
echo [4/5] Launching ForgeShell server in WSL background via setsid...
wsl.exe -- bash -lc "cd '%WSL_PROJ_DIR%/backend' && setsid node server.js > server.log 2>&1 < /dev/null &"

rem Step 4: Health poll loop (up to 20 seconds)
echo Waiting for server to become healthy on http://localhost:3000/api/health ...
set /a TRIES=0
:POLL_LOOP
set /a TRIES+=1
ping -n 2 127.0.0.1 >nul
curl.exe -s --max-time 2 http://localhost:3000/api/health 2>nul | findstr /i "ForgeShell" >nul 2>&1
if not errorlevel 1 (
    echo [OK] Server started and health check passed!
    goto LAUNCH_BROWSER
)
if %TRIES% geq 20 (
    echo [ERROR] Timed out waiting for ForgeShell server after 20 seconds.
    echo Check backend\server.log inside WSL for errors.
    pause
    exit /b 1
)
goto POLL_LOOP

:LAUNCH_BROWSER
echo.
echo [5/5] Opening http://localhost:3000 in your default browser...
start http://localhost:3000
echo.
echo ===================================================
echo   ForgeShell is running!
echo   Web UI: http://localhost:3000
echo   To stop: kill the node process inside WSL
echo ===================================================
ping -n 3 127.0.0.1 >nul
exit /b 0

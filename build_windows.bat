@echo off
setlocal
echo ==========================================================
echo   XV Bass Pedal - Compilacion Windows x64 (VST3 + App)
echo ==========================================================

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] No se encontro CMake. Instalalo desde https://cmake.org/download/
    pause
    exit /b 1
)

cmake -B build -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cmake --build build --config Release --parallel
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo.
echo [OK] Compilacion finalizada con exito!
echo VST3 generado en:
echo   build\XVBassPedal_artefacts\Release\VST3\XV Bass Pedal.vst3
echo.
echo Copialo a: C:\Program Files\Common Files\VST3\
pause

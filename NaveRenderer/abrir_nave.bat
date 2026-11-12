@echo off
setlocal
cd /d "%~dp0"
set "NAVE_EXE="
if exist "build\nave.exe" set "NAVE_EXE=build\nave.exe"
if exist "build\Release\nave.exe" set "NAVE_EXE=build\Release\nave.exe"
if exist "build-portable\nave.exe" set "NAVE_EXE=build-portable\nave.exe"
if not defined NAVE_EXE (
  echo Primero ejecuta compilar.bat. Se necesitan CMake, C++17 y SDL2.
  pause
  exit /b 1
)
"%NAVE_EXE%" "Nave.obj"
if errorlevel 1 pause

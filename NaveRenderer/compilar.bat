@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo Instala CMake y agregalo al PATH. Tambien necesitas C++17 y SDL2.
  goto :error
)
rem El preset local es opcional y no se distribuye: contiene rutas de este equipo.
if exist "CMakeUserPresets.json" (
  cmake --preset nave-local
  if errorlevel 1 goto :error
  cmake --build --preset nave-local
  if errorlevel 1 goto :error
) else (
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release %*
  if errorlevel 1 goto :error
  cmake --build build --config Release
  if errorlevel 1 goto :error
)
echo.
echo Compilacion terminada. Abre abrir_nave.bat para ver tu nave.
pause
exit /b 0
:error
echo.
echo No se pudo compilar. Copia el mensaje de error de arriba.
echo Si la nave esta abierta, cierrala antes de recompilar.
pause
exit /b 1

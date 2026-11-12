# Proyecto 3 — Sistema solar en software

**Giancarlo Sagastume · Carné 24278 · Entrega individual.**
Contribución: diseño de la escena y nave, integración del renderer, efectos y pruebas, con asistencia de IA.

Exploración espacial en C++17 con un rasterizador propio: matrices modelo/vista/proyección, recorte, triángulos y profundidad en CPU. SDL2 solo gestiona ventana, entrada y presentación del framebuffer.

## Ejecutar

Requisitos: compilador C++17, CMake 3.16+ y **SDL2** de desarrollo compatible con el compilador. Desde la raíz del repositorio:

```sh
cd NaveRenderer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

- Windows/Visual Studio: `.\build\Release\nave.exe .\Nave.obj`
- Windows/Ninja: `.\build\nave.exe .\Nave.obj`
- Linux: `./build/nave ./Nave.obj`

Si CMake no encuentra SDL2, agrega a la configuración `-DSDL2_DIR="directorio que contiene SDL2Config.cmake"` con una ruta real. En Windows también están `compilar.bat` y `abrir_nave.bat`; cierra la nave antes de recompilar. Los modelos y recursos están incluidos.

## Controles

| Control | Acción |
|---|---|
| Ratón / flechas; Q / E | Orientar; rodar |
| W / S; A / D | Adelante / atrás; lateral |
| Espacio / Ctrl izquierdo | Subir / bajar |
| Shift; X mantenida | Turbo; igualar velocidad con el objetivo |
| Mirar un planeta | Selección automática e información |
| F; C | Cabina / exterior; distancia de cámara exterior |
| R; P | Reiniciar; captura PPM |
| Esc | Liberar ratón; otro Esc cierra |

## Características a evaluar

| Característica | Dónde verla / implementación |
|---|---|
| Cuatro planetas que orbitan y rotan | Darkstone rocoso, Twins rojo/azul y Storm Giant con humo y relámpagos; `solar_system.h` |
| Modelo de superficie orientado y unido al planeta | Choza diminuta en Darkstone; `drawHut` y `hutAnchor`, en `scene.h` |
| Shaders propios y animación | Sol animado, superficies, faros y tormenta; `scene.h` y `planet_materials.h` |
| Deformación y subdivisión de geometría | Relieve de Darkstone (`rockRadius`) e icosferas subdivididas (`makeIcosphere`) |
| Navegación progresiva, gravedad y colisión | Acercarse, rodear y alejarse sin cambiar de escena; `ship.h` |
| Descarte de objetos y niveles de detalle | `sphereVisible` y `drawSphere`; comparar `tri dibujados/enviados` en el título al girar y acercarse/alejarse |

La distancia llega a 0 km al tocar la superficie. Los faros iluminan el lado nocturno y chocar con el sol provoca una reaparición tras 3 segundos. Pruebas: `ctest --test-dir build --output-on-failure -C Release`.

## Video de demostración

**Pendiente de agregar el enlace antes de entregar. Es un requisito obligatorio.**

Debe mostrar vuelo espacial → acercamiento → vuelta alrededor de un planeta → alejamiento. Incluir los efectos, la choza y los contadores de triángulos ayuda a demostrar las características solicitadas.

## Recursos y asistencia

Modelo de nave: `NaveRenderer/Nave.obj`. Choza y texturas generadas por código; sin paquetes de texturas externos. Se utilizó asistencia de **ChatGPT/Codex y Claude** para desarrollo y depuración; el autor debe poder explicar el código y modificar parámetros durante la demostración.

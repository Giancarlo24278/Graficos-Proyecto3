cmake_minimum_required(VERSION 3.16)
project(NaveRenderer LANGUAGES CXX)
option(HEADLESS "Generar PPM sin ventana ni SDL" OFF)
add_executable(nave main.cpp)
target_compile_features(nave PRIVATE cxx_std_17)
if(HEADLESS)
    target_compile_definitions(nave PRIVATE HEADLESS)
else()
    set(SDL2_NO_MWINDOWS TRUE)
    find_package(SDL2 CONFIG REQUIRED)
    target_link_libraries(nave PRIVATE SDL2::SDL2)
    if(WIN32)
        # La DLL debe estar junto al ejecutable para abrirlo con doble clic.
        get_target_property(_sdl2_type SDL2::SDL2 TYPE)
        if(_sdl2_type STREQUAL "SHARED_LIBRARY")
            add_custom_command(TARGET nave POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:nave>
                VERBATIM)
        endif()
    endif()
endif()
set(NAVE_RUNTIME_DLL_DIR "" CACHE PATH "Carpeta de las DLL del compilador portatil")
if(WIN32 AND NAVE_RUNTIME_DLL_DIR)
    foreach(_runtime IN ITEMS libc++.dll libunwind.dll libwinpthread-1.dll)
        if(EXISTS "${NAVE_RUNTIME_DLL_DIR}/${_runtime}")
            add_custom_command(TARGET nave POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${NAVE_RUNTIME_DLL_DIR}/${_runtime}" $<TARGET_FILE_DIR:nave>
                VERBATIM)
        endif()
    endforeach()
endif()
configure_file(Nave.obj Nave.obj COPYONLY)

include(CTest)
if(BUILD_TESTING)
    add_executable(nave_tests tests/world_tests.cpp)
    target_compile_features(nave_tests PRIVATE cxx_std_17)
    add_test(NAME world_behavior COMMAND nave_tests "${CMAKE_CURRENT_SOURCE_DIR}/Nave.obj")
    if(NOT HEADLESS)
        add_executable(nave_input_test tests/sdl_smoke.cpp)
        target_compile_features(nave_input_test PRIVATE cxx_std_17)
        target_link_libraries(nave_input_test PRIVATE SDL2::SDL2)
        add_test(NAME sdl_input COMMAND nave_input_test "${CMAKE_CURRENT_SOURCE_DIR}/Nave.obj")
        # Las capturas de prueba quedan aisladas del render.ppm del usuario.
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/input-test")
        set_tests_properties(sdl_input PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/input-test" TIMEOUT 20)
    endif()
endif()

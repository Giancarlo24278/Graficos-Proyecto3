#include <cstdio>
#include <iostream>
#include <stdexcept>
#include "obj_loader.h"
#include "scene.h"

// -DHEADLESS permite generar una imagen sin instalar SDL.
#ifndef HEADLESS
#define SDL_MAIN_HANDLED
#include <SDL.h>
#endif

// La fisica avanza en pasos fijos: igual en una computadora lenta o rapida.
inline constexpr float PHYSICS_STEP=1.0f/240.0f;

int main(int argc,char* argv[]) {
    try {
        std::string path=argc>1?argv[1]:"Nave.obj";
        Mesh mesh=loadOBJ(path);
        assignParts(mesh);
        World world;
        world.shipModel=prepareShipModel(mesh);
        world.reset();
        Framebuffer fb;
        std::cout<<mesh.triangles.size()<<" triangulos en la nave\n";
#ifdef HEADLESS
        // nave Nave.obj salida.ppm [segundos]: simula sin controles y guarda un cuadro.
        float seconds=argc>3?std::stof(argv[3]):0.0f;
        for(float t=0;t<seconds;t+=PHYSICS_STEP) {
            world.step(ShipInput{},PHYSICS_STEP);
            world.updateHud(PHYSICS_STEP);
        }
        world.render(fb);
        fb.savePPM(argc>2?argv[2]:"render.ppm");
        std::cout<<"Triangulos: "<<fb.stats.drawn<<" dibujados de "<<fb.stats.submitted
                 <<", objetos descartados: "<<fb.stats.objectsCulled<<"\n";
#else
        SDL_SetMainReady();
        if(SDL_Init(SDL_INIT_VIDEO)!=0) throw std::runtime_error(SDL_GetError());
        SDL_Window* window=SDL_CreateWindow("Sistema solar",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
            int(SCREEN_WIDTH*WINDOW_SCALE),int(SCREEN_HEIGHT*WINDOW_SCALE),SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
        SDL_Renderer* renderer=window?SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC):nullptr;
        if(window && !renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
        SDL_Texture* texture=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,SCREEN_WIDTH,SCREEN_HEIGHT):nullptr;
        if(!window || !renderer || !texture) {
            std::string error=SDL_GetError();
            if(texture) SDL_DestroyTexture(texture);
            if(renderer) SDL_DestroyRenderer(renderer);
            if(window) SDL_DestroyWindow(window);
            SDL_Quit();throw std::runtime_error(error);
        }
        // SDL solo escala el framebuffer a la ventana (manteniendo la proporcion).
        SDL_RenderSetLogicalSize(renderer,SCREEN_WIDTH,SCREEN_HEIGHT);
        SDL_SetRelativeMouseMode(SDL_TRUE);
        bool running=true,mouseCaptured=true,save=false;
        Uint64 frequency=SDL_GetPerformanceFrequency(),last=SDL_GetPerformanceCounter();
        float accumulator=0,titleTimer=0;int frames=0;
        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_QUIT) running=false;
                if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT && !mouseCaptured) {
                    // Esc libera el cursor para poder pulsar el boton de vista.
                    // SDL_RenderSetLogicalSize ya convierte los eventos del raton
                    // al framebuffer, incluyendo el escalado y las bandas.
                    if(viewButtonContains(float(event.button.x),float(event.button.y))) world.toggleView();
                    else {mouseCaptured=true;SDL_SetRelativeMouseMode(SDL_TRUE);}
                }
                if(event.type==SDL_MOUSEMOTION && mouseCaptured && !world.dead) {
                    // El raton da un impulso de giro; el amortiguamiento lo suaviza.
                    world.ship.spin.x-=event.motion.yrel*MOUSE_TURN;
                    world.ship.spin.y-=event.motion.xrel*MOUSE_TURN;
                }
                if(event.type==SDL_KEYDOWN && !event.key.repeat) {
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            // Primer Esc suelta el raton; el segundo cierra.
                            if(mouseCaptured) {mouseCaptured=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                            else running=false;
                            break;
                        case SDLK_f:world.toggleView();break;
                        case SDLK_c:
                            world.cameraDistance=world.cameraDistance<6?10.0f:4.0f;
                            world.refreshTarget();break;
                        case SDLK_r:if(!world.dead) world.reset();break;
                        case SDLK_p:save=true;break;
                    }
                }
            }
            if(!running) break;

            const Uint8* k=SDL_GetKeyboardState(nullptr);
            auto axis=[&](SDL_Scancode plus,SDL_Scancode minus) {return float(k[plus])-float(k[minus]);};
            ShipInput input;
            input.thrust={axis(SDL_SCANCODE_D,SDL_SCANCODE_A),axis(SDL_SCANCODE_SPACE,SDL_SCANCODE_LCTRL),
                          axis(SDL_SCANCODE_W,SDL_SCANCODE_S)};
            input.turn={axis(SDL_SCANCODE_UP,SDL_SCANCODE_DOWN),axis(SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT),
                        axis(SDL_SCANCODE_E,SDL_SCANCODE_Q)};
            input.boost=k[SDL_SCANCODE_LSHIFT];
            input.matchVelocity=k[SDL_SCANCODE_X];

            // Tiempo real transcurrido: los movimientos no dependen de los FPS.
            Uint64 now=SDL_GetPerformanceCounter();
            float dt=std::min(0.1f,float(double(now-last)/frequency));last=now;
            accumulator+=dt;
            while(accumulator>=PHYSICS_STEP) {
                world.step(input,PHYSICS_STEP);
                accumulator-=PHYSICS_STEP;
            }
            world.updateHud(dt);

            world.render(fb);
            SDL_UpdateTexture(texture,nullptr,fb.pixels.data(),SCREEN_WIDTH*sizeof(uint32_t));
            if(save) {fb.savePPM("render.ppm");std::cout<<"Guardado: render.ppm\n";save=false;}
            SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,nullptr,nullptr);
            SDL_RenderPresent(renderer);

            // El titulo conserva los contadores de rendimiento para depuracion.
            ++frames;titleTimer+=dt;
            if(titleTimer>=0.25f) {
                const Body& t=world.system.bodies[world.target];
                char title[256];
                std::snprintf(title,sizeof(title),
                    "%s | %s | %d km | %s | %d FPS | tri %d/%d | F vista, C distancia camara, R reiniciar",
                    world.dead?"Nave destruida":t.name.c_str(),
                    world.targetInSight?"objetivo automatico":"ultimo objetivo",world.displayedDistanceKm,
                    world.firstPerson?"cabina":"exterior",
                    int(frames/titleTimer+0.5f),fb.stats.drawn,fb.stats.submitted);
                SDL_SetWindowTitle(window,title);
                frames=0;titleTimer=0;
            }
        }
        SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
#endif
        return 0;
    } catch(const std::exception& e) {std::cerr<<"Error: "<<e.what()<<'\n';return 1;}
}

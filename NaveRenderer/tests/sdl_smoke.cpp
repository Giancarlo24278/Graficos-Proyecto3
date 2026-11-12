// Ejecuta el bucle real con el driver SDL dummy y eventos de teclado/raton.
#define SDL_MAIN_HANDLED
#include <SDL.h>
void smokePresent(SDL_Renderer* renderer);
#define SDL_RenderPresent smokePresent
#define main naveApplicationMain
#include "../main.cpp"
#undef main
#undef SDL_RenderPresent
#include <filesystem>

namespace {
int frames=0;
bool mouseScenario=false;
void pushKey(SDL_Keycode key) {
    SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.sym=key;
    SDL_PushEvent(&event);
}
bool cockpitWasSaved() {
    std::ifstream file("render.ppm",std::ios::binary);
    std::string magic;int w=0,h=0,max=0;
    file>>magic>>w>>h>>max;file.get();
    if(magic!="P6" || w!=SCREEN_WIDTH || h!=SCREEN_HEIGHT || max!=255) return false;
    // Interior de la pantalla unica; este pixel no existe en la vista exterior.
    file.seekg((444*w+310)*3,std::ios::cur);
    unsigned char pixel[3]{};file.read(reinterpret_cast<char*>(pixel),3);
    return file.good() && pixel[0]==6 && pixel[1]==22 && pixel[2]==21;
}
}

// Inyectar despues del primer cuadro garantiza que ventana, renderer y captura
// del raton ya estan listos, sin depender de tiempos de arranque del equipo.
void smokePresent(SDL_Renderer* renderer) {
    SDL_RenderPresent(renderer);
    if(++frames==1) {
        pushKey(SDLK_TAB);
        if(mouseScenario) {
            pushKey(SDLK_ESCAPE);
            SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;
            // SDL entrega los clics reales ya en coordenadas logicas; PushEvent
            // inyecta directamente, sin la conversion del backend de entrada.
            click.button.x=VIEW_BUTTON_X+1;
            click.button.y=VIEW_BUTTON_Y+1;
            SDL_PushEvent(&click);
        } else pushKey(SDLK_f);
        pushKey(SDLK_p);
    } else {
        SDL_Event quit{};quit.type=SDL_QUIT;SDL_PushEvent(&quit);
    }
}

int main(int argc,char* argv[]) {
    if(argc<2) return 1;
    // Tambien al ejecutar la prueba a mano, aislar la captura del usuario.
    std::string model=std::filesystem::absolute(argv[1]).string();
    auto testFolder=std::filesystem::absolute(argv[0]).parent_path()/"input-test";
    std::filesystem::create_directories(testFolder);
    std::filesystem::current_path(testFolder);
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    for(bool mouse:{false,true}) {
        mouseScenario=mouse;frames=0;
        // Se borra solo la captura producida por esta prueba, dentro de su build.
        std::filesystem::remove("render.ppm");
        char* args[]={argv[0],model.data()};
        int result=naveApplicationMain(2,args);
        if(result!=0 || !cockpitWasSaved()) {
            std::cerr<<"Fallo al entrar a cabina con "<<(mouse?"el boton":"F")<<'\n';return 1;
        }
    }
    std::cout<<"Cambio de vista con F y boton: correcto\n";
    return 0;
}

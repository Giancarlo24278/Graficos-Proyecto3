#include <filesystem>
#include <iostream>
#include <chrono>
#include "../obj_loader.h"
#include "../scene.h"

namespace {
int checks=0;
constexpr float H=1.0f/240.0f;
void check(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
void aimAt(World& world,int body) {
    world.reset();world.firstPerson=true;
    Vec3 offset=normalized(Vec3{0,1,0.3f})*(world.system.bodies[body].radius+12.0f);
    world.system.sunOffset=-world.system.orbitalPosition(body)+offset;
    world.ship=Ship{};world.ship.lookAt(world.system.position(body));
    world.refreshTarget();
}
void preview(World& world,Framebuffer& fb,const std::filesystem::path& folder,const char* name) {
    if(folder.empty()) return;
    world.render(fb);fb.savePPM((folder/(std::string(name)+".ppm")).string());
}
void viewPlanet(World& world,int body,Vec3 outward,float distance,double time=0) {
    world.reset();world.system.time=time;world.firstPerson=true;world.headlightsEnabled=true;
    outward=normalized(outward);
    world.system.sunOffset=-world.system.orbitalPosition(body)-outward*distance;
    world.ship.lookAt(-outward);world.ship.velocity=world.system.velocity(body);world.refreshTarget();
}
}

int main(int argc,char* argv[]) {
    try {
        World world;world.reset();Framebuffer fb;
        std::filesystem::path previews=argc>2?argv[2]:"";
        if(!previews.empty()) std::filesystem::create_directories(previews);
        if(argc>1) {Mesh mesh=loadOBJ(argv[1]);assignParts(mesh);world.shipModel=prepareShipModel(mesh);}
        const auto& bodies=world.system.bodies;
        check(bodies.size()==5,"Debe haber un sol y cuatro planetas");
        check(bodies[1].name=="Darkstone" && bodies[1].habitable=="???" && bodies[1].explored,
            "Ficha de Darkstone incorrecta");
        check(bodies[2].name=="Little Red" && bodies[3].name=="Little Blue","Faltan los Twins");
        check(bodies[2].explored && bodies[3].explored && bodies[2].habitable=="No" && bodies[3].habitable=="No",
            "Los Twins deben ser explorados e inhabitables");
        check(bodies[2].radius==bodies[3].radius && bodies[2].radius<bodies[1].radius,"Twins pequenos");
        check(bodies[4].name=="Storm Giant" && !bodies[4].explored && bodies[4].habitable=="No" &&
            bodies[4].radius>5.0f && bodies[4].orbit.radius>85.0f,"Storm Giant grande y mas alejado");
        check(bodies[1].appearance==PlanetAppearance::Rock && bodies[2].appearance==PlanetAppearance::Ember &&
            bodies[3].appearance==PlanetAppearance::Ice && bodies[4].appearance==PlanetAppearance::Storm,
            "Material propio para cada planeta");
        check(std::abs(bodies[1].surfaceGravity-2.45f)<1e-5f &&
            std::abs(bodies[4].surfaceGravity-2.45f)<1e-5f &&
            std::abs(bodies[2].surfaceGravity-2.1f)<1e-5f &&
            std::abs(bodies[3].surfaceGravity-2.1f)<1e-5f,"Gravedad planetaria un 30% menor");
        check(bodies[0].surfaceGravity==5.0f,"Conservar gravedad del sol");

        for(auto kind:{PlanetAppearance::Rock,PlanetAppearance::Ember,PlanetAppearance::Ice,PlanetAppearance::Storm}) {
            Vec3 average;float variation=0;
            const auto& texture=planetTexture(kind);
            Vec3 first=texture.sample({1,0,0}).color;
            for(int k=0;k<48;++k) {
                Vec3 direction=normalized(Vec3{std::sin(k*1.7f),std::cos(k*0.7f),std::sin(k*0.3f+1)});
                Vec3 color=texture.sample(direction).color;average+=color;
                variation+=length(color-first);
                check(std::isfinite(color.x+color.y+color.z) && color.x>=0 && color.y>=0 && color.z>=0,
                    "Material finito y sin canales negativos");
            }
            average=average*(1.0f/48);
            check(variation>0.1f,"Textura con variacion, sin esfera de color plano");
            if(kind==PlanetAppearance::Rock) check(average.z<0.2f && average.x<0.2f,"Roca negra, no gris claro");
            if(kind==PlanetAppearance::Ember) check(average.x>average.y*1.7f && average.y>average.z*2,"Paleta rojo naranja");
            if(kind==PlanetAppearance::Ice) check(average.y>average.x*1.5f && average.z>average.x*1.5f,"Paleta azul turquesa");
            if(kind==PlanetAppearance::Storm) check(std::abs(average.z-average.x)<0.08f,"Nubes de tono gris");
            check(length(texture.sample({1,0.2f,0.99999f}).color-texture.sample({0.99999f,0.2f,1}).color)<0.02f,
                "Textura continua entre caras del cubo");
        }
        check(stormFlashAt(0).strength<0.001f && stormFlashAt(4).strength<0.001f &&
            stormFlashAt(2.12).strength>0.9f && stormFlashAt(2.37).strength>0.7f,
            "Destellos morados intermitentes, no luz permanente");
        check(length(stormFlashAt(2.12).direction-stormFlashAt(9.62).direction)>0.5f,
            "Las tormentas aparecen en distintas regiones");
        Ship lampShip;
        Headlights lamps=makeHeadlights(lampShip);
        float nearLight=length(headlightIrradiance({0,0,-3},{0,0,1},lamps));
        check(nearLight>length(headlightIrradiance({0,0,-8},{0,0,1},lamps))*3,"Faros mas fuertes al acercarse");
        check(length(headlightIrradiance({0,0,-22},{0,0,1},lamps))==0 &&
            length(headlightIrradiance({0,0,3},{0,0,-1},lamps))==0 &&
            length(headlightIrradiance({4,0,-3},{0,0,1},lamps))==0,"Faros con alcance y cono delanteros");
        check(length(headlightIrradiance({0,0,-3},{0,0,-1},lamps))==0,"No iluminar el reverso de la superficie");
        Headlights blocked=lamps;blocked.blockerCount=1;blocked.blockers[0]={{0,0,-1.6f},0.8f};
        check(length(headlightIrradiance({0,0,-3},{0,0,1},blocked))==0,"Los faros no atraviesan otro planeta");
        lampShip.lookAt({1,0,0});
        check(length(headlightIrradiance({3,0,0},{-1,0,0},makeHeadlights(lampShip)))>1 &&
            length(headlightIrradiance({0,0,-3},{0,0,1},makeHeadlights(lampShip)))==0,"Los faros giran con la nave");
        Headlights off=lamps;off.enabled=false;
        PlanetShader lit{&planetTexture(PlanetAppearance::Rock),PlanetAppearance::Rock,{0,0,-100},lamps,{},rotationY(0)};
        PlanetShader unlit=lit;unlit.headlights=off;
        Varying nightSurface{{0,0,-3},{0,0,1},{0,0,1}};
        check(length(lit(nightSurface))>length(unlit(nightSurface))*4,"Los faros iluminan realmente la roca nocturna");
        Vec3 hut0=hutAnchor({},4.5f,0),hut1=hutAnchor({},4.5f,PI/2);
        check(length(hut1-transformDir(rotationY(PI/2),hut0))<1e-5f && length(hut0)<4.5f,
            "Choza anclada a la roca que gira con el planeta");
        Camera smokeCamera=makeCamera({},{0,0,-1},{0,1,0});
        fb.clear();std::fill(fb.depth.begin(),fb.depth.end(),100.0f);auto opaquePixels=fb.pixels;
        drawStormHaze(fb,smokeCamera,{0,0,-20},6,0,0,{-20,10,0},stormFlashAt(0));
        check(fb.pixels==opaquePixels,"El humo no atraviesa objetos opacos cercanos");
        fb.clear();drawStormHaze(fb,smokeCamera,{0,0,-20},6,0,0,{-20,10,0},stormFlashAt(0));
        check(fb.pixels[270*SCREEN_WIDTH+640]!=Framebuffer::pack(BACKGROUND),"Humo presente fuera del disco del planeta");
        auto stillSmoke=fb.pixels;
        fb.clear();drawStormHaze(fb,smokeCamera,{0,0,-20},6,0,1,{-20,10,0},stormFlashAt(1));
        check(fb.pixels!=stillSmoke,"Humo en movimiento incluso sin destellos");

        // Seleccion por mirada, incluido cada gemelo de forma independiente.
        for(int i=0;i<5;++i) {
            aimAt(world,i);
            check(world.targetInSight && world.target==i,"Objetivo automatico incorrecto");
            if(i!=1) check(world.displayedDistanceKm==world.roundedDistanceKm(),"Contador del nuevo objetivo");
        }
        world.reset();world.firstPerson=true;
        world.ship.lookAt({0,1,0.05f});world.refreshTarget();
        check(!world.targetInSight,"La ficha debe ocultarse al mirar al vacio");
        check(world.target==1,"Conservar la referencia para igualar velocidad al mirar al vacio");
        SolarSystem aligned=makeSolarSystem();aligned.bodies.resize(2);
        aligned.sunOffset={0,0,-10};aligned.bodies[0].radius=2;
        aligned.bodies[1].orbit={20,100,-PI/2};aligned.bodies[1].radius=4;
        check(gazeTarget(makeCamera({},{0,0,-1},{0,1,0}),aligned)==0,
            "El sol cercano debe ocultar el planeta alineado");
        aligned.bodies.resize(1);aligned.sunOffset={0,0,-100};aligned.bodies[0].radius=0.5f;
        check(gazeTarget(makeCamera({},normalized(Vec3{0.02f,0,-1}),{0,1,0}),aligned)==0,
            "Margen de seleccion para objetivos pequenos");
        check(gazeTarget(makeCamera({},normalized(Vec3{0.15f,0,-1}),{0,1,0}),aligned)==-1,
            "No seleccionar fuera del margen de mirada");

        world.reset();world.firstPerson=false;
        check(length(world.camera().eye)>1,"Camara exterior detras de la nave");
        world.toggleView();
        check(length(world.camera().eye)<1e-6f && dot(world.camera().forward,world.ship.forward)>0.999f,
            "La cabina debe estar dentro de la nave y mirar hacia la nariz");
        world.toggleView();check(!world.firstPerson,"Volver a camara exterior");
        check(viewButtonContains(25,23) && !viewButtonContains(10,10),"Area clicable del boton");

        aimAt(world,1);
        int distance=world.displayedDistanceKm;
        world.system.sunOffset-=normalized(world.system.position(1))*4.0f;
        world.updateHud(0.01f);check(world.displayedDistanceKm==distance-40,"Distancia actual sin retraso al acercarse");
        world.updateHud(1);check(world.displayedDistanceKm==distance-40,"No mover el contador estando quieto");
        world.system.sunOffset+=normalized(world.system.position(1))*10.0f;
        world.updateHud(0.04f);check(world.displayedDistanceKm==distance+60,"Distancia actual sin retraso al alejarse");
        world.system.sunOffset=-world.system.orbitalPosition(1)+Vec3{0,0,5.2f};
        check(world.roundedDistanceKm()==0,"La distancia en superficie no debe ser negativa");
        world.step(ShipInput{},H);check(!world.dead && world.ship.touching,"Aterrizar en un planeta no mata");
        check(world.displayedDistanceKm==0,"Distancia visible cero al tocar la superficie");
        for(int i=1;i<5;++i) {
            aimAt(world,i);
            world.system.sunOffset=-world.system.orbitalPosition(i)+Vec3{0,0,world.system.bodies[i].radius+SHIP_RADIUS-0.1f};
            world.ship.lookAt(world.system.position(i));world.refreshTarget();
            world.displayedDistanceKm=300; // Reproduce el contador atrasado reportado.
            world.step(ShipInput{},H);world.updateHud(H);
            check(world.ship.touching && world.target==i && world.displayedDistanceKm==0,
                "Cada planeta debe indicar cero al aterrizar, incluso llegando desde 300 km");
        }

        world.reset();world.firstPerson=false;world.render(fb);
        check(fb.pixels[74*SCREEN_WIDTH+650]==HUD_EDGE,"Ficha lateral en vista exterior");
        world.toggleView();world.render(fb);
        check(fb.pixels[74*SCREEN_WIDTH+650]!=HUD_EDGE,"Sin ficha lateral en cabina");
        check(fb.pixels[435*SCREEN_WIDTH+504]==HUD_ACCENT,"Ficha del objetivo dentro de la pantalla de cabina");
        world.ship.lookAt({0,1,0.05f});world.refreshTarget();world.render(fb);
        check(!world.targetInSight && fb.pixels[435*SCREEN_WIDTH+504]!=HUD_ACCENT,
            "Ocultar datos al dejar de mirar el objetivo");
        fb.clear();
        drawTargetMarker(fb,makeCamera({},{0,0,-1},{0,1,0}),{0,0,-20},2,false);
        check(fb.pixels[103*SCREEN_WIDTH+622]!=HUD_TEXT,"Cabina sin linea hacia una ficha lateral");

        // El barrido debe detectar el sol aunque se atraviese entero en un paso.
        world.reset();world.firstPerson=true;world.system.sunOffset={0,0,-20};
        world.ship.velocity={0,0,-15000};world.step(ShipInput{},H);
        check(world.dead,"El choque rapido con el sol debe ser letal");
        Vec3 frozen=world.system.sunOffset;
        ShipInput input;input.thrust={1,1,1};input.turn={1,1,1};
        world.step(input,H);
        check(length(world.system.sunOffset-frozen)<1e-6f && length(world.ship.velocity)==0,
            "Sin movimiento ni controles durante la muerte");
        world.toggleView();check(world.firstPerson,"No cambiar de vista durante la muerte");
        world.deathElapsed=0.1f;world.render(fb);uint32_t earlyWhite=fb.pixels[0];
        world.deathElapsed=0.8f;world.render(fb);
        check(fb.pixels[0]==0xffffffffu && earlyWhite!=0xffffffffu,"Fundido progresivo a blanco");
        preview(world,fb,previews,"solar_death");
        world.deathElapsed=0;
        for(int i=0;i<719;++i) world.step(input,H);
        check(world.dead,"No reaparecer antes de tres segundos");
        world.step(input,H);
        check(!world.dead && world.target==1 && world.deathElapsed==0,"Reaparicion automatica a los tres segundos");
        check(length(world.system.position(0))>world.system.bodies[0].radius+SHIP_RADIUS,
            "Reaparecer fuera del sol");
        check(world.firstPerson,"Conservar la vista elegida al reaparecer");
        preview(world,fb,previews,"cockpit_dark_stone");
        world.toggleView();preview(world,fb,previews,"exterior_dark_stone");
        for(int i=2;i<5;++i) {
            aimAt(world,i);
            preview(world,fb,previews,i==2?"little_red":i==3?"little_blue":"storm_giant");
        }
        aimAt(world,1);world.ship.lookAt(-world.system.position(1));world.refreshTarget();
        preview(world,fb,previews,"cockpit_no_target");
        if(!previews.empty()) {
            for(int i=1;i<5;++i) {
                auto sys=makeSolarSystem();Vec3 sunny=normalized(-sys.orbitalPosition(i)+Vec3{0,8,0});
                viewPlanet(world,i,sunny,sys.bodies[i].radius*3.2f+1);
                preview(world,fb,previews,i==1?"darkstone_day":i==2?"little_red_day":i==3?"little_blue_day":"storm_giant_day");
            }
            auto sys=makeSolarSystem();Vec3 night=normalized(sys.orbitalPosition(1));
            viewPlanet(world,1,night,sys.bodies[1].radius+2);
            preview(world,fb,previews,"headlights_on");
            world.headlightsEnabled=false;preview(world,fb,previews,"headlights_off");
            world.reset();world.firstPerson=true;world.headlightsEnabled=true;
            Vec3 hut=hutAnchor(world.system.orbitalPosition(1),4.5f,0),up=DARKSTONE_HUT_NORMAL;
            Vec3 tangent=normalized(cross({0,1,0},up)),front=cross(tangent,up);
            Vec3 position=hut+up*1.1f+front*1.3f+tangent*0.35f;
            world.system.sunOffset=-position;world.ship.lookAt(hut+up*0.06f-position);world.refreshTarget();
            preview(world,fb,previews,"darkstone_hut");
            for(double time:{2.12,3.4}) {
                Vec3 direction=transformDir(rotationY(-float(2*PI*2.12/90)),stormFlashAt(2.12).direction);
                viewPlanet(world,4,direction,19,time);
                preview(world,fb,previews,time<3?"storm_purple_flash":"storm_smoke_calm");
            }
            auto start=std::chrono::steady_clock::now();
            for(int k=0;k<20;++k) {world.system.time+=1.0/60;world.render(fb);}
            float ms=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count()/20;
            std::cout<<"Cuadro de Storm Giant con humo: "<<ms<<" ms\n";
        }
        std::cout<<checks<<" comprobaciones correctas\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FALLO: "<<error.what()<<'\n';return 1;}
}

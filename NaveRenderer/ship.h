#pragma once
#include "solar_system.h"

// CAMBIA ESTOS VALORES para ajustar como se siente la nave.
inline constexpr float SHIP_LENGTH=1.6f;      // Largo del modelo en unidades del mundo.
inline constexpr float SHIP_RADIUS=0.8f;      // Esfera de colision de la nave.
inline constexpr float THRUST=7.0f;           // Aceleracion de los propulsores (u/s^2).
inline constexpr float BOOST=2.5f;            // Multiplicador al mantener Shift.
inline constexpr float MATCH_ACCEL=9.0f;      // Aceleracion maxima al igualar velocidad.
inline constexpr float TURN_ACCEL=3.0f;       // Aceleracion de giro con teclado (rad/s^2).
inline constexpr float TURN_DAMPING=4.0f;     // Que tan rapido se frena el giro (1/s).
inline constexpr float MOUSE_TURN=0.012f;     // rad/s de giro que suma cada pixel de raton.
inline constexpr float SURFACE_FRICTION=4.0f; // Frenado sobre la superficie (1/s).

struct ShipInput {
    Vec3 thrust;   // x=derecha, y=arriba, z=adelante. Cada uno de -1 a 1.
    Vec3 turn;     // x=cabeceo, y=guinada, z=alabeo. Cada uno de -1 a 1.
    bool boost=false, matchVelocity=false;
};

struct Ship {
    // Velocidad respecto al sol. La POSICION no existe: la nave es el origen.
    Vec3 velocity;
    // Ejes de la nave en el mundo. forward = hacia donde apunta la nariz.
    Vec3 right{1,0,0}, up{0,1,0}, forward{0,0,-1};
    Vec3 spin;            // Velocidad de giro en ejes locales (rad/s).
    bool touching=false;  // true si esta apoyada en un planeta.

    void orthonormalize() {
        forward=normalized(forward);right=normalized(cross(forward,up));up=cross(right,forward);
    }
    void lookAt(Vec3 direction) {forward=direction;up={0,1,0};orthonormalize();}
};

// Un paso fijo de la simulacion. 'target' = cuerpo con el que igualamos velocidad.
// Devuelve true si hubo contacto letal con el sol.
inline bool stepShip(Ship& ship, SolarSystem& sys, const ShipInput& in, int target, float h) {
    // 1. Giro progresivo: el teclado acelera el giro y el amortiguamiento lo frena.
    ship.spin+=in.turn*(TURN_ACCEL*h);
    ship.spin=ship.spin*std::exp(-TURN_DAMPING*h);
    Vec3 r=ship.right,u=ship.up,f=ship.forward;
    for(Vec3* axis:{&ship.right,&ship.up,&ship.forward}) {
        *axis=rotateAround(*axis,r,ship.spin.x*h);   // Cabeceo: nariz arriba/abajo.
        *axis=rotateAround(*axis,u,ship.spin.y*h);   // Guinada: nariz izquierda/derecha.
        *axis=rotateAround(*axis,f,ship.spin.z*h);   // Alabeo: rodar.
    }
    ship.orthonormalize();

    // 2. Fuerzas: propulsores + gravedad de TODOS los cuerpos.
    Vec3 accel=(ship.right*in.thrust.x+ship.up*in.thrust.y+ship.forward*in.thrust.z)*
               (THRUST*(in.boost?BOOST:1.0f));
    if(in.matchVelocity) {
        // Empuja contra la velocidad relativa hasta quedar quieta respecto al objetivo.
        Vec3 relative=ship.velocity-sys.velocity(target);
        float speed=length(relative);
        if(speed>1e-4f) accel-=relative*(std::min(MATCH_ACCEL,speed/h)/speed);
    }
    accel+=sys.gravityAtShip();
    ship.velocity+=accel*h;

    // 3. La nave no se mueve: se mueve el mapa.
    Vec3 previousSun=sys.position(0);
    sys.advance(h,ship.velocity);

    // Barrido del trayecto completo: incluso con turbo no podemos cruzar el sol
    // entre dos pasos de fisica sin detectar el impacto.
    Vec3 sun=sys.position(0),segment=sun-previousSun;
    float segmentLength2=dot(segment,segment);
    float t=segmentLength2>1e-12f?std::clamp(-dot(previousSun,segment)/segmentLength2,0.0f,1.0f):0.0f;
    if(length(previousSun+segment*t)<=sys.bodies[0].radius+SHIP_RADIUS) return true;

    // 4. Colision basica: no se puede atravesar ningun cuerpo.
    ship.touching=false;
    for(int i=0;i<int(sys.bodies.size());++i) {
        if(sys.bodies[i].isSun) continue;
        Vec3 d=sys.position(i);
        float dist=length(d),minDist=sys.bodies[i].radius+SHIP_RADIUS;
        if(dist>=minDist || dist<1e-6f) continue;
        Vec3 normal=d*(-1.0f/dist);              // Del centro del planeta hacia la nave.
        sys.sunOffset-=normal*(minDist-dist);    // Empujar la nave = mover el mapa al reves.
        // Velocidad del suelo bajo la nave: orbita + giro del planeta.
        Vec3 ground=sys.velocity(i)+cross(sys.angularVelocity(i),-d);
        Vec3 relative=ship.velocity-ground;
        float intoSurface=dot(relative,normal);
        if(intoSurface<0) relative-=normal*intoSurface;
        relative=relative*std::exp(-SURFACE_FRICTION*h);
        ship.velocity=ground+relative;
        ship.touching=true;
    }
    return false;
}

#pragma once
#include <array>
#include <map>
#include "geometry.h"

// Orbita circular en el plano XZ. Depende del TIEMPO, no de los cuadros.
struct Orbit {float radius=0, period=1, phase=0;};
inline float orbitAngle(const Orbit& o, double t) {
    return o.phase+float(2*PI*std::fmod(t/o.period,1.0));
}
inline Vec3 orbitPosition(const Orbit& o, double t) {
    float a=orbitAngle(o,t);return {o.radius*std::cos(a),0,o.radius*std::sin(a)};
}
inline Vec3 orbitVelocity(const Orbit& o, double t) {
    float a=orbitAngle(o,t),w=2*PI/o.period;
    return {-o.radius*w*std::sin(a),0,o.radius*w*std::cos(a)};
}

enum class PlanetAppearance {Rock, Ember, Ice, Storm};

struct Body {
    std::string name;
    float radius;
    float surfaceGravity;  // Aceleracion en la superficie (unidades/s^2).
    Vec3 albedo;           // Color base de 0 a 1.
    Orbit orbit;           // Alrededor del sol.
    Orbit pair;            // Alrededor del centro del par (solo los gemelos).
    float spinPeriod;      // Segundos por vuelta sobre su propio eje.
    bool isSun=false;
    std::string description;
    std::string habitable="No";
    bool explored=false;
    std::string size="Mediano";
    PlanetAppearance appearance=PlanetAppearance::Rock;
    // G*M sale de g = G*M/R^2.
    float gm() const {return surfaceGravity*radius*radius;}
};

class SolarSystem {
public:
    std::vector<Body> bodies;
    double time=0;
    // Posicion del sol VISTA DESDE LA NAVE. La nave siempre esta en (0,0,0):
    // cuando "avanza", lo que cambia es este vector y todo el mapa se desplaza
    // en sentido contrario. La camara nunca sale del origen.
    Vec3 sunOffset;

    // Posicion respecto al sol (lo que se calcula con las orbitas).
    Vec3 orbitalPosition(int i) const {
        const Body& b=bodies[i];return orbitPosition(b.orbit,time)+orbitPosition(b.pair,time);
    }
    // Posicion respecto a la nave: la que se usa para dibujar y para la fisica.
    Vec3 position(int i) const {return sunOffset+orbitalPosition(i);}
    Vec3 velocity(int i) const {
        const Body& b=bodies[i];return orbitVelocity(b.orbit,time)+orbitVelocity(b.pair,time);
    }
    // El angulo negativo hace que giren en el mismo sentido en que orbitan.
    float spinAngle(int i) const {
        return -float(2*PI*std::fmod(time/bodies[i].spinPeriod,1.0));
    }
    Vec3 angularVelocity(int i) const {return {0,-2*PI/bodies[i].spinPeriod,0};}

    // Suma de las atracciones de todos los cuerpos sobre la nave (en el origen).
    Vec3 gravityAtShip() const {
        Vec3 total;
        for(int i=0;i<int(bodies.size());++i) {
            Vec3 d=position(i);float r=std::max(length(d),bodies[i].radius);
            total+=d*(bodies[i].gm()/(r*r*r));
        }
        return total;
    }
    // Avanza el reloj y mueve el MAPA al reves de la velocidad de la nave.
    void advance(float h, Vec3 shipVelocity) {time+=h;sunOffset-=shipVelocity*h;}
};

inline const Vec3 PLANET_GRAY{0.62f,0.62f,0.64f};
inline constexpr float PLANET_GRAVITY_SCALE=0.7f;

// CAMBIA ESTOS VALORES: radio, gravedad, orbita {radio, periodo, fase}, giro.
// Escala pequena a proposito: los planetas estan cerca y el viaje es corto.
inline SolarSystem makeSolarSystem() {
    SolarSystem s;
    s.bodies={
        {"Sol",        6.0f,5.0f,{1.0f,0.85f,0.4f},{},            {},              80.0f,true,
            "Estrella / contacto letal", "No",false,"Grande"},
        {"Darkstone",  4.5f,3.5f,{0.12f,0.13f,0.15f},{32,200,0.0f}, {},          60.0f,false,
            "Mundo de piedra negra", "???",true,"Mediano",PlanetAppearance::Rock},
        // Los gemelos comparten orbita alrededor del sol y giran entre si.
        {"Little Red", 2.0f,3.0f,{0.95f,0.27f,0.04f},{58,300,2.3f}, {4.5f,45,0.0f},40.0f,false,
            "Twins / mundo de fuego", "No",true,"Pequeño",PlanetAppearance::Ember},
        {"Little Blue",2.0f,3.0f,{0.05f,0.55f,0.68f},{58,300,2.3f}, {4.5f,45,PI}, 35.0f,false,
            "Twins / mundo de hielo", "No",true,"Pequeño",PlanetAppearance::Ice},
        // Su superficie es una cubierta de nubes, rodeada de humo persistente.
        {"Storm Giant",6.0f,3.5f,PLANET_GRAY,      {96,420,4.4f}, {},              90.0f,false,
            "Gigante de tormentas", "No",false,"Grande",PlanetAppearance::Storm},
    };
    for(Body& body:s.bodies) if(!body.isSun) body.surfaceGravity*=PLANET_GRAVITY_SCALE;
    return s;
}

// Esfera geodesica: icosaedro subdividido, todos los vertices a radio 1.
// Cada nivel multiplica por 4 los triangulos (nivel 0 = 20).
inline Mesh makeIcosphere(int subdivisions) {
    const float t=(1.0f+std::sqrt(5.0f))*0.5f;
    std::vector<Vec3> v={{-1,t,0},{1,t,0},{-1,-t,0},{1,-t,0},{0,-1,t},{0,1,t},
                         {0,-1,-t},{0,1,-t},{t,0,-1},{t,0,1},{-t,0,-1},{-t,0,1}};
    for(Vec3& p:v) p=normalized(p);
    std::vector<std::array<int,3>> faces={{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},
        {3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
    for(int level=0;level<subdivisions;++level) {
        std::map<std::pair<int,int>,int> middle;
        auto mid=[&](int a,int b) {
            auto key=std::minmax(a,b);
            auto found=middle.find(key);
            if(found!=middle.end()) return found->second;
            v.push_back(normalized((v[a]+v[b])*0.5f));
            return middle[key]=int(v.size())-1;
        };
        std::vector<std::array<int,3>> next;
        for(auto f:faces) {
            int ab=mid(f[0],f[1]),bc=mid(f[1],f[2]),ca=mid(f[2],f[0]);
            next.push_back({f[0],ab,ca});next.push_back({f[1],bc,ab});
            next.push_back({f[2],ca,bc});next.push_back({ab,bc,ca});
        }
        faces=std::move(next);
    }
    Mesh mesh;mesh.vertices=v;
    for(auto f:faces) {
        // Orden antihorario visto desde afuera, para descartar caras traseras.
        Vec3 a=v[f[0]],b=v[f[1]],c=v[f[2]];
        if(dot(cross(b-a,c-a),a+b+c)<0) std::swap(f[1],f[2]);
        Triangle tri{{f[0],f[1],f[2]},"Esfera"};
        mesh.triangles.push_back(tri);
    }
    return mesh;
}

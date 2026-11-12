#pragma once
#include <limits>
#include <random>
#include "renderer.h"
#include "ship.h"
#include "hud.h"
#include "planet_materials.h"

inline constexpr float FOV_Y=60.0f*PI/180.0f, NEAR_PLANE=0.05f, FAR_PLANE=3000.0f;
inline constexpr float AMBIENT=0.06f;  // Luz minima del lado de noche.

// ---------------------------------------------------------------- Camara
struct Camera {
    Vec3 eye,right,up,forward;
    Mat4 view,projection,viewProj;
    float tanHalfY,aspect,focalPx;
};
inline Camera makeCamera(Vec3 eye, Vec3 forward, Vec3 upHint) {
    Camera c;
    c.eye=eye;c.forward=normalized(forward);
    c.right=normalized(cross(c.forward,upHint));c.up=cross(c.right,c.forward);
    c.aspect=float(SCREEN_WIDTH)/SCREEN_HEIGHT;c.tanHalfY=std::tan(FOV_Y*0.5f);
    c.focalPx=SCREEN_HEIGHT*0.5f/c.tanHalfY;
    c.view=lookFrom(c.eye,c.right,c.up,c.forward);
    c.projection=perspective(FOV_Y,c.aspect,NEAR_PLANE,FAR_PLANE);
    c.viewProj=c.projection*c.view;
    return c;
}
// Camara de persecucion pegada a la nave: detras y un poco arriba.
// Gira con la nave; lo unico que "viaja" es el mapa.
inline Camera chaseCamera(const Ship& ship, const SolarSystem& sys, float distance) {
    Vec3 eye=ship.forward*-distance+ship.up*(distance*0.35f);
    // Si la camara quedaria dentro de un planeta, la sacamos a su superficie.
    for(int i=0;i<int(sys.bodies.size());++i) {
        Vec3 c=sys.position(i);float minDist=sys.bodies[i].radius+0.3f;
        Vec3 d=eye-c;
        if(length(d)<minDist) eye=c+normalized(d)*minDist;
    }
    Vec3 lookAt=ship.forward*distance;
    return makeCamera(eye,lookAt-eye,ship.up);
}
inline bool projectToScreen(const Camera& cam, Vec3 p, float& sx, float& sy) {
    Vec4 c=cam.viewProj*Vec4{p.x,p.y,p.z,1};
    if(c.w<NEAR_PLANE) return false;
    sx=(c.x/c.w*0.5f+0.5f)*SCREEN_WIDTH;sy=(0.5f-c.y/c.w*0.5f)*SCREEN_HEIGHT;
    return true;
}
// Descarte por frustum: true si la esfera toca el volumen que ve la camara.
inline bool sphereVisible(const Camera& cam, Vec3 center, float r) {
    Vec3 c=transformPoint(cam.view,center);
    if(c.z-r>-NEAR_PLANE || c.z+r<-FAR_PLANE) return false;
    float tx=cam.tanHalfY*cam.aspect,ty=cam.tanHalfY;
    float nx=1.0f/std::sqrt(1+tx*tx),ny=1.0f/std::sqrt(1+ty*ty);
    if(( c.x+c.z*tx)*nx>r || (-c.x+c.z*tx)*nx>r) return false;
    if(( c.y+c.z*ty)*ny>r || (-c.y+c.z*ty)*ny>r) return false;
    return true;
}

// Primer impacto de un rayo con una esfera. El mas cercano tapa a los demas.
inline float raySphereDistance(Vec3 origin,Vec3 direction,Vec3 center,float radius) {
    Vec3 d=center-origin;
    float along=dot(d,direction),height2=dot(d,d)-along*along;
    if(height2>radius*radius) return std::numeric_limits<float>::infinity();
    float half=std::sqrt(std::max(0.0f,radius*radius-height2));
    if(along+half<0) return std::numeric_limits<float>::infinity();
    return std::max(0.0f,along-half);
}
inline int gazeTarget(const Camera& cam,const SolarSystem& sys) {
    int hit=-1;float nearest=std::numeric_limits<float>::infinity();
    for(int i=0;i<int(sys.bodies.size());++i) {
        float distance=raySphereDistance(cam.eye,cam.forward,sys.position(i),sys.bodies[i].radius);
        if(distance<nearest) {nearest=distance;hit=i;}
    }
    if(hit>=0) return hit;

    // Margen de dos grados para apuntar a planetas diminutos a lo lejos.
    float bestGap=2.0f*PI/180.0f;
    for(int i=0;i<int(sys.bodies.size());++i) {
        Vec3 delta=sys.position(i)-cam.eye;float distance=length(delta);
        if(distance<1e-5f || dot(delta,cam.forward)<=0) continue;
        Vec3 direction=delta*(1.0f/distance);
        float angle=std::acos(std::clamp(dot(direction,cam.forward),-1.0f,1.0f));
        float angularRadius=std::asin(std::min(1.0f,sys.bodies[i].radius/distance));
        float gap=angle-angularRadius;
        if(gap>=bestGap) continue;
        // No seleccionar a traves de un cuerpo que oculta el centro del candidato.
        bool hidden=false;
        for(int j=0;j<int(sys.bodies.size());++j) {
            if(j!=i && raySphereDistance(cam.eye,direction,sys.position(j),sys.bodies[j].radius)<
                distance-sys.bodies[i].radius) {hidden=true;break;}
        }
        if(!hidden) {bestGap=gap;hit=i;}
    }
    return hit;
}

// ---------------------------------------------------------------- Shaders
// SHADER DE FRAGMENTOS de los planetas: luz puntual del sol calculada por pixel.
struct PlanetShader {
    const PlanetTexture* texture;
    PlanetAppearance appearance;
    Vec3 sunPos;
    Headlights headlights;
    StormFlash flash;
    Mat4 cloudDrift;
    Vec3 operator()(const Varying& v) const {
        Vec3 n=normalized(v.normal);
        Vec3 toSun=normalized(sunPos-v.world);
        float diffuse=std::max(0.0f,dot(n,toSun));
        Vec3 local=normalized(v.local);
        Vec3 p=appearance==PlanetAppearance::Storm?transformDir(cloudDrift,local):local;
        Vec3 albedo=texture->sample(p).color;
        if(appearance==PlanetAppearance::Rock) albedo=albedo*(0.88f+0.22f*noise3(local*360.0f));
        Vec3 illumination=Vec3{1.0f,0.98f,0.95f}*(AMBIENT+(1-AMBIENT)*diffuse);
        illumination+=headlightIrradiance(v.world,n,headlights);
        Vec3 color=multiplyColor(albedo,illumination);
        if(appearance==PlanetAppearance::Storm)
            color+=Vec3{0.70f,0.14f,1.20f}*(stormGlow(local,flash)*0.45f);
        return color;
    }
};
// SHADER DE FRAGMENTOS del sol: no recibe luz, la emite. El borde del disco se
// ve mas oscuro y anaranjado (oscurecimiento del limbo) y la superficie ondula.
struct SunShader {
    Vec3 eye;float time;
    Vec3 operator()(const Varying& v) const {
        Vec3 n=normalized(v.normal);
        float facing=std::max(0.0f,dot(n,normalized(eye-v.world)));
        float ripple=0.06f*std::sin(time*1.7f+9.0f*v.local.y+6.0f*v.local.x);
        float k=std::clamp(std::sqrt(facing)+ripple,0.0f,1.0f);
        Vec3 rim{1.0f,0.42f,0.08f},core{1.0f,0.96f,0.78f};
        return rim+(core-rim)*k;
    }
};

// ---------------------------------------------------------------- Estrellas
struct Star {Vec3 dir;uint32_t color;};
// Semilla fija: las estrellas siempre quedan en el mismo lugar.
inline std::vector<Star> makeStars(int count, unsigned seed=1234) {
    std::mt19937 rng(seed);std::normal_distribution<float> n(0,1);
    std::uniform_real_distribution<float> b(0.25f,1.0f);
    std::vector<Star> stars;
    for(int i=0;i<count;++i) {
        float k=b(rng);k*=k;
        stars.push_back({normalized({n(rng),n(rng),n(rng)}),Framebuffer::pack(Vec3{k,k,k*1.1f})});
    }
    return stars;
}
// Las estrellas estan "en el infinito": solo dependen de hacia donde mira la camara.
inline void drawStars(Framebuffer& fb, const Camera& cam, const std::vector<Star>& stars) {
    for(const Star& s:stars) {
        float x,y;
        if(projectToScreen(cam,cam.eye+s.dir*1000.0f,x,y)) fb.plot(int(x),int(y),s.color);
    }
}

// ---------------------------------------------------------------- Planetas
// Varias resoluciones de la misma esfera: se usa menos detalle cuando se ve chica.
struct SphereLods {std::vector<Mesh> levels;};
inline SphereLods makeSphereLods() {
    SphereLods lods;
    for(int s:{2,3,4,5}) lods.levels.push_back(makeIcosphere(s));
    return lods;
}

template<class Shader>
inline void drawSphere(Framebuffer& fb, const Camera& cam, const SphereLods& lods,
                       Vec3 center, float radius, float spin, const Shader& shader,bool rocky=false) {
    if(!sphereVisible(cam,center,radius)) {++fb.stats.objectsCulled;return;}
    float pixels=radius*cam.focalPx/std::max(length(center-cam.eye),0.001f);
    int lod=pixels<40?0:pixels<150?1:pixels<500?2:3;
    if(rocky) lod=std::max(1,lod);
    const Mesh& mesh=lods.levels[lod];
    // MATRIZ DE MODELO: escala al radio, gira sobre su eje y se coloca en su lugar.
    Mat4 rotation=rotationY(spin);
    Mat4 model=translation(center)*rotation*scaling(radius);
    static std::vector<ClipVertex> verts;
    verts.resize(mesh.vertices.size());
    for(std::size_t i=0;i<mesh.vertices.size();++i) {
        Vec3 p=mesh.vertices[i];
        Vec3 world=rocky?center+transformDir(rotation,p)*rockRadius(p,radius):transformPoint(model,p);
        verts[i]={cam.viewProj*Vec4{world.x,world.y,world.z,1},
                  {world,transformDir(rotation,rocky?rockNormal(p):p),p}};
    }
    for(const Triangle& t:mesh.triangles)
        fb.triangle(verts[t.indices[0]],verts[t.indices[1]],verts[t.indices[2]],true,shader);
}

// ---------------------------------------------------------------- Detalles del mundo
inline void drawLitTriangle(Framebuffer& fb,const Camera& cam,Vec3 a,Vec3 b,Vec3 c,
                            Vec3 color,Vec3 sunPos,const Headlights& lights,bool emissive=false) {
    Vec3 n=normalized(cross(b-a,c-a));
    if(dot(n,cam.eye-(a+b+c)*(1.0f/3))<0) n=-n;
    auto vertex=[&](Vec3 p) {return ClipVertex{cam.viewProj*Vec4{p.x,p.y,p.z,1},{p,n,{}}};};
    fb.triangle(vertex(a),vertex(b),vertex(c),false,[&](const Varying& v) {
        if(emissive) return color;
        float sun=std::max(0.0f,dot(n,normalized(sunPos-v.world)));
        return multiplyColor(color,Vec3{1,0.98f,0.95f}*(0.09f+0.91f*sun)+headlightIrradiance(v.world,n,lights));
    });
}
inline void drawLitBox(Framebuffer& fb,const Camera& cam,Vec3 center,Vec3 right,Vec3 up,Vec3 forward,
                       Vec3 halfSize,Vec3 color,Vec3 sunPos,const Headlights& lights) {
    std::array<Vec3,8> points;
    for(int i=0;i<8;++i) points[i]=center+right*((i&1)?halfSize.x:-halfSize.x)+
        up*((i&2)?halfSize.y:-halfSize.y)+forward*((i&4)?halfSize.z:-halfSize.z);
    constexpr int faces[6][4]={{0,1,3,2},{4,6,7,5},{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6}};
    for(const auto& f:faces) {
        drawLitTriangle(fb,cam,points[f[0]],points[f[1]],points[f[2]],color,sunPos,lights);
        drawLitTriangle(fb,cam,points[f[0]],points[f[2]],points[f[3]],color,sunPos,lights);
    }
}
inline const Vec3 DARKSTONE_HUT_NORMAL=normalized(Vec3{0.32f,-0.19f,-0.93f});
inline Vec3 hutAnchor(Vec3 center,float radius,float spin) {
    return center+transformDir(rotationY(spin),DARKSTONE_HUT_NORMAL)*
        (rockRadius(DARKSTONE_HUT_NORMAL,radius)-0.018f);
}
inline void drawHut(Framebuffer& fb,const Camera& cam,Vec3 center,float radius,float spin,
                    Vec3 sunPos,const Headlights& lights) {
    Vec3 base=hutAnchor(center,radius,spin);
    if(!sphereVisible(cam,base,0.25f) || 0.2f*cam.focalPx/std::max(0.01f,length(base-cam.eye))<0.5f) return;
    Mat4 rotation=rotationY(spin);
    Vec3 up=transformDir(rotation,DARKSTONE_HUT_NORMAL);
    Vec3 right=transformDir(rotation,normalized(cross({0,1,0},DARKSTONE_HUT_NORMAL)));
    Vec3 forward=cross(right,up);
    auto point=[&](float x,float y,float z) {return base+right*x+up*y+forward*z;};
    drawLitBox(fb,cam,point(0,0.055f,0),right,up,forward,{0.09f,0.055f,0.075f},
        {0.27f,0.18f,0.105f},sunPos,lights);
    auto tri=[&](Vec3 a,Vec3 b,Vec3 c,Vec3 color,bool glow=false) {
        drawLitTriangle(fb,cam,a,b,c,color,sunPos,lights,glow);
    };
    Vec3 a=point(-0.12f,0.10f,-0.095f),b=point(0.12f,0.10f,-0.095f),
         c=point(-0.12f,0.10f,0.095f),d=point(0.12f,0.10f,0.095f),
         ridgeA=point(0,0.18f,-0.095f),ridgeB=point(0,0.18f,0.095f);
    Vec3 roof{0.15f,0.12f,0.10f};
    tri(a,c,ridgeB,roof);tri(a,ridgeB,ridgeA,roof);
    tri(b,ridgeA,ridgeB,roof);tri(b,ridgeB,d,roof);
    tri(a,ridgeA,b,{0.31f,0.21f,0.13f});tri(c,d,ridgeB,{0.31f,0.21f,0.13f});
    // Puerta y ventanita calida: la choza sigue siendo diminuta respecto al planeta.
    Vec3 door{0.065f,0.039f,0.022f};
    tri(point(-0.03f,0.004f,0.076f),point(0.02f,0.004f,0.076f),point(0.02f,0.078f,0.076f),door);
    tri(point(-0.03f,0.004f,0.076f),point(0.02f,0.078f,0.076f),point(-0.03f,0.078f,0.076f),door);
    Vec3 window{0.95f,0.56f,0.20f};
    tri(point(0.044f,0.050f,0.0765f),point(0.074f,0.050f,0.0765f),point(0.074f,0.080f,0.0765f),window,true);
    tri(point(0.044f,0.050f,0.0765f),point(0.074f,0.080f,0.0765f),point(0.044f,0.080f,0.0765f),window,true);
}
inline void drawHeadlightHousings(Framebuffer& fb,const Camera& cam,const Ship& ship,Vec3 sunPos,const Headlights& lights) {
    if(!lights.enabled) return;
    for(Vec3 origin:lights.positions) {
        drawLitBox(fb,cam,origin-ship.forward*0.035f,ship.right,ship.up,ship.forward,
            {0.055f,0.04f,0.035f},{0.19f,0.22f,0.24f},sunPos,Headlights{{},{},false});
        Vec3 a=origin-ship.right*0.042f-ship.up*0.025f,b=origin+ship.right*0.042f-ship.up*0.025f,
             c=origin+ship.right*0.042f+ship.up*0.025f,d=origin-ship.right*0.042f+ship.up*0.025f;
        Vec3 white{1.0f,0.96f,0.82f};
        drawLitTriangle(fb,cam,a,b,c,white,sunPos,lights,true);
        drawLitTriangle(fb,cam,a,c,d,white,sunPos,lights,true);
    }
}
inline void drawLightningSegment(Framebuffer& fb,const Camera& cam,Vec3 a,Vec3 b,float width,Vec3 color) {
    Vec3 side=normalized(cross(b-a,cam.forward))*width;
    auto vertex=[&](Vec3 p) {return ClipVertex{cam.viewProj*Vec4{p.x,p.y,p.z,1},{p,{},{}}};};
    fb.triangle(vertex(a-side),vertex(a+side),vertex(b+side),false,[&](const Varying&) {return color;});
    fb.triangle(vertex(a-side),vertex(b+side),vertex(b-side),false,[&](const Varying&) {return color;});
}
inline void drawStormLightning(Framebuffer& fb,const Camera& cam,Vec3 center,float radius,float spin,const StormFlash& flash) {
    if(flash.strength<0.03f || !sphereVisible(cam,center,radius*1.04f)) return;
    Vec3 tangent=normalized(cross({0,1,0},flash.direction)),up=cross(flash.direction,tangent);
    Mat4 rotation=rotationY(spin);
    auto point=[&](float x,float y) {
        return center+transformDir(rotation,normalized(flash.direction+tangent*x+up*y))*(radius*1.014f);
    };
    const std::array<Vec3,6> nodes={point(0,0.20f),point(-0.025f,0.12f),point(0.04f,0.055f),
        point(-0.01f,0),point(0.08f,-0.09f),point(0.03f,-0.15f)};
    auto segment=[&](Vec3 a,Vec3 b) {
        drawLightningSegment(fb,cam,a,b,radius*0.003f,Vec3{0.85f,0.62f,1.0f}*flash.strength);
        drawLightningSegment(fb,cam,a,b,radius*0.011f,Vec3{0.40f,0.055f,0.85f}*flash.strength);
    };
    for(int i=1;i<int(nodes.size());++i) segment(nodes[i-1],nodes[i]);
    segment(nodes[2],point(-0.09f,0.01f));segment(point(-0.09f,0.01f),point(-0.15f,-0.04f));
}

// Humo volumetrico: tres muestras del tramo visible dentro de una envoltura.
// La profundidad opaca limita el tramo, para no pintar niebla sobre la nave
// ni sobre otro planeta que este delante. No se modifica el z-buffer.
inline void drawStormHaze(Framebuffer& fb,const Camera& cam,Vec3 center,float radius,float spin,
                          double time,Vec3 sunPos,const StormFlash& flash) {
    float outer=radius*1.18f;
    if(!sphereVisible(cam,center,outer)) return;
    Vec3 offset=cam.eye-center;
    float z=dot(center-cam.eye,cam.forward);
    int x0=0,x1=SCREEN_WIDTH-1,y0=0,y1=SCREEN_HEIGHT-1;
    if(z>outer+NEAR_PLANE) {
        float sx,sy;projectToScreen(cam,center,sx,sy);
        float r=outer*cam.focalPx/(z-outer);
        x0=std::max(0,int(std::floor(sx-r)));x1=std::min(SCREEN_WIDTH-1,int(std::ceil(sx+r)));
        y0=std::max(0,int(std::floor(sy-r)));y1=std::min(SCREEN_HEIGHT-1,int(std::ceil(sy+r)));
    }
    float sphereC=dot(offset,offset)-outer*outer;
    Mat4 cloudRotation=rotationY(-spin-float(time)*0.08f),surfaceRotation=rotationY(-spin);
    const auto& texture=planetTexture(PlanetAppearance::Storm);
    for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
        Vec3 ray=cam.forward+cam.right*((x+0.5f-SCREEN_WIDTH*0.5f)/cam.focalPx)+
            cam.up*((SCREEN_HEIGHT*0.5f-y-0.5f)/cam.focalPx);
        float a=dot(ray,ray),b=dot(offset,ray),discriminant=b*b-a*sphereC;
        if(discriminant<=0) continue;
        float root=std::sqrt(discriminant),near=std::max(NEAR_PLANE,(-b-root)/a),far=(-b+root)/a;
        int index=y*SCREEN_WIDTH+x;
        if(fb.depth[index]>0) far=std::min(far,1/fb.depth[index]);
        if(far<=near) continue;
        float delta=(far-near)/3,tau=0;Vec3 sum;
        for(int step=0;step<3;++step) {
            Vec3 position=cam.eye+ray*(near+(step+0.5f)*delta),relative=position-center;
            float height=length(relative)/radius;
            Vec3 local=transformDir(cloudRotation,relative);
            float billow=texture.sample(local).height;
            float density=(0.25f+billow*0.9f)*(1-smoothRange(1.005f,1.10f+0.075f*billow,height));
            float weight=density*delta*std::sqrt(a)/radius*2.8f;
            float daylight=std::max(0.0f,dot(normalized(relative),normalized(sunPos-position)));
            Vec3 color=Vec3{0.33f,0.35f,0.38f}*(0.28f+0.72f*daylight);
            color+=Vec3{0.75f,0.12f,1.1f}*(stormGlow(transformDir(surfaceRotation,relative),flash)*0.55f);
            sum+=color*weight;tau+=weight;
        }
        if(tau>1e-5f) fb.pixels[index]=hudBlend(fb.pixels[index],Framebuffer::pack(sum*(1/tau)),1-std::exp(-tau));
    }
}

// ---------------------------------------------------------------- Nave
struct ShipModel {std::vector<Vec3> vertices;std::vector<Triangle> triangles;std::vector<Vec3> colors;};
// true = dibuja la nave volteada (la parte de abajo del OBJ queda arriba).
inline constexpr bool FLIP_UPSIDE_DOWN=true;

// Centra el OBJ, lo escala a SHIP_LENGTH y lo pasa a ejes de la nave.
// En el OBJ la nariz apunta a +X; en la nave, adelante es -Z local.
inline ShipModel prepareShipModel(const Mesh& mesh) {
    // Algunos vertices de tu OBJ no pertenecen a ninguna cara: no cuentan.
    Vec3 lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
    for(const auto& t:mesh.triangles) for(int i:t.indices) {
        Vec3 p=mesh.vertices[i];
        lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
        hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
    }
    Vec3 center=(lo+hi)*0.5f;float scale=SHIP_LENGTH/std::max(hi.x-lo.x,0.001f);
    ShipModel ship;ship.triangles=mesh.triangles;
    for(Vec3 p:mesh.vertices) {
        p=(p-center)*scale;
        // El OBJ viene de cabeza: giro de 180 grados sobre su eje largo (X).
        if(FLIP_UPSIDE_DOWN) p={p.x,-p.y,-p.z};
        ship.vertices.push_back({p.z,p.y,-p.x});
    }
    for(const Triangle& face:mesh.triangles) {
        Color base=colorForPart(face.part);
        auto custom=FACE_COLORS.find(face.faceId);
        if(custom!=FACE_COLORS.end()) base=custom->second;
        ship.colors.push_back({base.r/255.0f,base.g/255.0f,base.b/255.0f});
    }
    return ship;
}

inline void drawShip(Framebuffer& fb, const Camera& cam, const ShipModel& model,
                     const Ship& ship, Vec3 sunPos) {
    // La nave esta en el origen: su matriz de modelo es solo su orientacion.
    Mat4 m=fromBasis(ship.right,ship.up,-ship.forward);
    static std::vector<ClipVertex> verts;
    verts.resize(model.vertices.size());
    for(std::size_t i=0;i<model.vertices.size();++i) {
        Vec3 world=transformPoint(m,model.vertices[i]);
        verts[i]={cam.viewProj*Vec4{world.x,world.y,world.z,1},{world,{},model.vertices[i]}};
    }
    for(std::size_t f=0;f<model.triangles.size();++f) {
        const Triangle& t=model.triangles[f];
        const ClipVertex &a=verts[t.indices[0]],&b=verts[t.indices[1]],&c=verts[t.indices[2]];
        // Sombreado plano por cara. Las normales del OBJ no siempre miran hacia
        // afuera, asi que se voltean hacia la camara (caras de dos lados).
        Vec3 n=normalized(cross(b.v.world-a.v.world,c.v.world-a.v.world));
        Vec3 centroid=(a.v.world+b.v.world+c.v.world)*(1.0f/3);
        if(dot(n,cam.eye-centroid)<0) n=-n;
        float diffuse=std::max(0.0f,dot(n,normalized(sunPos-centroid)));
        Vec3 color=model.colors[f]*(0.3f+0.7f*diffuse);
        fb.triangle(a,b,c,false,[color](const Varying&) {return color;});
    }
}

// ---------------------------------------------------------------- HUD
// Esquinas alrededor del objetivo, para encontrarlo aunque este lejos.
inline void drawTargetMarker(Framebuffer& fb, const Camera& cam, Vec3 center, float radius, bool showCallout=true) {
    float x,y;
    if(!projectToScreen(cam,center,x,y) || x<0 || y<0 || x>=SCREEN_WIDTH || y>=SCREEN_HEIGHT) return;
    float dist=std::max(length(center-cam.eye),0.001f);
    int s=int(std::clamp(radius*cam.focalPx/dist*1.1f,14.0f,400.0f)),arm=std::max(4,std::min(14,s/3));
    int cx=int(x),cy=int(y);uint32_t color=HUD_ACCENT;
    for(int sx:{-1,1}) for(int sy:{-1,1}) {
        int px=cx+sx*s,py=cy+sy*s;
        fb.hline(std::min(px,px-sx*arm),std::max(px,px-sx*arm),py,color);
        fb.vline(px,std::min(py,py-sy*arm),std::max(py,py-sy*arm),color);
    }
    if(!showCallout) return;
    hudRing(fb,cx,cy,10,HUD_TEXT);fb.plot(cx,cy,HUD_TEXT);
    // Linea de llamada hacia la ficha, como en la referencia.
    if(cx<622) {
        hudLine(fb,cx+11,cy,622,103,HUD_DIM);
        hudLine(fb,622,103,641,103,HUD_DIM);
        hudRing(fb,622,103,2,HUD_TEXT);
    }
}
// Cruz = centro de la mirada que selecciona objetivos. Cuadro = direccion de
// movimiento respecto al objetivo (si coinciden, vas directo hacia el).
inline void drawFlightMarkers(Framebuffer& fb, const Camera& cam, Vec3 relativeVelocity) {
    float x=SCREEN_WIDTH*0.5f,y=SCREEN_HEIGHT*0.5f;
    {
        uint32_t c=Framebuffer::pack(Vec3{0.9f,0.9f,0.9f});
        fb.hline(int(x)-6,int(x)-2,int(y),c);fb.hline(int(x)+2,int(x)+6,int(y),c);
        fb.vline(int(x),int(y)-6,int(y)-2,c);fb.vline(int(x),int(y)+2,int(y)+6,c);
    }
    if(length(relativeVelocity)>0.05f && projectToScreen(cam,normalized(relativeVelocity)*500.0f,x,y)) {
        uint32_t c=Framebuffer::pack(Vec3{1.0f,0.75f,0.2f});
        fb.hline(int(x)-4,int(x)+4,int(y)-4,c);fb.hline(int(x)-4,int(x)+4,int(y)+4,c);
        fb.vline(int(x)-4,int(y)-4,int(y)+4,c);fb.vline(int(x)+4,int(y)-4,int(y)+4,c);
    }
}

// ---------------------------------------------------------------- Escena
struct World {
    SolarSystem system=makeSolarSystem();
    Ship ship;
    int target=1;
    ShipModel shipModel;
    SphereLods spheres=makeSphereLods();
    std::vector<Star> stars=makeStars(1500);
    float cameraDistance=4.0f;
    bool firstPerson=false,targetInSight=false,dead=false;
    bool headlightsEnabled=true;
    float deathElapsed=0;
    int displayedDistanceKm=0;
    static constexpr float KM_PER_UNIT=10.0f, RESPAWN_SECONDS=3.0f;

    Camera camera() const {
        // La cabina mira desde la nave y gira exactamente con sus ejes.
        return firstPerson?makeCamera({},ship.forward,ship.up):chaseCamera(ship,system,cameraDistance);
    }
    float surfaceDistanceKm() const {
        float gap=length(system.position(target))-system.bodies[target].radius-SHIP_RADIUS;
        return std::max(0.0f,gap)*KM_PER_UNIT;
    }
    int roundedDistanceKm() const {
        return int(std::min(999999.0f,std::round(surfaceDistanceKm())));
    }
    void refreshTarget() {
        int seen=gazeTarget(camera(),system);
        targetInSight=seen>=0;
        if(seen>=0 && seen!=target) {
            target=seen;
        }
        displayedDistanceKm=roundedDistanceKm();
    }
    void toggleView() {
        if(dead) return;
        firstPerson=!firstPerson;refreshTarget();
    }
    // Distancia geometrica actual: el contador no debe quedarse atras al viajar.
    void updateHud(float) {
        if(dead) return;
        displayedDistanceKm=roundedDistanceKm();
    }

    // Empieza flotando cerca de Darkstone, quieta respecto a el.
    void reset() {
        system=makeSolarSystem();
        Vec3 planet=system.orbitalPosition(1);
        Vec3 radial=normalized(planet),tangent=normalized(system.velocity(1));
        Vec3 start=planet+tangent*14.0f-radial*5.0f+Vec3{0,3.0f,0};
        system.sunOffset=-start;
        ship=Ship{};ship.velocity=system.velocity(1);
        ship.lookAt(planet-start);
        target=1;dead=false;deathElapsed=0;
        displayedDistanceKm=roundedDistanceKm();refreshTarget();
    }
    Vec3 relativeVelocity() const {return ship.velocity-system.velocity(target);}

    void step(const ShipInput& input,float h) {
        if(dead) {
            deathElapsed+=h;
            if(deathElapsed+1e-5f>=RESPAWN_SECONDS) reset();
            return;
        }
        if(stepShip(ship,system,input,target,h)) {
            dead=true;deathElapsed=0;ship.velocity={};ship.spin={};
            return;
        }
        refreshTarget();
    }

    void render(Framebuffer& fb) const {
        fb.clear();
        Camera cam=camera();
        drawStars(fb,cam,stars);
        Vec3 sunPos=system.position(0);
        Headlights lights=makeHeadlights(ship,headlightsEnabled && !dead,&system);
        StormFlash flash=stormFlashAt(system.time);
        for(int i=0;i<int(system.bodies.size());++i) {
            const Body& b=system.bodies[i];
            Vec3 c=system.position(i);float spin=system.spinAngle(i);
            if(b.isSun) drawSphere(fb,cam,spheres,c,b.radius,spin,SunShader{cam.eye,float(system.time)});
            else {
                Headlights bodyLights=lights;bodyLights.receivingBody=i;
                PlanetShader shader{&planetTexture(b.appearance),b.appearance,sunPos,bodyLights,flash,
                    rotationY(float(system.time)*0.035f)};
                drawSphere(fb,cam,spheres,c,b.radius,spin,shader,b.appearance==PlanetAppearance::Rock);
                if(b.appearance==PlanetAppearance::Rock) drawHut(fb,cam,c,b.radius,spin,sunPos,bodyLights);
                if(b.appearance==PlanetAppearance::Storm) drawStormLightning(fb,cam,c,b.radius,spin,flash);
            }
        }
        if(!firstPerson) {
            drawShip(fb,cam,shipModel,ship,sunPos);
            drawHeadlightHousings(fb,cam,ship,sunPos,lights);
        }
        for(int i=0;i<int(system.bodies.size());++i) {
            const Body& b=system.bodies[i];
            if(!b.isSun && b.appearance==PlanetAppearance::Storm)
                drawStormHaze(fb,cam,system.position(i),b.radius,system.spinAngle(i),system.time,sunPos,flash);
        }
        if(dead) {drawDeathScreen(fb,deathElapsed);return;}
        drawFlightMarkers(fb,cam,relativeVelocity());
        if(targetInSight) {
            drawTargetMarker(fb,cam,system.position(target),system.bodies[target].radius,!firstPerson);
            if(!firstPerson) drawPlanetInfo(fb,system.bodies[target],displayedDistanceKm);
        }
        if(firstPerson) drawCockpit(fb,targetInSight?&system.bodies[target]:nullptr,
            displayedDistanceKm,relativeVelocity(),ship.touching);
        drawViewButton(fb,firstPerson);drawControlHint(fb,firstPerson);
    }
};

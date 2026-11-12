#pragma once
#include <cstdint>
#include "ship.h"

inline float smoothRange(float a,float b,float v) {
    float t=std::clamp((v-a)/(b-a),0.0f,1.0f);return t*t*(3-2*t);
}
inline Vec3 mixColor(Vec3 a,Vec3 b,float t) {return a+(b-a)*std::clamp(t,0.0f,1.0f);}
inline Vec3 multiplyColor(Vec3 a,Vec3 b) {return {a.x*b.x,a.y*b.y,a.z*b.z};}
inline float noiseHash(int x,int y,int z) {
    uint32_t n=uint32_t(x)*374761393u+uint32_t(y)*668265263u+uint32_t(z)*2246822519u;
    n=(n^(n>>13))*1274126177u;n^=n>>16;
    return float(n&0x00ffffffu)/16777215.0f;
}
inline float noise3(Vec3 p) {
    int x=int(std::floor(p.x)),y=int(std::floor(p.y)),z=int(std::floor(p.z));
    float u=p.x-x,v=p.y-y,w=p.z-z;
    u=u*u*(3-2*u);v=v*v*(3-2*v);w=w*w*(3-2*w);
    auto lerp=[](float a,float b,float t) {return a+(b-a)*t;};
    return lerp(lerp(lerp(noiseHash(x,y,z),noiseHash(x+1,y,z),u),
                     lerp(noiseHash(x,y+1,z),noiseHash(x+1,y+1,z),u),v),
                lerp(lerp(noiseHash(x,y,z+1),noiseHash(x+1,y,z+1),u),
                     lerp(noiseHash(x,y+1,z+1),noiseHash(x+1,y+1,z+1),u),v),w);
}
inline float fractalNoise(Vec3 p,int octaves=4) {
    float sum=0,weight=0.55f,total=0;
    for(int i=0;i<octaves;++i) {
        sum+=noise3(p)*weight;total+=weight;
        p=p*2.03f+Vec3{13.1f,7.7f,19.3f};weight*=0.5f;
    }
    return sum/total;
}
struct RockCell {float edge,tone;};
inline RockCell rockCells(Vec3 p) {
    int bx=int(std::floor(p.x)),by=int(std::floor(p.y)),bz=int(std::floor(p.z));
    float nearest=100,second=100,tone=0;
    for(int z=bz-1;z<=bz+1;++z) for(int y=by-1;y<=by+1;++y) for(int x=bx-1;x<=bx+1;++x) {
        Vec3 site{float(x)+0.2f+0.6f*noiseHash(x,y,z),float(y)+0.2f+0.6f*noiseHash(x+17,y,z),
                  float(z)+0.2f+0.6f*noiseHash(x,y+47,z)};
        float d=dot(site-p,site-p);
        if(d<nearest) {second=nearest;nearest=d;tone=noiseHash(x+53,y-11,z+7);}
        else if(d<second) second=d;
    }
    return {std::sqrt(second)-std::sqrt(nearest),tone};
}

struct SurfaceSample {Vec3 color;float height;};
inline SurfaceSample proceduralSurface(PlanetAppearance kind,Vec3 p) {
    if(kind==PlanetAppearance::Rock) {
        RockCell cell=rockCells(p*13.0f),small=rockCells(p*46.0f);
        float grain=fractalNoise(p*90.0f,3);
        float cracks=smoothRange(0.008f,0.055f,cell.edge);
        float fineCracks=0.6f+0.4f*smoothRange(0.006f,0.035f,small.edge);
        float stone=(0.65f+cell.tone*0.75f)*(0.8f+grain*0.4f);
        Vec3 color=Vec3{0.115f,0.125f,0.145f}*(stone*fineCracks);
        color=mixColor({0.008f,0.009f,0.012f},color,cracks);
        return {color,std::clamp((0.35f+cell.tone*0.4f+grain*0.25f)*cracks,0.0f,1.0f)};
    }
    float warp=fractalNoise(p*3.7f);
    Vec3 q{p.x*3.0f+warp*1.6f,p.y*8.0f,p.z*3.0f-warp};
    float cloud=fractalNoise(q*2.1f),fine=fractalNoise(q*9.0f,3);
    if(kind==PlanetAppearance::Ember) {
        float flow=0.5f+0.5f*std::sin(p.y*10.0f+warp*8.0f+cloud*4.0f);
        float orange=std::clamp(cloud*1.5f+flow*0.25f-0.35f,0.0f,1.0f);
        Vec3 color=mixColor({0.46f,0.055f,0.015f},{1.0f,0.49f,0.055f},orange);
        color=mixColor(color,{1.0f,0.68f,0.19f},smoothRange(0.58f,0.85f,cloud)*0.7f);
        return {color*(0.88f+fine*0.22f),cloud};
    }
    if(kind==PlanetAppearance::Ice) {
        float latitude=p.y+0.10f*(warp-0.5f);
        float bands=0.5f+0.5f*std::sin(latitude*23.0f+cloud*3.0f);
        float streak=0.5f+0.5f*std::sin(latitude*150.0f+warp*20.0f+fine*9.0f);
        Vec3 color=mixColor({0.015f,0.19f,0.28f},{0.055f,0.65f,0.72f},cloud*0.9f+bands*0.22f);
        float equator=std::exp(-std::pow((latitude+0.12f)*13.0f,2.0f));
        float white=std::clamp(equator*0.7f+smoothRange(0.72f,0.98f,bands)*fine*0.3f+streak*0.08f,0.0f,0.9f);
        return {mixColor(color,{0.63f,0.91f,0.93f},white),cloud};
    }
    float billow=smoothRange(0.22f,0.8f,cloud*0.8f+fine*0.2f);
    return {mixColor({0.19f,0.205f,0.22f},{0.66f,0.68f,0.70f},billow),billow};
}

// Se hornea una textura cubica continua por planeta una vez. El shader solo
// interpola texeles: evita evaluar ruido fractal en cada pixel de cada cuadro.
struct PlanetTexture {
    int size;
    std::vector<SurfaceSample> texels;
    static Vec3 cubeDirection(int face,float u,float v) {
        switch(face) {
            case 0:return normalized({1,v,-u});case 1:return normalized({-1,v,u});
            case 2:return normalized({u,1,-v});case 3:return normalized({u,-1,v});
            case 4:return normalized({u,v,1});default:return normalized({-u,v,-1});
        }
    }
    explicit PlanetTexture(PlanetAppearance kind):size(kind==PlanetAppearance::Rock?512:256),texels(6*size*size) {
        for(int f=0;f<6;++f) for(int y=0;y<size;++y) for(int x=0;x<size;++x)
            texels[(f*size+y)*size+x]=proceduralSurface(kind,cubeDirection(f,2.0f*x/(size-1)-1,2.0f*y/(size-1)-1));
    }
    SurfaceSample sample(Vec3 d) const {
        float ax=std::abs(d.x),ay=std::abs(d.y),az=std::abs(d.z),u,v;int f;
        if(ax>=ay && ax>=az) {float k=1/std::max(ax,1e-8f);f=d.x>=0?0:1;u=(d.x>=0?-d.z:d.z)*k;v=d.y*k;}
        else if(ay>=az) {float k=1/ay;f=d.y>=0?2:3;u=d.x*k;v=(d.y>=0?-d.z:d.z)*k;}
        else {float k=1/az;f=d.z>=0?4:5;u=(d.z>=0?d.x:-d.x)*k;v=d.y*k;}
        float x=std::clamp((u+1)*0.5f*(size-1),0.0f,float(size-1));
        float y=std::clamp((v+1)*0.5f*(size-1),0.0f,float(size-1));
        int ix=std::min(int(x),size-2),iy=std::min(int(y),size-2);float tx=x-ix,ty=y-iy;
        auto blend=[](SurfaceSample a,SurfaceSample b,float t) {
            return SurfaceSample{a.color+(b.color-a.color)*t,a.height+(b.height-a.height)*t};
        };
        int index=(f*size+iy)*size+ix;
        return blend(blend(texels[index],texels[index+1],tx),
                     blend(texels[index+size],texels[index+size+1],tx),ty);
    }
};
inline const PlanetTexture& planetTexture(PlanetAppearance kind) {
    static const std::array<PlanetTexture,4> textures={PlanetTexture(PlanetAppearance::Rock),
        PlanetTexture(PlanetAppearance::Ember),PlanetTexture(PlanetAppearance::Ice),PlanetTexture(PlanetAppearance::Storm)};
    return textures[static_cast<int>(kind)];
}
inline float rockRadius(Vec3 local,float radius) {
    return radius*(0.994f+0.006f*planetTexture(PlanetAppearance::Rock).sample(local).height);
}
inline Vec3 rockNormal(Vec3 p) {
    p=normalized(p);
    Vec3 tangent=normalized(cross(std::abs(p.y)<0.9f?Vec3{0,1,0}:Vec3{1,0,0},p)),bitangent=cross(p,tangent);
    const auto& texture=planetTexture(PlanetAppearance::Rock);
    float h=texture.sample(p).height;
    float du=texture.sample(p+tangent*0.012f).height-h,dv=texture.sample(p+bitangent*0.012f).height-h;
    return normalized(p-tangent*(du*0.5f)-bitangent*(dv*0.5f));
}

struct StormFlash {float strength=0;Vec3 direction;};
inline StormFlash stormFlashAt(double time) {
    float cycle=float(std::floor(time/7.5)),phase=float(time-cycle*7.5);
    float strength=std::exp(-std::pow((phase-2.12f)/0.055f,2.0f))+
                   0.85f*std::exp(-std::pow((phase-2.37f)/0.08f,2.0f));
    float angle=cycle*2.39996f;
    Vec3 direction=transformDir(rotationY(angle),normalized(Vec3{0.4f,0.2f,-1}));
    return {std::min(1.0f,strength),direction};
}
inline float stormGlow(Vec3 local,const StormFlash& flash) {
    float alignment=dot(normalized(local),flash.direction);
    float patch=smoothRange(0.90f,0.998f,alignment);
    return patch*patch*flash.strength;
}

struct Headlights {
    std::array<Vec3,2> positions;
    Vec3 forward;
    bool enabled=true;
    struct Blocker {Vec3 center;float radius=0;};
    std::array<Blocker,8> blockers{};
    int blockerCount=0,receivingBody=-1;
};
inline Headlights makeHeadlights(const Ship& ship,bool enabled=true,const SolarSystem* system=nullptr) {
    Vec3 front=ship.forward*0.60f+ship.up*0.06f;
    Headlights lights{{front-ship.right*0.34f,front+ship.right*0.34f},ship.forward,enabled};
    if(system) {
        lights.blockerCount=std::min(int(lights.blockers.size()),int(system->bodies.size()));
        for(int i=0;i<lights.blockerCount;++i) lights.blockers[i]={system->position(i),system->bodies[i].radius};
    }
    return lights;
}
inline Vec3 headlightIrradiance(Vec3 world,Vec3 normal,const Headlights& lights) {
    if(!lights.enabled) return {};
    float sum=0;
    for(Vec3 position:lights.positions) {
        Vec3 ray=world-position;float d2=dot(ray,ray);
        if(d2>18*18 || d2<1e-8f) continue;
        float distance=std::sqrt(d2);ray=ray*(1/distance);
        float cone=smoothRange(0.8988f,0.9781f,dot(ray,lights.forward));
        float diffuse=std::max(0.0f,dot(normal,-ray));
        if(cone*diffuse<=0) continue;
        bool blocked=false;
        for(int i=0;i<lights.blockerCount;++i) {
            if(i==lights.receivingBody) continue;
            Vec3 toCenter=lights.blockers[i].center-position;
            float along=dot(toCenter,ray),r=lights.blockers[i].radius;
            float discriminant=r*r-dot(toCenter,toCenter)+along*along;
            if(discriminant<0) continue;
            float half=std::sqrt(discriminant);
            if(along+half>0.001f && along-half<distance-0.01f) {blocked=true;break;}
        }
        if(blocked) continue;
        sum+=cone*diffuse*(1-smoothRange(10.0f,18.0f,distance))*3.5f/(1+0.25f*d2);
    }
    return Vec3{1.0f,0.94f,0.82f}*sum;
}

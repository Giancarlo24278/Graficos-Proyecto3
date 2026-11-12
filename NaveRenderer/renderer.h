#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include "geometry.h"

// Resolucion interna del framebuffer. La ventana la agranda WINDOW_SCALE veces.
inline constexpr int SCREEN_WIDTH=960, SCREEN_HEIGHT=540;
inline constexpr float WINDOW_SCALE=1.5f;

// Datos que cada vertice le entrega al shader de fragmentos. El rasterizador
// los interpola con correccion de perspectiva.
struct Varying {
    Vec3 world;   // Posicion en el mundo (la nave esta en el origen).
    Vec3 normal;  // Normal en el mundo.
    Vec3 local;   // Posicion en el espacio del modelo (para patrones).
};
inline Varying operator+(const Varying& a, const Varying& b) {
    return {a.world+b.world,a.normal+b.normal,a.local+b.local};
}
inline Varying operator*(const Varying& a, float s) {return {a.world*s,a.normal*s,a.local*s};}

struct ClipVertex {Vec4 clip; Varying v;};
// En pantalla guardamos 1/w y los varyings ya multiplicados por 1/w.
struct ScreenVertex {float x,y,invW; Varying v;};

// Contadores para comparar cuanto trabajo hace el renderizador.
struct RenderStats {int submitted=0,drawn=0,objectsCulled=0;};

class Framebuffer {
public:
    std::vector<uint32_t> pixels;
    std::vector<float> depth;
    RenderStats stats;
    Framebuffer():pixels(SCREEN_WIDTH*SCREEN_HEIGHT),depth(pixels.size()) {clear();}
    void clear() {
        std::fill(pixels.begin(),pixels.end(),pack(BACKGROUND));
        // Profundidad = 1/w: mayor = mas cerca. 0 equivale a "infinitamente lejos".
        std::fill(depth.begin(),depth.end(),0.0f);
        stats={};
    }
    static uint32_t pack(Color c) {
        return 0xff000000u|(uint32_t(c.r)<<16)|(uint32_t(c.g)<<8)|c.b;
    }
    static uint32_t pack(Vec3 c) {
        auto channel=[](float v) {return uint32_t(std::clamp(v,0.0f,1.0f)*255.0f+0.5f);};
        return 0xff000000u|(channel(c.x)<<16)|(channel(c.y)<<8)|channel(c.z);
    }
    void plot(int x,int y,uint32_t color) {
        if(x>=0 && y>=0 && x<SCREEN_WIDTH && y<SCREEN_HEIGHT) pixels[y*SCREEN_WIDTH+x]=color;
    }
    void hline(int x0,int x1,int y,uint32_t color) {for(int x=x0;x<=x1;++x) plot(x,y,color);}
    void vline(int x,int y0,int y1,uint32_t color) {for(int y=y0;y<=y1;++y) plot(x,y,color);}

    static float edge(const ScreenVertex& a,const ScreenVertex& b,float x,float y) {
        return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
    }

    // ESTA ES LA FUNCION QUE PINTA TRIANGULOS EN SOFTWARE.
    // Recibe los vertices ya multiplicados por proyeccion*vista*modelo.
    // 'shade' es el shader de fragmentos: recibe los varyings y regresa un color.
    template<class Shader>
    void triangle(const ClipVertex& a,const ClipVertex& b,const ClipVertex& c,
                  bool cullBack,Shader&& shade) {
        ++stats.submitted;
        // 1. Recorte contra el plano cercano (z=-w). Sin esto, los vertices que
        //    quedan detras de la camara se proyectan al reves.
        const ClipVertex* in[3]={&a,&b,&c};
        ClipVertex poly[4];int count=0;
        for(int i=0;i<3;++i) {
            const ClipVertex& p=*in[i];const ClipVertex& q=*in[(i+1)%3];
            float dp=p.clip.z+p.clip.w,dq=q.clip.z+q.clip.w;
            if(dp>=0) poly[count++]=p;
            if((dp>=0)!=(dq>=0)) {
                float t=dp/(dp-dq);
                poly[count++]={{p.clip.x+(q.clip.x-p.clip.x)*t,p.clip.y+(q.clip.y-p.clip.y)*t,
                                p.clip.z+(q.clip.z-p.clip.z)*t,p.clip.w+(q.clip.w-p.clip.w)*t},
                               p.v*(1-t)+q.v*t};
            }
        }
        if(count<3) return;
        // 2. Division de perspectiva y paso a pixeles (Y crece hacia abajo).
        ScreenVertex s[4];
        for(int i=0;i<count;++i) {
            float invW=1.0f/poly[i].clip.w;
            s[i]={(poly[i].clip.x*invW*0.5f+0.5f)*SCREEN_WIDTH,
                  (0.5f-poly[i].clip.y*invW*0.5f)*SCREEN_HEIGHT,invW,poly[i].v*invW};
        }
        // 3. El poligono recortado (3 o 4 vertices) se dibuja como abanico.
        bool drawn=false;
        for(int i=1;i+1<count;++i) drawn|=rasterize(s[0],s[i],s[i+1],cullBack,shade);
        if(drawn) ++stats.drawn;
    }

    // Exportacion sin librerias adicionales: PPM, convertible a PNG con Python.
    void savePPM(const std::string& path) const {
        std::ofstream out(path,std::ios::binary);
        if(!out) throw std::runtime_error("No se pudo guardar: "+path);
        out<<"P6\n"<<SCREEN_WIDTH<<" "<<SCREEN_HEIGHT<<"\n255\n";
        for(uint32_t pixel:pixels) {
            const char rgb[3]={char((pixel>>16)&255),char((pixel>>8)&255),char(pixel&255)};
            out.write(rgb,3);
        }
        if(!out) throw std::runtime_error("Error escribiendo: "+path);
    }

private:
    template<class Shader>
    bool rasterize(const ScreenVertex& a,const ScreenVertex& b,const ScreenVertex& c,
                   bool cullBack,Shader& shade) {
        float area=edge(a,b,c.x,c.y);
        // Con Y hacia abajo, un triangulo de frente (antihorario) da area negativa.
        if(std::abs(area)<1e-9f || (cullBack && area>0)) return false;
        int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x}))));
        int x1=std::min(SCREEN_WIDTH-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
        int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y}))));
        int y1=std::min(SCREEN_HEIGHT-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
        if(x0>x1 || y0>y1) return false;
        float inv=1.0f/area;
        // Las coordenadas baricentricas cambian lo mismo en cada pixel hacia la
        // derecha, asi que se suman en lugar de recalcularse.
        float dw0=-(c.y-b.y)*inv,dw1=-(a.y-c.y)*inv,dw2=-(b.y-a.y)*inv;
        for(int y=y0;y<=y1;++y) {
            float px=x0+0.5f,py=y+0.5f;
            float w0=edge(b,c,px,py)*inv,w1=edge(c,a,px,py)*inv,w2=edge(a,b,px,py)*inv;
            uint32_t* row=&pixels[y*SCREEN_WIDTH];float* zrow=&depth[y*SCREEN_WIDTH];
            for(int x=x0;x<=x1;++x,w0+=dw0,w1+=dw1,w2+=dw2) {
                if(w0<0 || w1<0 || w2<0) continue;
                float invW=w0*a.invW+w1*b.invW+w2*c.invW;
                // Prueba de profundidad ANTES del shader: si queda tapado no se pinta.
                if(invW<=zrow[x]) continue;
                zrow[x]=invW;
                Varying v=(a.v*w0+b.v*w1+c.v*w2)*(1.0f/invW);
                row[x]=pack(shade(v));
            }
        }
        return true;
    }
};

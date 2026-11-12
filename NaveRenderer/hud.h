#pragma once
#include <array>
#include <cstdio>
#include "renderer.h"
#include "solar_system.h"

// HUD en el mismo framebuffer: sin fuentes, texturas ni dependencias externas.
inline constexpr uint32_t HUD_INK=0xff08151fu, HUD_EDGE=0xff47717fu;
inline constexpr uint32_t HUD_TEXT=0xffe5f1f3u, HUD_DIM=0xff93aeb8u;
inline constexpr uint32_t HUD_ACCENT=0xff76e5d0u, HUD_GOLD=0xffffcb79u;
inline constexpr int VIEW_BUTTON_X=24, VIEW_BUTTON_Y=22, VIEW_BUTTON_W=206, VIEW_BUTTON_H=36;

inline uint32_t hudBlend(uint32_t bg,uint32_t fg,float opacity) {
    int a=int(std::clamp(opacity,0.0f,1.0f)*255);
    uint32_t result=0xff000000u;
    for(int shift:{0,8,16}) {
        int channel=(int((bg>>shift)&255)*(255-a)+int((fg>>shift)&255)*a+127)/255;
        result|=uint32_t(channel)<<shift;
    }
    return result;
}
inline void hudFill(Framebuffer& fb,int x,int y,int w,int h,uint32_t color,float opacity=1) {
    for(int py=std::max(0,y);py<std::min(SCREEN_HEIGHT,y+h);++py)
        for(int px=std::max(0,x);px<std::min(SCREEN_WIDTH,x+w);++px) {
            uint32_t& pixel=fb.pixels[py*SCREEN_WIDTH+px];
            pixel=opacity>=1?color:hudBlend(pixel,color,opacity);
        }
}
inline void hudBorder(Framebuffer& fb,int x,int y,int w,int h,uint32_t color) {
    fb.hline(x,x+w-1,y,color);fb.hline(x,x+w-1,y+h-1,color);
    fb.vline(x,y,y+h-1,color);fb.vline(x+w-1,y,y+h-1,color);
}
inline void hudLine(Framebuffer& fb,int x,int y,int x1,int y1,uint32_t color) {
    int dx=std::abs(x1-x),sx=x<x1?1:-1,dy=-std::abs(y1-y),sy=y<y1?1:-1,err=dx+dy;
    for(;;) {
        fb.plot(x,y,color);
        if(x==x1 && y==y1) break;
        int e=2*err;
        if(e>=dy) {err+=dy;x+=sx;}
        if(e<=dx) {err+=dx;y+=sy;}
    }
}
inline void hudRing(Framebuffer& fb,int x,int y,int r,uint32_t color) {
    int px=r,py=0,err=1-r;
    while(px>=py) {
        for(int sx:{-1,1}) for(int sy:{-1,1}) {
            fb.plot(x+sx*px,y+sy*py,color);fb.plot(x+sx*py,y+sy*px,color);
        }
        ++py;
        if(err<0) err+=2*py+1;else {--px;err+=2*(py-px)+1;}
    }
}

// Fuente 5x7; se aceptan minusculas y la eñe de Tamaño / Pequeño.
inline std::array<unsigned char,7> hudGlyph(unsigned c) {
    if(c>='a' && c<='z') c-=32;
    switch(c) {
        case 'A':return {14,17,17,31,17,17,17};case 'B':return {30,17,17,30,17,17,30};
        case 'C':return {14,17,16,16,16,17,14};case 'D':return {30,17,17,17,17,17,30};
        case 'E':return {31,16,16,30,16,16,31};case 'F':return {31,16,16,30,16,16,16};
        case 'G':return {14,17,16,23,17,17,15};case 'H':return {17,17,17,31,17,17,17};
        case 'I':return {14,4,4,4,4,4,14};case 'J':return {7,2,2,2,18,18,12};
        case 'K':return {17,18,20,24,20,18,17};case 'L':return {16,16,16,16,16,16,31};
        case 'M':return {17,27,21,21,17,17,17};case 'N':return {17,25,25,21,19,19,17};
        case 'O':return {14,17,17,17,17,17,14};case 'P':return {30,17,17,30,16,16,16};
        case 'Q':return {14,17,17,17,21,18,13};case 'R':return {30,17,17,30,20,18,17};
        case 'S':return {15,16,16,14,1,1,30};case 'T':return {31,4,4,4,4,4,4};
        case 'U':return {17,17,17,17,17,17,14};case 'V':return {17,17,17,17,17,10,4};
        case 'W':return {17,17,17,21,21,21,10};case 'X':return {17,17,10,4,10,17,17};
        case 'Y':return {17,17,10,4,4,4,4};case 'Z':return {31,1,2,4,8,16,31};
        case 0xd1:case 0xf1:return {10,4,17,25,21,19,17};
        case '0':return {14,17,19,21,25,17,14};case '1':return {4,12,4,4,4,4,14};
        case '2':return {14,17,1,2,4,8,31};case '3':return {30,1,1,14,1,1,30};
        case '4':return {2,6,10,18,31,2,2};case '5':return {31,16,16,30,1,1,30};
        case '6':return {14,16,16,30,17,17,14};case '7':return {31,1,2,4,8,8,8};
        case '8':return {14,17,17,14,17,17,14};case '9':return {14,17,17,15,1,1,14};
        case ':':return {0,4,4,0,4,4,0};case '.':return {0,0,0,0,0,12,12};
        case ',':return {0,0,0,0,4,4,8};case '-':return {0,0,0,31,0,0,0};
        case '/':return {1,2,2,4,8,8,16};case '?':return {14,17,1,2,4,0,4};
        case '[':return {14,8,8,8,8,8,14};case ']':return {14,2,2,2,2,2,14};
        case '+':return {0,4,4,31,4,4,0};case '|':return {4,4,4,4,4,4,4};
        default:return {};
    }
}
inline void hudText(Framebuffer& fb,int x,int y,const std::string& text,uint32_t color,int scale=1) {
    for(std::size_t i=0;i<text.size();++i) {
        unsigned c=static_cast<unsigned char>(text[i]);
        if(c==0xc3 && i+1<text.size()) c=0xc0|(static_cast<unsigned char>(text[++i])&0x3f);
        auto rows=hudGlyph(c);
        for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if(rows[row]&(1<<(4-col))) hudFill(fb,x+col*scale,y+row*scale,scale,scale,color);
        x+=6*scale;
    }
}
inline void hudCenteredText(Framebuffer& fb,int y,const std::string& text,uint32_t color,int scale=1) {
    int letters=0;
    for(unsigned char c:text) if((c&0xc0)!=0x80) ++letters;
    int width=std::max(0,letters*6*scale-scale);
    hudText(fb,(SCREEN_WIDTH-width)/2,y,text,color,scale);
}
inline bool viewButtonContains(float x,float y) {
    return x>=VIEW_BUTTON_X && x<VIEW_BUTTON_X+VIEW_BUTTON_W &&
           y>=VIEW_BUTTON_Y && y<VIEW_BUTTON_Y+VIEW_BUTTON_H;
}
inline void drawViewButton(Framebuffer& fb,bool firstPerson) {
    hudFill(fb,VIEW_BUTTON_X,VIEW_BUTTON_Y,VIEW_BUTTON_W,VIEW_BUTTON_H,HUD_INK,0.9f);
    hudBorder(fb,VIEW_BUTTON_X,VIEW_BUTTON_Y,VIEW_BUTTON_W,VIEW_BUTTON_H,HUD_EDGE);
    hudFill(fb,VIEW_BUTTON_X,VIEW_BUTTON_Y,3,VIEW_BUTTON_H,HUD_ACCENT);
    hudText(fb,VIEW_BUTTON_X+15,VIEW_BUTTON_Y+11,firstPerson?"[F] EXTERIOR":"[F] CABINA",HUD_TEXT,2);
    hudText(fb,24,70,"MIRA UN PLANETA PARA ESCANEAR",HUD_DIM);
}

inline void drawPlanetInfo(Framebuffer& fb,const Body& body,int distanceKm) {
    constexpr int x=642,y=74,w=294,h=217;
    hudFill(fb,x,y,w,h,HUD_INK,0.87f);
    hudBorder(fb,x,y,w,h,HUD_EDGE);
    hudFill(fb,x,y,3,49,HUD_ACCENT);
    hudText(fb,x+16,y+12,"OBJETIVO / ESCANER",HUD_ACCENT);
    hudText(fb,x+16,y+29,body.name,HUD_TEXT,2);
    hudText(fb,x+16,y+54,body.description,HUD_DIM);
    fb.hline(x+16,x+w-17,y+73,HUD_EDGE);
    char distance[40];std::snprintf(distance,sizeof(distance),"%d KM",distanceKm);
    hudText(fb,x+16,y+86,distance,HUD_GOLD,3);
    hudText(fb,x+16,y+116,"DISTANCIA A LA SUPERFICIE",HUD_DIM);
    fb.hline(x+16,x+w-17,y+132,HUD_EDGE);
    const std::array<std::string,3> info={"Habitable: "+body.habitable,
        std::string("Explorado: ")+(body.explored?"Si":"No"),"Tamaño: "+body.size};
    for(int i=0;i<3;++i) {
        int row=y+145+i*22;
        hudFill(fb,x+17,row+4,3,3,HUD_ACCENT);
        hudText(fb,x+29,row,info[i],HUD_TEXT,2);
    }
}

// Una sola pantalla sobre una base trapezoidal: cabina sencilla sin tapar el cielo.
inline void drawCockpit(Framebuffer& fb,const Body* target,int distanceKm,Vec3 relativeVelocity,bool touching) {
    for(int y=385;y<SCREEN_HEIGHT;++y) {
        int inset=std::max(0,53-(y-385)/2);
        hudFill(fb,185+inset,y,590-2*inset,1,0xff111b23u);
    }
    hudLine(fb,238,385,722,385,HUD_EDGE);
    hudLine(fb,238,385,168,526,HUD_EDGE);hudLine(fb,722,385,792,526,HUD_EDGE);
    hudFill(fb,0,526,SCREEN_WIDTH,14,0xff111b23u);
    hudFill(fb,253,399,454,115,0xff061615u);
    hudBorder(fb,251,397,458,119,HUD_EDGE);
    hudText(fb,268,408,"CABINA / ESCANER",HUD_ACCENT);
    if(target) {
        hudText(fb,268,426,target->name,HUD_TEXT,2);
        hudText(fb,268,447,target->description,HUD_DIM);
        hudText(fb,268,465,std::to_string(distanceKm)+" KM",HUD_GOLD,3);
        fb.vline(491,408,491,HUD_EDGE);
        hudText(fb,506,408,"DATOS DEL OBJETIVO",HUD_ACCENT);
        const std::array<std::string,3> info={"Habitable: "+target->habitable,
            std::string("Explorado: ")+(target->explored?"Si":"No"),"Tamaño: "+target->size};
        for(int i=0;i<3;++i) {
            int row=430+i*24;
            hudFill(fb,504,row+5,3,3,HUD_ACCENT);
            hudText(fb,515,row,info[i],HUD_TEXT,2);
        }
    } else {
        hudText(fb,268,434,"SIN OBJETIVO",HUD_TEXT,2);
        hudText(fb,268,469,"MIRA UN PLANETA PARA VER SU INFORMACION",HUD_DIM);
    }
    char telemetry[80];
    std::snprintf(telemetry,sizeof(telemetry),"VEL REL %5.1f KM/S  |  %s",length(relativeVelocity)*10.0f,
        touching?"EN SUPERFICIE":"EN VUELO");
    hudText(fb,268,502,telemetry,HUD_DIM);
}
inline void drawControlHint(Framebuffer& fb,bool firstPerson) {
    if(!firstPerson) hudFill(fb,24,510,912,22,HUD_INK,0.8f);
    hudText(fb,firstPerson?30:37,firstPerson?529:517,
        "WASD MOVER  ESP/CTRL ALTURA  SHIFT TURBO  X IGUALAR VEL  F VISTA  R REINICIAR  ESC RATON",HUD_DIM);
}
inline void drawDeathScreen(Framebuffer& fb,float elapsed) {
    float white=std::clamp(0.15f+elapsed/0.8f,0.0f,1.0f);
    hudFill(fb,0,0,SCREEN_WIDTH,SCREEN_HEIGHT,0xffffffffu,white);
    constexpr uint32_t ink=0xff283742u;
    hudCenteredText(fb,222,"NAVE DESTRUIDA",ink,3);
    hudCenteredText(fb,264,"CONTACTO CON EL SOL",ink,2);
    int remaining=std::max(1,int(std::ceil(3.0f-elapsed)));
    hudCenteredText(fb,302,"REAPARICION EN "+std::to_string(remaining),ink);
}

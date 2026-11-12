#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include "colors.h"

inline constexpr float PI=3.14159265358979323846f;

struct Vec3 { float x=0, y=0, z=0; };
inline Vec3 operator+(Vec3 a, Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a, Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator-(Vec3 a) {return {-a.x,-a.y,-a.z};}
inline Vec3 operator*(Vec3 a, float s) {return {a.x*s,a.y*s,a.z*s};}
inline Vec3& operator+=(Vec3& a, Vec3 b) {a=a+b;return a;}
inline Vec3& operator-=(Vec3& a, Vec3 b) {a=a-b;return a;}
inline float dot(Vec3 a, Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline float length(Vec3 a) {return std::sqrt(dot(a,a));}
inline Vec3 normalized(Vec3 a) {
    float len=std::sqrt(dot(a,a));return len>1e-12f?a*(1.0f/len):Vec3{};
}
// Gira v alrededor del eje unitario 'axis' (formula de Rodrigues).
inline Vec3 rotateAround(Vec3 v, Vec3 axis, float angle) {
    float c=std::cos(angle),s=std::sin(angle);
    return v*c+cross(axis,v)*s+axis*(dot(axis,v)*(1-c));
}

struct Vec4 { float x=0, y=0, z=0, w=0; };

// Matriz 4x4 guardada por filas: m[fila][columna]. Se usa como M*v.
struct Mat4 {
    float m[4][4]{};
    static Mat4 identity() {Mat4 r;for(int i=0;i<4;++i) r.m[i][i]=1;return r;}
};
inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for(int i=0;i<4;++i) for(int j=0;j<4;++j)
        for(int k=0;k<4;++k) r.m[i][j]+=a.m[i][k]*b.m[k][j];
    return r;
}
inline Vec4 operator*(const Mat4& a, Vec4 v) {
    auto row=[&](int i) {return a.m[i][0]*v.x+a.m[i][1]*v.y+a.m[i][2]*v.z+a.m[i][3]*v.w;};
    return {row(0),row(1),row(2),row(3)};
}
inline Vec3 transformPoint(const Mat4& a, Vec3 p) {Vec4 r=a*Vec4{p.x,p.y,p.z,1};return {r.x,r.y,r.z};}
inline Vec3 transformDir(const Mat4& a, Vec3 d) {Vec4 r=a*Vec4{d.x,d.y,d.z,0};return {r.x,r.y,r.z};}

inline Mat4 translation(Vec3 t) {
    Mat4 r=Mat4::identity();r.m[0][3]=t.x;r.m[1][3]=t.y;r.m[2][3]=t.z;return r;
}
inline Mat4 scaling(float s) {
    Mat4 r=Mat4::identity();r.m[0][0]=r.m[1][1]=r.m[2][2]=s;return r;
}
// Giro sobre el eje Y (regla de la mano derecha).
inline Mat4 rotationY(float angle) {
    Mat4 r=Mat4::identity();float c=std::cos(angle),s=std::sin(angle);
    r.m[0][0]=c;r.m[0][2]=s;r.m[2][0]=-s;r.m[2][2]=c;return r;
}
// Las columnas son los ejes locales del objeto escritos en el mundo.
inline Mat4 fromBasis(Vec3 right, Vec3 up, Vec3 back) {
    Mat4 r=Mat4::identity();
    r.m[0][0]=right.x;r.m[1][0]=right.y;r.m[2][0]=right.z;
    r.m[0][1]=up.x;   r.m[1][1]=up.y;   r.m[2][1]=up.z;
    r.m[0][2]=back.x; r.m[1][2]=back.y; r.m[2][2]=back.z;
    return r;
}
// MATRIZ DE VISTA: lleva el mundo al espacio de la camara, que mira hacia -Z.
inline Mat4 lookFrom(Vec3 eye, Vec3 right, Vec3 up, Vec3 forward) {
    Mat4 r=Mat4::identity();
    Vec3 rows[3]={right,up,-forward};
    for(int i=0;i<3;++i) {
        r.m[i][0]=rows[i].x;r.m[i][1]=rows[i].y;r.m[i][2]=rows[i].z;
        r.m[i][3]=-dot(rows[i],eye);
    }
    return r;
}
// MATRIZ DE PROYECCION en perspectiva (misma convencion que OpenGL).
inline Mat4 perspective(float fovY, float aspect, float near, float far) {
    Mat4 r;float f=1.0f/std::tan(fovY*0.5f);
    r.m[0][0]=f/aspect;r.m[1][1]=f;
    r.m[2][2]=(far+near)/(near-far);r.m[2][3]=2*far*near/(near-far);
    r.m[3][2]=-1;
    return r;
}

struct Triangle {
    int indices[3];
    std::string object;
    int faceId=0;
    Part part=Part::Body;
};
struct Mesh {std::vector<Vec3> vertices; std::vector<Triangle> triangles;};

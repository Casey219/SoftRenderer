#pragma once
#include "tgaimage.h"
#include "geometry.h"

void lookat(const vec3 eye, const vec3 center, const vec3 up);
void init_perspective(const double f);
void init_perspective(double fovy, double aspect, double near, double far);
void init_viewport(const int x, const int y, const int w, const int h);
void init_zbuffer(const int width, const int height);

struct Shader {
    static TGAColor sample2D(const TGAImage& img, const vec2& uvf) {
        return img.get(uvf[0] * img.width(), uvf[1] * img.height());
    }
    virtual vec4 vertex(const int face, const int vert) = 0;
    virtual std::pair<bool, TGAColor> fragment(const vec3 bc) const = 0;
};

typedef vec4 Triangle[3]; 
void rasterize(const Triangle& clip, const Shader& shader, TGAImage& framebuffer);



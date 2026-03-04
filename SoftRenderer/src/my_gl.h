#pragma once
#include "tgaimage.h"
#include "geometry.h"

void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color);

double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy);

void triangle2D(int ax, int ay, int bx, int by, int cx, int cy, TGAImage& framebuffer, TGAColor color);

void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage& framebuffer, TGAImage& zbuffer, TGAColor color);

void triangle(Vec3f v0,Vec3f v1,Vec3f v2,TGAImage& framebuffer, TGAImage& zbuffer, TGAColor color);

Matrix lookat(Vec3f eye, Vec3f center, Vec3f up);

Matrix perspective(float fov_deg, float aspect, float znear, float zfar);

Matrix viewport(int x, int y, int w, int h);
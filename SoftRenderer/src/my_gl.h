#pragma once
#include <array>
#include "tgaimage.h"
#include "geometry.h"

void lookat(const vec3 eye, const vec3 center, const vec3 up);
void init_perspective(const double f);
void init_perspective(double fovy, double aspect, double near, double far);
void init_orthographic(double left, double right, double bottom, double top, double near, double far);
void init_viewport(const int x, const int y, const int w, const int h);
void init_zbuffer(const int width, const int height);

struct VertexOut {
    vec4 clip_position = {};
    vec4 world_position = {};
    vec4 view_position = {};
    vec4 normal = {};
    vec2 uv = {};
};

using Triangle = std::array<VertexOut, 3>;

struct AABB {
    vec3 minimum = {};
    vec3 maximum = {};
    bool valid = false;

    void expand(const vec3& point);
    vec3 center() const;
    std::array<vec3, 8> corners() const;
};

struct DirectionalLightFrustum {
    mat<4, 4> view = {};
    mat<4, 4> projection = {};
};

DirectionalLightFrustum fit_directional_light(const vec3& light_direction,
                                               const AABB& world_bounds,
                                               double padding_ratio = 0.1);

struct ShadowMap {
    int width = 0;
    int height = 0;
    std::vector<double> depth = {};
    mat<4, 4> light_clip_from_world = {};

    double visibility(const vec4& world_position, double bias, int pcf_radius = 1) const;
};

struct Shader {
    virtual ~Shader() = default;
    virtual VertexOut vertex(const int face, const int vert) = 0;
    virtual std::pair<bool, TGAColor> fragment(const vec3 bc, const Triangle& triangle) const = 0;
};

std::vector<Triangle> clip_triangle(const Triangle& triangle);
void rasterize(const Triangle& clip, const Shader& shader, TGAImage& framebuffer);



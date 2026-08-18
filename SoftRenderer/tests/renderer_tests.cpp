#include <cmath>
#include <iostream>
#include <string>
#include "my_gl.h"

namespace {
constexpr double epsilon = 1e-8;

VertexOut make_vertex(const vec4 position, const vec2 uv = {}) {
    VertexOut vertex;
    vertex.clip_position = position;
    vertex.view_position = position;
    vertex.normal = { 0., 0., 1., 0. };
    vertex.uv = uv;
    return vertex;
}

bool almost_equal(const double lhs, const double rhs) {
    return std::abs(lhs - rhs) <= epsilon;
}

bool inside_clip_volume(const vec4& p) {
    return p.x + p.w >= -epsilon && p.w - p.x >= -epsilon &&
           p.y + p.w >= -epsilon && p.w - p.y >= -epsilon &&
           p.z + p.w >= -epsilon && p.w - p.z >= -epsilon;
}

bool expect(const bool condition, const std::string& message) {
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}

struct SolidShader final : Shader {
    mutable int fragment_count = 0;

    VertexOut vertex(const int, const int) override { return {}; }

    std::pair<bool, TGAColor> fragment(const vec3, const Triangle&) const override {
        ++fragment_count;
        return { false, { 0, 0, 255, 255 } };
    }
};
} // namespace

int main() {
    bool passed = true;

    const Triangle inside = {
        make_vertex({ -0.5, -0.5, 0., 1. }, { 0., 0. }),
        make_vertex({  0.5, -0.5, 0., 1. }, { 1., 0. }),
        make_vertex({  0.0,  0.5, 0., 1. }, { 0.5, 1. })
    };
    const auto unchanged = clip_triangle(inside);
    passed &= expect(unchanged.size() == 1, "an inside triangle must remain one triangle");
    if (unchanged.size() == 1) {
        for (int i = 0; i < 3; ++i) {
            passed &= expect(almost_equal(unchanged[0][i].clip_position.x, inside[i].clip_position.x) &&
                             almost_equal(unchanged[0][i].clip_position.y, inside[i].clip_position.y) &&
                             almost_equal(unchanged[0][i].uv.x, inside[i].uv.x) &&
                             almost_equal(unchanged[0][i].uv.y, inside[i].uv.y),
                             "an inside vertex and its attributes must remain unchanged");
        }
    }

    const std::array<Triangle, 6> outside = {
        Triangle{ make_vertex({ -2., -0.5, 0., 1. }), make_vertex({ -2., 0.5, 0., 1. }), make_vertex({ -3., 0., 0., 1. }) },
        Triangle{ make_vertex({  2., -0.5, 0., 1. }), make_vertex({  3., 0., 0., 1. }), make_vertex({  2., 0.5, 0., 1. }) },
        Triangle{ make_vertex({ -0.5, -2., 0., 1. }), make_vertex({ 0.5, -2., 0., 1. }), make_vertex({ 0., -3., 0., 1. }) },
        Triangle{ make_vertex({ -0.5,  2., 0., 1. }), make_vertex({ 0., 3., 0., 1. }), make_vertex({ 0.5, 2., 0., 1. }) },
        Triangle{ make_vertex({ -0.5, 0., -2., 1. }), make_vertex({ 0.5, 0., -2., 1. }), make_vertex({ 0., 0.5, -3., 1. }) },
        Triangle{ make_vertex({ -0.5, 0.,  2., 1. }), make_vertex({ 0., 0.5, 3., 1. }), make_vertex({ 0.5, 0., 2., 1. }) }
    };
    for (std::size_t plane = 0; plane < outside.size(); ++plane)
        passed &= expect(clip_triangle(outside[plane]).empty(),
                         "a triangle outside clip plane " + std::to_string(plane) + " must be discarded");

    const Triangle crossing_near = {
        make_vertex({ -0.5, -0.5,  0., 1. }, { 0., 0. }),
        make_vertex({  0.5, -0.5,  0., 1. }, { 1., 0. }),
        make_vertex({  0.0,  0.5, -2., 1. }, { 0.5, 1. })
    };
    const auto clipped = clip_triangle(crossing_near);
    passed &= expect(clipped.size() == 2, "a near-plane crossing triangle must become two triangles");

    int near_plane_vertices = 0;
    for (const Triangle& triangle : clipped) {
        for (const VertexOut& vertex : triangle) {
            passed &= expect(inside_clip_volume(vertex.clip_position),
                             "every generated vertex must lie inside the clip volume");
            if (almost_equal(vertex.clip_position.z, -vertex.clip_position.w)) {
                ++near_plane_vertices;
                passed &= expect(almost_equal(vertex.uv.y, 0.5),
                                 "a generated near-plane vertex must interpolate UV attributes");
            }
        }
    }
    passed &= expect(near_plane_vertices >= 2, "near-plane clipping must generate two boundary vertices");

    init_viewport(0, 0, 64, 64);
    init_zbuffer(64, 64);
    TGAImage framebuffer(64, 64, TGAImage::RGB);
    SolidShader shader;
    rasterize(crossing_near, shader, framebuffer);
    int shaded_pixels = 0;
    for (int y = 0; y < framebuffer.height(); ++y)
        for (int x = 0; x < framebuffer.width(); ++x)
            if (framebuffer.get(x, y)[2] != 0) ++shaded_pixels;
    passed &= expect(shaded_pixels > 0,
                     "a near-plane crossing triangle must reach the fragment shader after clipping");

    const VertexOut bottom_left = make_vertex({ -1., -1., 0., 1. });
    const VertexOut bottom_right = make_vertex({ 1., -1., 0., 1. });
    const VertexOut top_right = make_vertex({ 1., 1., 0., 1. });
    const VertexOut top_left = make_vertex({ -1., 1., 0., 1. });
    const Triangle lower_right = { bottom_left, bottom_right, top_right };
    const Triangle upper_left = { bottom_left, top_right, top_left };

    init_viewport(0, 0, 4, 4);
    TGAImage coverage_buffer(4, 4, TGAImage::RGB);
    shader.fragment_count = 0;
    init_zbuffer(4, 4);
    rasterize(lower_right, shader, coverage_buffer);
    const int lower_right_fragments = shader.fragment_count;
    shader.fragment_count = 0;
    init_zbuffer(4, 4);
    rasterize(upper_left, shader, coverage_buffer);
    const int upper_left_fragments = shader.fragment_count;
    passed &= expect(lower_right_fragments + upper_left_fragments == 16,
                     "two triangles sharing an edge must cover a 4x4 target exactly once");

    shader.fragment_count = 0;
    init_zbuffer(4, 4);
    const Triangle backface = { bottom_left, top_right, bottom_right };
    rasterize(backface, shader, coverage_buffer);
    passed &= expect(shader.fragment_count == 0, "a clockwise triangle must be culled");

    shader.fragment_count = 0;
    init_zbuffer(4, 4);
    const Triangle degenerate = {
        bottom_left,
        make_vertex({ 0., 0., 0., 1. }),
        top_right
    };
    rasterize(degenerate, shader, coverage_buffer);
    passed &= expect(shader.fragment_count == 0, "a degenerate triangle must not shade fragments");

    TGAImage texture(2, 2, TGAImage::RGB);
    texture.set(0, 0, { 0, 0, 0, 255 });
    texture.set(1, 0, { 100, 100, 100, 255 });
    texture.set(0, 1, { 200, 200, 200, 255 });
    texture.set(1, 1, { 255, 255, 255, 255 });

    const Sampler clamp_nearest = { AddressMode::Clamp, FilterMode::Nearest };
    passed &= expect(clamp_nearest.sample(texture, 1., 1.)[0] == 255,
                     "clamp sampling at UV 1 must select the last texel without overflow");
    passed &= expect(clamp_nearest.sample(texture, -0.25, 1.25)[0] == 200,
                     "clamp sampling must select the nearest border texel");

    const Sampler repeat_nearest = { AddressMode::Repeat, FilterMode::Nearest };
    passed &= expect(repeat_nearest.sample(texture, 0.25, 0.25)[0] ==
                     repeat_nearest.sample(texture, 1.25, 1.25)[0],
                     "repeat sampling must be periodic for UV coordinates above one");
    passed &= expect(repeat_nearest.sample(texture, -0.25, 0.25)[0] == 100,
                     "repeat sampling must wrap negative UV coordinates");

    const Sampler clamp_bilinear = { AddressMode::Clamp, FilterMode::Bilinear };
    passed &= expect(clamp_bilinear.sample(texture, 0.5, 0.5)[0] == 139,
                     "bilinear sampling at the texture center must average four texels");
    passed &= expect(clamp_bilinear.sample(texture, 0., 0.)[0] == 0 &&
                     clamp_bilinear.sample(texture, 1., 1.)[0] == 255,
                     "bilinear clamp sampling must preserve corner texels");

    const Sampler repeat_bilinear = { AddressMode::Repeat, FilterMode::Bilinear };
    passed &= expect(repeat_bilinear.sample(texture, 0.25, 0.25)[0] ==
                     repeat_bilinear.sample(texture, 1.25, 1.25)[0],
                     "bilinear repeat sampling must remain periodic");

    if (!passed) return 1;
    std::cout << "All renderer tests passed.\n";
    return 0;
}

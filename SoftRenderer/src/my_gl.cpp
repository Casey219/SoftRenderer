#include <algorithm>
#include "my_gl.h"

mat<4, 4> ModelView, Viewport, Perspective; 
std::vector<double> zbuffer;               // 深度缓冲

namespace {
mat<4, 4> make_lookat(const vec3 eye, const vec3 center, const vec3 up) {
    const vec3 g = normalized(center - eye);
    const vec3 r = normalized(cross(g, up));
    const vec3 t = normalized(cross(r, g));
    return mat<4, 4>{ {{r.x,r.y,r.z,0}, {t.x,t.y,t.z,0}, {-g.x,-g.y,-g.z,0}, {0,0,0,1}} } *
        mat<4, 4>{{{1, 0, 0, -eye.x}, { 0,1,0,-eye.y }, { 0,0,1,-eye.z }, { 0,0,0,1 }}};
}

mat<4, 4> make_orthographic(const double left, const double right,
                            const double bottom, const double top,
                            const double near, const double far) {
    mat<4, 4> result = { 0 };
    result[0][0] = 2. / (right - left);
    result[1][1] = 2. / (top - bottom);
    result[2][2] = -2. / (far - near);
    result[0][3] = -(right + left) / (right - left);
    result[1][3] = -(top + bottom) / (top - bottom);
    result[2][3] = -(far + near) / (far - near);
    result[3][3] = 1.;
    return result;
}
} // namespace

void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    ModelView = make_lookat(eye, center, up);
}

//void init_perspective(const double f) {
//    Perspective = { {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0, -1 / f,1}} };
//}

void init_perspective(double fovy, double aspect, double near, double far) {
    double tanHalfFovy = std::tan(fovy / 2.0);
    Perspective = mat<4, 4>{ 0 }; // 先全部初始化为0

    Perspective[0][0] = 1.0 / (aspect * tanHalfFovy);
    Perspective[1][1] = 1.0 / (tanHalfFovy);
    Perspective[2][2] = -(far + near) / (far - near);
    Perspective[2][3] = -(2.0 * far * near) / (far - near);
    Perspective[3][2] = -1.0;
}

void init_orthographic(const double left, const double right,
                       const double bottom, const double top,
                       const double near, const double far) {
    Perspective = make_orthographic(left, right, bottom, top, near, far);
}

void init_viewport(const int x, const int y, const int w, const int h) {
    Viewport = { {{w / 2., 0, 0, x + w / 2.}, {0, h / 2., 0, y + h / 2.}, {0,0,1,0}, {0,0,0,1}} };
}

void init_zbuffer(const int width, const int height) {
    //zbuffer = std::vector(width * height, -1.);
    zbuffer = std::vector(width * height, 1.);
    //zbuffer = std::vector(width * height, -1000.);
}

void AABB::expand(const vec3& point) {
    if (!valid) {
        minimum = maximum = point;
        valid = true;
        return;
    }
    for (int axis = 0; axis < 3; ++axis) {
        minimum[axis] = std::min(minimum[axis], point[axis]);
        maximum[axis] = std::max(maximum[axis], point[axis]);
    }
}

vec3 AABB::center() const {
    return (minimum + maximum) * .5;
}

std::array<vec3, 8> AABB::corners() const {
    std::array<vec3, 8> result = {};
    for (int corner = 0; corner < 8; ++corner) {
        result[corner] = {
            corner & 1 ? maximum.x : minimum.x,
            corner & 2 ? maximum.y : minimum.y,
            corner & 4 ? maximum.z : minimum.z
        };
    }
    return result;
}

DirectionalLightFrustum fit_directional_light(const vec3& light_direction,
                                               const AABB& world_bounds,
                                               const double padding_ratio) {
    DirectionalLightFrustum result;
    if (!world_bounds.valid) return result;

    constexpr double minimum_extent = 1e-3;
    const double direction_length = norm(light_direction);
    const vec3 direction = direction_length > minimum_extent
        ? light_direction / direction_length
        : vec3{ 0., 1., 0. };
    const vec3 bounds_center = world_bounds.center();
    const double radius = norm(world_bounds.maximum - world_bounds.minimum) * .5;
    const vec3 eye = bounds_center + direction * (radius + 1.);
    const vec3 preferred_up = std::abs(direction * vec3{ 0., 1., 0. }) > .99
        ? vec3{ 0., 0., 1. }
        : vec3{ 0., 1., 0. };
    result.view = make_lookat(eye, bounds_center, preferred_up);

    const auto corners = world_bounds.corners();
    vec3 light_min = (result.view * vec4{ corners[0].x, corners[0].y, corners[0].z, 1. }).xyz();
    vec3 light_max = light_min;
    for (std::size_t i = 1; i < corners.size(); ++i) {
        const vec3 point = (result.view * vec4{ corners[i].x, corners[i].y, corners[i].z, 1. }).xyz();
        for (int axis = 0; axis < 3; ++axis) {
            light_min[axis] = std::min(light_min[axis], point[axis]);
            light_max[axis] = std::max(light_max[axis], point[axis]);
        }
    }

    const double padding = std::max(padding_ratio, 0.);
    const double pad_x = std::max((light_max.x - light_min.x) * padding, minimum_extent);
    const double pad_y = std::max((light_max.y - light_min.y) * padding, minimum_extent);
    const double pad_z = std::max((light_max.z - light_min.z) * padding, minimum_extent);
    result.left = light_min.x - pad_x;
    result.right = light_max.x + pad_x;
    result.bottom = light_min.y - pad_y;
    result.top = light_max.y + pad_y;
    result.near_plane = std::max(-light_max.z - pad_z, minimum_extent);
    result.far_plane = std::max(-light_min.z + pad_z, result.near_plane + minimum_extent);
    result.projection = make_orthographic(result.left, result.right,
                                          result.bottom, result.top,
                                          result.near_plane, result.far_plane);
    return result;
}

namespace {
constexpr double clip_epsilon = 1e-9;
constexpr double raster_epsilon = 1e-12;

VertexOut interpolate(const VertexOut& from, const VertexOut& to, const double t) {
    VertexOut result;
    result.clip_position = from.clip_position + (to.clip_position - from.clip_position) * t;
    result.world_position = from.world_position + (to.world_position - from.world_position) * t;
    result.view_position = from.view_position + (to.view_position - from.view_position) * t;
    result.world_normal = from.world_normal + (to.world_normal - from.world_normal) * t;
    result.normal = from.normal + (to.normal - from.normal) * t;
    result.uv = from.uv + (to.uv - from.uv) * t;
    return result;
}

double plane_distance(const vec4& p, const int plane) {
    switch (plane) {
    case 0: return p.x + p.w; // left
    case 1: return p.w - p.x; // right
    case 2: return p.y + p.w; // bottom
    case 3: return p.w - p.y; // top
    case 4: return p.z + p.w; // near
    case 5: return p.w - p.z; // far
    default: return -1.;
    }
}

std::vector<VertexOut> clip_against_plane(const std::vector<VertexOut>& input, const int plane) {
    std::vector<VertexOut> output;
    if (input.empty()) return output;

    output.reserve(input.size() + 1);
    VertexOut previous = input.back();
    double previous_distance = plane_distance(previous.clip_position, plane);
    bool previous_inside = previous_distance >= -clip_epsilon;

    for (const VertexOut& current : input) {
        const double current_distance = plane_distance(current.clip_position, plane);
        const bool current_inside = current_distance >= -clip_epsilon;

        if (current_inside != previous_inside) {
            const double denominator = previous_distance - current_distance;
            if (std::abs(denominator) > clip_epsilon) {
                const double t = previous_distance / denominator;
                output.push_back(interpolate(previous, current, t));
            }
        }
        if (current_inside) output.push_back(current);

        previous = current;
        previous_distance = current_distance;
        previous_inside = current_inside;
    }
    return output;
}

double edge_function(const vec2& from, const vec2& to, const vec2& point) {
    return (to.x - from.x) * (point.y - from.y) -
           (to.y - from.y) * (point.x - from.x);
}

// The framebuffer uses a y-up coordinate system. A CCW triangle owns its
// top and left edges; the opposite orientation of a shared edge is excluded.
bool is_top_left_edge(const vec2& from, const vec2& to) {
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    return dy < 0. || (dy == 0. && dx < 0.);
}

bool is_inside_edge(const double edge_value, const bool inclusive) {
    return edge_value > 0. || (edge_value == 0. && inclusive);
}

void rasterize_clipped_triangle(const Triangle& clip, const Shader& shader, TGAImage& framebuffer) {
    vec4 ndc[3] = { clip[0].clip_position / clip[0].clip_position.w,
                    clip[1].clip_position / clip[1].clip_position.w,
                    clip[2].clip_position / clip[2].clip_position.w }; // 转化为NDC坐标
    vec2 screen[3] = { (Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy() }; // 转化为屏幕坐标

    const double area = edge_function(screen[0], screen[1], screen[2]);
    if (std::abs(area) <= raster_epsilon) return; // 退化三角形
    if (area < 0.) return;                        // 背面剔除

    const bool edge0_inclusive = is_top_left_edge(screen[1], screen[2]);
    const bool edge1_inclusive = is_top_left_edge(screen[2], screen[0]);
    const bool edge2_inclusive = is_top_left_edge(screen[0], screen[1]);

    // 求像素中心可能落入三角形的整数包围盒
    auto [bbminx, bbmaxx] = std::minmax({ screen[0].x, screen[1].x, screen[2].x }); 
    auto [bbminy, bbmaxy] = std::minmax({ screen[0].y, screen[1].y, screen[2].y }); 
    const int min_x = std::max(static_cast<int>(std::ceil(bbminx - .5)), 0);
    const int max_x = std::min(static_cast<int>(std::floor(bbmaxx - .5)), framebuffer.width() - 1);
    const int min_y = std::max(static_cast<int>(std::ceil(bbminy - .5)), 0);
    const int max_y = std::min(static_cast<int>(std::floor(bbmaxy - .5)), framebuffer.height() - 1);

#pragma omp parallel for
    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            const vec2 sample = { x + .5, y + .5 };
            const double edge0 = edge_function(screen[1], screen[2], sample);
            const double edge1 = edge_function(screen[2], screen[0], sample);
            const double edge2 = edge_function(screen[0], screen[1], sample);
            if (!is_inside_edge(edge0, edge0_inclusive) ||
                !is_inside_edge(edge1, edge1_inclusive) ||
                !is_inside_edge(edge2, edge2_inclusive)) continue;

            const vec3 bc_screen = { edge0 / area, edge1 / area, edge2 / area };
            vec3 bc_clip = { bc_screen.x / clip[0].clip_position.w,
                             bc_screen.y / clip[1].clip_position.w,
                             bc_screen.z / clip[2].clip_position.w };
            bc_clip = bc_clip / (bc_clip.x + bc_clip.y + bc_clip.z);
            double z = bc_screen * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };  // 深度的线性插值
            if (z >= zbuffer[x + y * framebuffer.width()]) continue;   // z-buffer深度测试
            //if (z <= zbuffer[x + y * framebuffer.width()]) continue;   // z-buffer深度测试
            auto [discard, color] = shader.fragment(bc_clip, clip);
            if (discard) continue;                                 // fragment shader可以丢弃当前fragment
            zbuffer[x + y * framebuffer.width()] = z;                 
            framebuffer.set(x, y, color);                          
        }
    }
}
} // namespace

double ShadowMap::depth_bias(const double geometric_ndotl,
                             const ShadowBiasSettings& settings) const {
    if (world_units_per_texel <= 0. || depth_range <= clip_epsilon) return 0.;

    const double cosine = std::clamp(geometric_ndotl, 0., 1.);
    const double sine = std::sqrt(std::max(0., 1. - cosine * cosine));
    const double slope = std::min(sine / std::max(cosine, 0.1),
                                  std::max(settings.maximum_slope, 0.));
    const double world_bias = world_units_per_texel *
        (std::max(settings.constant_texels, 0.) +
         std::max(settings.slope_scale_texels, 0.) * slope);
    return 2. * world_bias / depth_range;
}

double ShadowMap::visibility(const vec4& world_position, const double bias, const int pcf_radius) const {
    if (width <= 0 || height <= 0 || depth.size() != static_cast<std::size_t>(width * height))
        return 1.;

    const vec4 clip = light_clip_from_world * world_position;
    if (std::abs(clip.w) <= clip_epsilon) return 1.;
    const vec3 ndc = clip.xyz() / clip.w;
    if (ndc.x < -1. || ndc.x > 1. || ndc.y < -1. || ndc.y > 1. ||
        ndc.z < -1. || ndc.z > 1.) return 1.;

    const double screen_x = (ndc.x * .5 + .5) * width;
    const double screen_y = (ndc.y * .5 + .5) * height;
    const int center_x = static_cast<int>(std::floor(screen_x));
    const int center_y = static_cast<int>(std::floor(screen_y));
    const int radius = std::max(pcf_radius, 0);

    double lit_samples = 0.;
    int sample_count = 0;
    for (int offset_y = -radius; offset_y <= radius; ++offset_y) {
        for (int offset_x = -radius; offset_x <= radius; ++offset_x) {
            const int x = center_x + offset_x;
            const int y = center_y + offset_y;
            if (x < 0 || x >= width || y < 0 || y >= height) continue;
            lit_samples += ndc.z - bias <= depth[x + y * width] ? 1. : 0.;
            ++sample_count;
        }
    }
    return sample_count > 0 ? lit_samples / sample_count : 1.;
}

std::vector<Triangle> clip_triangle(const Triangle& triangle) {
    std::vector<VertexOut> polygon(triangle.begin(), triangle.end());
    for (int plane = 0; plane < 6 && !polygon.empty(); ++plane)
        polygon = clip_against_plane(polygon, plane);

    std::vector<Triangle> triangles;
    if (polygon.size() < 3) return triangles;

    triangles.reserve(polygon.size() - 2);
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
        triangles.push_back({ polygon[0], polygon[i], polygon[i + 1] });
    return triangles;
}

void rasterize(const Triangle& triangle, const Shader& shader, TGAImage& framebuffer) {
    for (const Triangle& clipped : clip_triangle(triangle))
        rasterize_clipped_triangle(clipped, shader, framebuffer);
}

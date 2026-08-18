#include <algorithm>
#include "my_gl.h"

mat<4, 4> ModelView, Viewport, Perspective; 
std::vector<double> zbuffer;               // 深度缓冲

void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 g = normalized(center - eye);
    vec3 r = normalized(cross(g, up));
    vec3 t = normalized(cross(r, g));
    ModelView = mat<4, 4>{ {{r.x,r.y,r.z,0}, {t.x,t.y,t.z,0}, {-g.x,-g.y,-g.z,0}, {0,0,0,1}} } *
        mat<4, 4>{{{1, 0, 0, -eye.x}, { 0,1,0,-eye.y }, { 0,0,1,-eye.z }, { 0,0,0,1 }}};
        //mat<4, 4>{{{1, 0, 0, -center.x}, { 0,1,0,-center.y }, { 0,0,1,-center.z }, { 0,0,0,1 }}};
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

void init_viewport(const int x, const int y, const int w, const int h) {
    Viewport = { {{w / 2., 0, 0, x + w / 2.}, {0, h / 2., 0, y + h / 2.}, {0,0,1,0}, {0,0,0,1}} };
}

void init_zbuffer(const int width, const int height) {
    //zbuffer = std::vector(width * height, -1.);
    zbuffer = std::vector(width * height, 1.);
    //zbuffer = std::vector(width * height, -1000.);
}

namespace {
constexpr double clip_epsilon = 1e-9;
constexpr double raster_epsilon = 1e-12;

VertexOut interpolate(const VertexOut& from, const VertexOut& to, const double t) {
    VertexOut result;
    result.clip_position = from.clip_position + (to.clip_position - from.clip_position) * t;
    result.view_position = from.view_position + (to.view_position - from.view_position) * t;
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

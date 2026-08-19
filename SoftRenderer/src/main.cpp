#include "my_gl.h"
#include "model.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <string_view>

extern mat<4, 4> Viewport, ModelView, Perspective; 
extern std::vector<double> zbuffer;     

struct BlankShader : Shader {
    const Model& model;

    BlankShader(const Model& m) : model(m) {}

    virtual VertexOut vertex(const int face, const int vert) override {
        VertexOut output;
        output.world_position = model.vert(face, vert);
        output.view_position = ModelView * output.world_position;
        output.clip_position = Perspective * output.view_position;
        return output;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3, const Triangle&) const override {
        return { false, {255, 255, 255, 255} };
    }
};

struct BlinnPhongShader : Shader {
    const Model& model;
    const ShadowMap& shadow_map;
    vec4 l;
    vec4 light_world_direction;
    Sampler sampler = { AddressMode::Repeat, FilterMode::Bilinear };
    ShadowBiasSettings shadow_bias_settings = {};
    int shadow_pcf_radius = 1;

    BlinnPhongShader(const vec3 light, const Model& m, const ShadowMap& shadow,
                     const ShadowBiasSettings& bias_settings, const int pcf_radius)
        : model(m), shadow_map(shadow), shadow_bias_settings(bias_settings),
          shadow_pcf_radius(pcf_radius) {
        light_world_direction = normalized(vec4{ light.x, light.y, light.z, 0. });
        l = normalized(ModelView * light_world_direction);
    }

    virtual VertexOut vertex(const int face, const int vert) override {
        VertexOut output;
        output.uv = model.uv(face, vert);
        output.world_normal = model.normal(face, vert);
        output.normal = ModelView.invert_transpose() * output.world_normal;
        output.world_position = model.vert(face, vert);
        output.view_position = ModelView * output.world_position;
        output.clip_position = Perspective * output.view_position;
        return output;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bc, const Triangle& triangle) const override {
        mat<2, 4> E = { triangle[1].view_position - triangle[0].view_position,
                        triangle[2].view_position - triangle[0].view_position };
        mat<2, 2> U = { triangle[1].uv - triangle[0].uv,
                        triangle[2].uv - triangle[0].uv };
        mat<2, 4> T = U.invert() * E;
        const vec4 geometric_normal = normalized(
            triangle[0].normal * bc[0] +
            triangle[1].normal * bc[1] +
            triangle[2].normal * bc[2]);
        const vec4 world_geometric_normal = normalized(
            triangle[0].world_normal * bc[0] +
            triangle[1].world_normal * bc[1] +
            triangle[2].world_normal * bc[2]);
        mat<4, 4> D = { normalized(T[0]),  // tangent
                      normalized(T[1]),  // bitangent
                      geometric_normal,
                      {0,0,0,1} }; 
        vec2 uv = triangle[0].uv * bc[0] + triangle[1].uv * bc[1] + triangle[2].uv * bc[2];
        vec4 shading_normal = normalized(D.transpose() * model.normal(uv, sampler));
        vec4 world_position = triangle[0].world_position * bc[0] +
                              triangle[1].world_position * bc[1] +
                              triangle[2].world_position * bc[2];

        // Blinn-Phong
        vec4 viewDir = vec4{0.0, 0.0, 1.0, 0.0}; 
		vec4 h = normalized(l + viewDir);    // 半程向量

        const double geometric_ndotl = std::max(0., geometric_normal * l);
        const double shadow_bias = shadow_map.depth_bias(geometric_ndotl, shadow_bias_settings);
        const double normal_orientation = world_geometric_normal * light_world_direction >= 0. ? 1. : -1.;
        const vec4 shadow_position = world_position + world_geometric_normal *
            (normal_orientation * shadow_map.world_units_per_texel *
             shadow_bias_settings.normal_offset_texels);
        const double visibility = shadow_map.visibility(shadow_position, shadow_bias, shadow_pcf_radius);
        const double ndotl = std::max(0., shading_normal * l);
        double ambient = 0.4;
        double diffuse = visibility * ndotl;
        double specular = (1.0 + 3.0 * sampler.sample(model.specular(), uv.x, uv.y)[0] / 255.0) * std::pow(std::max(0.0, shading_normal * h), 35); // Blinn-Phong specular
        specular *= visibility;
        TGAColor gl_FragColor = sampler.sample(model.diffuse(), uv.x, uv.y);
        //      TGAColor gl_FragColor = {255, 255, 255, 255};
        for (int channel : {0, 1, 2}) {
            const long value = std::lround(gl_FragColor[channel] * (ambient + diffuse + specular));
            gl_FragColor[channel] = static_cast<std::uint8_t>(std::clamp(value, 0l, 255l));
        }
        return { false, gl_FragColor };                             
    }
};

struct RenderOptions {
    int width = 800;
    int height = 800;
    int shadow_width = 2048;
    int shadow_height = 2048;
    int shadow_pcf_radius = 1;
    ShadowBiasSettings shadow_bias = {};
    std::filesystem::path output_directory = ".";
    std::vector<std::string> model_paths = {};
};

void print_usage(const char* executable) {
    std::cerr
        << "Usage: " << executable << " [options] <model.obj> [more-models.obj ...]\n"
        << "Options:\n"
        << "  --size <width>x<height>          Framebuffer resolution (default: 800x800)\n"
        << "  --shadow-size <width>x<height>   Shadow-map resolution (default: 2048x2048)\n"
        << "  --output <directory>             Output directory (default: current directory)\n"
        << "  --pcf-radius <0-8>               PCF filter radius (default: 1)\n"
        << "  --bias-constant <texels>         Constant shadow bias (default: 0.5)\n"
        << "  --bias-slope <texels>            Slope-scaled shadow bias (default: 1.0)\n"
        << "  --bias-max-slope <value>         Maximum shadow bias slope (default: 4.0)\n"
        << "  --normal-offset <texels>         Normal offset for shadow lookup (default: 0.25)\n"
        << "  --help                           Show this message\n";
}

bool parse_positive_int(const std::string_view text, int& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() && value > 0;
}

bool parse_non_negative_int(const std::string_view text, int& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() && value >= 0;
}

bool parse_non_negative_double(const std::string_view text, double& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() &&
           std::isfinite(value) && value >= 0.;
}

bool parse_dimensions(const std::string_view text, int& width, int& height) {
    const std::size_t separator = text.find_first_of("xX");
    if (separator == std::string_view::npos) return false;
    return parse_positive_int(text.substr(0, separator), width) &&
           parse_positive_int(text.substr(separator + 1), height);
}

// Returns 1 for success, 0 for --help, and -1 for an invalid invocation.
int parse_options(const int argc, char** argv, RenderOptions& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") return 0;
        if (argument.size() < 2 || argument[0] != '-' || argument[1] != '-') {
            options.model_paths.emplace_back(argument);
            continue;
        }
        if (++index == argc) {
            std::cerr << "Error: missing value for " << argument << '\n';
            return -1;
        }

        const std::string_view value = argv[index];
        bool valid = false;
        if (argument == "--size") {
            valid = parse_dimensions(value, options.width, options.height);
        } else if (argument == "--shadow-size") {
            valid = parse_dimensions(value, options.shadow_width, options.shadow_height);
        } else if (argument == "--output") {
            options.output_directory = value;
            valid = !options.output_directory.empty();
        } else if (argument == "--pcf-radius") {
            valid = parse_non_negative_int(value, options.shadow_pcf_radius) &&
                    options.shadow_pcf_radius <= 8;
        } else if (argument == "--bias-constant") {
            valid = parse_non_negative_double(value, options.shadow_bias.constant_texels);
        } else if (argument == "--bias-slope") {
            valid = parse_non_negative_double(value, options.shadow_bias.slope_scale_texels);
        } else if (argument == "--bias-max-slope") {
            valid = parse_non_negative_double(value, options.shadow_bias.maximum_slope);
        } else if (argument == "--normal-offset") {
            valid = parse_non_negative_double(value, options.shadow_bias.normal_offset_texels);
        } else {
            std::cerr << "Error: unknown option " << argument << '\n';
            return -1;
        }
        if (!valid) {
            std::cerr << "Error: invalid value '" << value << "' for " << argument << '\n';
            return -1;
        }
    }
    if (options.model_paths.empty()) {
        std::cerr << "Error: at least one OBJ model is required\n";
        return -1;
    }
    return 1;
}

bool drop_zbuffer(const std::string& filename, const std::vector<double>& depth, const int width, const int height) {
    TGAImage zimg(width, height, TGAImage::GRAYSCALE, { 0,0,0,0 });
    double minz = 1.;
    double maxz = -1.;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            const double z = depth[x + y * width];
            if (z >= 1.) continue;
            minz = std::min(z, minz);
            maxz = std::max(z, maxz);
        }
    }
    if (maxz < minz) {
        return zimg.write_tga_file(filename, true, false);
    }
    const double range = std::max(maxz - minz, 1e-12);
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            const double z = depth[x + y * width];
            if (z >= 1.) continue;
            const auto value = static_cast<unsigned char>((z - minz) / range * 255.);
            zimg.set(x, y, { value, value, value, 255 });
        }
    }
    return zimg.write_tga_file(filename, true, false);
}

int main(int argc, char** argv) {
    RenderOptions options;
    const int parse_result = parse_options(argc, argv, options);
    if (parse_result != 1) {
        print_usage(argv[0]);
        return parse_result == 0 ? 0 : 1;
    }

    constexpr vec3 light_direction{ 1., 1., 1. }; // 从表面指向方向光
    constexpr vec3 eye{ -1., 0., 2. };
    constexpr vec3 center{ 0., 0., 0. };
    constexpr vec3 up{ 0., 1., 0. };
    constexpr double pi = 3.14159265358979323846;
    constexpr double fovy = 45. * pi / 180.;

    std::vector<Model> models;
    models.reserve(options.model_paths.size());
    for (const std::string& model_path : options.model_paths)
        models.emplace_back(model_path);

    AABB scene_bounds;
    for (const Model& model : models)
        for (int vertex = 0; vertex < model.nverts(); ++vertex)
            scene_bounds.expand(model.vert(vertex).xyz());
    if (!scene_bounds.valid) {
        std::cerr << "Error: no vertices were loaded" << std::endl;
        return 1;
    }

    ShadowMap shadow_map;
    shadow_map.width = options.shadow_width;
    shadow_map.height = options.shadow_height;

    std::error_code output_error;
    std::filesystem::create_directories(options.output_directory, output_error);
    if (output_error) {
        std::cerr << "Error: could not create output directory '" << options.output_directory
                  << "': " << output_error.message() << '\n';
        return 1;
    }

    { // 方向光深度 pass
        const DirectionalLightFrustum light_frustum =
            fit_directional_light(light_direction, scene_bounds, 0.1);
        ModelView = light_frustum.view;
        Perspective = light_frustum.projection;
        shadow_map.light_clip_from_world = Perspective * ModelView;
        shadow_map.world_units_per_texel = std::max(
            (light_frustum.right - light_frustum.left) / options.shadow_width,
            (light_frustum.top - light_frustum.bottom) / options.shadow_height);
        shadow_map.depth_range = light_frustum.far_plane - light_frustum.near_plane;
        init_viewport(0, 0, options.shadow_width, options.shadow_height);
        init_zbuffer(options.shadow_width, options.shadow_height);
        TGAImage depth_target(options.shadow_width, options.shadow_height, TGAImage::GRAYSCALE);

        for (const Model& model : models) {
            BlankShader shader{ model };
            for (int face = 0; face < model.nfaces(); ++face) {
                const Triangle triangle = { shader.vertex(face, 0),
                                            shader.vertex(face, 1),
                                            shader.vertex(face, 2) };
                rasterize(triangle, shader, depth_target);
            }
        }
        shadow_map.depth = zbuffer;
        if (!drop_zbuffer((options.output_directory / "shadowmap.tga").string(), shadow_map.depth,
                          options.shadow_width, options.shadow_height)) {
            std::cerr << "Error: could not write shadow-map debug image\n";
            return 1;
        }
    }

    { // 相机颜色 pass，在片元着色阶段直接查询阴影
        lookat(eye, center, up);
        init_perspective(fovy, options.width / static_cast<double>(options.height), .1, 100.);
        init_viewport(options.width / 16, options.height / 16,
                      options.width * 7 / 8, options.height * 7 / 8);
        init_zbuffer(options.width, options.height);
        TGAImage framebuffer(options.width, options.height, TGAImage::RGB, { 177, 195, 209, 255 });

        for (const Model& model : models) {
            BlinnPhongShader shader(light_direction, model, shadow_map,
                                    options.shadow_bias, options.shadow_pcf_radius);
            for (int face = 0; face < model.nfaces(); ++face) {
                const Triangle triangle = { shader.vertex(face, 0),
                                            shader.vertex(face, 1),
                                            shader.vertex(face, 2) };
                rasterize(triangle, shader, framebuffer);
            }
        }
        if (!framebuffer.write_tga_file((options.output_directory / "framebuffer.tga").string(), true, false)) {
            std::cerr << "Error: could not write framebuffer image\n";
            return 1;
        }
        if (!drop_zbuffer((options.output_directory / "zbuffer.tga").string(), zbuffer,
                          options.width, options.height)) {
            std::cerr << "Error: could not write z-buffer debug image\n";
            return 1;
        }
    }

    return 0;
}

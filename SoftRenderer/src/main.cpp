#include "my_gl.h"
#include "model.h"
//#include <algorithm>

extern mat<4, 4> Viewport, ModelView, Perspective; 
extern std::vector<double> zbuffer;     

struct BlankShader : Shader {
    const Model& model;

    BlankShader(const Model& m) : model(m) {}

    virtual VertexOut vertex(const int face, const int vert) override {
        VertexOut output;
        output.view_position = ModelView * model.vert(face, vert);
        output.clip_position = Perspective * output.view_position;
        return output;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3, const Triangle&) const override {
        return { false, {255, 255, 255, 255} };
    }
};

struct BlinnPhongShader : Shader {
    const Model& model;
    vec4 l;              
    Sampler sampler = { AddressMode::Repeat, FilterMode::Bilinear };

    BlinnPhongShader(const vec3 light, const Model& m) : model(m) {
        l = normalized((ModelView * vec4{ light.x, light.y, light.z, 0. })); 
    }

    virtual VertexOut vertex(const int face, const int vert) override {
        VertexOut output;
        output.uv = model.uv(face, vert);
        output.normal = ModelView.invert_transpose() * model.normal(face, vert);
        output.view_position = ModelView * model.vert(face, vert);
        output.clip_position = Perspective * output.view_position;
        return output;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bc, const Triangle& triangle) const override {
        mat<2, 4> E = { triangle[1].view_position - triangle[0].view_position,
                        triangle[2].view_position - triangle[0].view_position };
        mat<2, 2> U = { triangle[1].uv - triangle[0].uv,
                        triangle[2].uv - triangle[0].uv };
        mat<2, 4> T = U.invert() * E;
        mat<4, 4> D = { normalized(T[0]),  // tangent
                      normalized(T[1]),  // bitangent
                      normalized(triangle[0].normal * bc[0] + triangle[1].normal * bc[1] + triangle[2].normal * bc[2]),
                      {0,0,0,1} }; 
        vec2 uv = triangle[0].uv * bc[0] + triangle[1].uv * bc[1] + triangle[2].uv * bc[2];
        vec4 n = normalized(D.transpose() * model.normal(uv, sampler));

        // Blinn-Phong
        vec4 viewDir = vec4{0.0, 0.0, 1.0, 0.0}; 
		vec4 h = normalized(l + viewDir);    // 半程向量

        double ambient = 0.4;                                 
        double diffuse = 1.0 * std::max(0., n * l);                 
        double specular = (1.0 + 3.0 * sampler.sample(model.specular(), uv.x, uv.y)[0] / 255.0) * std::pow(std::max(0.0, n * h), 35); // Blinn-Phong specular
        TGAColor gl_FragColor = sampler.sample(model.diffuse(), uv.x, uv.y);
        //      TGAColor gl_FragColor = {255, 255, 255, 255};
        for (int channel : {0, 1, 2})
            gl_FragColor[channel] = std::min<int>(255, gl_FragColor[channel] * (ambient + diffuse + specular));
        return { false, gl_FragColor };                             
    }
};

void drop_zbuffer(std::string filename, std::vector<double>& zbuffer, int width, int height) {
    TGAImage zimg(width, height, TGAImage::GRAYSCALE, { 0,0,0,0 });
    double minz = +1000;
    double maxz = -1000;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            minz = std::min(z, minz);
            maxz = std::max(z, maxz);
        }
    }
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            z = (z - minz) / (maxz - minz) * 255;
            zimg.set(x, y, { static_cast<unsigned char>(z), 255, 255, 255 });
        }
    }
    zimg.write_tga_file(filename);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width = 800;      
    constexpr int height = 800;
    constexpr int shadoww = 8000;   
    constexpr int shadowh = 8000;
	constexpr vec3  light{ 1, 1, 1 }; 
    constexpr vec3    eye{ -1, 0, 2 }; // 相机位置
	constexpr vec3 center{ 0, 0, 0 }; // 相机看向原点
    constexpr vec3     up{ 0, 1, 0 }; // 相机up向量
	const double M_PI = 3.14159265358979323846;
    double fovy = 45.0 * M_PI / 180.0;  // 45度视野（转换为弧度）
    double aspect = width / (double)height; // 屏幕宽高比
    double near = 0.1;   // 近裁剪面
    double far = 100.0; // 远裁剪面（
    
    lookat(eye, center, up);
    //init_perspective(norm(eye - center));
	init_perspective(fovy, aspect, near, far);
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8);
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB, { 177, 195, 209, 255 });
    //    TGAImage framebuffer(width, height, TGAImage::RGB);

    for (int m = 1; m < argc; m++) {                    
        Model model(argv[m]);                       
        BlinnPhongShader shader(light, model); // 使用 Blinn-Phong 着色器
        for (int f = 0; f < model.nfaces(); f++) {      
            Triangle clip = { shader.vertex(f, 0),  
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            rasterize(clip, shader, framebuffer);   
        }
    }
    framebuffer.write_tga_file("framebuffer.tga");
    drop_zbuffer("zbuffer1.tga", zbuffer, width, height);

    std::vector<bool> mask(width * height, false);
    std::vector<double> zbuffer_copy = zbuffer;
    mat<4, 4> M = (Viewport * Perspective * ModelView).invert();

    { // 光源空间渲染pass
        lookat(light, center, up);
        //init_perspective(norm(eye - center));
        init_perspective(fovy, aspect, near, far);
        init_viewport(shadoww / 16, shadowh / 16, shadoww * 7 / 8, shadowh * 7 / 8);
        init_zbuffer(shadoww, shadowh);
        TGAImage trash(shadoww, shadowh, TGAImage::RGB, { 177, 195, 209, 255 });

        for (int m = 1; m < argc; m++) {                    
            Model model(argv[m]);                       
            BlankShader shader{ model };
            for (int f = 0; f < model.nfaces(); f++) {      
                Triangle clip = { shader.vertex(f, 0),  
                                  shader.vertex(f, 1),
                                  shader.vertex(f, 2) };
                rasterize(clip, shader, trash);         
            }
        }
        trash.write_tga_file("shadowmap.tga");
    }

    drop_zbuffer("zbuffer2.tga", zbuffer, shadoww, shadowh);

    mat<4, 4> N = Viewport * Perspective * ModelView;


    //后处理
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            //vec4 fragment = M * vec4{ (double)x, (double)y, zbuffer_copy[x + y * width], 1. };
            //vec4 q = N * fragment;
            //vec3 p = q.xyz() / q.w;
            //bool lit = (fragment.z >0.99 ||                                   // 背景
            ////bool lit = (fragment.z < -100 ||                                   // 背景
            //    (p.x < 0 || p.x >= shadoww || p.y < 0 || p.y >= shadowh) // 超出阴影贴图范围
            //    ||  (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] - .005));  // 可见
            //mask[x + y * width] = lit;
            // 1. 修改背景判断标准：直接判断原始 zbuffer 值
            double z_val = zbuffer_copy[x + y * width];
            if (z_val > 0.99) { // 如果 z 是初始化值 1.0，说明是背景
                mask[x + y * width] = true;
                continue;
            }

            // 2. 还原坐标
            vec4 fragment = M * vec4{ (double)x, (double)y, z_val, 1. };
            vec4 q = N * fragment;
            vec3 p = q.xyz() / q.w; // 映射到光源视角的屏幕坐标
            if ((p.x < 0 || p.x >= shadoww || p.y < 0 || p.y >= shadowh)) {
                mask[x + y * width] = true; // 超出阴影贴图范围，认为在光照下
				continue;
            }
            // 3. 阴影判定 (此时 z 越小越近)
            // 如果当前深度 p.z 明显大于 阴影贴图深度，则在阴影中 (mask = false)
            // 这里的 bias 应设为正数，例如 +0.005
            bool in_shadow = (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] + .005);
            //bool in_shadow = (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] );

            mask[x + y * width] = !in_shadow;
            

        }
    }

    TGAImage maskimg(width, height, TGAImage::GRAYSCALE);
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue;
            maskimg.set(x, y, { 255, 255, 255, 255 });
        }
    }
    maskimg.write_tga_file("mask.tga");

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue;
            TGAColor c = framebuffer.get(x, y);
            vec3 a = { c[0], c[1], c[2] };
            if (norm(a) < 80) continue;
            a = normalized(a) * 80;
            framebuffer.set(x, y, { static_cast<unsigned char>(a[0]), static_cast<unsigned char>(a[1]), static_cast<unsigned char>(a[2]), 255 });
            

            //framebuffer.set(x, y, c);
        
        }
    }
    framebuffer.write_tga_file("shadow.tga");


    return 0;
}

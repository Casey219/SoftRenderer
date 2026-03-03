#include "tgaimage.h"
#include <chrono>
#include <iostream>
#include <tuple> 
#include "geometry.h"
#include "model.h"
#include<cmath>



constexpr TGAColor white = { 255, 255, 255, 255 }; 
constexpr TGAColor green = { 0, 255,   0, 255 };
constexpr TGAColor red = { 0,   0, 255, 255 };
constexpr TGAColor blue = { 255, 128,  64, 255 };
constexpr TGAColor yellow = { 0, 200, 255, 255 };

const double PI = acos(-1.0);

//time≈2.11
void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
	bool steep = std::abs(ax - bx) < std::abs(ay - by);
	if (steep) {
		std::swap(ax, ay);
		std::swap(bx, by);
	}
	if (ax > bx) {
		std::swap(ax, bx);
		std::swap(ay, by);
	}

	const int dx = bx - ax;
	const int dy = std::abs(by - ay);
	const int y_step = (by > ay) ? 1 : -1;

	int y = ay;
	int ierror = 0;

	for (int x = ax; x <= bx; ++x) {
		if (steep)
			framebuffer.set(y, x, color);
		else
			framebuffer.set(x, y, color);

		ierror += 2 * dy;
		const int step = (ierror > dx) ? 1 : 0;
		y += y_step * step;
		ierror -= 2 * dx * step;
	}
}


double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
	return .5 * ((by-ay) * (ax + bx) + (ay - cy) * (ax + cx) + (cy - by) * (cx + bx));
}

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage& framebuffer, TGAColor color) {
	// bounding box for the triangle
	int bbminx = std::min(std::min(ax, bx), cx); 
	int bbminy = std::min(std::min(ay, by), cy);
	int bbmaxx = std::max(std::max(ax, bx), cx);
	int bbmaxy = std::max(std::max(ay, by), cy);
	double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
	if (total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel
#pragma omp parallel for
	for (int x = bbminx; x <= bbmaxx; ++x) {
		for (int y = bbminy; y <= bbmaxy; y++) {
			double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
			double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
			double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
			if (alpha < 0 || beta < 0 || gamma < 0) continue; // negative barycentric coordinate => the pixel is outside the triangle
			framebuffer.set(x, y, color);
		}
	}
}


std::tuple<int, int, int> project(Vec3f& v, const int width, const int height) {
	return
	{
		(v.x + 1.) * width / 2.,
		(v.y + 1.) * height / 2.,
		(v.z + 1.) * 255. / 2
	};
}

void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage& framebuffer, TGAImage& zbuffer,TGAColor color) {
	// bounding box for the triangle
	int bbminx = std::min(std::min(ax, bx), cx);
	int bbminy = std::min(std::min(ay, by), cy);
	int bbmaxx = std::max(std::max(ax, bx), cx);
	int bbmaxy = std::max(std::max(ay, by), cy);
	double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
	if (total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel
#pragma omp parallel for
	for (int x = bbminx; x <= bbmaxx; ++x) {
		for (int y = bbminy; y <= bbmaxy; y++) {
			double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
			double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
			double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
			if (alpha < 0 || beta < 0 || gamma < 0) continue; // negative barycentric coordinate => the pixel is outside the triangle
			unsigned char z = static_cast<unsigned char>(alpha * az + beta * bz + gamma * cz);
			if (z <= zbuffer.get(x, y)[0]) continue;
			zbuffer.set(x, y, {z});
			framebuffer.set(x, y, color);
			
		}
	}
}

Matrix lookat(Vec3f eye, Vec3f center, Vec3f up) {
	Vec3f z = (eye - center).normalize(); 
	Vec3f x = (up ^ z).normalize();      
	Vec3f y = (z ^ x).normalize();      

	Matrix Minv = Matrix::identity(4);
	Matrix Tr = Matrix::identity(4);

	
	for (int i = 0; i < 3; i++) {
		Minv[0][i] = x.raw[i];
		Minv[1][i] = y.raw[i];
		Minv[2][i] = z.raw[i];
		
		Tr[i][3] = -eye.raw[i];
	}

	return Minv * Tr;
}

//Matrix projection(const float f) {
//	Matrix p = Matrix::identity(4);
//	p[3][2] = -1/f; // f 是相机到原点的距离
//	return p;
//}

Matrix projection(float fov_deg, float aspect, float znear, float zfar) {
	Matrix m = Matrix::identity(4);
	float tanHalfFovy = tan(fov_deg * PI / 360.0f);
	m[0][0] = 1.0f / (aspect * tanHalfFovy);
	m[1][1] = 1.0f / (tanHalfFovy);
	m[2][2] = -(zfar + znear) / (zfar - znear);
	m[2][3] = -(2.0f * zfar * znear) / (zfar - znear);
	m[3][2] = -1.0f;
	m[3][3] = 0.0f;
	return m;
}

Matrix viewport(int x, int y, int w, int h) {
	Matrix m = Matrix::identity(4);
	m[0][3] = x + w / 2.f;
	m[1][3] = y + h / 2.f;
	m[2][3] = 255.f / 2.f;

	m[0][0] = w / 2.f;
	m[1][1] = h / 2.f;
	m[2][2] = 255.f / 2.f;
	return m;
}

Matrix v2m(Vec3f v) {
	Matrix m(4, 1);
	m[0][0] = v.x;
	m[1][0] = v.y;
	m[2][0] = v.z;
	m[3][0] = 1.f;
	return m;
}

// 辅助工具：4x1 矩阵转 Vec3f (包含齐次除法)
Vec3f m2v(Matrix m) {
	return Vec3f(m[0][0] / m[3][0], m[1][0] / m[3][0], m[2][0] / m[3][0]);
}

std::tuple<int, int, int> getXYZ(Vec3f& v) {
	return { v.x,v.y,v.z };
}


int main(int argc, char **argv) {
	constexpr int width = 1600;
	constexpr int height = 1600;
	
	TGAImage framebuffer(width, height, TGAImage::RGB);
	TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

	std::string filename = "../obj/diablo3_pose/diablo3_pose.obj";
	Model model(filename.c_str());

	Vec3f camera_position(0.0f, 0.0f, 1.0f);
	//Vec3f camera_position(0.0f, 3.0f, 0.0f);
	Vec3f center(0.0f, 0.0f, 0.0f);
	Vec3f up(0.0f, 1.0f, 0.0f);

	Matrix Projection = projection(45.0f,width/height,0.1f,1000.0f);
	//Matrix Projection = projection((camera_position - center).norm());
	Matrix MVP = Projection*lookat(camera_position, center, up);
	Matrix Viewport = viewport(0, 0, width, height);
	for (int i = 0; i < model.nfaces(); i++) {
		std::vector<int> face = model.face(i);
		
		Vec3f v0 = model.vert(face[0]);
		Vec3f v1 = model.vert(face[1]);
		Vec3f v2 = model.vert(face[2]);

		/*Matrix tmp = MVP * v2m(v0);
		std::cout << "W component: " << tmp[3][0] << std::endl;*/

		Vec3f screen_v0 = m2v(Viewport * MVP * v2m(v0));
		Vec3f screen_v1 = m2v(Viewport * MVP * v2m(v1));
		Vec3f screen_v2 = m2v(Viewport * MVP * v2m(v2));


		auto [x0, y0, z0] = getXYZ(screen_v0);
		auto [x1, y1, z1] = getXYZ(screen_v1);
		auto [x2, y2, z2] = getXYZ(screen_v2);
		
		
		TGAColor rnd;
		for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
		
		//triangle(screen_v0.x, screen_v0.x, screen_v0.x,x1, y1,z1, x2, y2,z2, framebuffer,zbuffer, rnd);
		triangle(x0, y0, z0,x1, y1,z1, x2, y2,z2, framebuffer,zbuffer, rnd);
	}

	
	framebuffer.write_tga_file("framebuffer.tga");
	zbuffer.write_tga_file("zbuffer.tga");


	

	return 0;
}
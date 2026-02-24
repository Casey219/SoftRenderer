#include "tgaimage.h"
#include <chrono>
#include <iostream>
#include <tuple> 
#include "geometry.h"
#include "model.h"



constexpr TGAColor white = { 255, 255, 255, 255 }; 
constexpr TGAColor green = { 0, 255,   0, 255 };
constexpr TGAColor red = { 0,   0, 255, 255 };
constexpr TGAColor blue = { 255, 128,  64, 255 };
constexpr TGAColor yellow = { 0, 200, 255, 255 };


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


int main(int argc, char **argv) {
	constexpr int width = 1600;
	constexpr int height = 1600;
	
	TGAImage framebuffer(width, height, TGAImage::RGB);
	TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

	//triangle_wireframe(7, 45, 45, 60, 35, 100, framebuffer, red);

	//triangle_blending_color(7, 45,45, 60 ,35, 100,  framebuffer, red,green,blue);

	/*triangle(7, 45, 35, 100, 45, 60, framebuffer, red);
	triangle(120, 35, 90, 5, 45, 110, framebuffer, white);
	triangle(115, 83, 80, 90, 85, 120, framebuffer, green);*/

	std::string filename = "../obj/diablo3_pose/diablo3_pose.obj";
	Model model(filename.c_str());
	for (int i = 0; i < model.nfaces(); i++) {
		std::vector<int> face = model.face(i);
		
		Vec3f v0 = model.vert(face[0]);
		Vec3f v1 = model.vert(face[1]);
		Vec3f v2 = model.vert(face[2]);

		auto [x0, y0, z0] = project(v0, width, height);
		auto [x1, y1, z1] = project(v1, width, height);
		auto [x2, y2, z2] = project(v2, width, height);
		
		TGAColor rnd;
		for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
		
		triangle(x0, y0, z0,x1, y1,z1, x2, y2,z2, framebuffer,zbuffer, rnd);
	}

	
	framebuffer.write_tga_file("framebuffer.tga");
	zbuffer.write_tga_file("zbuffer.tga");

	return 0;
}
#include "tgaimage.h"
#include <chrono>
#include <iostream>
#include <tuple> 
#include "geometry.h"
#include "model.h"

constexpr int width = 800;
constexpr int height = 800;

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



int main(int argc, char **argv) {
	
	std::string filename = "../obj/diablo3_pose/diablo3_pose.obj";
	Model model(filename.c_str());

	TGAImage framebuffer(width, height, TGAImage::RGB);

	
	for (int i = 0; i < model.nfaces(); i++) {
		//创建face数组用于保存一个face的三个顶点坐标
		std::vector<int> face = model.face(i);
		for (int j = 0; j < 3; j++) {
			//顶点v0
			Vec3f v0 = model.vert(face[j]);
			//顶点v1
			Vec3f v1 = model.vert(face[(j + 1) % 3]);
			//根据顶点v0和v1画线
			//先要进行模型坐标到屏幕坐标的转换
			//(-1,-1)对应(0,0)   (1,1)对应(width,height)
			int x0 = (v0.x + 1.) * width / 2.;
			int y0 = (v0.y + 1.) * height / 2.;
			int x1 = (v1.x + 1.) * width / 2.;
			int y1 = (v1.y + 1.) * height / 2.;
			//画线
			line(x0, y0, x1, y1, framebuffer, red);
		}
	}

	
	

	framebuffer.write_tga_file("framebuffer.tga");

	return 0;
}
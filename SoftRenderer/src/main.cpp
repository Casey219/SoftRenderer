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

////Scanline rendering
//void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage& framebuffer, TGAColor color) {
//	// sort the vertices, a,b,c in ascending y order (bubblesort yay!)
//	if (ay > by) { std::swap(ax, bx); std::swap(ay, by); }
//	if (ay > cy) { std::swap(ax, cx); std::swap(ay, cy); }
//	if (by > cy) { std::swap(bx, cx); std::swap(by, cy); }
//	int height = cy - ay;
//
//	if (ay != by) {
//		int segment_height = by - ay;
//		for (int y = ay; y <= by; ++y) {
//			int x1 = ax + (cx - ax) * (y - ay) / height;
//			int x2 = ax + (bx - ax) * (y - ay) / segment_height;
//			for (int x = std::min(x1, x2); x < std::max(x1, x2); ++x) { // draw a horizontal line
//				framebuffer.set(x, y, color);
//			}
//		}
//	}
//	if (by != cy) { // if the upper half is not degenerate
//		int segment_height = cy - by;
//		for (int y = by; y <= cy; y++) { // sweep the horizontal line from by to cy
//			int x1 = ax + ((cx - ax) * (y - ay)) / height;
//			int x2 = bx + ((cx - bx) * (y - by)) / segment_height;
//			for (int x = std::min(x1, x2); x < std::max(x1, x2); x++)  // draw a horizontal line
//				framebuffer.set(x, y, color);
//		}
//	}
//}

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

void triangle_blending_color(int ax, int ay, int bx, int by, int cx, int cy, TGAImage& framebuffer, TGAColor acolor,TGAColor bcolor,TGAColor ccolor) {
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
			TGAColor color = alpha * acolor + beta * bcolor + gamma * ccolor;
			framebuffer.set(x, y, color);
		}
	}
}


void triangle_wireframe(int ax, int ay, int bx, int by, int cx, int cy, TGAImage& framebuffer, TGAColor color) {
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
			/*if ((alpha == 0 && beta * gamma != 0) || (beta == 0 && alpha * gamma != 0) || (gamma == 0 && alpha * beta != 0)) {
				framebuffer.set(x, y, color);
			}*/
			double threhold = 0.05;
			if (alpha < threhold|| beta < threhold || gamma < threhold) {
				framebuffer.set(x, y, color);
			}
			
			
		}
	}
}




int main(int argc, char **argv) {
	constexpr int width = 1600;
	constexpr int height = 1600;
	
	TGAImage framebuffer(width, height, TGAImage::RGB);

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

		int x0 = (v0.x + 1.) * width / 2.;
		int y0 = (v0.y + 1.) * height / 2.;
		int x1 = (v1.x + 1.) * width / 2.;
		int y1 = (v1.y + 1.) * height / 2.;
		int x2 = (v2.x + 1.) * width / 2.;
		int y2 = (v2.y + 1.) * height / 2.;
		TGAColor rnd;
		for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
		triangle_wireframe(x0, y0, x1, y1, x2, y2, framebuffer, rnd);
		//draw triangles
		//triangle(x0, y0, x1, y1, x2, y2, framebuffer, rnd);
		//triangle_blending_color(x0, y0, x1, y1, x2, y2, framebuffer,red, green, blue);
		}

	
	framebuffer.write_tga_file("framebuffer.tga");

	return 0;
}
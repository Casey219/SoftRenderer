#include "tgaimage.h"
#include <chrono>
#include <iostream>

constexpr TGAColor white = { 255, 255, 255, 255 }; 
constexpr TGAColor green = { 0, 255,   0, 255 };
constexpr TGAColor red = { 0,   0, 255, 255 };
constexpr TGAColor blue = { 255, 128,  64, 255 };
constexpr TGAColor yellow = { 0, 200, 255, 255 };

////First attempt, the simplest approach
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//	for (float t = 0.; t < 1; t += .02) {
//		int x = std::round(ax + (bx - ax) * t);
//		int y = std::round(ay + (by - ay) * t);
//		framebuffer.set(x, y, color);
//	}
//}

//Second attempt, different sampling strategy time≈1.6
void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
	if (ax > bx) { // make it left−to−right
		std::swap(ax, bx);
		std::swap(ay, by);
	}
	for (int x = ax; x <= bx; ++x) {
		float t = (x - ax) / (float)(bx - ax);
		//float t = (x-ax) / static_cast<float>(bx-ax);
		int y = ay + (by - ay) * t;
		framebuffer.set(x, y, color);
	}
}


////Third attempt, the bulletproof one time≈2.11
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//	bool steep = std::abs(ax - bx) < std::abs(ay - by);
//	if (steep) {
//		std::swap(ax, ay);
//		std::swap(bx, by);
//	}
//	if (ax > bx) { // make it left−to−right
//		std::swap(ax, bx);
//		std::swap(ay, by);
//	}
//	for (int x = ax; x <= bx; ++x) {
//		float t = (x - ax) / (float)(bx - ax);
//		int y = ay + (by - ay) * t;
//		if(steep) framebuffer.set(y, x, color);// if transposed, de−transpose
//		else framebuffer.set(x, y, color);
//	}
//}


////Fourth attempt(optimizations):Round 1 
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//	bool steep = std::abs(ax - bx) < std::abs(ay - by);
//	if (steep) {
//		std::swap(ax, ay);
//		std::swap(bx, by);
//	}
//	if (ax > bx) { // make it left−to−right
//		std::swap(ax, bx);
//		std::swap(ay, by);
//	}
//	float y = ay;
//	for (int x = ax; x <= bx; ++x) {
//		float t = (x - ax) / (float)(bx - ax);
//		int y = ay + (by - ay) * t;
//		if (steep) framebuffer.set(y, x, color);// if transposed, de−transpose
//		else framebuffer.set(x, y, color);
//		y += (by - ay) / static_cast<float>(bx - ax);
//	}
//}

////Fourth attempt(optimizations):Round 2
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//	bool steep = std::abs(ax - bx) < std::abs(ay - by);
//	if (steep) {
//		std::swap(ax, ay);
//		std::swap(bx, by);
//	}
//	if (ax > bx) { // make it left−to−right
//		std::swap(ax, bx);
//		std::swap(ay, by);
//	}
//	int y = ay;
//	float error = 0;
//	for (int x = ax; x <= bx; ++x) {
//		float t = (x - ax) / (float)(bx - ax);
//		int y = ay + (by - ay) * t;
//		if (steep) framebuffer.set(y, x, color);// if transposed, de−transpose
//		else framebuffer.set(x, y, color);
//		error += (by - ay) / static_cast<float>(bx - ax);
//		if (error > .5) {
//			y += by > ay ? 1 : -1;
//			error -= 1.0;
//		}
//	}
//}


////Fourth attempt(optimizations):Round 3  Remove float 
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//	bool steep = std::abs(ax - bx) < std::abs(ay - by);
//	if (steep) {
//		std::swap(ax, ay);
//		std::swap(bx, by);
//	}
//	if (ax > bx) { // make it left−to−right
//		std::swap(ax, bx);
//		std::swap(ay, by);
//	}
//	int y = ay;
//	int ierror = 0;
//	for (int x = ax; x <= bx; ++x) {
//		float t = (x - ax) / (float)(bx - ax);
//		int y = ay + (by - ay) * t;
//		if (steep) framebuffer.set(y, x, color);// if transposed, de−transpose
//		else framebuffer.set(x, y, color);
//		ierror += 2*std::abs(by - ay) ;
//		if (ierror > bx-ax) {
//			y += by > ay ? 1 : -1;
//			ierror -= 2*(bx-ax);
//		}
//	}
//}

////Fourth attempt(optimizations):Round 4  Remove conditional statements time≈2.17
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//	bool steep = std::abs(ax - bx) < std::abs(ay - by);
//	if (steep) {
//		std::swap(ax, ay);
//		std::swap(bx, by);
//	}
//	if (ax > bx) { // make it left−to−right
//		std::swap(ax, bx);
//		std::swap(ay, by);
//	}
//	int y = ay;
//	int ierror = 0;
//	for (int x = ax; x <= bx; ++x) {
//		float t = (x - ax) / (float)(bx - ax);
//		int y = ay + (by - ay) * t;
//		if (steep) framebuffer.set(y, x, color);// if transposed, de−transpose
//		else framebuffer.set(x, y, color);
//		ierror += 2 * std::abs(by - ay);
//
//		y += (by > ay ? 1 : -1)* (ierror > bx - ax);
//		ierror-= 2 * (bx - ax) * (ierror > bx - ax);
//	}
//}


////time≈2.11
//void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
//	bool steep = std::abs(ax - bx) < std::abs(ay - by);
//	if (steep) {
//		std::swap(ax, ay);
//		std::swap(bx, by);
//	}
//	if (ax > bx) {
//		std::swap(ax, bx);
//		std::swap(ay, by);
//	}
//
//	const int dx = bx - ax;
//	const int dy = std::abs(by - ay);
//	const int y_step = (by > ay) ? 1 : -1;
//
//	int y = ay;
//	int ierror = 0;
//
//	for (int x = ax; x <= bx; ++x) {
//		if (steep)
//			framebuffer.set(y, x, color);
//		else
//			framebuffer.set(x, y, color);
//
//		ierror += 2 * dy;
//		const int step = (ierror > dx) ? 1 : 0;
//		y += y_step * step;
//		ierror -= 2 * dx * step;
//	}
//}


int main(int argc, char **argv) {
	constexpr int width = 64;
	constexpr int height = 64;
	TGAImage framebuffer(width, height, TGAImage::RGB);

	int ax = 7, ay = 3;
	int bx = 12, by = 37;
	int cx = 62, cy = 53;

	framebuffer.set(ax, ay, white);
	framebuffer.set(bx, by, green);
	framebuffer.set(cx, cy, red);

	line(ax, ay, bx, by, framebuffer, blue);
	line(cx, cy, bx, by, framebuffer, green);
	line(cx, cy, ax, ay, framebuffer, yellow);
	line(ax, ay, cx, cy, framebuffer, red);

	/*auto start = std::chrono::high_resolution_clock::now();

	std::srand(std::time({}));
	for (int i = 0; i < (1 << 24); i++) {
		int ax = rand() % width, ay = rand() % height;
		int bx = rand() % width, by = rand() % height;
		line(ax, ay, bx, by, framebuffer, TGAColor{ static_cast<std::uint8_t>(rand() % 255), static_cast<std::uint8_t>(rand() % 255), static_cast<std::uint8_t>(rand() % 255), static_cast<std::uint8_t>(rand() % 255) });
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> diff = end - start;

	std::cout << "耗时: " << diff.count() << " 秒" << std::endl;*/
	

	framebuffer.write_tga_file("framebuffer.tga");

	return 0;
}
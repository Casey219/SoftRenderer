#include "tgaimage.h"
#include <chrono>
#include <iostream>
#include "geometry.h"
#include "model.h"

#include"my_gl.h"



constexpr TGAColor white = { 255, 255, 255, 255 }; 
constexpr TGAColor green = { 0, 255,   0, 255 };
constexpr TGAColor red = { 0,   0, 255, 255 };
constexpr TGAColor blue = { 255, 128,  64, 255 };
constexpr TGAColor yellow = { 0, 200, 255, 255 };



int main(int argc, char **argv) {
	constexpr int width = 1600;
	constexpr int height = 1600;
	
	TGAImage framebuffer(width, height, TGAImage::RGB);
	//TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

	std::string filename = "../obj/diablo3_pose/diablo3_pose.obj";
	Model model(filename.c_str());

	Vec3f camera_position(0.0f, 0.0f, 3.0f);
	//Vec3f camera_position(0.0f, 3.0f, 0.0f);
	Vec3f center(0.0f, 0.0f, 0.0f);
	Vec3f up(0.0f, 1.0f, 0.0f);

	Matrix Projection = perspective(45.0f,(float)width/height,0.1f,100.0f);
	init_zbuffer(width, height);
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

		/*Vec3f screen_v0 = m2v(Viewport * MVP * v2m(v0));
		Vec3f screen_v1 = m2v(Viewport * MVP * v2m(v1));
		Vec3f screen_v2 = m2v(Viewport * MVP * v2m(v2));*/

		Matrix clip_v0 = MVP * v2m(v0);
		Matrix clip_v1 = MVP * v2m(v1);
		Matrix clip_v2 = MVP * v2m(v2);

		// 齐次裁剪
		if (clip_v0[3][0] <= 0 ||clip_v1[3][0] <= 0 ||clip_v2[3][0] <= 0)
			continue;

		// NDC z
		float z0 = clip_v0[2][0] / clip_v0[3][0];
		float z1 = clip_v1[2][0] / clip_v1[3][0];
		float z2 = clip_v2[2][0] / clip_v2[3][0];

		// 屏幕坐标（仍然用 m2v）
		Vec3f screen_v0 = m2v(Viewport * clip_v0);
		Vec3f screen_v1 = m2v(Viewport * clip_v1);
		Vec3f screen_v2 = m2v(Viewport * clip_v2);

		
		TGAColor rnd;
		for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
		
		//triangle(screen_v0,screen_v1,screen_v2, framebuffer, rnd);
		triangle(
			screen_v0.x, screen_v0.y, z0,
			screen_v1.x, screen_v1.y, z1,
			screen_v2.x, screen_v2.y, z2,
			framebuffer, rnd
		);
	}

	
	framebuffer.write_tga_file("framebuffer.tga");

	return 0;
}
#include "tgaimage.h"
#include <chrono>
#include <iostream>
#include <tuple> 
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
	TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

	std::string filename = "../obj/diablo3_pose/diablo3_pose.obj";
	Model model(filename.c_str());

	Vec3f camera_position(0.0f, 0.0f, 1.0f);
	//Vec3f camera_position(0.0f, 3.0f, 0.0f);
	Vec3f center(0.0f, 0.0f, 0.0f);
	Vec3f up(0.0f, 1.0f, 0.0f);

	Matrix Projection = perspective(45.0f,width/height,0.1f,100.0f);
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

		
		TGAColor rnd;
		for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
		
		triangle(screen_v0,screen_v1,screen_v2, framebuffer,zbuffer, rnd);
	}

	
	framebuffer.write_tga_file("framebuffer.tga");
	zbuffer.write_tga_file("zbuffer.tga");

	return 0;
}
#include "my_gl.h"

const double PI = acos(-1.0);
const int depth = 1;

std::vector<double> zbuffer;               // depth buffer

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
	//return .5 * ((by - ay) * (ax + bx) + (ay - cy) * (ax + cx) + (cy - by) * (cx + bx));
	return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

void triangle2D(int ax, int ay, int bx, int by, int cx, int cy, TGAImage& framebuffer, TGAColor color) {
	// bounding box for the triangle
	int bbminx = std::min(std::min(ax, bx), cx);
	int bbminy = std::min(std::min(ay, by), cy);
	int bbmaxx = std::max(std::max(ax, bx), cx);
	int bbmaxy = std::max(std::max(ay, by), cy);
	double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
	if (total_area <=0) return; 
	//if (total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel
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

void init_zbuffer(int width, int height) {
	zbuffer = std::vector(width * height, 1.0);
	//zbuffer = std::vector(width * height, std::numeric_limits<double>::infinity());
}


void triangle(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, TGAImage& framebuffer, TGAColor color) {
//void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage& framebuffer, TGAColor color) {
	// bounding box for the triangle
	/*int bbminx = std::min(std::min(ax, bx), cx);
	int bbminy = std::min(std::min(ay, by), cy);
	int bbmaxx = std::max(std::max(ax, bx), cx);
	int bbmaxy = std::max(std::max(ay, by), cy);*/
	/*double zi0 = 1.0 / az;
	double zi1 = 1.0 / bz;
	double zi2 = 1.0 / cz;*/

	int width = framebuffer.width(),height=framebuffer.height();
	auto [bbminx, bbmaxx] = std::minmax({ ax,bx,cx }); // bounding box for the triangle
	auto [bbminy, bbmaxy] = std::minmax({ ay,by,cy}); // defined by its top left and bottom right corners
	bbminx = std::max(bbminx, .0f);
	bbminy = std::max(bbminy, .0f);
	bbmaxx = std::min(bbmaxx, (float)width - 1);
	bbmaxy = std::min(bbmaxy, (float)height - 1);
//#pragma omp parallel for
	double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
	if (total_area <=0) return; // backface culling + discarding triangles that cover less than a pixel
	//if (total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel
	
//#pragma omp parallel for
	for (int x = bbminx; x <= bbmaxx; ++x) {
		for (int y = bbminy; y <= bbmaxy; y++) {
			double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
			double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
			double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
			if (alpha < 0 || beta < 0 || gamma < 0) continue; // negative barycentric coordinate => the pixel is outside the triangle
			//double interpolated_reciprocal_z = alpha * zi0 + beta * zi1 + gamma * zi2;
			//double z = 1.0 / interpolated_reciprocal_z;
			double z = (alpha * az + beta * bz + gamma * cz); // perspective correct interpolation of z
			if (z > zbuffer[x+y*width]) continue;
			zbuffer[x + y * width] = z;
			framebuffer.set(x, y, color);

		}
	}
}

void triangle(Vec3f v0, Vec3f v1, Vec3f v2, TGAImage& framebuffer, TGAColor color)
{
	auto [x0, y0, z0] = v0.getXYZ();
	auto [x1, y1, z1] = v1.getXYZ();
	auto [x2, y2, z2] = v2.getXYZ();
	triangle(x0, y0, z0, x1, y1, z1, x2, y2, z2, framebuffer, color);
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

Matrix perspective(float fov_deg, float aspect, float znear, float zfar) {
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
	

	m[0][0] = w / 2.f;
	m[1][1] = h / 2.f;
	
	m[2][2] = depth/2.f;
	m[2][3] = depth/2.f;
	return m;
}
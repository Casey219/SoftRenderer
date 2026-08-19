#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>
#include <limits>
#include "tgaimage.h"

namespace {
double address_uv(const double coordinate, const AddressMode mode) {
    if (mode == AddressMode::Clamp)
        return std::clamp(coordinate, 0., 1.);
    return coordinate - std::floor(coordinate);
}

int address_texel(const int coordinate, const int size, const AddressMode mode) {
    if (mode == AddressMode::Clamp)
        return std::clamp(coordinate, 0, size - 1);
    return (coordinate % size + size) % size;
}

TGAColor lerp_color(const TGAColor& c00, const TGAColor& c10,
                    const TGAColor& c01, const TGAColor& c11,
                    const double tx, const double ty) {
    TGAColor result;
    result.bytespp = c00.bytespp;
    for (int channel = 0; channel < result.bytespp; ++channel) {
        const double top = c00[channel] * (1. - tx) + c10[channel] * tx;
        const double bottom = c01[channel] * (1. - tx) + c11[channel] * tx;
        const double value = top * (1. - ty) + bottom * ty;
        result[channel] = static_cast<std::uint8_t>(std::clamp(std::lround(value), 0l, 255l));
    }
    return result;
}
} // namespace

TGAImage::TGAImage(const int w, const int h, const int bpp, TGAColor c)
    : w(w), h(h), bpp(static_cast<std::uint8_t>(bpp)),
      data(static_cast<std::size_t>(w) * h * bpp, 0) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            set(i, j, c);
}

bool TGAImage::read_tga_file(const std::string filename) {
    std::ifstream in;
    in.open(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "can't open file " << filename << "\n";
        return false;
    }
    TGAHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in.good()) {
        std::cerr << "an error occured while reading the header\n";
        return false;
    }
    w = header.width;
    h = header.height;
    bpp = header.bitsperpixel >> 3;
    if (w <= 0 || h <= 0 || (bpp != GRAYSCALE && bpp != RGB && bpp != RGBA)) {
        std::cerr << "bad bpp (or width/height) value\n";
        return false;
    }
    size_t nbytes = bpp * w * h;
    data = std::vector<std::uint8_t>(nbytes, 0);
    if (3 == header.datatypecode || 2 == header.datatypecode) {
        in.read(reinterpret_cast<char*>(data.data()), nbytes);
        if (!in.good()) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    }
    else if (10 == header.datatypecode || 11 == header.datatypecode) {
        if (!load_rle_data(in)) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    }
    else {
        std::cerr << "unknown file format " << (int)header.datatypecode << "\n";
        return false;
    }
    if (!(header.imagedescriptor & 0x20))
        flip_vertically();
    if (header.imagedescriptor & 0x10)
        flip_horizontally();
    std::cerr << w << "x" << h << "/" << bpp * 8 << "\n";
    return true;
}

bool TGAImage::load_rle_data(std::ifstream& in) {
    size_t pixelcount = w * h;
    size_t currentpixel = 0;
    size_t currentbyte = 0;
    TGAColor colorbuffer;
    do {
        const int header_value = in.get();
        if (header_value == EOF) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
        std::uint8_t chunkheader = static_cast<std::uint8_t>(header_value);
        if (chunkheader < 128) {
            chunkheader++;
            for (int i = 0; i < chunkheader; i++) {
                in.read(reinterpret_cast<char*>(colorbuffer.bgra), bpp);
                if (!in.good()) {
                    std::cerr << "an error occured while reading the header\n";
                    return false;
                }
                for (int t = 0; t < bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel > pixelcount) {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        }
        else {
            chunkheader -= 127;
            in.read(reinterpret_cast<char*>(colorbuffer.bgra), bpp);
            if (!in.good()) {
                std::cerr << "an error occured while reading the header\n";
                return false;
            }
            for (int i = 0; i < chunkheader; i++) {
                for (int t = 0; t < bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel > pixelcount) {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        }
    } while (currentpixel < pixelcount);
    return true;
}

bool TGAImage::write_tga_file(const std::string filename, const bool vflip, const bool rle) const {
    constexpr std::uint8_t developer_area_ref[4] = { 0, 0, 0, 0 };
    constexpr std::uint8_t extension_area_ref[4] = { 0, 0, 0, 0 };
    constexpr std::uint8_t footer[18] = { 'T','R','U','E','V','I','S','I','O','N','-','X','F','I','L','E','.','\0' };
    std::ofstream out;
    out.open(filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "can't open file " << filename << "\n";
        return false;
    }
    if (w <= 0 || h <= 0 ||
        w > std::numeric_limits<std::uint16_t>::max() ||
        h > std::numeric_limits<std::uint16_t>::max()) {
        std::cerr << "image dimensions are outside the TGA format range\n";
        return false;
    }
    TGAHeader header = {};
    header.bitsperpixel = static_cast<std::uint8_t>(bpp << 3);
    header.width = static_cast<std::uint16_t>(w);
    header.height = static_cast<std::uint16_t>(h);
    header.datatypecode = (bpp == GRAYSCALE ? (rle ? 11 : 3) : (rle ? 10 : 2));
    header.imagedescriptor = vflip ? 0x00 : 0x20; // top-left or bottom-left origin
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!out.good()) goto err;
    if (!rle) {
        out.write(reinterpret_cast<const char*>(data.data()), w * h * bpp);
        if (!out.good()) goto err;
    }
    else if (!unload_rle_data(out)) goto err;
    out.write(reinterpret_cast<const char*>(developer_area_ref), sizeof(developer_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char*>(extension_area_ref), sizeof(extension_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char*>(footer), sizeof(footer));
    if (!out.good()) goto err;
    return true;
err:
    std::cerr << "can't dump the tga file\n";
    return false;
}

bool TGAImage::unload_rle_data(std::ofstream& out) const {
    const std::uint8_t max_chunk_length = 128;
    size_t npixels = w * h;
    size_t curpix = 0;
    while (curpix < npixels) {
        size_t chunkstart = curpix * bpp;
        size_t curbyte = curpix * bpp;
        std::uint8_t run_length = 1;
        bool raw = true;
        while (curpix + run_length < npixels && run_length < max_chunk_length) {
            bool succ_eq = true;
            for (int t = 0; succ_eq && t < bpp; t++)
                succ_eq = (data[curbyte + t] == data[curbyte + t + bpp]);
            curbyte += bpp;
            if (1 == run_length)
                raw = !succ_eq;
            if (raw && succ_eq) {
                run_length--;
                break;
            }
            if (!raw && !succ_eq)
                break;
            run_length++;
        }
        curpix += run_length;
        out.put(raw ? run_length - 1 : run_length + 127);
        if (!out.good()) return false;
        out.write(reinterpret_cast<const char*>(data.data() + chunkstart), (raw ? run_length * bpp : bpp));
        if (!out.good()) return false;
    }
    return true;
}

TGAColor TGAImage::get(const int x, const int y) const {
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) return {};
    TGAColor ret = { 0, 0, 0, 0, bpp };
    const std::uint8_t* p = data.data() + (x + y * w) * bpp;
    for (int i = bpp; i--; ret.bgra[i] = p[i]);
    return ret;
}

void TGAImage::set(int x, int y, const TGAColor& c) {
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) return;
    memcpy(data.data() + (x + y * w) * bpp, c.bgra, bpp);
}

void TGAImage::flip_horizontally() {
    for (int i = 0; i < w / 2; i++)
        for (int j = 0; j < h; j++)
            for (int b = 0; b < bpp; b++)
                std::swap(data[(i + j * w) * bpp + b], data[(w - 1 - i + j * w) * bpp + b]);
}

void TGAImage::flip_vertically() {
    for (int i = 0; i < w; i++)
        for (int j = 0; j < h / 2; j++)
            for (int b = 0; b < bpp; b++)
                std::swap(data[(i + j * w) * bpp + b], data[(i + (h - 1 - j) * w) * bpp + b]);
}

int TGAImage::width() const {
    return w;
}

int TGAImage::height() const {
    return h;
}

TGAColor Sampler::sample(const TGAImage& texture, const double u, const double v) const {
    if (texture.width() <= 0 || texture.height() <= 0 ||
        !std::isfinite(u) || !std::isfinite(v)) return {};

    const double addressed_u = address_uv(u, address_mode);
    const double addressed_v = address_uv(v, address_mode);

    if (filter_mode == FilterMode::Nearest) {
        const int x = address_texel(static_cast<int>(std::floor(addressed_u * texture.width())),
                                    texture.width(), address_mode);
        const int y = address_texel(static_cast<int>(std::floor(addressed_v * texture.height())),
                                    texture.height(), address_mode);
        return texture.get(x, y);
    }

    const double x = addressed_u * texture.width() - .5;
    const double y = addressed_v * texture.height() - .5;
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const double tx = x - std::floor(x);
    const double ty = y - std::floor(y);

    return lerp_color(
        texture.get(address_texel(x0, texture.width(), address_mode),
                    address_texel(y0, texture.height(), address_mode)),
        texture.get(address_texel(x1, texture.width(), address_mode),
                    address_texel(y0, texture.height(), address_mode)),
        texture.get(address_texel(x0, texture.width(), address_mode),
                    address_texel(y1, texture.height(), address_mode)),
        texture.get(address_texel(x1, texture.width(), address_mode),
                    address_texel(y1, texture.height(), address_mode)),
        tx, ty);
}

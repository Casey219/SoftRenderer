#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "model.h"

Model::Model(const char* filename) : verts_(), faces_() {
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            Vec3f v;
            for (int i = 0; i < 3; ++i) {
                if (!(iss >> v.raw[i])) {
                    std::cerr << "Vertex parse error\n";
                    break;
                }
            }
            verts_.push_back(v);
        }
        else if (prefix == "f") {
            std::vector<int> f;
            std::string token;
            while (iss >> token) {
                std::istringstream token_ss(token);
                std::string v_idx_str;
                std::getline(token_ss, v_idx_str, '/');
                if (!v_idx_str.empty()) {
                    try {
                        int idx = std::stoi(v_idx_str) - 1;
                        f.push_back(idx);
                    }
                    catch (...) {
                        std::cerr << "Invalid face index: " << v_idx_str << std::endl;
                    }
                }
            }
            if (!f.empty()) faces_.push_back(f);
        }
    }
    std::cerr << "# v# " << verts_.size() << " f# " << faces_.size() << std::endl;
}

Model::~Model() {
}



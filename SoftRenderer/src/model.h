#pragma once
#ifndef __MODEL_H__
#define __MODEL_H__

#include <vector>
#include "geometry.h"
#include <cassert>

class Model {
private:
	std::vector<Vec3f> verts_;
	std::vector<std::vector<int> > faces_;
public:
    explicit Model(const char* filename);
	~Model();

    int nverts() const { return (int)verts_.size(); }
    int nfaces() const { return (int)faces_.size(); }

    Vec3f vert(int i) const {
        assert(i >= 0 && i < (int)verts_.size());
        return verts_[i];
    }

    const std::vector<int>& face(int idx) const {
        assert(idx >= 0 && idx < (int)faces_.size());
        return faces_[idx];
    }
};

#endif //__MODEL_H__
#ifndef OCTREE_H
#define OCTREE_H

#include "AABB.h"
#include <array>
class Triangle;

class Octree
{
public:
    Octree(const AABB& bounds = AABB(), int depth = 0, int maxDepth = 6, int maxContent = 8);

    AABB mBounds;
    int mDepth;
    int mMaxDepth;
    int mMaxContent;
    std::vector<int> mContent;
    std::array<std::unique_ptr<Octree>, 8> mChildren;

    bool isLeaf() const { return !mChildren[0]; }
    void subdivide();
    //
    void insert(std::vector<Triangle>& triangleVector, int index);
};

#endif // OCTREE_H

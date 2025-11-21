#include "Octree.h"
#include "Triangle.h"

Octree::Octree(const AABB &bounds, int depth, int maxDepth, int maxContent) : mBounds(bounds), mDepth(depth), mMaxDepth(maxDepth), mMaxContent(maxContent)
{}

void Octree::subdivide()
{
    // Anthropic - Claude
    QVector3D center = mBounds.center();
    QVector3D halfSize = mBounds.size() * 0.5f;

    for (int i = 0; i < 8; ++i) {
        QVector3D offset(
            // Bitwise AND operator - https://www.geeksforgeeks.org/c/bitwise-operators-in-c-cpp/
            (i & 1) ? halfSize.x() * 0.5f : -halfSize.x() * 0.5f,
            (i & 2) ? halfSize.y() * 0.5f : -halfSize.y() * 0.5f,
            (i & 4) ? halfSize.z() * 0.5f : -halfSize.z() * 0.5f
        );

        QVector3D childMin = center + offset - halfSize * 0.5f;
        QVector3D childMax = center + offset + halfSize * 0.5f;

        mChildren[i] = std::make_unique<Octree>(AABB(childMin, childMax), mDepth + 1, mMaxDepth, mMaxContent);
    }
}

void Octree::insert(std::vector<Triangle> &triangleVector, int index)
{
    AABB triangleBounds = triangleVector.at(index).Bounds();
    if (!mBounds.intersectsAABB(triangleBounds)) return; // If the triangle doesn't intersect with the cell just return

    // Triangle is likely to intersect with this cell so try adding it we'll ignore edge cases for now
    if (isLeaf())
    {
        if (mContent.size() < mMaxContent || mDepth >= mMaxDepth) mContent.push_back(index);

        subdivide();
    }

}

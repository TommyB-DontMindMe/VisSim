#include "Octree.h"
#include "Triangle.h"

Octree::Octree(std::vector<Triangle> &contentSource, const AABB &bounds, int depth, int maxDepth, int maxContent) : mContentSource(contentSource), mBounds(bounds), mDepth(depth), mMaxDepth(maxDepth), mMaxContent(maxContent)
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

        mChildren[i] = std::make_unique<Octree>(mContentSource, AABB(childMin, childMax), mDepth + 1, mMaxDepth, mMaxContent);
    }
    // End of Claude generated section

    // Attempt distributing content to children
    for (int index : mContent)
    {
        for (int i = 0; i < 8; ++i)
        {
            mChildren.at(i)->insert(index);
        }
    }
    mContent.clear();
}

void Octree::insert(int index)
{
    AABB triangleBounds = mContentSource.at(index).Bounds();
    if (!mBounds.intersectsAABB(triangleBounds)) return; // If the triangle doesn't intersect with the cell just return

    // Triangle is likely to intersect with this cell so try adding it we'll ignore edge cases for now
    if (isLeaf())
    {
        if (mContent.size() < mMaxContent || mDepth >= mMaxDepth)
        {
            mContent.push_back(index);
            return;
        }
        if (!mChildren[0]) subdivide();
    }
    for (int i = 0; i < 8; ++i)
    {
        mChildren[i]->insert(index);
    }
}

void Octree::query(const AABB &iBounds, std::vector<int> &oIndices) const
{
    if (!mBounds.intersectsAABB(iBounds)) return;

    // https://cplusplus.com/reference/vector/vector/insert/
    if (isLeaf()) oIndices.insert(oIndices.end(), mContent.begin(), mContent.end());
    else
    {
        for (int i = 0; i < 8; ++i)
        {
            if (mChildren[i]->mBounds.intersectsAABB(iBounds)) mChildren[i]->query(iBounds, oIndices);
        }
    }
}

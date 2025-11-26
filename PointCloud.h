#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "VisualObject.h"
class PointCloud : public VisualObject
{
public:
    PointCloud(const std::string& filename, const QVector3D& min, const QVector3D& max);

    QVector2D Circumcenter(const QVector2D& A, const QVector2D& B, const QVector2D& C);
    // For CircumRadius I might just take the distance between A and Circumcenter

};

// Should probably be moved to it's own file but it can live here for now
struct DelaunayTriangle
{
    int v0, v1, v2;
    QVector3D circumCenter;
    float circumRadius{0};

    DelaunayTriangle(int a, int b, int c) : v0(a), v1(b), v2(c) {}

    bool operator==(const DelaunayTriangle& other) const;
};
struct Edge
{
    int v0, v1;
    Edge(int a, int b) : v0(std::min(a, b)), v1(std::max(a, b)) {}

    bool operator==(const Edge& other) const;
};

#endif // POINTCLOUD_H

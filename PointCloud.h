#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "VisualObject.h"
class PointCloud : public VisualObject
{
public:
    PointCloud(const std::string& filename, const QVector3D& min, const QVector3D& max);
};

namespace Delaunay
{

QVector2D Circumcenter(const QVector2D& A, const QVector2D& B, const QVector2D& C);
// For CircumRadius I might just take the distance between A and Circumcenter
// Should probably be moved to it's own file but it can live here for now
struct Triangle
{
    int v0, v1, v2;
    QVector2D circumCenter;
    float circumRadius{0};

    Triangle(const int& a, const int& b, const int& c) : v0(a), v1(b), v2(c) {}

    bool operator==(const Triangle& other) const {
        return (v0 == other.v0 && v1 == other.v1 && v2 == other.v2) ||
               (v0 == other.v1 && v1 == other.v2 && v2 == other.v0) ||
               (v0 == other.v2 && v1 == other.v0 && v2 == other.v1);
    }
};
struct Edge
{
    int v0, v1;
    Edge(int a, int b) : v0(a), v1(b) {}

    bool operator==(const Edge& other) const { return (v0 == other.v0 && v1 == other.v1) || (v0 == other.v1 && v1 == other.v0); }
};

}
#endif // POINTCLOUD_H

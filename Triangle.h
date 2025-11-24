#ifndef TRIANGLE_H
#define TRIANGLE_H
#include <qvectornd.h>


class Vertex;
class AABB;

struct Triangle {
    Triangle(const QVector3D& A, const QVector3D& B, const QVector3D& C);
    Triangle(const Vertex& A, const Vertex& B, const Vertex& C);

    QVector3D v0, v1, v2;   // Vertices
    QVector3D normal;
    float d00, d01, d11, denom;
};

namespace TriangleHelpers {
inline void ComputeVariables(Triangle &Tri);
AABB TriangleBounds(const Triangle& Tri);
QVector3D ProjectPointOnPlane(const Triangle& Tri, const QVector3D& P);
QVector3D ProjectPointOnEdge(const QVector3D&P, const QVector3D& A, const QVector3D& B); // Not really specific to triangles, should probably be moved elsewhere
QVector3D ClosestPoint(const Triangle& Tri, const QVector3D& point);

}

#endif // TRIANGLE_H

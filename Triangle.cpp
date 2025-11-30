#include "Triangle.h"
#include <QVector3D>
#include <QVector4D>
#include "Vertex.h"
#include "AABB.h"



Triangle::Triangle(const QVector3D &A, const QVector3D &B, const QVector3D &C) : v0(A), v1(B), v2(C)
{
    TriangleHelpers::ComputeVariables(*this);
}

Triangle::Triangle(const Vertex &A, const Vertex &B, const Vertex &C) : Triangle(A.pos(), B.pos(), C.pos()) {}



QVector3D TriangleHelpers::ProjectPointOnPlane(const Triangle &Tri, const QVector3D &P)
{

    QVector3D AP = P - Tri.v0;
    float t = QVector3D::dotProduct(AP, Tri.normal);
    return P - (Tri.normal * t);
}

AABB TriangleHelpers::TriangleBounds(const Triangle &Tri)
{

    QVector3D min = QVector3D(
        std::min({Tri.v0.x(), Tri.v1.x(), Tri.v2.x()}),
        std::min({Tri.v0.y(), Tri.v1.y(), Tri.v2.y()}),
        std::min({Tri.v0.z(), Tri.v1.z(), Tri.v2.z()}));
    QVector3D max = QVector3D(
        std::max({Tri.v0.x(), Tri.v1.x(), Tri.v2.x()}),
        std::max({Tri.v0.y(), Tri.v1.y(), Tri.v2.y()}),
        std::max({Tri.v0.z(), Tri.v1.z(), Tri.v2.z()}));

    return AABB(min, max);
}

/**
 * Calculates the barycentric coordinates of a point P relative to triangle Tri
 * @param The triangle the barycentric coordinates relate to
 * @param The point whose coordinates you wish to find, should be on the same plane as the triangle Tri
 * @return The Barycentric coordinates of point P in relation to Triangle Tri
 */
TriangleHelpers::Barycentric TriangleHelpers::CalculateBarycentric(const Triangle &Tri, const QVector3D &P)
{
    QVector3D AB = Tri.v1 - Tri.v0;
    QVector3D AC = Tri.v2 - Tri.v0;
    QVector3D AP = P - Tri.v0;

    float d20 = QVector3D::dotProduct(AP, AB);
    float d21 = QVector3D::dotProduct(AP, AC);
    float denomMult = 1 / Tri.denom;

    float v = (Tri.d11 * d20 - Tri.d01 * d21) * denomMult;
    float w = (Tri.d00 * d21 - Tri.d01 * d20) * denomMult;
    float u = 1.0f - v - w;

    return Barycentric(u, v, w);
}

QVector3D TriangleHelpers::ClosestPoint(const Triangle &Tri, const QVector3D &point)
{
    QVector3D P = ProjectPointOnPlane(Tri, point);
    Barycentric bary = CalculateBarycentric(Tri, point);
    float v = bary.v, w = bary.w, u = bary.u;

    // If the projected point is inside the triangle then it will be the closes point
    if (v >= 0 && w >= 0 && u >= 0) return P;
    // The closest point lies on AC
    if (v < 0) return ProjectPointOnEdge(P, Tri.v0, Tri.v2);
    // The closest point lies on AB
    if (w < 0) return ProjectPointOnEdge(P, Tri.v0, Tri.v1);
    // The closest point lies on BC
    return ProjectPointOnEdge(P, Tri.v1, Tri.v2);
}

QVector3D TriangleHelpers::ProjectPointOnEdge(const QVector3D &P, const QVector3D &A, const QVector3D &B)
{
    QVector3D AB = B - A;
    QVector3D AP = P - A;

    float t = std::max(0.0f, std::min(1.0f, QVector3D::dotProduct(AP, AB) / AB.lengthSquared()));
    return A + AB * t;
}

void TriangleHelpers::ComputeVariables(Triangle &Tri)
{
    QVector3D AB = Tri.v1 - Tri.v0;
    QVector3D AC = Tri.v2 - Tri.v0;
    Tri.normal = QVector3D::crossProduct(AB, AC).normalized();

    Tri.d00 = QVector3D::dotProduct(AB, AB);
    Tri.d01 = QVector3D::dotProduct(AB, AC);
    Tri.d11 = QVector3D::dotProduct(AC, AC);
    Tri.denom = Tri.d00 * Tri.d11 - Tri.d01 * Tri.d01;
}

bool TriangleHelpers::PointInTriangle(const Triangle &Tri, const QVector3D &P)
{
    Barycentric bary = CalculateBarycentric(Tri, P);
    return bary.isInside();
}

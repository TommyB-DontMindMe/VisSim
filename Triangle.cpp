#include "Triangle.h"
#include <QVector3D>
#include <QVector4D>
#include "Vertex.h"

Triangle::Triangle(Vertex &A, Vertex &B, Vertex &C) : v0(&A), v1(&B), v2(&C)
{
    Update();
}

QVector3D Triangle::VectorA() const { return v1->pos() - v0->pos(); }
QVector3D Triangle::VectorB() const { return v2->pos() - v0->pos(); }

float Triangle::Distance(const QVector3D &P) const
{
    return P.distanceToPoint(ClosestPoint(P));
}

QVector3D Triangle::ProjectPointOnPlane(const QVector3D &P) const
{
    QVector3D AP = P - v0->pos();
    float t = QVector3D::dotProduct(AP, mNormal) / mNormal.lengthSquared();
    return P - (mNormal * t);
}

QVector3D Triangle::ProjectPointOnEdge(const QVector3D &P, const QVector3D &A, const QVector3D &B) const
{
    QVector3D AB = B - A;
    QVector3D AP = P - A;

    float t = std::max(0.0f, std::min(1.0f, QVector3D::dotProduct(AP, AB) / AB.lengthSquared()));
    return A + AB * t;
}

QVector3D Triangle::ClosestPoint(const QVector3D &point) const
{
    QVector3D P = ProjectPointOnPlane(point);
    QVector3D AB = VectorA(), AC = VectorB(), AP = P - v0->pos();

    // from Real Time Collision Detection (Ericson, 2005, p.47-48)
    float d00 = QVector3D::dotProduct(AB, AB);  //
    float d01 = QVector3D::dotProduct(AB, AC);  // These could be calculated in the update function and cached for this function
    float d11 = QVector3D::dotProduct(AC, AC);  //
    float d20 = QVector3D::dotProduct(AP, AB);
    float d21 = QVector3D::dotProduct(AP, AC);
    float denom = d00 * d11 - d01 * d01;        // This one too
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    // If the projected point is inside the triangle then it will be the closes point
    if (v >= 0 && w >= 0 && u >= 0) return P;
    // The closest point lies on AC
    if (v < 0) return ProjectPointOnEdge(P, v0->pos(), v2->pos());
    // The closest point lies on AB
    if (w < 0) return ProjectPointOnEdge(P, v0->pos(), v1->pos());
    // The closest point lies on BC
    return ProjectPointOnEdge(P, v1->pos(), v2->pos());

}

QVector3D Triangle::CalculateNormal() const
{
    return QVector3D::crossProduct(VectorA(), VectorB());
}


void Triangle::Update()
{
    mNormal = CalculateNormal();
}

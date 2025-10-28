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

float Triangle::Distance(QVector3D point) const
{
    return QVector3D::dotProduct(mPlaneEquation.toVector3D(), point) + mPlaneEquation.w();
}

QVector3D Triangle::CalculateNormal() const
{
    return QVector3D::crossProduct(VectorA(), VectorB()).normalized();
}

QVector4D Triangle::CalculatePlaneEquation() const
{
    float x = mNormal.x();
    float y = mNormal.y();
    float z = mNormal.z();
    float w = -x * v0->pos().x() - y * v0->y - z * v0->z;
    return QVector4D(x, y, z, w);
}


void Triangle::Update()
{
    mNormal = CalculateNormal();
    mPlaneEquation = CalculatePlaneEquation();
}

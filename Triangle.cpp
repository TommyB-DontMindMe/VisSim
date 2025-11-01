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

QVector3D Triangle::ClosestPoint(const QVector3D &point) const
{
    QVector3D AB = VectorA();
    QVector3D AC = VectorB();
    QVector3D AP = point - v0->pos();

    float a = QVector3D::dotProduct(AB, AB);
    float b = QVector3D::dotProduct(AB, AC);
    float c = QVector3D::dotProduct(AC, AC);
    float d = QVector3D::dotProduct(AB, AP);
    float e = QVector3D::dotProduct(AC, AP);

    float det = a * c - b * b;
    float s = b * e - c * d;
    float t = b * d - a * e;

    if (s + t <= det) {
        if (s < 0.0f) {
            if (t < 0.0f){
                s = std::clamp(-d / a, 0.0f, 1.0f);
                t = 0.0f;
            } else {
                s = 0.0f;
                t = std::clamp(e / c, 0.0f, 1.0f);
            }
        } else if (t < 0.0f) {
            s = std::clamp(d / a, 0.0f, 1.0f);
            t = 0.0f;
        } else {
            float invDet = 1.0f / det;
            s *= invDet;
            t *= invDet;
        }
    } else {
        if (s < 0.0f) {
            s = 0.0f;
            t = 1.0f;
        } else if (t < 0.0f) {
            s = 1.0f;
            t = 0.0f;
        } else {
            float numer = c + e - b - d;
            if (numer <= 0.0f) {
                s = 0.0f;
            } else {
                float denom = a - 2.0f * b + c;
                s = (numer >= denom ? 1.0f : numer / denom);
            }
            t = 1.0f - s;
        }
    }
    return v0->pos() + s * AB + t * AC;
}

QVector3D Triangle::CalculateNormal() const
{
    return QVector3D::crossProduct(VectorA(), VectorB());
}

QVector4D Triangle::CalculatePlaneEquation() const
{
    QVector3D normalizedNormal = mNormal.normalized();
    float x = normalizedNormal.x();
    float y = normalizedNormal.y();
    float z = normalizedNormal.z();
    float w = -x * v0->pos().x() - y * v0->y - z * v0->z;
    return QVector4D(x, y, z, w);
}


void Triangle::Update()
{
    mNormal = CalculateNormal();
    mPlaneEquation = CalculatePlaneEquation();
}

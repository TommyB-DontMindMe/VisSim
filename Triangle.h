#ifndef TRIANGLE_H
#define TRIANGLE_H
#include <qvectornd.h>


class Vertex;

class Triangle {
public:
    Triangle(Vertex& A, Vertex& B, Vertex& C);
    QVector3D VectorA() const;
    QVector3D VectorB() const;

    float Distance(QVector3D point) const;
    QVector3D SurfaceNormal() const { return mNormal; };
    QVector3D ClosestPoint(const QVector3D& point) const;
    void Update();

    //std::weak_ptr<Triangle> n0, n1, n2; // Neighbors opposite each Vertex

private:
    Vertex *v0, *v1, *v2;   // Vertices

    QVector3D mNormal;
    QVector3D CalculateNormal() const;
    QVector4D mPlaneEquation;
    QVector4D CalculatePlaneEquation() const;
};

#endif // TRIANGLE_H

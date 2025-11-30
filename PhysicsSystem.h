#ifndef PHYSICSSYSTEM_H
#define PHYSICSSYSTEM_H

#include <QVector3D>
#include "vector"
class Octree;
class Triangle;
class Sphere;
class VisualObject;

class PhysicsSystem
{
public:
    PhysicsSystem();

    QVector3D mGravity{0.0, -9.81, 0.0};
    std::vector<Sphere> mSpheres;
    std::vector<Triangle> mTriangles;
    Octree* mWorldSpace;

    VisualObject* mSphereModel;


    void Update(float deltaTime);
};

namespace SweepOperations
{

struct Collision
{
    bool hit = false;
    float t = 1.0;
    QVector3D contactPoint;
    QVector3D contactNormal;
    int triangleIndex = -1;
};

Collision SweepSpherePlane(const QVector3D& sPosition, const QVector3D& sVelocity, float sRadius, const Triangle& tri);
Collision SweepSphereEdge(const QVector3D& sPosition, const QVector3D& sVelocity, float sRadius, const QVector3D& eVertexA, const QVector3D& eVertexB);
Collision SweepSpherePoint(const QVector3D& sPosition, const QVector3D& sVelocity, float sRadius, const QVector3D& point);
Collision SweepSphereTriangle(const QVector3D& sPosition, const QVector3D& sVelocity, float sRadius, const Triangle& tri, int triangleIndex);

} // namespace SweepOperations

#endif // PHYSICSSYSTEM_H

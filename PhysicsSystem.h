#ifndef PHYSICSSYSTEM_H
#define PHYSICSSYSTEM_H

#include <QVector3D>
#include "vector"
class Octree;
class Triangle;
class Sphere;

class PhysicsSystem
{
public:
    PhysicsSystem();

    QVector3D mGravity{0.0, -9.81, 0.0};
    std::vector<Sphere> mSpheres;
    std::vector<Triangle> mTriangles;
    Octree* mWorldSpace;


    void Update(float deltaTime);
};

#endif // PHYSICSSYSTEM_H

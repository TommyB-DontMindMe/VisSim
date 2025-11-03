#ifndef SPHERE_H
#define SPHERE_H
#include "PhysicsObject.h"
#include "VisualObject.h"
class Triangle;

class Sphere : public PhysicsObject
{
public:
    Sphere(VisualObject* v);
    VisualObject* mVisual;

    float mRadius{0.15};
    virtual void Update(float deltaTime) override;
    virtual void Update(float deltaTime, const std::vector<Triangle>& triangles);

    virtual void HandleCollisions(QVector3D& newPosition, float deltaTime, const std::vector<Triangle>& triangles);
};

#endif // SPHERE_H

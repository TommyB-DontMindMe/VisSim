#ifndef SPHERE_H
#define SPHERE_H
#include "PhysicsObject.h"
#include "VisualObject.h"

class Sphere : public PhysicsObject
{
public:
    Sphere(VisualObject* v);
    VisualObject* mVisual;

    float mRadius{0.0};
    virtual void Update(float deltaTime) override;
};

#endif // SPHERE_H

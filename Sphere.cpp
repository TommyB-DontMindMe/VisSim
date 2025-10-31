#include "Sphere.h"

Sphere::Sphere(VisualObject *v) : mVisual(v) {}

void Sphere::Update(float deltaTime)
{
    if (mGravity) mAcceleration += *mGravity * deltaTime;
    mVelocity += mAcceleration * deltaTime;
    mPosition += mVelocity * deltaTime;

    if (mVisual) mVisual->setPosition(mPosition);
}

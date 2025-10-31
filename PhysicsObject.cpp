#include "PhysicsObject.h"

PhysicsObject::PhysicsObject() {}

void PhysicsObject::Update(float deltaTime)
{
    if (mGravity) mAcceleration += *mGravity;
    mVelocity += mAcceleration;
    mPosition += mVelocity;
}

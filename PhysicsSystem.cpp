#include "PhysicsSystem.h"
#include "Octree.h"
#include "Sphere.h"
#include "Triangle.h"

PhysicsSystem::PhysicsSystem() {}

void PhysicsSystem::Update(float deltaTime)
{
    for (Sphere s : mSpheres) {
        s.mAcceleration += mGravity * deltaTime;
        s.mPosition += s.mAcceleration * deltaTime;
    }

    // For each Sphere use the Octree to find all triangles it can colide with
    // Calculate how far until collision, update position and velocity, repeat until vector is spent
    // Update the position of sphere renders
}

#include "Sphere.h"
#include "Triangle.h"

Sphere::Sphere(VisualObject *v) : mVisual(v) {}

void Sphere::Update(float deltaTime)
{
    if (mGravity) mAcceleration += *mGravity * deltaTime;
    mVelocity += mAcceleration * deltaTime;
    mPosition += mVelocity * deltaTime;

    if (mVisual) mVisual->setPosition(mPosition);
}

void Sphere::Update(float deltaTime, const std::vector<Triangle> &triangles)
{
    if (mGravity) mAcceleration = *mGravity;
    mVelocity += mAcceleration * deltaTime;
    QVector3D newPosition = mPosition + mVelocity * deltaTime;

    HandleCollisions(newPosition, deltaTime, triangles);

    if (mVisual) mVisual->setPosition(mPosition);
}

void Sphere::HandleCollisions(QVector3D& newPosition, float deltaTime, const std::vector<Triangle>& triangles)
{
    for (const Triangle& tri : triangles) {
        QVector3D closest = tri.ClosestPoint(mPosition);
        QVector3D toSphere = newPosition - closest;
        float distance = toSphere.length();

        if (distance < mRadius) {
            QVector3D normal = toSphere.normalized();
            float penetration = mRadius - distance;

            newPosition += normal * penetration;


            float vn = QVector3D::dotProduct(mVelocity, normal);
            QVector3D velocityNormal = normal * vn;
            QVector3D velocityTangent = mVelocity - velocityNormal;
            mVelocity = velocityNormal + velocityTangent;
        }
    }
    mPosition = newPosition;
}

#ifndef SPHERE_H
#define SPHERE_H

#include <QVector3D>

struct Sphere
{
    Sphere(const QVector3D& position, const QVector3D& velocity, float radius = 0.15) : mPosition(position), mVelocity(velocity), mRadius(radius) {}

    QVector3D mPosition;
    QVector3D mVelocity;
    float mRadius{0.15};
};

#endif // SPHERE_H

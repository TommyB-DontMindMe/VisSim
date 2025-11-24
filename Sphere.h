#ifndef SPHERE_H
#define SPHERE_H

#include <QVector3D>

struct Sphere
{
    int mId;

    QVector3D mPosition;
    QVector3D mVelocity;
    QVector3D mAcceleration;
    float mRadius{0.15};
};

#endif // SPHERE_H

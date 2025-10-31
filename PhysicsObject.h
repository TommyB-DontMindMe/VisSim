#ifndef PHYSICSOBJECT_H
#define PHYSICSOBJECT_H

#include <qvectornd.h>
class PhysicsObject
{
public:
    PhysicsObject();

    QVector3D* mGravity;

    QVector3D mPosition;
    QVector3D mVelocity;
    QVector3D mAcceleration;

    virtual void Update(float deltaTime);
};

#endif // PHYSICSOBJECT_H

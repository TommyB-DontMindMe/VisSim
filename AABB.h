#ifndef AABB_H
#define AABB_H

#include <QVector3D>
class Sphere;

class AABB
{
public:
    AABB() : mMin(0,0,0), mMax(0,0,0) {}
    AABB(const QVector3D& iMin, const QVector3D& iMax) : mMin(iMin), mMax(iMax) {}

    QVector3D mMin;
    QVector3D mMax;


    QVector3D center() const { return (mMin + mMax) * 0.5; }
    QVector3D size() const { return mMax - mMin; }

    bool containsPoint(const QVector3D& point) const;
    bool intersectsSphere(const Sphere& sphere) const;
    bool intersectsAABB(const AABB& boundingBox) const;
};

#endif // AABB_H

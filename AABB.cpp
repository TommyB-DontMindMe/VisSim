#include "AABB.h"
#include "Sphere.h"


bool AABB::containsPoint(const QVector3D &point)
{
    return (point.x() >= mMin.x() && point.x() <= mMax.x() &&
            point.y() >= mMin.y() && point.y() <= mMax.y() &&
            point.z() >= mMin.z() && point.z() <= mMax.z());
}

bool AABB::intersectsSphere(const Sphere &sphere)
{
    float distanceSquared = 0.0f;

    for (int i = 0; i < 3; ++i)
    {
        float positionComponent = sphere.mPosition[i];
        float boundsMin = mMin[i];
        float boundsMax = mMax[i];

        if (positionComponent < boundsMin) distanceSquared += (boundsMin - positionComponent) * (boundsMin - positionComponent);
        if (positionComponent > boundsMax) distanceSquared += (positionComponent - boundsMax) * (positionComponent - boundsMax);
    }

    return distanceSquared <= sphere.mRadius * sphere.mRadius;
}

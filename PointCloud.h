#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "VisualObject.h"
class PointCloud : public VisualObject
{
public:
    PointCloud(const std::string& filename, const QVector3D& min, const QVector3D& max);
};

#endif // POINTCLOUD_H

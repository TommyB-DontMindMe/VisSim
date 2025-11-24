#include "PointCloud.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>


PointCloud::PointCloud(const std::string &filename, const QVector3D &min, const QVector3D &max)
{
    drawType = 1;
    // Open file

    std::ifstream fileIn;
    fileIn.open(filename, std::ifstream::in);
    if (!fileIn) {
        qDebug() << "ERROR: Could not open file for reading: " << filename.c_str();
        return;
    }

    //Text variables
    std::string oneLine{};
    std::string oneWord{};

    std::vector<QVector3D> tempPoints;

    float minX, minY, minZ;
    minX = minY = minZ = std::numeric_limits<float>::infinity();
    float maxX, maxY, maxZ;
    maxX = maxY = maxZ = -std::numeric_limits<float>::infinity();

    {
        std::string arraySize{};
        std::getline(fileIn, arraySize);

        qDebug() << "Reading a pointcloud with " << arraySize << " points.";
    }
    // For Each line read the coordinates and store as a vector3D
    while (std::getline(fileIn, oneLine)) {
        std::stringstream sStream;
        //Pushing line into stream
        sStream << oneLine;
        //Streaming one word out of line
        oneWord = ""; //resetting the value or else the last value might survive!
        sStream >> oneWord;

        QVector3D tempVertex;
        sStream >> oneWord;
        tempVertex.setX(std::stof(oneWord));
        sStream >> oneWord;
        tempVertex.setY(std::stof(oneWord));
        sStream >> oneWord;
        tempVertex.setZ(std::stof(oneWord));

        // Update extremes
        minX = std::min(tempVertex.x(), minX);
        minY = std::min(tempVertex.y(), minY);
        minZ = std::min(tempVertex.z(), minZ);
        maxX = std::max(tempVertex.x(), maxX);
        maxY = std::max(tempVertex.y(), maxY);
        maxZ = std::max(tempVertex.z(), maxZ);

        tempPoints.push_back(tempVertex);
        continue;
    }

    // Determine the expanse of each dimension
    QVector3D spanMin(minX, minY, minZ);
    QVector3D spanMax(maxX, maxY, maxZ);
    float factorX = std::abs(max.x() - min.x()) / std::abs(maxX - minX);
    float factorY = std::abs(max.y() - min.y()) / std::abs(maxY - minY);
    float factorZ = std::abs(max.z() - min.z()) / std::abs(maxZ - minZ);

    for (QVector3D p : tempPoints) {
        QVector3D relP = p - spanMin;

        Vertex adjustedP(relP.x() * factorX, relP.y() * factorY, relP.z() * factorZ, factorX, factorY, factorZ, factorX, factorZ);
        mVertices.push_back(adjustedP);
    }
}

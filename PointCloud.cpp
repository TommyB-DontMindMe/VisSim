#include "PointCloud.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>


PointCloud::PointCloud(const std::string &filename, const QVector3D &min, const QVector3D &max)
{
    QVector3D targetSpan = max - min;
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
    QVector3D currentSpan = spanMax - spanMin;

    QVector3D factor = targetSpan / currentSpan;

    for (const QVector3D& p : tempPoints) {
        QVector3D relP = p - spanMin;

        Vertex adjustedP(relP * factor, QVector3D(0, 0, 0), QVector2D(factor.x(), factor.z()));
        mVertices.push_back(adjustedP);
    }
    // Create a super triangle that encompasses all points
    // Point A = min, b = A.x() + targetSpan.x() * 2, c = A.z() + targetSpan.z() * 2
    QVector2D superA(min.x(), min.z());
    QVector2D superB(min.x() + 2 * targetSpan.x(), min.z());
    QVector2D superC(min.x(), min.z() + 2 * targetSpan.z());
    DelaunayTriangle super(superA, superB, superC);

    // Add the point, if it lies within a triangle: remove that triangle and form new triangles between the point and the former triangle vertices

    // Remove all triangles that share a point with the super triangle

    // For every remaining triangle insert indices, making sure they go counter-clockwise seen from above to ensure proper face normals

    // I'll be using Vertex rgb to store normals
    // For every vertex in each triangle rgb == 0 ? rgb = triangle.normal : rgb = (rgb + triangle.normal) / 2;
}

// Based on https://github.com/delfrrr/delaunator-cpp
QVector2D PointCloud::Circumcenter(const QVector2D &A, const QVector2D &B, const QVector2D &C)
{
    const QVector2D D{B - A};
    const QVector2D E{C - A};

    const double b1 = QVector2D::dotProduct(D, D);
    const double c1 = QVector2D::dotProduct(E, E);
    const double d = 2 * (D.x() * E.y() - D.y() * E.x()); // QVector2D has no crossProduct function

    if (std::abs(d) < 1e-9) return (A + B + C) / 3; // Points are collinear, return the average position

    const QVector2D num(E.y() * b1 - D.y() * c1, D.x() * c1 - E.x() * b1);
    return A + num / d;
}

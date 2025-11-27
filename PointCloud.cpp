#include "PointCloud.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>


PointCloud::PointCloud(const std::string &filename, const QVector3D &min, const QVector3D &max)
{

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

    QVector3D targetSpan = max - min;
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
    const QVector2D superA(min.x(), min.z());
    const QVector2D superB(min.x(), min.z() + 2 * targetSpan.z());
    const QVector2D superC(min.x() + 2 * targetSpan.x(), min.z());
    Delaunay::DelaunayTriangle superTri(-2, -1, -3); // I need to iterate over this as well, but I'd like to keep its indices distinct so it'll be easier to remove the "scaffolding" later
    superTri.circumCenter = Delaunay::Circumcenter(superA, superB, superB);
    superTri.circumRadius = superA.distanceToPoint(superTri.circumCenter);
    auto superIndex = [&](int index) -> QVector2D
    {
        switch (index) {
        case -1:
            return superA;
        case -2:
            return superB;
        case -3:
            return superC;
        default:
            if (index >= 0) return mVertices[index].poXZ();
            throw std::out_of_range("");
        }
    };

    std::vector<Delaunay::DelaunayTriangle> tempTris{superTri};

    // Add the point, if it lies within a triangle: remove that triangle and form new triangles between the point and the former triangle vertices
    for (int i = 0; i < mVertices.size(); ++i)
    {
        QVector2D point(mVertices[i].poXZ());
        for (auto tri = tempTris.begin(); tri != tempTris.end();)
        {
            if (point.distanceToPoint(tri->circumCenter) <= tri->circumRadius)
            {
                // Create new triangles
                Delaunay::DelaunayTriangle newA(tri->v1, tri->v0, i);
                newA.circumCenter = Delaunay::Circumcenter(superIndex(newA.v0), superIndex(newA.v1), superIndex(newA.v2));
                newA.circumRadius = superIndex(newA.v0).distanceToPoint(newA.circumCenter);

                Delaunay::DelaunayTriangle newB(tri->v0, tri->v2, i);
                newB.circumCenter = Delaunay::Circumcenter(superIndex(newB.v0), superIndex(newB.v1), superIndex(newB.v2));
                newB.circumRadius = superIndex(newB.v0).distanceToPoint(newB.circumCenter);

                Delaunay::DelaunayTriangle newC(tri->v2, tri->v1, i);
                newC.circumCenter = Delaunay::Circumcenter(superIndex(newC.v0), superIndex(newC.v1), superIndex(newC.v2));
                newC.circumRadius = superIndex(newC.v0).distanceToPoint(newC.circumCenter);

                // Remove the invalid triangle and insert the new ones
                tri = tempTris.erase(tri);

                tri = tempTris.insert(tri, newA);
                tri = tempTris.insert(tri, newB);
                tri = tempTris.insert(tri, newC);

                // The new triangles should be fine in regards to this point so we can skip them here
                tri += 3;
            }
            else ++tri;
        }
    }

    // Remove all triangles that share a point with the super triangle
    for (auto tri = tempTris.begin(); tri != tempTris.end();)
    {
        if (tri->v0 < 0 || tri->v1 < 0 || tri->v2 < 0)
            tri = tempTris.erase(tri);
        else
        {
            mIndices.push_back(tri->v0);
            mIndices.push_back(tri->v1);
            mIndices.push_back(tri->v2);
            ++tri;
        }
    }

    // I'll be using Vertex rgb to store normals
    // For every vertex in each triangle rgb == 0 ? rgb = triangle.normal : rgb = (rgb + triangle.normal) / 2;
}

// Based on https://github.com/delfrrr/delaunator-cpp
QVector2D Delaunay::Circumcenter(const QVector2D &A, const QVector2D &B, const QVector2D &C)
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

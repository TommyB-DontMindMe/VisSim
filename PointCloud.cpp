#include "PointCloud.h"
#include "Triangle.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>


PointCloud::PointCloud(const std::string &filename, const QVector3D &min, const QVector3D &max, std::vector<Triangle>& oTriangles)
{
    drawType = 2;
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
        //sStream >> oneWord;

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
    Delaunay::Triangle superTri(-2, -1, -3); // I need to iterate over this as well, but I'd like to keep its indices distinct so it'll be easier to remove the "scaffolding" later
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

    std::vector<Delaunay::Triangle> tempTris{superTri};

    // Add the point
    for (int i = 0; i < mVertices.size(); ++i)
    {
        QVector2D point(mVertices[i].poXZ());
        std::vector<Delaunay::Triangle> invalidatedTris;
        std::vector<Delaunay::Edge> polygonEdges;

        // If the point lies within the circumcircle of a triangle remove the triangle
        for (const Delaunay::Triangle& tri : tempTris)
        {
            if (point.distanceToPoint(tri.circumCenter) <= tri.circumRadius)
                invalidatedTris.push_back(tri);
        }
        // Find the edges of the abscense left by the triangles removed
        for (const Delaunay::Triangle& tri : invalidatedTris)
        {
            std::vector<Delaunay::Edge> triEdges = {
                Delaunay::Edge(tri.v0, tri.v1),
                Delaunay::Edge(tri.v1, tri.v2),
                Delaunay::Edge(tri.v2, tri.v0)
            };
            // We can ignore all edges between removed triangles
            for (const Delaunay::Edge& edge : triEdges)
            {
                bool isShared{ false };
                for (const Delaunay::Triangle& otherTri : invalidatedTris)
                {
                    if (&tri == &otherTri) continue;
                    if ((otherTri.v0 == edge.v1 && otherTri.v1 == edge.v0) ||
                        (otherTri.v1 == edge.v1 && otherTri.v2 == edge.v0) ||
                        (otherTri.v2 == edge.v1 && otherTri.v0 == edge.v0))
                    {
                        isShared = true;
                        break;
                    }
                }

                if (!isShared)
                    polygonEdges.push_back(edge);
            }
        }

        // Remove all invalidated Triangles
        for (auto tri = tempTris.begin(); tri != tempTris.end();)
        {
            bool isInvalid{ false };
            for (const Delaunay::Triangle& badTri : invalidatedTris)
            {
                if (*tri == badTri)
                {
                    isInvalid = true;
                    break;
                }
            }

            if (isInvalid)
                tri = tempTris.erase(tri);
            else
                ++tri;
        }

        // Create new triangles from the exposed edges
        for (const Delaunay::Edge& edge : polygonEdges)
        {
            Delaunay::Triangle newTri(edge.v0, edge.v1, i);
            newTri.circumCenter = Delaunay::Circumcenter(superIndex(newTri.v0), superIndex(newTri.v1), superIndex(newTri.v2));
            newTri.circumRadius = superIndex(newTri.v0).distanceToPoint(newTri.circumCenter);
            tempTris.push_back(newTri);
        }
    }

    // Store the indices of all remaining triangles, ignore those who are connected to the superTriangle
    for (const Delaunay::Triangle& tri : tempTris)
    {
        if (tri.v0 < 0 || tri.v1 < 0 || tri.v2 < 0) continue;

        mIndices.push_back(tri.v0);
        mIndices.push_back(tri.v1);
        mIndices.push_back(tri.v2);

        Vertex &vertex0 = mVertices[tri.v0], &vertex1 = mVertices[tri.v1], &vertex2 = mVertices[tri.v2];

        Triangle newTri(vertex0.pos(), vertex1.pos(), vertex2.pos());
        oTriangles.push_back(newTri);

        vertex0.r += newTri.normal.x();
        vertex0.g += newTri.normal.y();
        vertex0.b += newTri.normal.z();

        vertex1.r += newTri.normal.x();
        vertex1.g += newTri.normal.y();
        vertex1.b += newTri.normal.z();

        vertex2.r += newTri.normal.x();
        vertex2.g += newTri.normal.y();
        vertex2.b += newTri.normal.z();
    }
    for (Vertex& v : mVertices)
    {
        QVector3D normal(v.r, v.g, v.b);
        normal.normalize();
        v.r = normal.x();
        v.g = normal.y();
        v.b = normal.z();
    }
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

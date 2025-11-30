#include "PhysicsSystem.h"
#include "Octree.h"
#include "Sphere.h"
#include "Triangle.h"

PhysicsSystem::PhysicsSystem() {}

void PhysicsSystem::Update(float deltaTime)
{
    for (Sphere& s : mSpheres)
    {
        // I can expand on this later to account for other forces acting on a sphere.
        QVector3D acceleration = mGravity;
        s.mVelocity += acceleration * deltaTime;

        QVector3D targetPosition = s.mPosition + s.mVelocity * deltaTime;

        // For each Sphere use the Octree to find all triangles it can colide with
        std::vector<int> trisInReach;

        Sphere searchSphere = s;
        searchSphere.mRadius += s.mVelocity.length() * deltaTime; // This creates a sphere that covers all places the original sphere could occupy
        mWorldSpace->query(searchSphere, trisInReach);

        // Find the first collision along the current path
        SweepOperations::Collision earliest;

        for (int triIndex : trisInReach)
        {
            SweepOperations::Collision result = SweepOperations::SweepSphereTriangle(s.mPosition, s.mVelocity, s.mRadius, mTriangles.at(triIndex), triIndex);
            if (result.hit && result.t < earliest.t) earliest = result;
        }

        if (earliest.hit)
        {
            s.mPosition += s.mVelocity * deltaTime * earliest.t;
            float normalVelocity = QVector3D::dotProduct(s.mVelocity, earliest.contactNormal);
            s.mVelocity -= earliest.contactNormal * normalVelocity;

            // s.mVelocity *= friction;

            QVector3D remainingVelocity = s.mVelocity * (1.0 - earliest.t) * deltaTime;
            QVector3D tangent = remainingVelocity - earliest.contactNormal * QVector3D::dotProduct(remainingVelocity, earliest.contactNormal);
            s.mPosition += tangent;
        }
        else
            s.mPosition = targetPosition;
    }
}

SweepOperations::Collision SweepOperations::SweepSpherePlane(const QVector3D &sPosition, const QVector3D &sVelocity, float sRadius, const Triangle &tri)
{
    SweepOperations::Collision result;

    float distance0 = QVector3D::dotProduct(tri.normal, sPosition - tri.v0);
    float distance1 = QVector3D::dotProduct(tri.normal, sPosition + sVelocity - tri.v0);

    // If the sphere is in front of the plane but moving away. No collision
    if (distance0 > 0 && distance1 > distance0) return result;

    float signedRadius = (distance0 > 0) ? sRadius : -sRadius; // Radius in the direction of the plane
    float adjustedDistance0 = distance0 - signedRadius; // Distance from plane to sphere surface at t0
    float adjustedDistance1 = distance1 - signedRadius; // Distance from plane to sphere surface at t1

    // If the closest closest point on the sphere never crosses the surface of the plane. No collision
    if (adjustedDistance0 * adjustedDistance1 > 0) return result;


    // The sphere impacts the plane when adjustedDistance0 + t * (adjustedDistance0 - adjustedDistance1) = 0
    float denom = adjustedDistance0 - adjustedDistance1;
    // If there is negligible change in the distance between the sphere and the plane from beginning to end, then the sphere is moving parallel to the plane. No collision
    if (std::abs(denom) < 1e-6f) return result;

    float t = adjustedDistance0 / denom;
    if (t < 0 || t > 1) return result;


    result.hit = true;
    result.t = t;
    result.contactPoint = (sPosition + sVelocity * t) - tri.normal * signedRadius;
    result.contactNormal = (distance0 > 0) ? tri.normal : -tri.normal;

    return result;
}

SweepOperations::Collision SweepOperations::SweepSphereEdge(const QVector3D &sPosition, const QVector3D &sVelocity, float sRadius, const QVector3D &eVertexA, const QVector3D &eVertexB)
{
    SweepOperations::Collision result;
    result.t = 2.0;

    QVector3D edge = eVertexB - eVertexA;
    QVector3D AS = sPosition - eVertexA;

    float edgeSq = edge.lengthSquared();
    float edgeDotVelocity = QVector3D::dotProduct(edge, sVelocity);
    float edgeDotPosition = QVector3D::dotProduct(edge, AS);

    float a = edgeSq * sVelocity.lengthSquared() - edgeDotVelocity * edgeDotVelocity;
    float b = edgeSq * 2.0 * QVector3D::dotProduct(sVelocity, AS) - 2.0 * edgeDotVelocity * edgeDotPosition;
    float c = edgeSq * (AS.lengthSquared() - sRadius * sRadius) - edgeDotPosition * edgeDotPosition;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0 || std::abs(a) < 1e-6f) return result;

    float t = (-b - std::sqrt(discriminant) / 2 * a);
    if (t < 0 || t > 1) return result; // Collision does not occur in current timeframe. No collision

    QVector3D tPosition = sPosition + sVelocity * t;
    // Check if point of collision occurs on the edge and not elswhere on the line
    float edgeParam = QVector3D::dotProduct(edge, tPosition - eVertexA) / edgeSq;
    if (edgeParam < 0 || edgeParam > 1) return result;

    QVector3D closestPoint = eVertexA + edge * t;

    result.hit = true;
    result.t = t;
    result.contactPoint = closestPoint;
    result.contactNormal = (tPosition - closestPoint).normalized();

    return result;
}

SweepOperations::Collision SweepOperations::SweepSpherePoint(const QVector3D &sPosition, const QVector3D &sVelocity, float sRadius, const QVector3D &point)
{
    SweepOperations::Collision result;
    result.t = 2.0;

    QVector3D distance0 = sPosition - point;

    float a = sVelocity.lengthSquared();
    float b = 2.0 * QVector3D::dotProduct(sVelocity, distance0);
    float c = distance0.lengthSquared() - sRadius * sRadius;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0 || std::abs(a) < 1e-6f) return result;

    float t = (-b - std::sqrt(discriminant) / (2 * a));
    if (t < 0 || t > 1) return result; // Collision does not occur in current timeframe. No collision

    QVector3D tPosition = sPosition + sVelocity * t;

    result.hit = true;
    result.t = t;
    result.contactPoint = tPosition;
    result.contactNormal = (tPosition - point).normalized();

    return result;
}

SweepOperations::Collision SweepOperations::SweepSphereTriangle(const QVector3D &sPosition, const QVector3D &sVelocity, float sRadius, const Triangle &tri, int triangleIndex)
{
    SweepOperations::Collision result;
    result.t = 2.0;

    SweepOperations::Collision planeHit = SweepOperations::SweepSpherePlane(sPosition, sVelocity, sRadius, tri);
    if (!planeHit.hit) return result; // If the sphere doesn't hit the triangle plane it cannot hit the triangle

    if(TriangleHelpers::PointInTriangle(tri, planeHit.contactPoint))
    {
        planeHit.triangleIndex = triangleIndex;
        return planeHit;
    }

    // Check edge cases
    SweepOperations::Collision best = result;

    SweepOperations::Collision edge0 = SweepOperations::SweepSphereEdge(sPosition, sVelocity, sRadius, tri.v0, tri.v1);
    SweepOperations::Collision edge1 = SweepOperations::SweepSphereEdge(sPosition, sVelocity, sRadius, tri.v1, tri.v2);
    SweepOperations::Collision edge2 = SweepOperations::SweepSphereEdge(sPosition, sVelocity, sRadius, tri.v2, tri.v0);

    if (edge0.hit && edge0.t < best.t) best = edge0;
    if (edge1.hit && edge1.t < best.t) best = edge1;
    if (edge2.hit && edge2.t < best.t) best = edge2;


    SweepOperations::Collision vert0 = SweepOperations::SweepSpherePoint(sPosition, sVelocity, sRadius, tri.v0);
    SweepOperations::Collision vert1 = SweepOperations::SweepSpherePoint(sPosition, sVelocity, sRadius, tri.v1);
    SweepOperations::Collision vert2 = SweepOperations::SweepSpherePoint(sPosition, sVelocity, sRadius, tri.v2);

    if (vert0.hit && vert0.t < best.t) best = vert0;
    if (vert1.hit && vert1.t < best.t) best = vert1;
    if (vert2.hit && vert2.t < best.t) best = vert2;

    if (best.t <= 1.0)
    {
        best.hit = true;
        best.triangleIndex = triangleIndex;
        return best;
    }

    return result;
}

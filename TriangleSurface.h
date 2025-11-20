#ifndef TRIANGLESURFACE_H
#define TRIANGLESURFACE_H

#include <string>
#include "VisualObject.h"
#include <vector>
#include "Triangle.h"

//Defaults to a quad, but can read a mesh from file
class TriangleSurface : public VisualObject
{
public:
    TriangleSurface();
    TriangleSurface(const std::string& filename, std::vector<Triangle>& oTris);

    bool CreateFromObj(const std::string& filename, std::vector<Triangle>& oTris);

    // std::vector<Triangle> mTris;
};

#endif // TRIANGLESURFACE_H

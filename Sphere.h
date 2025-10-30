#ifndef SPHERE_H
#define SPHERE_H
#include "VisualObject.h"

class Sphere : public VisualObject
{
public:
    Sphere();

    float mRadius{0.0};
};

#endif // SPHERE_H

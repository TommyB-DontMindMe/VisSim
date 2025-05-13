#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include "VisualObject.h"
#include <string>

class HeightMap : public VisualObject
{
public:
    HeightMap();

    void makeTerrain(std::string heightMapImage);

    void makeTerrain(unsigned char* textureData, int width, int height);

private:
	int mWidth{ 0 };
	int mHeight{ 0 };
	int mChannels{ 0 };
};

#endif // HEIGHTMAP_H

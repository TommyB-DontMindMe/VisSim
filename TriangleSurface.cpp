#include "TriangleSurface.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <QDebug>
TriangleSurface::TriangleSurface() : VisualObject()
{
    Vertex v1{ 0.0f,  0.0f,  0.0f,   1.0f, 0.0f, 0.0f,  0.0f, 0.0f};  //  bottom-left corner
	Vertex v2{ 1.0f,  0.0f,  0.0f,   0.0f, 1.0f, 0.0f,  1.0f, 0.0f }; //  bottom-right corner
	Vertex v3{ 0.0f,  1.0f,  0.0f,   0.0f, 0.0f, 1.0f,  0.0f, 1.0f }; //  top-left corner
	Vertex v4{ 1.0f,  1.0f,  0.0f,   1.0f, 1.0f, 0.0f,  1.0f, 1.0f }; //  top-right corner

    //Pushing 1st triangle,
    mVertices.push_back(v1);
    mVertices.push_back(v2);
    mVertices.push_back(v3);
    mVertices.push_back(v4);

	//Indexes for the two triangles to form a quad
	mIndices.push_back(0);
	mIndices.push_back(1);
	mIndices.push_back(2);
	mIndices.push_back(2);
	mIndices.push_back(1);
	mIndices.push_back(3);

    //Temporary scale and positioning
    mMatrix.scale(0.5f);
    mMatrix.translate(0.5f, 0.1f, 0.1f);
}

TriangleSurface::TriangleSurface(const std::string &filename, std::vector<Triangle> &oTris)
{
    CreateFromObj(filename, oTris);
}

bool TriangleSurface::CreateFromObj(const std::string &filename, std::vector<Triangle> &oTris)
{
    qDebug() << "Reading " << filename.c_str();
    std::ifstream fileIn;
    fileIn.open(filename, std::ifstream::in);
    if (!fileIn)
    {
        qDebug() << "ERROR: Could not open file for reading: " << filename.c_str();
        return false;
    }
    //One line at a time-variable
    std::string oneLine{};
    //One word at a time-variable
    std::string oneWord{};

    // temp variables to store the data from the file
    std::vector<QVector3D> tempVertecies;
    std::vector<QVector3D> tempNormals;
    std::vector<QVector2D> tempUVs;

    // Varible for constructing the indices vector
    // All obj files use indices
    unsigned int temp_index = 0;

    //Reading one line at a time from file to oneLine
    while (std::getline(fileIn, oneLine))
    {
        //Doing a trick to get one word at a time
        std::stringstream sStream;
        //Pushing line into stream
        sStream << oneLine;
        //Streaming one word out of line
        oneWord = ""; //resetting the value or else the last value might survive!
        sStream >> oneWord;

        if (oneWord == "#")
        {
            //Ignore this line
            //qDebug() << "Line is comment "  << QString::fromStdString(oneWord);
            continue;
        }
        if (oneWord == "")
        {
            //Ignore this line
            //qDebug() << "Line is blank ";
            continue;
        }
        if (oneWord == "v")
        {
            //qDebug() << "Line is vertex "  << QString::fromStdString(oneWord) << " ";
            QVector3D tempVertex;
            sStream >> oneWord;
            tempVertex.setX(std::stof(oneWord));
            sStream >> oneWord;
            tempVertex.setY(std::stof(oneWord));
            sStream >> oneWord;
            tempVertex.setZ(std::stof(oneWord));

            //Vertex made - pushing it into vertex-vector
            tempVertecies.push_back(tempVertex);

            continue;   //jump out of the while loop
        }
        if (oneWord == "vt")
        {
            //qDebug() << "Line is UV-coordinate "  << QString::fromStdString(oneWord) << " ";
            QVector2D tempUV;
            sStream >> oneWord;
            tempUV.setX(std::stof(oneWord));
            sStream >> oneWord;
            tempUV.setY(std::stof(oneWord));

            //UV made - pushing it into UV-vector
            tempUVs.push_back(tempUV);

            continue;
        }
        if (oneWord == "vn")
        {
            //qDebug() << "Line is normal "  << QString::fromStdString(oneWord) << " ";
            QVector3D tempNormal;
            sStream >> oneWord;
            tempNormal.setX(std::stof(oneWord));
            sStream >> oneWord;
            tempNormal.setY(std::stof(oneWord));
            sStream >> oneWord;
            tempNormal.setZ(std::stof(oneWord));

            //Vertex made - pushing it into vertex-vector
            tempNormals.push_back(tempNormal);
            continue;
        }
        if (oneWord == "f")
        {
            int index, normal, uv;
            for (int i = 0; i < 3; i++)
            {
                sStream >> oneWord;                             //one word read
                std::stringstream tempWord(oneWord);            //to use getline on this one word
                std::string segment;                            //the numbers in the f-line
                std::vector<std::string> segmentArray;          //temp array of the numbers
                while (std::getline(tempWord, segment, '/'))    //splitting word in segments at /
                {
                    segmentArray.push_back(segment);
                }
                index = std::stoi(segmentArray[0]);             //first is vertex
                if (segmentArray[1] != "")                      //second is uv
                    uv = std::stoi(segmentArray[1]);
                else
                {
                    qDebug() << "No UVs in mesh!!!";            //uv not present
                    uv = 0;                                     //this will become -1 in a couple of lines
                }
                normal = std::stoi(segmentArray[2]);            //third is normal

                //Fixing the indexes because obj f-lines starts with 1, not 0
                --index;
                --uv;
                --normal;

                if (uv > -1)    //uv present!
                {
                    Vertex tempVert(tempVertecies[index], tempNormals[normal], tempUVs[uv]);
                    mVertices.push_back(tempVert);
                }
                else            //no uv in mesh data, use 0, 0 as uv
                {
                    Vertex tempVert(tempVertecies[index], tempNormals[normal], QVector2D(0.0f, 0.0f));
                    mVertices.push_back(tempVert);
                }
                mIndices.push_back(temp_index++);
            }
            oTris.push_back(Triangle(mVertices[temp_index -3], mVertices[temp_index - 2], mVertices[temp_index - 1]));
            continue;
        }
    }
    fileIn.close();

    qDebug() << filename.c_str() << " successfully loaded";

    return true;
}

#pragma once
#include <vector>
#include "SimpleVertex.h"
#include <windows.h>

class Sphere {
public:
    Sphere(float radius, int numTheta, int numPhi, bool outerNorm);
    std::vector<SimpleVertex> vertices;
    std::vector<int> indices;

};
#pragma once
#include <vector>
#include "SimpleVertex.h"
#include <windows.h>

class Sphere {
public:
    Sphere();
    std::vector<SimpleVertex> vertices;
    std::vector<WORD> indices;

private:
    const int numLines = 16;
    const float spacing = 1.0f / numLines;
    const float sphereRadius = 1.0f;
};
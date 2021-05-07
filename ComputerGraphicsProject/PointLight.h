#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class PointLight {
public:

    XMFLOAT4 Pos;
    XMFLOAT4 Color;

    float constAttenuation = 0.1f;
    float linearAttenuation = 1.0f;
    float expAttenuation = 1.0f;

};
#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class PointLight {
public:

    XMFLOAT4 Pos;
    XMFLOAT4 Color;

    float constAttenuation = 0.5f;
    float quadraticAttenuation = 2.0f;

};
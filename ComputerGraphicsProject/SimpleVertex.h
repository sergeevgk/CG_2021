#pragma once
#include <DirectXColors.h>
#include <DirectXMath.h>
using namespace DirectX;

struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
    XMFLOAT3 Normal;
    XMFLOAT3 Tangent;
};
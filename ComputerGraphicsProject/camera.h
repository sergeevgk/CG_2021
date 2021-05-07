#pragma once
#include <windows.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <minwindef.h>
#include "WorldBorders.h"
using namespace DirectX;

class Camera
{
public:
    Camera();

    XMVECTOR Pos, Dir, Up;

    XMMATRIX GetViewMatrix();

    void Move(const XMVECTOR& dv);

    void MoveNormal(float dn);

    void MoveTangent(float dt);

    void RotateHorisontal(float angle);

    void RotateVertical(float angle);

    void PositionClip(const WorldBorders& b);

private:
    XMVECTOR getTangentVector();

    float verticalAngle = 0.0f;
};
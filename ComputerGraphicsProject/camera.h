#pragma once
#include <windows.h>
#include <DirectXColors.h>
#include <minwindef.h>
using namespace DirectX;

class Camera
{
public:
    Camera();

    XMMATRIX GetViewMatrix();

    void Move(float dx = 0.0f, float dy = 0.0f, float dz = 0.0f);

    void MoveNormal(float dn);

    void MoveTangent(float dt);

    void RotateHorisontal(float angle);

    void RotateVertical(float angle);


private:
    XMVECTOR getTangentVector();

    XMVECTOR pos, dir, up;
    float verticalAngle = 0.0f;
};
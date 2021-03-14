#include <windows.h>
#include <DirectXColors.h>
#include <minwindef.h>
#include "Camera.h"
using namespace DirectX;

Camera::Camera()
{
    pos = XMVectorSet(0.0f, 13.0f, -2.5f, 0.0f);
    dir = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
    up = XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f);
}

XMMATRIX Camera::GetViewMatrix()
{
    XMVECTOR focus = XMVectorAdd(pos, dir);
    return XMMatrixLookAtLH(pos, dir, up);
}

void Camera::Move(float dx, float dy, float dz)
{
    pos = XMVectorSet(XMVectorGetX(pos) + dx, XMVectorGetY(pos) + dy, XMVectorGetZ(pos) + dz, 0.0f);
    dir = XMVectorSet(XMVectorGetX(dir) + dx, XMVectorGetY(dir) + dy, XMVectorGetZ(dir) + dz, 0.0f);
}

void Camera::MoveNormal(float dn)
{
    pos = XMVectorAdd(pos, XMVectorScale(dir, dn));
}

void Camera::MoveTangent(float dt)
{
    XMVECTOR tangent = getTangentVector();
    pos = XMVectorAdd(pos, XMVectorScale(tangent, dt));
}

void Camera::RotateHorisontal(float angle)
{
    XMVECTOR rotation = XMQuaternionRotationAxis(up, angle);
    dir = XMVector3Rotate(dir, rotation);
}

void Camera::RotateVertical(float angle)
{
    XMVECTOR tangent = getTangentVector();
    angle = min(angle, XM_PIDIV2 - verticalAngle);
    angle = max(angle, -XM_PIDIV2 - verticalAngle);
    verticalAngle += angle;
    XMVECTOR rotation = XMQuaternionRotationAxis(-tangent, angle);
    dir = XMVector3Rotate(dir, rotation);
}

XMVECTOR Camera::getTangentVector()
{
    return XMVector4Normalize(XMVector3Cross(dir, up));
}

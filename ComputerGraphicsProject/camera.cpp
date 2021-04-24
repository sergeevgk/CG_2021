#include "Camera.h"
#include <algorithm>
using namespace DirectX;

Camera::Camera()
{
    Pos = XMVectorSet(0.0f, 4.0f, 1.0f, 0.0f);
    Dir = XMVectorSet(-1.0f, -9.0f, -5.0f, 0.0f);
    Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
}

XMMATRIX Camera::GetViewMatrix()
{
    XMVECTOR focus = XMVectorAdd(Pos, Dir);
    return XMMatrixLookAtLH(Pos, focus, Up);
}

void Camera::Move(const XMVECTOR& dv)
{
    Pos = Pos + dv;
}

void Camera::MoveNormal(float dn)
{
    Pos += dn * Dir;
}

void Camera::MoveTangent(float dt)
{
    XMVECTOR tangent = getTangentVector();
    Pos = XMVectorAdd(Pos, XMVectorScale(tangent, dt));
}

void Camera::RotateHorisontal(float angle)
{
    XMVECTOR rotation = XMQuaternionRotationAxis(Up, angle);
    Dir = XMVector3Rotate(Dir, rotation);
}

void Camera::RotateVertical(float angle)
{
    angle = std::clamp(angle, -XM_PIDIV2 - verticalAngle, XM_PIDIV2 - verticalAngle);
    verticalAngle += angle;
    auto t = -getTangentVector();
    XMVECTOR v = XMQuaternionRotationAxis(t, angle);
    Dir = XMVector3Rotate(Dir,v);
}

XMVECTOR Camera::getTangentVector()
{
    return XMVector3Normalize(XMVector3Cross(Dir, Up));
}

void Camera::PositionClip(const WorldBorders& b) {
    Pos = XMVectorClamp(Pos, b.Min, b.Max);
}

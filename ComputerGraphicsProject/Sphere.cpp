#include "Sphere.h"

Sphere::Sphere(float radius, int numTheta, int numPhi, bool outerNorm) {
    const float deltaTheta = DirectX::XM_PI / (numTheta - 1);
    const float deltaPhi = DirectX::XM_2PI / numPhi;
    float accumulateTheta = deltaTheta;

    vertices.push_back({ DirectX::XMFLOAT3(0.0f, radius, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) });
    for (int i = 0; i + 2 < numTheta; i++) {
        float sinTheta = sinf(accumulateTheta);
        float cosTheta = cosf(accumulateTheta);
        float phi = 0.0f;
        for (int j = 0; j < numPhi; j++) {
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);
            float x = sinTheta * sinPhi;
            float z = -sinTheta * cosPhi;
            float y = cosTheta;
            vertices.push_back({ DirectX::XMFLOAT3(x * radius, y * radius, z * radius), DirectX::XMFLOAT3(x, y, z) });
            phi += deltaPhi;
        }
        accumulateTheta += deltaTheta;
    }
    vertices.push_back({ DirectX::XMFLOAT3(0.0f, -radius, 0.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f) });

    const int northIndex = 0;
    const int southIndex = vertices.size() - 1;
    const int northLatitudeOffset = 1;
    const int southLatitudeOffset = vertices.size() - numPhi - 1;

    for (int i = 0; i < numPhi; i++) {
        indices.push_back(northIndex);
        indices.push_back(northLatitudeOffset + (i + 1) % numPhi);
        indices.push_back(northLatitudeOffset + i);
    }
    for (int i = 0; i + 3 < numTheta; i++) {
        for (int j = 0; j < numPhi; j++) {
            indices.push_back(northLatitudeOffset + i * numPhi + j);
            indices.push_back(northLatitudeOffset + i * numPhi + (j + 1) % numPhi);
            indices.push_back(northLatitudeOffset + (i + 1) * numPhi + j);
            indices.push_back(northLatitudeOffset + (i + 1) * numPhi + j);
            indices.push_back(northLatitudeOffset + i * numPhi + (j + 1) % numPhi);
            indices.push_back(northLatitudeOffset + (i + 1) * numPhi + (j + 1) % numPhi);
        }
    }
    for (int i = 0; i < numPhi; i++) {
        indices.push_back(southIndex);
        indices.push_back(southLatitudeOffset + i);
        indices.push_back(southLatitudeOffset + (i + 1) % numPhi);
    }
    if (!outerNorm) {
        std::reverse(indices.begin(), indices.end());
    }
}
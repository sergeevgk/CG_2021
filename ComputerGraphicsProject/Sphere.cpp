#include "Sphere.h"

Sphere::Sphere() {
    for (int latitude = 0; latitude <= numLines; latitude++)
    {
        for (int longitude = 0; longitude <= numLines; longitude++)
        {
            SimpleVertex v;

            v.Tex = DirectX::XMFLOAT2(longitude * spacing, 1.0f - latitude * spacing);

            float theta = v.Tex.x * 2.0f * static_cast<float>(3.14f);
            float phi = (v.Tex.y - 0.5f) * static_cast<float>(3.14f);
            float c = static_cast<float>(cos(phi));

            v.Normal = DirectX::XMFLOAT3(
                c * static_cast<float>(cos(theta)) * sphereRadius,
                static_cast<float>(sin(phi)) * sphereRadius,
                c * static_cast<float>(sin(theta)) * sphereRadius
            );
            v.Pos = DirectX::XMFLOAT3(v.Normal.x * sphereRadius, v.Normal.y * sphereRadius, v.Normal.z * sphereRadius);

            vertices.push_back(v);
        }
    }

    for (int latitude = 0; latitude < numLines; latitude++)
    {
        for (int longitude = 0; longitude < numLines; longitude++)
        {
            indices.push_back(latitude * (numLines + 1) + longitude);
            indices.push_back((latitude + 1) * (numLines + 1) + longitude);
            indices.push_back(latitude * (numLines + 1) + (longitude + 1));

            indices.push_back(latitude * (numLines + 1) + (longitude + 1));
            indices.push_back((latitude + 1) * (numLines + 1) + longitude);
            indices.push_back((latitude + 1) * (numLines + 1) + (longitude + 1));
        }
    }
}
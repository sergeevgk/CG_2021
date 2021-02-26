#pragma once

#include <vector>
#include "RenderTexture.h"

class AverageBrightnessProcess
{
public:
    AverageBrightnessProcess();
    ~AverageBrightnessProcess();

    HRESULT CreateResources(ID3D11Device* device, UINT width, UINT height);

    LARGE_INTEGER frequency;
    LARGE_INTEGER lastTime;

    float adaptedBrightness;

    std::vector<RenderTexture> renderTextures;

    ID3D11SamplerState* pSamplerState;
    ID3D11Texture2D*    pBrightnessTexture;

    void Clean();
};

#include <Windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <string>
#include <memory>
#include <cmath>
#include "AverageBrightnessProcess.h"

AverageBrightnessProcess::AverageBrightnessProcess() :
    adaptedBrightness(0.0)
{
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
}

HRESULT AverageBrightnessProcess::CreateResources(ID3D11Device* device, UINT width, UINT height)
{
    HRESULT hr = S_OK;

    D3D11_SAMPLER_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    sd.MaxAnisotropy = D3D11_MAX_MAXANISOTROPY;
    hr = device->CreateSamplerState(&sd, &pSamplerState);
    if (FAILED(hr))
        return hr;

    CD3D11_TEXTURE2D_DESC ltd(
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        1,
        1,
        1,
        1,
        0,
        D3D11_USAGE_STAGING,
        D3D11_CPU_ACCESS_READ
    );
    hr = device->CreateTexture2D(&ltd, nullptr, &pBrightnessTexture);
    if (FAILED(hr))
        return hr;

    hr = S_OK;

    size_t minSize = (size_t)(min(width, height));
    size_t n;
    for (n = 0; (size_t)(1) << (n + 1) <= minSize; n++);
    renderTextures.clear();
    renderTextures.reserve(n + 2);

    UINT size = 1 << n;
    RenderTexture initTexture(DXGI_FORMAT_R32G32B32A32_FLOAT);
    hr = initTexture.CreateResources(device, size, size);
    if (FAILED(hr))
        return hr;
    renderTextures.push_back(initTexture);

    for (size_t i = 0; i <= n; i++)
    {
        size = 1 << (n - i);
        RenderTexture renderTexture(DXGI_FORMAT_R32G32B32A32_FLOAT);
        hr = renderTexture.CreateResources(device, size, size);
        if (FAILED(hr))
            return hr;
        renderTextures.push_back(renderTexture);
    }

    return hr;
}

void AverageBrightnessProcess::Clean()
{
    for each (RenderTexture var in renderTextures)
    {
        var.Clean();
    }
    renderTextures.clear();
    if (pSamplerState) pSamplerState->Release();
    if (pBrightnessTexture) pBrightnessTexture->Release();
}

AverageBrightnessProcess::~AverageBrightnessProcess()
{}

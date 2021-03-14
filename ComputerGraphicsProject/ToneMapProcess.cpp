#include "ToneMapProcess.h"
#include <directxcolors.h>
using namespace DirectX;

HRESULT ToneMapProcess::CreateResources(ID3D11Device* device)
{
    HRESULT hr = S_OK;

    // Create the sampler state
    D3D11_SAMPLER_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
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

    return hr;
}

void ToneMapProcess::Clean()
{
    if (pSamplerState) pSamplerState->Release();
}


#include <windows.h>
#include "ToneMapProcess.h"
#include <d3d11_1.h>
#include <vector>

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

#include "resource.h"
#include "DDSTextureLoader.h"
#include <stdio.h>
#define NUM_LIGHTS 3

using namespace DirectX;

HRESULT ToneMapProcess::CreateDeviceDependentResources(ID3D11Device* device)
{
    HRESULT hr = S_OK;

    std::vector<BYTE> bytes;

    //// Create the vertex shader
    //hr = device->CreateVertexShader(device, L"CopyVertexShader.cso", bytes, &m_pVertexShader);
    //if (FAILED(hr))
    //    return hr;

    //// Create the pixel shader
    //hr = device->CreatePixelShader(device, L"ToneMapPixelShader.cso", bytes, &m_pPixelShader);
    //if (FAILED(hr))
    //    return hr;

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
    hr = device->CreateSamplerState(&sd, &m_pSamplerState);
    if (FAILED(hr))
        return hr;

    //m_pAverageLuminance = std::unique_ptr<AverageLuminanceProcess>(new AverageLuminanceProcess());
    //hr = m_pAverageLuminance->CreateDeviceDependentResources(device);

    return hr;
}

HRESULT ToneMapProcess::CreateWindowSizeDependentResources(ID3D11Device* device, UINT width, UINT height)
{
    HRESULT hr = S_OK;

    //hr = m_pAverageLuminance->CreateWindowSizeDependentResources(device, width, height);

    return hr;
}

void ToneMapProcess::Process(ID3D11DeviceContext* context, ID3D11ShaderResourceView* sourceTexture, ID3D11RenderTargetView* renderTarget, D3D11_VIEWPORT viewport)
{
    //m_pAverageLuminance->Process(context, sourceTexture);

    //ID3D11ShaderResourceView* averageLuminance = m_pAverageLuminance->GetResultShaderResourceView();

    //if (averageLuminance == nullptr)
    //    return;

    context->OMSetRenderTargets(1, &renderTarget, nullptr);
    context->RSSetViewports(1, &viewport);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(m_pVertexShader, nullptr, 0);
    context->PSSetShader(m_pPixelShader, nullptr, 0);
    context->PSSetShaderResources(0, 1, &sourceTexture);
    //context->PSSetShaderResources(1, 1, &averageLuminance);
    context->PSSetSamplers(0, 1, &m_pSamplerState);

    context->Draw(3, 0);

    ID3D11ShaderResourceView* nullsrv[] = { nullptr };
    context->PSSetShaderResources(0, 1, nullsrv);
    context->PSSetShaderResources(1, 1, nullsrv);
}

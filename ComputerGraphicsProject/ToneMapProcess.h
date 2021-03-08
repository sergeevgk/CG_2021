#pragma once
#include <windows.h>
#include <d3d11_1.h>

class ToneMapProcess
{
public:

    HRESULT CreateDeviceDependentResources(ID3D11Device* device);
    HRESULT CreateWindowSizeDependentResources(ID3D11Device* device, UINT width, UINT height);

    void Process(ID3D11DeviceContext* context, ID3D11ShaderResourceView* sourceTexture, ID3D11RenderTargetView* renderTarget, D3D11_VIEWPORT viewport);

private:
    //std::unique_ptr<AverageLuminanceProcess> m_pAverageLuminance;

    ID3D11VertexShader* m_pVertexShader;
    ID3D11PixelShader* m_pPixelShader;
    ID3D11SamplerState* m_pSamplerState;
};
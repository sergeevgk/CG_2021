#pragma once

class RenderTexture
{
public:
    RenderTexture(DXGI_FORMAT format);
    ~RenderTexture();

    HRESULT CreateResources(ID3D11Device* device, UINT width, UINT height);

    ID3D11Texture2D* GetRenderTarget()  const noexcept { return pRenderTarget; };
    ID3D11RenderTargetView* GetRenderTargetView()  const noexcept { return pRenderTargetView; };
    ID3D11ShaderResourceView* GetShaderResourceView()  const noexcept { return pShaderResourceView; };

    void Clean();
private:
    ID3D11Texture2D* pRenderTarget = nullptr;
    ID3D11RenderTargetView* pRenderTargetView = nullptr;
    ID3D11ShaderResourceView* pShaderResourceView = nullptr;
    DXGI_FORMAT format;
};

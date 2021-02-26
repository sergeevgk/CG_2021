#pragma once

class RenderTexture
{
public:
    RenderTexture(DXGI_FORMAT format);
    ~RenderTexture();

    HRESULT CreateResources(ID3D11Device* device, UINT width, UINT height);

    ID3D11Texture2D* GetRenderTarget() { return pRenderTarget; };
    ID3D11RenderTargetView* GetRenderTargetView() { return pRenderTargetView; };
    ID3D11ShaderResourceView* GetShaderResourceView() { return pShaderResourceView; };

    D3D11_VIEWPORT GetViewPort() { return vp; };
    void Clean();
private:
    ID3D11Texture2D* pRenderTarget;
    ID3D11RenderTargetView* pRenderTargetView;
    ID3D11ShaderResourceView* pShaderResourceView;

    DXGI_FORMAT format;
    D3D11_VIEWPORT vp;
};

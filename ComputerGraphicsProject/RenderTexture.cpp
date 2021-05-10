#include <fstream>
#include <comdef.h>
#include <vector>
#include <wrl.h>
#include <wrl/client.h>
#include <dxgi1_4.h>
#include <d3d11_3.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <memory>
#include <concrt.h>

#include "RenderTexture.h"

RenderTexture::RenderTexture(DXGI_FORMAT format) : format(format) {}

HRESULT RenderTexture::CreateResources(ID3D11Device* device, UINT width, UINT height)
{
    HRESULT hr = S_OK;

    CD3D11_TEXTURE2D_DESC rtd(
        format,
        width,
        height,
        1,
        1,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DEFAULT,
        0,
        1
    );

    hr = device->CreateTexture2D(&rtd, nullptr, &pRenderTarget);
    if (FAILED(hr))
        return hr;

    CD3D11_RENDER_TARGET_VIEW_DESC rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, format);

    hr = device->CreateRenderTargetView(pRenderTarget, &rtvd, &pRenderTargetView);
    if (FAILED(hr))
        return hr;

    CD3D11_SHADER_RESOURCE_VIEW_DESC srvd(D3D11_SRV_DIMENSION_TEXTURE2D, format);

    hr = device->CreateShaderResourceView(pRenderTarget, &srvd, &pShaderResourceView);


    return hr;
}

void RenderTexture::Clean() {
    if (pRenderTarget) pRenderTarget->Release();
    if (pRenderTargetView) pRenderTargetView->Release();
    if (pShaderResourceView) pShaderResourceView->Release();
}

RenderTexture::~RenderTexture()
{

}

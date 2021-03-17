#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "resource.h"
#include "DDSTextureLoader.h"
#include <stdio.h>
#include "Camera.h"
#include <string>
#include "RenderTexture.h"
#include <vector>
#include <cmath>
#include "AverageBrightnessProcess.h"
#include "ToneMapProcess.h"
#define NUM_LIGHTS 3

using namespace DirectX;
using namespace std;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
    XMFLOAT3 Normal;
    XMFLOAT3 Tangent;
};

struct CBChangesOnCameraAction
{
    XMMATRIX mView;
    XMFLOAT4 Eye;
};

struct CBChangeOnResize
{
    XMMATRIX mProjection;
};

struct CBChangesEveryFrame
{
    XMMATRIX mWorld;
};

struct LightConstantBuffer {
    XMFLOAT4 vLightPos[3];
    XMFLOAT4 vLightColor[3];
    XMFLOAT4 vOutputColor;
};

__declspec(align(16))
struct BrightnessConstantBuffer
{
    float AverageBrightness;
};

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Texture2D* g_pDepthStencil = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
RenderTexture* g_pRenderTexture = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;

ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pIndexBuffer = nullptr;
ID3D11Buffer* g_pCBChangesOnCameraAction = nullptr;
ID3D11Buffer* g_pCBChangeOnResize = nullptr;
ID3D11Buffer* g_pCBChangesEveryFrame = nullptr;
ID3D11Buffer* g_lightColorBuffer = nullptr;
ID3D11Buffer* m_pBrightnessBuffer;

ID3D11ShaderResourceView* g_pTextureRV = nullptr;
ID3D11SamplerState* g_pSamplerLinear = nullptr;

XMMATRIX                            g_World;
XMMATRIX                            g_View;
XMMATRIX                            g_Projection;
XMFLOAT4                            g_vMeshColor(0.7f, 0.7f, 0.7f, 1.0f);
XMVECTOR                            g_Eye;
XMVECTOR                            g_At;
XMVECTOR                            g_Up;
float                               g_Zoom = XM_PIDIV4 * 2.5;
Camera                              camera;
UINT32 m_indexCount;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11VertexShader* g_pVertexPostShader = nullptr;
ID3D11PixelShader* g_pCopyPixelPostShader = nullptr;
ID3D11PixelShader* g_pBrightnessPixelPostShader = nullptr;
ID3D11PixelShader* g_pTonemapPixelPostShader = nullptr;

float g_adaptedBrightness;

ID3D11Texture2D* g_pBrightnessTexture;
D3D11_VIEWPORT vp;

AverageBrightnessProcess* g_pAvLumProc = nullptr;
ToneMapProcess* g_pToneMapProc = nullptr;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
HRESULT CreateLights();
HRESULT CreatePS(WCHAR* fileName, string shaderName, ID3D11PixelShader** pixelShader);
HRESULT CreateVS(WCHAR* fileName, string shaderName, ID3D11VertexShader** vertexShader, ID3DBlob** pVSBlob);
HRESULT SetView(UINT width, UINT height);
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    if (FAILED(InitDevice()))
    {
        CleanupDevice();
        return 0;
    }

    // Main message loop
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    CleanupDevice();

    return (int)msg.wParam;
}

//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
        nullptr);
    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
        dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
            pErrorBlob->Release();
        }
        return hr;
    }
    if (pErrorBlob) pErrorBlob->Release();

    return S_OK;
}

HRESULT CreateProcessShaders() {
    // Compile the vertex shader
    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    hr = CreateVS(L"PS_VS.fx", "VS", &g_pVertexShader, &pVSBlob);
    if (FAILED(hr)) {
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    UINT numElements = ARRAYSIZE(layout);

    // Create the input layout
    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr))
        return hr;

    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    hr = CreatePS(L"PS_VS.fx", "PS", &g_pPixelShader);
    if (FAILED(hr)) {
        return hr;
    }
    return hr;
}

HRESULT CreatePostProcessShaders() {

    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    hr = CreateVS(L"TonemapShader.fx", "VS_COPY", &g_pVertexPostShader, &pVSBlob);
    if (FAILED(hr)) {
        return hr;
    }
    hr = CreatePS(L"TonemapShader.fx", "PS_COPY", &g_pCopyPixelPostShader);
    if (FAILED(hr)) {
        return hr;
    }
    hr = CreatePS(L"TonemapShader.fx", "PS_BRIGHTNESS", &g_pBrightnessPixelPostShader);
    if (FAILED(hr)) {
        return hr;
    }
    hr = CreatePS(L"TonemapShader.fx", "PS_TONEMAP", &g_pTonemapPixelPostShader);
    if (FAILED(hr)) {
        return hr;
    }
    return hr;
}

HRESULT CreatePS(WCHAR* fileName, string shaderName, ID3D11PixelShader** pixelShader) {
    // Compile the pixel shader
    ID3DBlob* pPSBlob = nullptr;
    HRESULT hr;
    hr = CompileShaderFromFile(fileName, shaderName.c_str(), "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
        MessageBox(nullptr,
            L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, pixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
        return hr;
    return hr;
}

HRESULT CreateVS(WCHAR* fileName, string shaderName, ID3D11VertexShader** vertexShader, ID3DBlob** pVSBlob) {

    // Compile the pixel shader
    HRESULT hr;
    hr = CompileShaderFromFile(fileName, shaderName.c_str(), "vs_4_0", pVSBlob);
    if (FAILED(hr))
    {
        MessageBox(nullptr,
            L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreateVertexShader((*pVSBlob)->GetBufferPointer(), (*pVSBlob)->GetBufferSize(), nullptr, vertexShader);
    if (FAILED(hr)) {
        (*pVSBlob)->Release();
        return hr;
    }
    return hr;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

        if (hr == E_INVALIDARG)
        {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
                D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
        }

        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return hr;

    // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
    IDXGIFactory1* dxgiFactory = nullptr;
    {
        IDXGIDevice* dxgiDevice = nullptr;
        hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter* adapter = nullptr;
            hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr))
            {
                hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (FAILED(hr))
        return hr;

    // Create swap chain
    // DirectX 11.0 systems
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);

    // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
    dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

    dxgiFactory->Release();

    if (FAILED(hr))
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr))
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory(&descDepth, sizeof(descDepth));
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
    if (FAILED(hr))
        return hr;

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr))
        return hr;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // Setup the viewport
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    CreateProcessShaders();
    CreatePostProcessShaders();

    // Create vertex buffer
    const int numLines = 16;
    const float spacing = 1.0f / numLines;
    const float sphereRadius = 1.0f;
    // Create vertex buffer
    std::vector<SimpleVertex> vertices;
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

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * static_cast<UINT>(vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices.data();
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr))
        return hr;

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // Create index buffer
    // Create vertex buffer
    std::vector<WORD> indices;
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

    // setup camera
    camera = Camera();
    m_indexCount = static_cast<UINT32>(indices.size());
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * m_indexCount;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices.data();
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr))
        return hr;

    // Set index buffer
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(CBChangesOnCameraAction);
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pCBChangesOnCameraAction);
    if (FAILED(hr))
        return hr;
    bd.ByteWidth = sizeof(CBChangeOnResize);
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pCBChangeOnResize);
    if (FAILED(hr))
        return hr;

    bd.ByteWidth = sizeof(CBChangesEveryFrame);
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pCBChangesEveryFrame);
    if (FAILED(hr))
        return hr;

    CD3D11_BUFFER_DESC albd(sizeof(BrightnessConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
    hr = g_pd3dDevice->CreateBuffer(&albd, nullptr, &m_pBrightnessBuffer);

    // Load the Texture
    hr = CreateDDSTextureFromFile(g_pd3dDevice, L"seafloor.dds", nullptr, &g_pTextureRV);
    if (FAILED(hr))
        return hr;

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
    if (FAILED(hr))
        return hr;

    // Initialize the world matrices
    g_World = XMMatrixIdentity();

    // Initialize the view matrix
    XMVECTOR Eye = XMVectorSet(0.0f, 13.0f, -2.5f, 0.0f);
    XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_Eye = Eye;
    g_At = At;
    g_Up = Up;
    g_View = XMMatrixLookAtLH(Eye, At, Up);
    XMFLOAT4 tempEye;
    XMStoreFloat4(&tempEye, g_Eye);
    g_View = camera.GetViewMatrix();
    CBChangesOnCameraAction cbChangesOnCameraAction;
    cbChangesOnCameraAction.mView = XMMatrixTranspose(g_View);
    cbChangesOnCameraAction.Eye = tempEye;
    g_pImmediateContext->UpdateSubresource(g_pCBChangesOnCameraAction, 0, nullptr, &cbChangesOnCameraAction, 0, 0);

    // Initialize the projection matrix
    g_Projection = XMMatrixPerspectiveFovLH(g_Zoom, width / (FLOAT)height, 0.01f, 100.0f);

    CBChangeOnResize cbChangesOnResize;
    cbChangesOnResize.mProjection = XMMatrixTranspose(g_Projection);
    g_pImmediateContext->UpdateSubresource(g_pCBChangeOnResize, 0, nullptr, &cbChangesOnResize, 0, 0);

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
    hr = g_pd3dDevice->CreateTexture2D(&ltd, nullptr, &g_pBrightnessTexture);
    if (FAILED(hr))
        return hr;
    CreateLights();

    g_pRenderTexture = new RenderTexture(DXGI_FORMAT_R32G32B32A32_FLOAT);
    hr = g_pRenderTexture->CreateResources(g_pd3dDevice, width, height);
    if (FAILED(hr))
        return hr;

    g_pAvLumProc = new AverageBrightnessProcess();
    g_pAvLumProc->CreateResources(g_pd3dDevice, width, height);
    g_pToneMapProc = new ToneMapProcess();
    g_pToneMapProc->CreateResources(g_pd3dDevice);
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();

    if (g_pSamplerLinear) g_pSamplerLinear->Release();
    if (g_pTextureRV) g_pTextureRV->Release();
    if (g_pCBChangesOnCameraAction) g_pCBChangesOnCameraAction->Release();
    if (g_pCBChangeOnResize) g_pCBChangeOnResize->Release();
    if (g_pCBChangesEveryFrame) g_pCBChangesEveryFrame->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pIndexBuffer) g_pIndexBuffer->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pDepthStencil) g_pDepthStencil->Release();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (g_lightColorBuffer) g_lightColorBuffer->Release();
    if (m_pBrightnessBuffer) m_pBrightnessBuffer->Release();
    if (g_pVertexPostShader) g_pVertexPostShader->Release();
    if (g_pCopyPixelPostShader) g_pCopyPixelPostShader->Release();
    if (g_pBrightnessPixelPostShader) g_pBrightnessPixelPostShader->Release();
    if (g_pTonemapPixelPostShader) g_pTonemapPixelPostShader->Release();
    if (g_pBrightnessTexture) g_pBrightnessTexture->Release();

    if (g_pRenderTexture) g_pRenderTexture->Clean();
    if (g_pRenderTexture) delete g_pRenderTexture;
    if (g_pAvLumProc) g_pAvLumProc->Clean();
    if (g_pAvLumProc) delete g_pAvLumProc;
    if (g_pToneMapProc) g_pToneMapProc->Clean();
    if (g_pToneMapProc) delete g_pToneMapProc;
}

void ChangeViewOnAction(XMVECTOR move) {
    g_Eye = XMVectorAdd(g_Eye, move);
    g_At = XMVectorAdd(g_At, move);
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_View = XMMatrixLookAtLH(g_Eye, g_At, Up);
    XMFLOAT4 tempEye;
    XMStoreFloat4(&tempEye, g_Eye);

    CBChangesOnCameraAction cbChangesOnCameraAction;
    cbChangesOnCameraAction.mView = XMMatrixTranspose(g_View);
    cbChangesOnCameraAction.Eye = tempEye;
    g_pImmediateContext->UpdateSubresource(g_pCBChangesOnCameraAction, 0, nullptr, &cbChangesOnCameraAction, 0, 0);
}

void ChangeCameraZoom(float mult) {
    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    g_Zoom *= mult;
    g_Projection = XMMatrixPerspectiveFovLH(g_Zoom, width / (FLOAT)height, 0.01f, 100.0f);

    CBChangeOnResize cbChangesOnResize;
    cbChangesOnResize.mProjection = XMMatrixTranspose(g_Projection);
    g_pImmediateContext->UpdateSubresource(g_pCBChangeOnResize, 0, nullptr, &cbChangesOnResize, 0, 0);
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    static POINT cursor;
    static float mouse_sence = 5e-3f;
    float speed = 8.0f;

    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        if (g_pSwapChain)
        {
            ChangeCameraZoom(1.0);
            g_pImmediateContext->OMSetRenderTargets(0, 0, 0);

            // Release all outstanding references to the swap chain's buffers.
            g_pRenderTargetView->Release();

            HRESULT hr;
            // Preserve the existing buffer count and format.
            // Automatically choose the width and height to match the client rect for HWNDs.
            hr = g_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

            if (SUCCEEDED(hr)) {
                hr = SetView(LOWORD(lParam), HIWORD(lParam));
            }
            if (FAILED(hr))
            {
                PostQuitMessage(0);
            }
        }
        break;

    case WM_KEYDOWN:

        XMVECTOR orth = XMVector3Cross(g_At, g_Up);
        XMVECTOR move = XMVECTOR();
        switch (wParam)
        {
        case VK_LEFT:
            move = XMVectorSet(speed * .1f, 0.0f, 0.0f, 0.0f) * (-1);
            camera.Move(XMVectorGetX(move), XMVectorGetY(move), XMVectorGetZ(move));
            break;
        case VK_RIGHT:
            move = XMVectorSet(speed * .1f, 0.0f, 0.0f, 0.0f);
            camera.Move(XMVectorGetX(move), XMVectorGetY(move), XMVectorGetZ(move));
            break;
        case VK_UP:
            move = XMVectorSet(0.0f, 0.0f, speed * .1f, 0.0f);
            camera.Move(XMVectorGetX(move), XMVectorGetY(move), XMVectorGetZ(move));
            break;
        case VK_DOWN:
            move = XMVectorSet(0.0f, 0.0f, speed * .1f, 0.0f) * (-1);
            camera.Move(XMVectorGetX(move), XMVectorGetY(move), XMVectorGetZ(move));
            break;
        case VK_ADD:
            ChangeCameraZoom(0.8f);
            break;
        case VK_SUBTRACT:
            ChangeCameraZoom(1.25f);
            break;
        }
        break;

    case WM_LBUTTONDOWN:
        ShowCursor(false);
        GetCursorPos(&cursor);
        SetCursorPos(cursor.x, cursor.y);
        return 0;

    case WM_MOUSEMOVE:
        if (wParam == MK_LBUTTON)
        {
            POINT current_pos;
            int dx, dy;
            GetCursorPos(&current_pos);
            dx = current_pos.x - cursor.x;
            dy = current_pos.y - cursor.y;
            SetCursorPos(cursor.x, cursor.y);
            camera.RotateHorisontal(dx * mouse_sence);
            if (dy != 0)
            {
                camera.RotateVertical(dy * mouse_sence);
            }
        }
        return 0;

    case WM_LBUTTONUP:
        SetCursorPos(cursor.x, cursor.y);
        ShowCursor(true);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);;
}

HRESULT SetView(UINT width, UINT height)
{
    HRESULT hr;
    // Get buffer and create a render-target-view.
    ID3D11Texture2D* pBuffer;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
        (void**)&pBuffer);
    if (FAILED(hr))
        return hr;
    hr = g_pd3dDevice->CreateRenderTargetView(pBuffer, NULL,
        &g_pRenderTargetView);
    if (FAILED(hr))
        return hr;
    pBuffer->Release();

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

    // Set up the viewport.
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    return S_OK;
}


HRESULT CreateLights()
{
    HRESULT hr = S_OK;
    XMFLOAT4 LightPositions[NUM_LIGHTS] =
    {
        XMFLOAT4(4.0f, 4.0f, 2.0f, 1.0f),
        XMFLOAT4(0.0f, 4.0f, 0.0f, 1.0f),
        XMFLOAT4(0.0f, 4.0f, 4.0f, 1.0f)
    };

    XMFLOAT4 LightColors[NUM_LIGHTS] =
    {
        XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),
        XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),
        XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)
    };

    LightConstantBuffer lcb1;
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        lcb1.vLightColor[i] = LightColors[i];
        lcb1.vLightPos[i] = LightPositions[i];
    }
    // Create the constant buffers for lights
    D3D11_BUFFER_DESC lpbd;
    ZeroMemory(&lpbd, sizeof(lpbd));
    lpbd.Usage = D3D11_USAGE_DEFAULT;
    lpbd.ByteWidth = sizeof(LightConstantBuffer);
    lpbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lpbd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer(&lpbd, nullptr, &g_lightColorBuffer);
    if (FAILED(hr))
        return hr;
    g_pImmediateContext->UpdateSubresource(g_lightColorBuffer, 0, nullptr, &lcb1, 0, 0);
    return hr;
}

void CopyTexture(ID3D11DeviceContext* context, ID3D11ShaderResourceView* sourceTexture,
    RenderTexture& dst, ID3D11PixelShader* pixelShader)
{
    ID3D11RenderTargetView* renderTarget = dst.GetRenderTargetView();

    D3D11_VIEWPORT viewport = dst.GetViewPort();

    context->OMSetRenderTargets(1, &renderTarget, nullptr);
    context->RSSetViewports(1, &viewport);

    context->PSSetShader(pixelShader, nullptr, 0);
    context->PSSetShaderResources(0, 1, &sourceTexture);

    context->Draw(4, 0);

    ID3D11ShaderResourceView* nullsrv[] = { nullptr };
    context->PSSetShaderResources(0, 1, nullsrv);
}

void PostRender()
{
    ID3D11ShaderResourceView* sourceTexture = g_pRenderTexture->GetShaderResourceView();
    if (g_pAvLumProc->renderTextures.size() == 0)
        return;
    float backgroundColour[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    for (size_t i = 0; i < g_pAvLumProc->renderTextures.size(); i++)
        g_pImmediateContext->ClearRenderTargetView(g_pAvLumProc->renderTextures[i].GetRenderTargetView(), backgroundColour);

    g_pImmediateContext->IASetInputLayout(nullptr);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    g_pImmediateContext->VSSetShader(g_pVertexPostShader, nullptr, 0);

    g_pImmediateContext->PSSetSamplers(0, 1, &(g_pAvLumProc->pSamplerState));

    CopyTexture(g_pImmediateContext, sourceTexture, g_pAvLumProc->renderTextures[0], g_pCopyPixelPostShader);
    CopyTexture(g_pImmediateContext, g_pAvLumProc->renderTextures[0].GetShaderResourceView(), g_pAvLumProc->renderTextures[1], g_pBrightnessPixelPostShader);

    for (size_t i = 2; i < g_pAvLumProc->renderTextures.size(); i++)
    {
        CopyTexture(g_pImmediateContext, g_pAvLumProc->renderTextures[i - 1].GetShaderResourceView(), g_pAvLumProc->renderTextures[i], g_pCopyPixelPostShader);
    }

    ID3D11ShaderResourceView* nullsrv[] = { nullptr };
    g_pImmediateContext->PSSetShaderResources(0, 1, nullsrv);

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    size_t timeDelta = currentTime.QuadPart - g_pAvLumProc->lastTime.QuadPart;
    g_pAvLumProc->lastTime = currentTime;
    double delta = static_cast<double>(timeDelta) / g_pAvLumProc->frequency.QuadPart;

    D3D11_MAPPED_SUBRESOURCE BrightnessAccessor;
    g_pImmediateContext->CopyResource(g_pBrightnessTexture, g_pAvLumProc->renderTextures[g_pAvLumProc->renderTextures.size() - 1].GetRenderTarget());
    g_pImmediateContext->Map(g_pBrightnessTexture, 0, D3D11_MAP_READ, 0, &BrightnessAccessor);
    float Brightness = *(float*)BrightnessAccessor.pData;
    g_pImmediateContext->Unmap(g_pBrightnessTexture, 0);

    float sigma = 0.04f / (0.04f + Brightness);
    float tau = sigma * 0.4f + (1 - sigma) * 0.1f;

    g_adaptedBrightness += (Brightness - g_adaptedBrightness) * static_cast<float>(1 - exp(-delta * tau));
    BrightnessConstantBuffer BrightnessBufferData = { g_adaptedBrightness };
    g_pImmediateContext->UpdateSubresource(m_pBrightnessBuffer, 0, nullptr, &BrightnessBufferData, 0, 0);

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
    g_pImmediateContext->RSSetViewports(1, &vp);

    g_pImmediateContext->IASetInputLayout(nullptr);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    g_pImmediateContext->VSSetShader(g_pVertexPostShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pTonemapPixelPostShader, nullptr, 0);
    g_pImmediateContext->PSSetConstantBuffers(0, 1, &m_pBrightnessBuffer);
    g_pImmediateContext->PSSetShaderResources(0, 1, &sourceTexture);
    g_pImmediateContext->PSSetSamplers(0, 1, &(g_pToneMapProc->pSamplerState));

    g_pImmediateContext->Draw(4, 0);

    g_pImmediateContext->PSSetShaderResources(0, 1, nullsrv);


}
//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    // Clear the back buffer
    //
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::MidnightBlue);
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTexture->GetRenderTargetView(), Colors::MidnightBlue);
    //
    // Clear the depth buffer to 1.0 (max depth)
    //
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    ID3D11RenderTargetView* renderTarget = g_pRenderTexture->GetRenderTargetView();
    D3D11_VIEWPORT viewport = g_pRenderTexture->GetViewPort();

    g_pImmediateContext->OMSetRenderTargets(1, &renderTarget, g_pDepthStencilView);
    g_pImmediateContext->RSSetViewports(1, &viewport);
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Update variables that change once per frame
    //
    CBChangesEveryFrame cb;
    cb.mWorld = XMMatrixTranspose(g_World);

    g_View = camera.GetViewMatrix();
    CBChangesOnCameraAction cbChangesOnCameraAction;
    cbChangesOnCameraAction.mView = XMMatrixTranspose(g_View);
    g_pImmediateContext->UpdateSubresource(g_pCBChangesOnCameraAction, 0, nullptr, &cbChangesOnCameraAction, 0, 0);
    g_pImmediateContext->UpdateSubresource(g_pCBChangesEveryFrame, 0, nullptr, &cb, 0, 0);
    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
    //
    // Render
    //
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pCBChangesOnCameraAction);
    g_pImmediateContext->VSSetConstantBuffers(1, 1, &g_pCBChangeOnResize);
    g_pImmediateContext->VSSetConstantBuffers(2, 1, &g_pCBChangesEveryFrame);
    g_pImmediateContext->VSSetConstantBuffers(3, 1, &g_lightColorBuffer);

    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->PSSetConstantBuffers(2, 1, &g_pCBChangesEveryFrame);
    g_pImmediateContext->PSSetConstantBuffers(3, 1, &g_lightColorBuffer);
    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pTextureRV);

    g_pImmediateContext->PSSetSamplers(0, 1, &g_pSamplerLinear);
    g_pImmediateContext->DrawIndexed(m_indexCount, 0, 0);

    PostRender();

    g_pSwapChain->Present(0, 0);
}

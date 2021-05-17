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
#include "RenderMode.h"
#include "SimpleVertex.h"
#include "Sphere.h"
#include "PointLight.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#define STB_IMAGE_IMPLEMENTATION
#include "STBImage/stb_image.h"

#define NUM_LIGHTS 1
#define NUM_RENDER_MODES 4

using namespace DirectX;
using namespace std;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

struct GeometryOperatorsConstantBuffer {
    XMMATRIX world;
    XMMATRIX worldNormals;
    XMMATRIX view;
    XMMATRIX projection;
    XMFLOAT4 cameraPos;
};

__declspec(align(16))
struct SpherePropsConstantBuffer {
    XMFLOAT4 color;
    float roughness;
    float metalness;
};

__declspec(align(16))
struct LightsConstantBuffer {
    XMFLOAT4 lightPos[NUM_LIGHTS];
    XMFLOAT4 lightColor[NUM_LIGHTS];
    XMFLOAT4 lightAttenuation[NUM_LIGHTS];
    float lightIntensity[3 * NUM_LIGHTS];
};

__declspec(align(16))
struct ExposureConstantBuffer {
    float exposureMult;
};

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_deviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;
ID3D11Texture2D* g_pDepthStencil = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
ID3D11InputLayout* g_pInputVertexLayout = nullptr;
RenderTexture* g_pRenderTexture = nullptr;
std::vector<RenderTexture*> g_pLuminanceLogTextures;

ID3D11Buffer* g_sphereVertexBuffer = nullptr;
ID3D11Buffer* g_sphereIndexBuffer = nullptr;
ID3D11Buffer* g_environmentIndexBuffer = nullptr;
ID3D11Buffer* g_environmentVertexBuffer = nullptr;
ID3D11Buffer* g_lightConstantBuffer = nullptr;
ID3D11Buffer* g_geometryConstantBuffer = nullptr;
ID3D11Buffer* g_spropsConstantBuffer = nullptr;
ID3D11Buffer* g_exposureConstantBuffer = nullptr;


ID3D11ShaderResourceView* g_pTextureRV = nullptr;
ID3D11ShaderResourceView* g_pIrradiance = nullptr;
ID3D11SamplerState* g_pSamplerLinear = nullptr;

XMMATRIX  g_sphereWorld;
XMMATRIX  g_World;
XMMATRIX  g_View;
XMMATRIX  g_Projection;
XMVECTOR  g_Eye;
XMVECTOR  g_At;
XMVECTOR  g_Up;
Camera    g_Camera;
WorldBorders  g_Borders;


ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11PixelShader* g_pCopyPixelPostShader = nullptr;
ID3D11PixelShader* g_pBrightnessPixelPostShader = nullptr;
ID3D11PixelShader* g_pTonemapPixelPostShader = nullptr;


ID3D11PixelShader* g_pPBRPixelShader = nullptr;
ID3D11PixelShader* g_pNormalDistPixelShader = nullptr;
ID3D11PixelShader* g_pGeometryPixelShader = nullptr;
ID3D11PixelShader* g_pFresnelPixelShader = nullptr;


ID3D11VertexShader* g_pVertexShaderCopy = nullptr;
ID3D11PixelShader* g_pPixelShaderToneMapping = nullptr;

ID3D11VertexShader* g_pEnvironmentVertexShader = nullptr;
ID3D11PixelShader* g_pEnvironmentPixelShader = nullptr;

ID3D11PixelShader* g_pCubeMapPixelShader = nullptr;
ID3D11PixelShader* g_pIrradianceMapPixelShader = nullptr;


D3D11_VIEWPORT vp;


const char* renderModes[NUM_RENDER_MODES] = { "PBR", "NDF", "Geometry", "Fresnel" };
RenderMode renderMode = RenderMode::PBR;

ID3D11DepthStencilState* g_pDepthStencilState = nullptr;


UINT vertexStride;
UINT vertexOffset;
UINT numIndices;
UINT numIndicesEnv;

float sphereColorRGB[4] = { 0.2f, 0.0f, 0.0f, 1.0f };;
XMFLOAT4 sphereColorSRGB;

float roughness = 0.5f;
float metalness = 0.5f;
float intensity = 10.0f;
float exposureMult = 1.0f;

PointLight lights[NUM_LIGHTS];

ID3DBlob* g_pBlob = nullptr;

HRESULT InitWindow(HINSTANCE hInstance, WNDPROC window_proc, int nCmdShow);
HRESULT Init();
HRESULT InitScene();
HRESULT CreatePS(WCHAR* fileName, string shaderName, ID3D11PixelShader** pixelShader);
HRESULT CreateVS(WCHAR* fileName, string shaderName, ID3D11VertexShader** vertexShader);
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (FAILED(InitWindow(hInstance, WndProc, nCmdShow)))
        return 0;

    if (FAILED(Init()))
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

HRESULT InitWindow(HINSTANCE hInstance, WNDPROC window_proc, int nCmdShow)
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


HRESULT CreatePS(WCHAR* fileName, string shaderName, ID3D11PixelShader** pixelShader) {
    // Compile the pixel shader
    ID3DBlob* pPSBlob = nullptr;
    HRESULT hr;
    hr = CompileShaderFromFile(fileName, shaderName.c_str(), "ps_5_0", &pPSBlob);
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

HRESULT CreateVS(WCHAR* fileName, string shaderName, ID3D11VertexShader** vertexShader) {

    // Compile the pixel shader
    ID3DBlob* pVSBlob = nullptr;
    HRESULT hr;
    hr = CompileShaderFromFile(fileName, shaderName.c_str(), "vs_5_0", &pVSBlob);
    if (FAILED(hr))
    {
        MessageBox(nullptr,
            L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return hr;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, vertexShader);
    if (FAILED(hr)) {
        pVSBlob->Release();
        return hr;
    }
    return hr;
}

HRESULT InitShaders() {
    HRESULT hr;
    CompileShaderFromFile(L"Shaders.fx", "vsMain", "vs_5_0", &g_pBlob);

    WCHAR* shaderFileName = L"Shaders.fx";

    hr = CreateVS(shaderFileName, "vsEnvironment", &g_pEnvironmentVertexShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psEnvironment", &g_pEnvironmentPixelShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreateVS(shaderFileName, "vsMain", &g_pVertexShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psPBR", &g_pPBRPixelShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psNDF", &g_pNormalDistPixelShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psGeometry", &g_pGeometryPixelShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psFresnel", &g_pFresnelPixelShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreateVS(shaderFileName, "vsCopyMain", &g_pVertexShaderCopy);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psToneMappingMain", &g_pPixelShaderToneMapping);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psCubeMap", &g_pCubeMapPixelShader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreatePS(shaderFileName, "psIrradianceMap", &g_pIrradianceMapPixelShader);
    if (FAILED(hr)) {
        return hr;
    }
}

HRESULT Init() {
    HRESULT hr;
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
            D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_deviceContext);

        if (hr == E_INVALIDARG)
        {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
                D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_deviceContext);
        }

        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return hr;

    IDXGIFactory1* dxgiFactory = nullptr;
    IDXGIDevice* dxgiDevice = nullptr;
    hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (SUCCEEDED(hr))
    {
        IDXGIAdapter* dxgiAdapter = nullptr;
        hr = dxgiDevice->GetAdapter(&dxgiAdapter);
        if (SUCCEEDED(hr))
        {
            hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
            dxgiAdapter->Release();
        }
        dxgiDevice->Release();
    }
    if (FAILED(hr))
        return hr;

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

    ID3D11Texture2D* p_framebuffer;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)(&p_framebuffer));
    if (FAILED(hr)) {
        return hr;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(p_framebuffer, 0, &g_renderTargetView);
    if (FAILED(hr)) {
        return hr;
    }
    p_framebuffer->Release();

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    g_pd3dDevice->CreateDepthStencilState(&depthStencilDesc, &g_pDepthStencilState);

    InitShaders();

    D3D11_INPUT_ELEMENT_DESC layout[] = {
          { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
          { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), g_pBlob->GetBufferPointer(), g_pBlob->GetBufferSize(), &g_pInputVertexLayout);
    if (FAILED(hr)) {
        return hr;
    }

    g_sphereWorld = XMMatrixIdentity();
    g_World = XMMatrixIdentity();
    g_View = XMMatrixIdentity();
    g_Projection = XMMatrixIdentity();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_deviceContext);
    InitScene();
}

ID3D11Buffer* createBuffer(ID3D11Device* p_device, UINT byteWidth, UINT bindFlags, const void* sysMem) {
    D3D11_BUFFER_DESC bufferDescription = CD3D11_BUFFER_DESC(byteWidth, bindFlags);
    D3D11_SUBRESOURCE_DATA initData = { 0 };
    initData.pSysMem = sysMem;
    ID3D11Buffer* p_buffer = nullptr;
    HRESULT hr = p_device->CreateBuffer(&bufferDescription, sysMem ? &initData : nullptr, &p_buffer);
    if (FAILED(hr)) {
        return nullptr;
    }
    return p_buffer;
}

HRESULT ResizeResources(int width, int height) {
    HRESULT hr;
    vp = { 0.0f, 0.0f, (FLOAT)(width), (FLOAT)(height), 0.0f, 1.0f };

    if (g_pDepthStencil) {
        g_pDepthStencil->Release();
    }

    CD3D11_TEXTURE2D_DESC depthDescription(DXGI_FORMAT_D24_UNORM_S8_UINT, (UINT)width, (UINT)height, 1, 1, D3D11_BIND_DEPTH_STENCIL);
    hr = g_pd3dDevice->CreateTexture2D(&depthDescription, nullptr, &g_pDepthStencil);
    if (FAILED(hr)) { return hr; }

    if (g_pDepthStencilView) {
        g_pDepthStencilView->Release();
    }

    CD3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc(D3D11_DSV_DIMENSION_TEXTURE2D, depthDescription.Format);
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &depth_stencil_view_desc, &g_pDepthStencilView);
    if (FAILED(hr)) { return hr; }

    float near_z = 0.01f, far_z = 100.0f;
    g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, near_z, far_z);

    if (g_pRenderTexture) {
        g_pRenderTexture->Clean();
        delete g_pRenderTexture;
    }
    g_pRenderTexture = new RenderTexture(DXGI_FORMAT_R32G32B32A32_FLOAT);
    g_pRenderTexture->CreateResources(g_pd3dDevice, width, height);
}

HRESULT Resize(int width, int height) {
    HRESULT hr;
    g_deviceContext->OMSetRenderTargets(0, 0, 0);

    g_renderTargetView->Release();

    g_deviceContext->Flush();

    hr = g_pSwapChain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) { return hr; }


    ID3D11Texture2D* p_buffer;

    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&p_buffer);
    if (FAILED(hr)) { return hr; }


    hr = g_pd3dDevice->CreateRenderTargetView(p_buffer, nullptr, &g_renderTargetView);
    if (FAILED(hr)) { return hr; }

    p_buffer->Release();

    g_deviceContext->OMSetRenderTargets(1, &g_renderTargetView, nullptr);

    ResizeResources(width, height);
}

HRESULT CreateCubeMap(UINT size, ID3D11PixelShader* pixelShader, ID3D11ShaderResourceView** srcEnvironmentRV, ID3D11ShaderResourceView** dstEnvironmentRV) {
    const int numSides = 6;
    CD3D11_TEXTURE2D_DESC environmentDesc(DXGI_FORMAT_R32G32B32A32_FLOAT, size, size, numSides, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, D3D11_RESOURCE_MISC_TEXTURECUBE);
    
    ID3D11Texture2D* envTexture = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&environmentDesc, nullptr, &envTexture);
    if (FAILED(hr)) { return hr; }

    RenderTexture rt(DXGI_FORMAT_R32G32B32A32_FLOAT);
    rt.CreateResources(g_pd3dDevice, size, size);

    auto rtv = rt.GetRenderTargetView();
    g_deviceContext->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (FLOAT)size, (FLOAT)size, 0.0f, 1.0f };
    g_deviceContext->RSSetViewports(1, &vp);

    g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_deviceContext->IASetInputLayout(g_pInputVertexLayout);

    SimpleVertex verts[numSides][4] = {
        { { { 1.0f, -1.0f, 1.0f } }, { { 1.0f, 1.0f, 1.0f } }, { { 1.0f, 1.0f, -1.0f } }, { { 1.0f, -1.0f, -1.0f } } },
        { { { -1.0f, -1.0f, -1.0f } }, { { -1.0f, 1.0f, -1.0f } }, { { -1.0f, 1.0f, 1.0f } }, { { -1.0f, -1.0f, 1.0f } } },
        { { { -1.0f, 1.0f, 1.0f } }, { { -1.0f, 1.0f, -1.0f } }, { { 1.0f, 1.0f, -1.0f } }, { { 1.0f, 1.0f, 1.0f } } },
        { { { -1.0f, -1.0f, -1.0f } }, { { -1.0f, -1.0f, 1.0f } }, { { 1.0f, -1.0f, 1.0f } }, { { 1.0f, -1.0f, -1.0f } } },
        { { { -1.0f, -1.0f, 1.0f } }, { { -1.0f, 1.0f, 1.0f } }, { { 1.0f, 1.0f, 1.0f } }, { { 1.0f, -1.0f, 1.0f } } },
        { { { 1.0f, -1.0f, -1.0f } }, { { 1.0f, 1.0f, -1.0f } }, { { -1.0f, 1.0f, -1.0f } }, { { -1.0f, -1.0f, -1.0f } } }
    };

    int indices[numSides] = {
        0, 1, 2,
        2, 3, 0
    };

    auto indexBuffer = createBuffer(g_pd3dDevice, sizeof(int) * numSides, D3D11_BIND_INDEX_BUFFER, indices);
    g_deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    indexBuffer->Release();

    XMVECTOR dirs[numSides] = {
        { 1, 0, 0 },
        { -1, 0, 0 },
        { 0, 1, 0 },
        { 0, -1, 0 },
        { 0, 0, 1 },
        { 0, 0, -1 }
    };

    XMVECTOR ups[numSides] = {
        { 0, 1, 0 },
        { 0, 1, 0 },
        { 0, 0, -1 },
        { 0, 0, 1 },
        { 0, 1, 0 },
        { 0, 1, 0 }
    };

    g_deviceContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_deviceContext->PSSetShader(pixelShader, nullptr, 0);

    g_deviceContext->PSSetShaderResources(0, 1, srcEnvironmentRV);
    g_deviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

    GeometryOperatorsConstantBuffer geometryConstantBuffer;
    geometryConstantBuffer.world = XMMatrixTranspose(XMMatrixIdentity());
    geometryConstantBuffer.projection = XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.01f, 1.0f));

    for (size_t i = 0; i < numSides; i++) {
        auto vertexBuffer = createBuffer(g_pd3dDevice, sizeof(SimpleVertex) * 4, D3D11_BIND_VERTEX_BUFFER, verts[i]);
        g_deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);
        vertexBuffer->Release();

        XMVECTOR position = { 0.0f, 0.0f, 0.0f };
        XMVECTOR focus = XMVectorAdd(position, dirs[i]);
        auto viewMatrix = XMMatrixLookAtLH(position, focus, ups[i]);

        geometryConstantBuffer.view = XMMatrixTranspose(viewMatrix);
        g_deviceContext->UpdateSubresource(g_geometryConstantBuffer, 0, nullptr, &geometryConstantBuffer, 0, 0);
        g_deviceContext->VSSetConstantBuffers(0, 1, &g_geometryConstantBuffer);

        g_deviceContext->DrawIndexed(6, 0, 0);
        g_deviceContext->CopySubresourceRegion(envTexture, (UINT)i, 0, 0, 0, rt.GetRenderTarget(), 0, nullptr);
    }

    ID3D11ShaderResourceView* nullsrv[] = { nullptr };
    g_deviceContext->PSSetShaderResources(0, 1, nullsrv);

    CD3D11_SHADER_RESOURCE_VIEW_DESC environmentRVDesc(D3D11_SRV_DIMENSION_TEXTURECUBE, environmentDesc.Format, 0, 1);
    g_pd3dDevice->CreateShaderResourceView(envTexture, &environmentRVDesc, dstEnvironmentRV);
    envTexture->Release();
    rt.Clean();
}

HRESULT InitScene() {
    HRESULT hr;
    g_Camera = Camera();

    g_Borders.Min = { -100.0f, -10.0f, -100.0f };
    g_Borders.Max = { 100.0f, 10.0f, 100.0f };

    Sphere sphere = Sphere(1.0f, 30, 30, true);
    auto vertices = sphere.vertices;
    auto indices = sphere.indices;

    numIndices = (UINT)indices.size();
    vertexStride = sizeof(SimpleVertex);
    vertexOffset = 0;

    g_Camera = Camera();

    lights[0].Pos = { 0.0f, 4.0f, -3.0f, 0.0f };
    lights[0].Color = (XMFLOAT4)Colors::White;

    g_sphereVertexBuffer = createBuffer(g_pd3dDevice, sizeof(SimpleVertex) * (UINT)vertices.size(), D3D11_BIND_VERTEX_BUFFER, vertices.data());
    g_sphereIndexBuffer = createBuffer(g_pd3dDevice, sizeof(int) * numIndices, D3D11_BIND_INDEX_BUFFER, indices.data());
    g_geometryConstantBuffer = createBuffer(g_pd3dDevice, sizeof(GeometryOperatorsConstantBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);
    g_spropsConstantBuffer = createBuffer(g_pd3dDevice, sizeof(SpherePropsConstantBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);
    g_lightConstantBuffer = createBuffer(g_pd3dDevice, sizeof(LightsConstantBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);
    g_exposureConstantBuffer = createBuffer(g_pd3dDevice, sizeof(ExposureConstantBuffer), D3D11_BIND_CONSTANT_BUFFER, nullptr);


    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory(&samplerDesc, sizeof(samplerDesc));
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_pd3dDevice->CreateSamplerState(&samplerDesc, &g_pSamplerLinear);
    if (FAILED(hr)) { return hr; }

    RECT winRect;
    GetClientRect(g_hWnd, &winRect);
    LONG width = winRect.right - winRect.left;
    LONG height = winRect.bottom - winRect.top;
    ResizeResources(width, height);


    Sphere environment(1.0f, 10, 10, false);

    auto& envVertices = environment.vertices;
    g_environmentVertexBuffer = createBuffer(g_pd3dDevice, sizeof(SimpleVertex) * (UINT)envVertices.size(), D3D11_BIND_VERTEX_BUFFER, envVertices.data());

    auto& envIndices = environment.indices;
    numIndicesEnv = (UINT)envIndices.size();
    g_environmentIndexBuffer = createBuffer(g_pd3dDevice, sizeof(unsigned) * numIndicesEnv, D3D11_BIND_INDEX_BUFFER, envIndices.data());

    int x, y, comp;
    float* stbiData = stbi_loadf("palermo_park_1k.hdr", &x, &y, &comp, STBI_rgb_alpha);

    CD3D11_TEXTURE2D_DESC environmentDesc(DXGI_FORMAT_R32G32B32A32_FLOAT, x, y, 1, 1, D3D11_BIND_SHADER_RESOURCE);

    D3D11_SUBRESOURCE_DATA subresourceData;
    subresourceData.pSysMem = stbiData;
    subresourceData.SysMemPitch = x * (4 * sizeof(float));

    ID3D11Texture2D* envTexture = nullptr;
    
    hr = g_pd3dDevice->CreateTexture2D(&environmentDesc, &subresourceData, &envTexture);
    if (FAILED(hr)) { return hr; }
    stbi_image_free(stbiData);

    CD3D11_SHADER_RESOURCE_VIEW_DESC environmentRVDesc(D3D11_SRV_DIMENSION_TEXTURE2D, environmentDesc.Format);
    ID3D11ShaderResourceView* environmentRV = nullptr;
    hr = g_pd3dDevice->CreateShaderResourceView(envTexture, &environmentRVDesc, &environmentRV);
    
    if (FAILED(hr)) { return hr; }
    envTexture->Release();
    
    CreateCubeMap(512, g_pCubeMapPixelShader, &environmentRV, &g_pTextureRV);
    environmentRV->Release();

    CreateCubeMap(32, g_pIrradianceMapPixelShader, &g_pTextureRV, &g_pIrradiance);
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------

void ReleaseShaders() {

    if (g_pVertexShader)               g_pVertexShader->Release();
    if (g_pPixelShader)                g_pPixelShader->Release();
    if (g_pCopyPixelPostShader)        g_pCopyPixelPostShader->Release();
    if (g_pBrightnessPixelPostShader)  g_pBrightnessPixelPostShader->Release();
    if (g_pTonemapPixelPostShader)     g_pTonemapPixelPostShader->Release();
    if (g_pPBRPixelShader)             g_pPBRPixelShader->Release();
    if (g_pNormalDistPixelShader)      g_pNormalDistPixelShader->Release();
    if (g_pGeometryPixelShader)        g_pGeometryPixelShader->Release();
    if (g_pFresnelPixelShader)         g_pFresnelPixelShader->Release();
    if (g_pVertexShaderCopy)           g_pVertexShaderCopy->Release();
    if (g_pPixelShaderToneMapping)     g_pPixelShaderToneMapping->Release();
    if (g_pEnvironmentVertexShader)    g_pEnvironmentVertexShader->Release();
    if (g_pEnvironmentPixelShader)     g_pEnvironmentPixelShader->Release();
    if (g_pCubeMapPixelShader)      g_pCubeMapPixelShader->Release();
    if (g_pIrradianceMapPixelShader)g_pIrradianceMapPixelShader->Release();
}

void CleanupBuffers() {
    g_geometryConstantBuffer->Release();
    g_spropsConstantBuffer->Release();
    g_lightConstantBuffer->Release();
    g_sphereVertexBuffer->Release();
    g_sphereIndexBuffer->Release();
    g_environmentVertexBuffer->Release();
    g_environmentIndexBuffer->Release();
    g_exposureConstantBuffer->Release();
}

void QuitImgui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void CleanupDevice()
{
    QuitImgui();
    ReleaseShaders();
    CleanupBuffers();

    if (g_deviceContext) g_deviceContext->ClearState();
    if (g_pInputVertexLayout) g_pInputVertexLayout->Release();
    if (g_pSamplerLinear) g_pSamplerLinear->Release();
    if (g_pTextureRV) g_pTextureRV->Release();
    if (g_pIrradiance) g_pIrradiance->Release();
    if (g_pDepthStencil) g_pDepthStencil->Release();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pDepthStencilState) g_pDepthStencilState->Release();
    if (g_renderTargetView) g_renderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_deviceContext) g_deviceContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (g_pBlob) g_pBlob->Release();
    if (g_pRenderTexture) {
        g_pRenderTexture->Clean();
        delete g_pRenderTexture;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return true;
    }
    PAINTSTRUCT ps;
    HDC hdc;
    static POINT cursor;
    static float mouseSence = 6e-3f;
    float speed = 9.0f;
    float moveUnit = 0.1f;

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
        if (g_pSwapChain) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            Resize(width, height);
        }

    case WM_KEYDOWN:
        XMVECTOR orth = XMVector3Cross(g_At, g_Up);
        switch (wParam)
        {
        case VK_LEFT:
            g_Camera.MoveTangent(-moveUnit);
            g_Camera.PositionClip(g_Borders);
            break;
        case VK_RIGHT:
            g_Camera.MoveTangent(moveUnit);
            g_Camera.PositionClip(g_Borders);
            break;
        case VK_UP:
            g_Camera.MoveNormal(moveUnit);
            g_Camera.PositionClip(g_Borders);
            break;
        case VK_DOWN:
            g_Camera.MoveNormal(-moveUnit);
            g_Camera.PositionClip(g_Borders);
            break;
        }
        break;

    case WM_LBUTTONDOWN:
        if (ImGui::GetIO().WantCaptureMouse) {
            break;
        }
        while (ShowCursor(false) >= 0);
        GetCursorPos(&cursor);
        return 0;

    case WM_MOUSEMOVE:
        if (ImGui::GetIO().WantCaptureMouse) {
            break;
        }
        if (wParam == MK_LBUTTON)
        {
            POINT currentPos;
            GetCursorPos(&currentPos);
            int dx = currentPos.x - cursor.x;
            int dy = currentPos.y - cursor.y;
            g_Camera.RotateHorisontal(dx * mouseSence);
            g_Camera.RotateVertical(dy * mouseSence);
            SetCursorPos(cursor.x, cursor.y);
        }

        return 0;

    case WM_LBUTTONUP:
        if (ImGui::GetIO().WantCaptureMouse) {
            break;
        }
        SetCursorPos(cursor.x, cursor.y);
        ShowCursor(true);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);;
}


void Render() {
    auto rtv = renderMode == RenderMode::PBR ? g_pRenderTexture->GetRenderTargetView() : g_renderTargetView;

    ID3D11PixelShader* ps = nullptr;

    switch (renderMode) {
    case RenderMode::PBR:
        ps = g_pPBRPixelShader;
        break;
    case RenderMode::NORMAL_DISTR:
        ps = g_pNormalDistPixelShader;
        break;
    case RenderMode::GEOMETRY:
        ps = g_pGeometryPixelShader;
        break;
    case RenderMode::FRESNEL:
        ps = g_pFresnelPixelShader;
        break;
    }
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;

    XMVECTORF32 bgrColor = { 0.05f, 0.05f, 0.1f, 1.0f };
    g_deviceContext->ClearRenderTargetView(rtv, bgrColor.f);
    g_deviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    g_deviceContext->RSSetViewports(1, &vp);

    g_deviceContext->OMSetRenderTargets(1, &rtv, g_pDepthStencilView);
    g_deviceContext->OMSetDepthStencilState(g_pDepthStencilState, 0);

    g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_deviceContext->IASetInputLayout(g_pInputVertexLayout);
    g_deviceContext->IASetVertexBuffers(0, 1, &g_sphereVertexBuffer, &stride, &offset);
    g_deviceContext->IASetIndexBuffer(g_sphereIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    g_View = g_Camera.GetViewMatrix();
    XMFLOAT4 cameraPosition;
    XMStoreFloat4(&cameraPosition, g_Camera.Pos);

    GeometryOperatorsConstantBuffer geometryCB;
    geometryCB.world = XMMatrixTranspose(g_World);
    geometryCB.worldNormals = XMMatrixInverse(nullptr, g_World);
    geometryCB.view = XMMatrixTranspose(g_View);
    geometryCB.projection = XMMatrixTranspose(g_Projection);
    geometryCB.cameraPos = cameraPosition;

    g_deviceContext->UpdateSubresource(g_geometryConstantBuffer, 0, nullptr, &geometryCB, 0, 0);

    float sphereColor[4];
    for (int i = 0; i < 4; ++i) {
        sphereColor[i] = powf(sphereColorRGB[i], 2.2f);
    }
    sphereColorSRGB = XMFLOAT4(sphereColor);
    SpherePropsConstantBuffer sprops_cbuffer = { sphereColorSRGB, roughness, metalness };
    g_deviceContext->UpdateSubresource(g_spropsConstantBuffer, 0, nullptr, &sprops_cbuffer, 0, 0);
    ExposureConstantBuffer exposureCB;
    exposureCB.exposureMult = exposureMult;
    LightsConstantBuffer lightsConstBuffer;
    for (int i = 0; i < NUM_LIGHTS; i++) {
        lightsConstBuffer.lightPos[i] = lights[i].Pos;
        lightsConstBuffer.lightColor[i] = lights[i].Color;
        XMFLOAT4 att(lights[i].constAttenuation, lights[i].quadraticAttenuation, 0.0f, 0.0f);
        lightsConstBuffer.lightAttenuation[i] = att;
        lightsConstBuffer.lightIntensity[0] = intensity;
    }
    
    g_deviceContext->UpdateSubresource(g_lightConstantBuffer, 0, nullptr, &lightsConstBuffer, 0, 0);
    g_deviceContext->UpdateSubresource(g_exposureConstantBuffer, 0, nullptr, &exposureCB, 0, 0);
    g_deviceContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_deviceContext->VSSetConstantBuffers(0, 1, &g_geometryConstantBuffer);
    g_deviceContext->PSSetShader(ps, nullptr, 0);
    g_deviceContext->PSSetConstantBuffers(0, 1, &g_geometryConstantBuffer);
    g_deviceContext->PSSetConstantBuffers(1, 1, &g_spropsConstantBuffer);
    g_deviceContext->PSSetConstantBuffers(2, 1, &g_lightConstantBuffer);
    g_deviceContext->PSSetConstantBuffers(3, 1, &g_exposureConstantBuffer);

    g_deviceContext->PSSetShaderResources(0, 1, &g_pIrradiance);
    g_deviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

    g_deviceContext->DrawIndexed(numIndices, 0, 0);

    g_deviceContext->IASetVertexBuffers(0, 1, &g_environmentVertexBuffer, &stride, &offset);
    g_deviceContext->IASetIndexBuffer(g_environmentIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    auto scale = XMMatrixScaling(5, 5, 5);
    auto translation = XMMatrixTranslation(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    g_sphereWorld = scale * translation;

    geometryCB.world = XMMatrixTranspose(g_sphereWorld);

    g_deviceContext->UpdateSubresource(g_geometryConstantBuffer, 0, nullptr, &geometryCB, 0, 0);

    g_deviceContext->VSSetShader(g_pEnvironmentVertexShader, nullptr, 0);
    g_deviceContext->VSSetConstantBuffers(0, 1, &g_geometryConstantBuffer);
    g_deviceContext->PSSetShader(g_pEnvironmentPixelShader, nullptr, 0);

    g_deviceContext->PSSetShaderResources(0, 1, &g_pTextureRV);
    g_deviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

    g_deviceContext->DrawIndexed(numIndicesEnv, 0, 0);

    if (renderMode == RenderMode::PBR) {

        g_deviceContext->ClearRenderTargetView(g_renderTargetView, Colors::Black.f);

        g_deviceContext->RSSetViewports(1, &vp);

        g_deviceContext->OMSetRenderTargets(1, &g_renderTargetView, nullptr);

        g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        g_deviceContext->IASetInputLayout(nullptr);

        g_deviceContext->VSSetShader(g_pVertexShaderCopy, nullptr, 0);
        g_deviceContext->PSSetShader(g_pPixelShaderToneMapping, nullptr, 0);

        auto srv = g_pRenderTexture->GetShaderResourceView();
        g_deviceContext->PSSetShaderResources(0, 1, &srv);
        g_deviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

        g_deviceContext->Draw(4, 0);

        ID3D11ShaderResourceView* nullsrv[] = { nullptr };
        g_deviceContext->PSSetShaderResources(0, 1, nullsrv);
    }


    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Scene parameters");
    ImGui::Text("Scene");
    ImGui::ListBox("Render mode", (int*)(&renderMode), renderModes, NUM_RENDER_MODES);
    ImGui::Text("Object");
    ImGui::SliderFloat("Roughness", &roughness, 0, 1);
    ImGui::SliderFloat("Metalness", &metalness, 0, 1);
    ImGui::SliderFloat("Light Intensity (PBR)", &intensity, 0, 3000);
    ImGui::SliderFloat("Exposure", &exposureMult, 1, 50);
    ImGui::ColorEdit3("Metal F0", sphereColorRGB);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pSwapChain->Present(1, 0);


}
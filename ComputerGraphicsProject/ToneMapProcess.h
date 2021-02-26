#pragma once
#include <windows.h>
#include <d3d11_1.h>

class ToneMapProcess
{
public:

    HRESULT CreateResources(ID3D11Device* device);

    ID3D11SamplerState* pSamplerState;

    void Clean();
};
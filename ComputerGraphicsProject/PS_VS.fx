Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);


cbuffer cbChangesOnCameraAction : register(b0)
{
    matrix View;
    float4 Eye;
};

cbuffer cbChangeOnResize : register(b1)
{
    matrix Projection;
};

cbuffer cbChangesEveryFrame : register(b2)
{
    matrix World;
};

cbuffer cbLightConstantBuffer :register(b3)
{
    float4 vLightPos[3];
    float4 vLightColor[3];
    float4 vOutputColor;
};

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent: TANGENT;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float3 ViewDir : TEXCOORD5;
    float3 normal : NORMAL;
    float3 tangent: TANGENT;
    float3 WorldPos : TEXTCOORD1;
    float3 LightDir1 : TEXCOORD2;
    float3 LightDir2 : TEXCOORD3;
    float3 LightDir3 : TEXCOORD4;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    output.Pos = mul(input.Pos, World);
    output.ViewDir = normalize(Eye.xyz - output.Pos.xyz);
    output.LightDir1 = output.Pos.xyz - vLightPos[0];
    output.LightDir2 = output.Pos.xyz - vLightPos[1];
    output.LightDir3 = output.Pos.xyz - vLightPos[2];

    output.Pos = mul(input.Pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);
    output.Tex = input.Tex;
    output.normal = mul(input.normal, World);
    output.tangent = mul(input.tangent, World);
    return output;
}

float f(float4 col)
{
    return (col.x + col.y + col.z) / 3.0;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float Attenuation(float3 lightDir)
{
    float d = length(lightDir);
    float val = 3000 / (1.0f + 10.0f * d + 100.0f * d * d * d);
    if (val < 0.3)
        val = 0;
    return val;
}

float4 DeepColor(PS_INPUT input) {
    float2 t = input.Tex;
    float2 dx = float2(1.0 / 256.0, 0.0);
    float2 dy = float2(0.0, 1.0 / 256.0);
    float3 N;
    float scale = 12.0;
    N.x = scale * (f(txDiffuse.Sample(samLinear, t + dx)) - f(txDiffuse.Sample(samLinear, t - dx)));
    N.y = scale * (f(txDiffuse.Sample(samLinear, t + dy)) - f(txDiffuse.Sample(samLinear, t - dy)));
    N.z = 1.0;
    normalize(N);
    //Create the biTangent
    float3 biTangent = cross(input.normal, input.tangent);
    //Create the "Texture Space"
    float3x3 texSpace = float3x3(input.tangent, biTangent, input.normal);
    //Convert normal from normal map to texture space and store in input.normal
    float3 normal = normalize(mul(N, texSpace));
    float4 finalColor = txDiffuse.Sample(samLinear, t);
    finalColor = saturate(dot(float3(0.0, 0.0, -1.0), normal) * finalColor);
    return finalColor;
}


float4 PS(PS_INPUT input) : SV_Target
{
    float4 color1, color2, color3;
    float atten1, atten2, atten3;
    float bright1, bright2, bright3;
    float3 lightDir1, lightDir2, lightDir3;
    float3 h1, h2, h3;
    float p1, p2, p3;

    atten1 = Attenuation(input.LightDir1);
    atten2 = Attenuation(input.LightDir2);
    atten3 = Attenuation(input.LightDir3);

    p1 = 2.0f;
    p2 = 2.0f;
    p3 = 2.0f;

    bright1 = 10;
    bright2 = 1;
    bright3 = 1;

    lightDir1 = normalize(input.LightDir1);
    lightDir2 = normalize(input.LightDir2);
    lightDir3 = normalize(input.LightDir3);

    h1 = normalize(input.ViewDir + input.LightDir1);
    h2 = normalize(input.ViewDir + input.LightDir2);
    h3 = normalize(input.ViewDir + input.LightDir3);

    color1 = vLightColor[0] * atten1 * bright1 * (1 + dot(input.normal, lightDir1) + pow(dot(input.normal, h1), p1));
    color2 = vLightColor[1] * atten2 * bright2 * (1 + dot(input.normal, lightDir2) + pow(dot(input.normal, h2), p2));
    color3 = vLightColor[2] * atten3 * bright3 * (1 + dot(input.normal, lightDir3) + pow(dot(input.normal, h3), p3));
    return txDiffuse.Sample(samLinear, input.Tex) * (color1 + color2 + color3);

}


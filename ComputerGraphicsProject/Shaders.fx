#define PI 3.14159265f
#define EPS 1e-3f
#define NUM_LIGHTS 1

Texture2D txDiffuse : register(t0);

TextureCube environment : register(t0);

SamplerState sampleLinear : register(s0);

cbuffer GeometryOperators : register(b0)
{
    matrix world;
    matrix worldNorms;
    matrix view;
    matrix projection;
    float4 cameraPos;
};

cbuffer SphereProps : register(b1)
{
    float4 colorBase;
    float roughness; 
    float metalness;
};

cbuffer Lights : register(b2)
{
    float4 ambientLight;
    float4 lightPos[NUM_LIGHTS];
    float4 lightColor[NUM_LIGHTS];
    float4 lightAtten[NUM_LIGHTS];
    float lightIntensity[NUM_LIGHTS];
};

struct VsInput {
    float4 localPos : POS;
    float3 localNorm : NOR;
};

struct VsOutput {
    float4 projectedPos : SV_POSITION;
    float4 worldPos : TEXCOORD0;
    float3 worldNorm : TEXCOORD1;
};

struct VsCopyOutput {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

struct VsEnvironmentOutput {
    float4 pos : SV_POSITION;
    float3 tex : TEXCOORD;
};

VsOutput vsMain(VsInput input) {
    VsOutput output = (VsOutput)0;
    output.worldPos = mul(input.localPos, world);
    output.worldNorm = normalize(mul(float4(input.localNorm, 1), worldNorms).xyz);

    output.projectedPos = mul(output.worldPos, view);
    output.projectedPos = mul(output.projectedPos, projection);

    return output;
}

float3 projectedRadiance(int index, float3 pos, float3 normal)
{
    const float deg = 1.0f;
    const float3 lightDir = lightPos[index].xyz - pos;
    const float dist = length(lightDir);
    const float dotMult = pow(saturate(dot(lightDir / dist, normal)), deg);
    float att = lightAtten[index].x + lightAtten[index].y * dist;
    att += lightAtten[index].z * dist * dist;
    return lightIntensity[index] * dotMult / att * lightColor[index].rgb;
}

float ndf(float3 normal, float3 halfway) {
    const float squaredRoughness = clamp(roughness * roughness, EPS, 1.0f);
    const float direction = saturate(dot(normal, halfway));
    return squaredRoughness / PI / pow(direction * direction * (squaredRoughness - 1.0f) + 1.0f, 2);
}

float geometryFunction(float3 normal, float3 dir)
{
    const float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
    const float dotMult = saturate(dot(normal, dir));
    return dotMult / (dotMult * (1.0f - k) + k);
}

float geometryFunction2dir(float3 normal, float3 lightDir, float3 cameraDir)
{
    return geometryFunction(normal, lightDir) * geometryFunction(normal, cameraDir);
}

float3 fresnelFunction(float3 cameraDir, float3 halfway)
{
    const float3 F0_noncond = 0.04f;
    const float3 F0 = (1.0 - metalness) * F0_noncond + metalness * colorBase.rgb;
    const float dotMult = saturate(dot(cameraDir, halfway));
    return F0 + (1.0f - F0) * pow(1.0f - dotMult, 5);
}

float3 brdf(float3 normal, float3 lightDir, float3 cameraDir)
{
    const float3 halfway = normalize(cameraDir + lightDir);
    const float camdir_dot_norm = saturate(dot(cameraDir, normal));
    const float ldir_dot_norm = saturate(dot(lightDir, normal));
    const float D = ndf(normal, halfway);
    const float G = geometryFunction2dir(normal, lightDir, cameraDir);
    const float3 F = fresnelFunction(cameraDir, halfway);

    const float3 fLamb = (1.0f - F) * (1.0f - metalness) * colorBase.rgb / PI;
    const float3 fCt = D * F * G / (4.0f * ldir_dot_norm * camdir_dot_norm + EPS);
    return fLamb + fCt;
}


float4 psLambert(VsOutput input) : SV_TARGET{
    float3 color = colorBase.rgb + ambientLight.rgb;
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        color += projectedRadiance(i, input.worldPos.xyz, normalize(input.worldNorm));
    }
    return float4(color, colorBase.a);
}

float4 psNDF(VsOutput input) : SV_TARGET{
    const float3 pos = input.worldPos.xyz;
    const float3 cameraDir = normalize(cameraPos.xyz - pos);
    float colorGrayscale = 0.0f;
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        const float3 lightDir = normalize(lightPos[i].xyz - pos);
        const float3 halfway = normalize(cameraDir + lightDir);
        colorGrayscale += ndf(normalize(input.worldNorm), halfway);
    }
    return colorGrayscale;
}

float4 psGeometry(VsOutput input) : SV_TARGET{
    const float3 pos = input.worldPos.xyz;
    const float3 cameraDir = normalize(cameraPos.xyz - pos);
    float colorGrayscale = 0.0f;
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        const float3 lightDir = normalize(lightPos[i].xyz - pos);
        colorGrayscale += geometryFunction2dir(normalize(input.worldNorm), lightDir, cameraDir);
    }
    return colorGrayscale;
}

float4 psFresnel(VsOutput input) : SV_TARGET{
    const float3 pos = input.worldPos.xyz;
    const float3 cameraDir = normalize(cameraPos.xyz - pos);
    float3 color = 0.0f;
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        const float3 lightDir = normalize(lightPos[i].xyz - pos);
        const float3 halfway = normalize(cameraDir + lightDir);
        color += fresnelFunction(cameraDir, halfway) * (dot(lightDir, normalize(input.worldNorm)) > 0.0f);
    }
    return float4(color, colorBase.a);
}

float4 psPBR(VsOutput input) : SV_TARGET{
    const float3 pos = input.worldPos.xyz;
    const float3 cameraDir = normalize(cameraPos.xyz - pos);
    float3 color = colorBase.rgb + ambientLight.rgb;
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        const float3 lightDir = normalize(lightPos[i].xyz - pos);
        const float3 halfway = normalize(cameraDir + lightDir);
        const float3 radiance = projectedRadiance(i, pos, normalize(input.worldNorm));
        color += radiance * brdf(normalize(input.worldNorm), lightDir, cameraDir);
    }
    return float4(color, colorBase.a);
}


VsCopyOutput vsCopyMain(uint input : SV_VERTEXID) {
    VsCopyOutput output = (VsCopyOutput)0;
    output.tex = float2(input & 1, input >> 1);
    output.pos = float4((output.tex.x - 0.5f) * 2, -(output.tex.y - 0.5f) * 2, 0, 1);
    return output;
}

float4 psLogLuminanceMain(VsCopyOutput input) : SV_TARGET{
    float4 p = txDiffuse.Sample(sampleLinear, input.tex);
    float l = 0.2126 * p.r + 0.7151 * p.g + 0.0722 * p.b;
    return log(l + 1);
}

static const float AverageBrightness = 0.5;

float3 Uncharted2Tonemap(float3 x)
{
    static const float a = 0.1;
    static const float b = 0.50;
    static const float c = 0.1;
    static const float d = 0.20;
    static const float e = 0.02;
    static const float f = 0.30;

    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

float Exposure()
{
    float brightness = AverageBrightness;
    float keyValue = 1.03 - 2 / (2 + log10(brightness + 1));
    return keyValue / brightness;
}

float3 TonemapFilmic(float3 color)
{
    static const float W = 11.2;

    float e = Exposure();
    float3 curr = Uncharted2Tonemap(e * color);
    float3 whiteScale = 1.0f / Uncharted2Tonemap(W);
    return curr * whiteScale;
}

float3 LinearToSRGB(float3 color)
{
    return pow(abs(color), 1 / 2.2f);
}

float4 psToneMappingMain(VsCopyOutput input) : SV_TARGET{
    float4 color = txDiffuse.Sample(sampleLinear, input.tex);
    return float4(LinearToSRGB(TonemapFilmic(color.xyz)), color.a);
}

VsEnvironmentOutput vsEnvironment(VsInput input) {
    VsEnvironmentOutput output = (VsEnvironmentOutput)0;
    output.pos = mul(input.localPos, world);
    output.pos = mul(output.pos, view);
    output.pos = mul(output.pos, projection);
    output.pos = output.pos.xyww;
    output.tex = input.localPos.xyz;
    return output;
}

float4 psEnvironment(VsEnvironmentOutput input) : SV_TARGET{
    return environment.Sample(sampleLinear, input.tex);
}

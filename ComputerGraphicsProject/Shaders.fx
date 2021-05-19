#define PI 3.14159265f
#define EPS 1e-3f
#define NUM_LIGHTS 1

#define N1 1000
#define N2 250

Texture2D txDiffuse : register(t0);

TextureCube environment : register(t0); 

TextureCube irradiance : register(t0);
TextureCube prefiltered : register(t1);
Texture2D preintegrated : register(t2);

SamplerState minMagMipLinear : register(s0);
SamplerState minMagMipLinearBorder : register(s1);


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
    float4 lightPos[NUM_LIGHTS];
    float4 lightColor[NUM_LIGHTS];
    float4 lightAtten[NUM_LIGHTS];
    float lightIntensity[NUM_LIGHTS];
};

cbuffer ExposureBuf :register(b3) {
    float exposureMult;
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

float3 ambient(float3 v, float3 n)
{
    const float3 F0_noncond = 0.04f;
    const float MAX_REFLECTION_LOD = 4.0;
    
    float3 r = normalize(reflect(-v, n));

    float3 prefilteredColor = prefiltered.SampleLevel(minMagMipLinear, r, roughness * MAX_REFLECTION_LOD).rgb;
    float3 F0 = lerp(F0_noncond, colorBase.rgb, metalness);

    const float dotMult = saturate(dot(v, n));
    float3 F = F0 + (max(1.0f - roughness, F0) - F0) * pow(1.0f - dotMult, 5);

    float2 envBRDF = preintegrated.Sample(minMagMipLinearBorder, float2(max(dot(n, v), 0.0), roughness)).xy;
    float3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    float3 kD = float3(1.0f, 1.0f, 1.0f) - F;
    float3 diffuse = irradiance.SampleLevel(minMagMipLinear, n, 0).rgb * colorBase.xyz;
    return kD * (1.0f - metalness) * diffuse + specular;
}

float3 projectedRadiance(int index, float3 pos, float3 normal)
{
    const float deg = 1.0f;
    const float3 lightDir = lightPos[index].xyz - pos;
    const float dist = length(lightDir);
    const float dotMult = pow(saturate(dot(lightDir / dist, normal)), deg);
    float att = lightAtten[index].x + lightAtten[index].y * dist * dist;
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
    float3 color = colorBase.rgb;
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
    float3 color = 0.0f;
    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        const float3 lightDir = normalize(lightPos[i].xyz - pos);
        const float3 halfway = normalize(cameraDir + lightDir);
        const float3 radiance = projectedRadiance(i, pos, normalize(input.worldNorm));
        color += radiance * brdf(normalize(input.worldNorm), lightDir, cameraDir);
    }
    color += ambient(cameraDir, normalize(input.worldNorm));
    return float4(color, colorBase.a);
}

float4 psCubeMap(VsOutput input) : SV_TARGET{
    float3 normal = normalize(input.worldPos.xyz);
    float u = 1.0 - atan2(normal.z, normal.x) / (2 * PI);
    float v = 0.5 - asin(normal.y) / PI;
    return txDiffuse.Sample(minMagMipLinear, float2(u, v));

}

float4 psIrradianceMap(VsOutput input) : SV_TARGET{
    float3 normal = normalize(input.worldPos.xyz);
    float3 dir = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(dir, normal));
    float3 bitangent = cross(normal, tangent);
    float3 irradiance = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < N1; i++) {
        for (int j = 0; j < N2; j++) {
            float phi = i * (2 * PI / N1);
            float theta = j * (PI / 2 / N2);
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * tangent + tangentSample.y * bitangent + tangentSample.z * normal;
            irradiance += environment.Sample(minMagMipLinear, sampleVec).rgb * cos(theta) * sin(theta);

        }
    }
    irradiance = PI * irradiance / (N1 * N2);
    return float4(irradiance, 1.0);
}

VsCopyOutput vsCopyMain(uint input : SV_VERTEXID) {
    VsCopyOutput output = (VsCopyOutput)0;
    output.tex = float2(input & 1, input >> 1);
    output.pos = float4((output.tex.x - 0.5f) * 2, -(output.tex.y - 0.5f) * 2, 0, 1);
    return output;
}

float4 psLogLuminanceMain(VsCopyOutput input) : SV_TARGET{
    float4 p = txDiffuse.Sample(minMagMipLinear, input.tex);

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

    float e = Exposure()*exposureMult;
    float3 curr = Uncharted2Tonemap(e * color);
    float3 whiteScale = 1.0f / Uncharted2Tonemap(W);
    return curr * whiteScale;
}

float3 LinearToSRGB(float3 color)
{
    return pow(abs(color), 1 / 2.2f);
}

float4 psToneMappingMain(VsCopyOutput input) : SV_TARGET{
    float4 color = txDiffuse.Sample(minMagMipLinear, input.tex);

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
    return environment.SampleLevel(minMagMipLinear, input.tex, 0);
}


float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 norm, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.z = sin(phi) * sinTheta;
    H.y = cosTheta;
    float3 up = abs(norm.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, norm));
    float3 bitangent = cross(norm, tangent);
    float3 sampleVec = tangent * H.x + bitangent * H.z + norm * H.y;
    return normalize(sampleVec);
}

float4 psPrefilteredColor(VsOutput input) : SV_TARGET{
    float3 norm = normalize(input.worldPos.xyz);
    float3 view = norm;
    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0, 0, 0);
    static const int SAMPLE_COUNT = 1024;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, norm, roughness);
        float3 L = normalize(2.0 * dot(view, H) * H - view);
        float ndotl = max(dot(norm, L), 0.0);
        float ndoth = max(dot(norm, H), 0.0);
        float hdotv = max(dot(H, view), 0.0);
        float D = ndf(norm, H);
        float pdf = (D * ndoth / (4.0 * hdotv)) + 0.0001;
        float resolution = 512.0;
        float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
        float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
        float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
        if (ndotl > 0.0) {
            prefilteredColor += environment.SampleLevel(minMagMipLinear, L, mipLevel).rgb * ndotl;
            totalWeight += ndotl;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;
    return float4(prefilteredColor, 1);
}

float SchlickGGX(float3 n, float3 v, float k) {
    const float dotMult = saturate(dot(n, v));
    return dotMult / (dotMult * (1.0f - k) + k);
}

float GeometrySmith(float3 n, float3 v, float3 l, float roughness) {
    const float k = roughness * roughness / 2.0f;
    return SchlickGGX(n, v, k) * SchlickGGX(n, l, k);
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.z = 0.0;
    V.y = NdotV;
    float A = 0.0;
    float B = 0.0;
    float3 N = float3(0.0, 1.0, 0.0);
    static const int SAMPLE_COUNT = 1024;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.y, 0.0);
        float NdotH = max(H.y, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        if (NdotL > 0.0) {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}

float4 psPreintegratedBRDF(VsOutput input) : SV_TARGET{
    return float4(IntegrateBRDF(input.worldPos.x, 1 - input.worldPos.y), 0, 1);
}

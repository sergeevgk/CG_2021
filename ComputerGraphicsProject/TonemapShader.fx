Texture2D<float4> sourceTexture : register(t0);

SamplerState samState : register(s0);

cbuffer AverageBrightnessBuffer : register(b0)
{
    float AverageBrightness;
}

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD;
};

PS_INPUT VS_COPY(uint input : SV_VERTEXID)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Tex = float2(input & 1, input >> 1);
    output.Pos = float4((output.Tex.x - 0.5f) * 2, -(output.Tex.y - 0.5f) * 2, 0, 1);
    return output;
}

float4 PS_COPY(PS_INPUT input) : SV_TARGET
{
    return sourceTexture.Sample(samState, input.Tex);
}

float4 PS_BRIGHTNESS(PS_INPUT input) : SV_TARGET
{
    float4 color = sourceTexture.Sample(samState, input.Tex);
    float l = 0.2126f * color.r + 0.7151f * color.g + 0.0722f * color.b;
    return log(l + 1);
}

float Exposure()
{
    float brightness = AverageBrightness;
    float keyValue = 1.03 - 2 / (2 + log10(brightness + 1));
    return keyValue / brightness;
}

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

float4 PS_TONEMAP(PS_INPUT input) : SV_TARGET
{
    float4 color = sourceTexture.Sample(samState, input.Tex);
    return float4(LinearToSRGB(TonemapFilmic(color.xyz)), color.a);
}

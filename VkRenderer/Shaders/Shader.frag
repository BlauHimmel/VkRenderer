#version 450
#extension GL_ARB_separate_shader_objects : enable

const float PI = 3.14159265359;
const int LIGHT_NUM = 8;

layout(binding = 1) uniform LightUniformBufferObject
{
    vec4 LightPosition[LIGHT_NUM];
    vec4 LightColor[LIGHT_NUM];
    vec3 ViewPosition;
} Lighting;

layout(binding = 2) uniform MaterialUniformBufferObject
{
    vec4 Albedo;
    float Metallic;
    float Roughness;
    float Ao;
} Material;

layout(binding = 3) uniform sampler2D AlbedoSampler;

layout(location = 0) in vec4 FragPosition;
layout(location = 1) in vec3 FragColor;
layout(location = 2) in vec3 FragNormal;
layout(location = 3) in vec2 FragTexCoord;
layout(location = 4) in vec3 FragPositionW;

layout(location = 0) out vec4 OutColor;

float DistributionGGX(vec3 N, vec3 H, float Roughness)
{
    float Alpha = Roughness * Roughness;
    float Alpha2 = Alpha * Alpha;
    float NDotH = max(dot(N, H), 0.0);
    float NDotH2 = NDotH * NDotH;

    float Numerator = Alpha2;
    float Denominator = NDotH2 * (Alpha2 - 1.0) + 1.0;
    Denominator = PI * Denominator * Denominator;

    // Prevent divide by zero for roughness=0.0 and NdotH=1.0
    return Numerator / max(Denominator, 0.0001); 
}

float GeometrySchlickGGX(float NDotV, float Roughness)
{
    float R = Roughness + 1.0;
    float K = R * R * 0.125;

    float Numerator = NDotV;
    float Denominator = NDotV * (1.0 - K) + K;

    return Numerator / Denominator;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float Roughness)
{
    float NDotV = max(dot(N, V), 0.0);
    float NDotL = max(dot(N, L), 0.0);
    float GGX1 = GeometrySchlickGGX(NDotV, Roughness);
    float GGX2 = GeometrySchlickGGX(NDotL, Roughness);
    return GGX1 * GGX2;
}

vec3 FresnelSchlick(float NDotV, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - NDotV, 5.0);
}

void main()
{
    vec3 Albedo = (Material.Albedo * texture(AlbedoSampler, FragTexCoord)).xyz;

    vec3 N = normalize(FragNormal);
    vec3 V = normalize(Lighting.ViewPosition.xyz - FragPositionW);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Albedo, Material.Metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < LIGHT_NUM; i++)
    {
        vec3 L = normalize(Lighting.LightPosition[i].xyz - FragPositionW);
        vec3 H = normalize(V + L);

        float Distance = length(Lighting.LightPosition[i].xyz - FragPositionW);
        float Attenuation = 1.0 / (Distance * Distance);
        vec3 Radiance = Lighting.LightColor[i].xyz * Attenuation;

        float NDF = DistributionGGX(N, H, Material.Roughness);
        float G = GeometrySmith(N, V, L, Material.Roughness);
        vec3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 Numerator = NDF * G * F;
        float Denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        // Prevent divide by zero for NdDtV=0.0 or NDotL=0.0
        vec3 Specular = Numerator / max(Denominator, 0.0001);

        vec3 Ks = F;
        vec3 Kd = vec3(1.0) - Ks;
        Kd *= (1.0 - Material.Metallic);

        float NDotL = max(dot(N, L), 0.0);
        Lo += (Kd * Albedo / PI + Specular) * Radiance * NDotL;
    }

    vec3 Ambient = vec3(0.03) * Albedo * Material.Ao;
    vec3 Color = Ambient + Lo;

    Color = Color / (Color + vec3(1.0));
    Color = pow(Color, vec3(1.0/2.2));

    OutColor = vec4(Color, 1.0);
}
/*
struct UBO
{
	float4x4 projectionMatrix;
	float4x4 modelMatrix;
	float4x4 viewMatrix;
};
[[vk::binding(0, 0)]]
ConstantBuffer<UBO> ubo;

struct VertexInput
{
    [[vk::location(0)]] float3 Pos;
    [[vk::location(1)]] float3 Color;
};

struct VSOutput
{
	float4 Pos : SV_POSITION;
	[[vk::location(0)]] float3 Color;
};

[shader("vertex")]
VSOutput vertexMain(VertexInput input)
{
	VSOutput output;
	output.Color = input.Color;
	output.Pos = mul(ubo.projectionMatrix, mul(ubo.viewMatrix, mul(ubo.modelMatrix, float4(input.Pos.xyz, 1.0))));
	return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input)
{
    return float4(1.0, 0.5, 0.2, 1.0);
}
*/


//-------------------------------------------------
struct UBO
{
	float4x4 projectionMatrix;
	float4x4 modelMatrix;
	float4x4 viewMatrix;
};
[[vk::binding(0, 0)]]
ConstantBuffer<UBO> ubo;


struct VertexInput
{
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float3 normal;
};


struct VertexToFragment {
	float4 clipPosition : SV_POSITION;
    [[vk::location(0)]] float3 worldNormal;
    [[vk::location(1)]] float3 worldPosition;
};

[shader("vertex")]
VertexToFragment vertexMain(VertexInput input)
{
    VertexToFragment output;

    float4 worldPos = mul(ubo.viewMatrix, float4(input.position, 1.0));
    output.worldPosition = worldPos.xyz;
    output.worldNormal = normalize(mul(ubo.viewMatrix, float4(input.normal, 1.0))).xyz;

	output.clipPosition = mul(ubo.projectionMatrix, mul(ubo.modelMatrix, worldPos)); 


    return output;
}

struct FragmentOutput {
    [[vk::location(0)]] float4 color;
};

// Constant directional light
static float3 lightDirection = normalize(float3(0.0, 0.0, 2.0)); // Should be normalized
static float3 lightColor = float3(1.0, 0.0, 0.0);
static float3 baseColor = float3(0.05, 0.05, 0.05);

static float3 viewPosition   = float3(0.0, 0.0, -2.5);            // Camera position in world space

[shader("fragment")]
float4 fragmentMain(VertexToFragment input)
{
    // Diffuse component
    float NdotL = max(dot(input.worldNormal, -lightDirection), 0.0);
    float3 diffuse = baseColor + lightColor * NdotL;

    // Specular component
	float3 refl = reflect(lightDirection, input.worldNormal);
	float3 reflex = pow(max(dot(refl, viewPosition), 0.0f), 4.0f);

    return float4(diffuse + reflex, 1.0);
}

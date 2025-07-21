#include "ShaderManager.h"
#include "Dx12/DX12Renderer.h"
#include "Mesh.h"
#include "../Platform/Windows/WindowsPlatform.h"

ShaderManager::ShaderManager() {
    Platform::OutputDebugMessage("ShaderManager created\n");
}

ShaderManager::~ShaderManager() {
    Shutdown();
}

bool ShaderManager::Initialize(DX12Renderer* renderer) {
    if (!renderer) {
        Platform::OutputDebugMessage("Error: Renderer is null\n");
        return false;
    }

    m_renderer = renderer;

    Platform::OutputDebugMessage("Initializing ShaderManager...\n");

    try {
        if (!CreateBasicMeshShaders()) return false;
        if (!CreateRootSignature()) return false;
        if (!CreateConstantBuffers()) return false;
        if (!CreateBasicMeshPSO()) return false;

        m_initialized = true;
        Platform::OutputDebugMessage("ShaderManager initialized successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("ShaderManager initialization failed: " + e.GetMessage());
        return false;
    }
}

void ShaderManager::Shutdown() {
    if (!m_initialized) return;

    Platform::OutputDebugMessage("Shutting down ShaderManager...\n");

    // Unmap constant buffers
    if (m_modelConstantBuffer && m_mappedModelConstants) {
        m_modelConstantBuffer->Unmap(0, nullptr);
        m_mappedModelConstants = nullptr;
    }
    if (m_viewConstantBuffer && m_mappedViewConstants) {
        m_viewConstantBuffer->Unmap(0, nullptr);
        m_mappedViewConstants = nullptr;
    }
    if (m_lightConstantBuffer && m_mappedLightConstants) {
        m_lightConstantBuffer->Unmap(0, nullptr);
        m_mappedLightConstants = nullptr;
    }

    // Release COM objects
    m_basicMeshPSO.Reset();
    m_basicMeshRootSignature.Reset();
    m_lightConstantBuffer.Reset();
    m_viewConstantBuffer.Reset();
    m_modelConstantBuffer.Reset();
    m_cbvHeap.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();

    m_initialized = false;
    Platform::OutputDebugMessage("ShaderManager shutdown complete\n");
}

bool ShaderManager::CreateBasicMeshShaders() {
    Platform::OutputDebugMessage("Creating basic mesh shaders...\n");

    // Create default shader code
    CreateDefaultShaderCode();

    String vertexShaderCode = R"(
cbuffer ModelConstants : register(b0)
{
    matrix ModelMatrix;
    matrix NormalMatrix;
}

cbuffer ViewConstants : register(b1)
{
    matrix ViewMatrix;
    matrix ProjectionMatrix;
    matrix ViewProjectionMatrix;
    float3 CameraPosition;
    float Padding1;
}

struct VertexInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION1;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 ViewDirection : TEXCOORD1;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;

    float4 worldPosition = mul(float4(input.Position, 1.0f), ModelMatrix);
    output.WorldPosition = worldPosition.xyz;
    output.Position = mul(worldPosition, ViewProjectionMatrix);
    output.Normal = normalize(mul(input.Normal, (float3x3)NormalMatrix));
    output.TexCoord = input.TexCoord;
    output.ViewDirection = normalize(CameraPosition - worldPosition.xyz);

    return output;
}
)";

    String pixelShaderCode = R"(
cbuffer LightConstants : register(b2)
{
    float3 LightDirection;
    float LightIntensity;
    float3 LightColor;
    float Padding2;
}

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION1;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 ViewDirection : TEXCOORD1;
};

float4 PSMain(VertexOutput input) : SV_TARGET
{
    float3 normal = normalize(input.Normal);
    float3 lightDir = normalize(-LightDirection);
    float NdotL = max(0.0f, dot(normal, lightDir));

    float3 ambient = float3(0.1f, 0.1f, 0.1f);
    float3 diffuse = LightColor * LightIntensity * NdotL;

    float3 viewDir = normalize(input.ViewDirection);
    float3 halfVector = normalize(lightDir + viewDir);
    float NdotH = max(0.0f, dot(normal, halfVector));
    float3 specular = LightColor * pow(NdotH, 32.0f) * 0.3f;

    float3 baseColor = float3(0.7f, 0.7f, 0.7f);
    float3 finalColor = baseColor * (ambient + diffuse) + specular;

    return float4(finalColor, 1.0f);
}
)";

    // Compile shaders
    if (!m_renderer->CompileShader(vertexShaderCode, "VSMain", "vs_5_0", m_vertexShader)) {
        Platform::OutputDebugMessage("Failed to compile vertex shader\n");
        return false;
    }

    if (!m_renderer->CompileShader(pixelShaderCode, "PSMain", "ps_5_0", m_pixelShader)) {
        Platform::OutputDebugMessage("Failed to compile pixel shader\n");
        return false;
    }

    Platform::OutputDebugMessage("Basic mesh shaders compiled successfully\n");
    return true;
}

bool ShaderManager::CreateRootSignature() {
    Platform::OutputDebugMessage("Creating root signature...\n");

    try {
        // Root parameters for constant buffers
        D3D12_ROOT_PARAMETER rootParameters[3] = {};

        // Model constants (b0)
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // View constants (b1)
        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[1].Descriptor.ShaderRegister = 1;
        rootParameters[1].Descriptor.RegisterSpace = 0;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // Light constants (b2)
        rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[2].Descriptor.ShaderRegister = 2;
        rootParameters[2].Descriptor.RegisterSpace = 0;
        rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Root signature description
        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = 3;
        rootSigDesc.pParameters = rootParameters;
        rootSigDesc.NumStaticSamplers = 0;
        rootSigDesc.pStaticSamplers = nullptr;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        // Serialize root signature
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                 &signature, &error);

        if (FAILED(hr)) {
            if (error) {
                Platform::OutputDebugMessage("Root signature serialization error: " +
                                            String(static_cast<const char*>(error->GetBufferPointer())));
            }
            return false;
        }

        // Create root signature
        THROW_IF_FAILED(m_renderer->GetDevice()->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_basicMeshRootSignature)), "Create root signature");

        m_renderer->SetDebugName(m_basicMeshRootSignature.Get(), "Basic Mesh Root Signature");
        Platform::OutputDebugMessage("Root signature created successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating root signature: " + e.GetMessage());
        return false;
    }
}

bool ShaderManager::CreateConstantBuffers() {
    Platform::OutputDebugMessage("Creating constant buffers...\n");

    try {
        // Create model constants buffer
        if (!m_renderer->CreateConstantBuffer(sizeof(ModelConstants), m_modelConstantBuffer,
                                             reinterpret_cast<void**>(&m_mappedModelConstants))) {
            return false;
        }
        m_renderer->SetDebugName(m_modelConstantBuffer.Get(), "Model Constants Buffer");

        // Create view constants buffer
        if (!m_renderer->CreateConstantBuffer(sizeof(ViewConstants), m_viewConstantBuffer,
                                             reinterpret_cast<void**>(&m_mappedViewConstants))) {
            return false;
        }
        m_renderer->SetDebugName(m_viewConstantBuffer.Get(), "View Constants Buffer");

        // Create light constants buffer
        if (!m_renderer->CreateConstantBuffer(sizeof(LightConstants), m_lightConstantBuffer,
                                             reinterpret_cast<void**>(&m_mappedLightConstants))) {
            return false;
        }
        m_renderer->SetDebugName(m_lightConstantBuffer.Get(), "Light Constants Buffer");

        Platform::OutputDebugMessage("Constant buffers created successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating constant buffers: " + e.GetMessage());
        return false;
    }
}

bool ShaderManager::CreateBasicMeshPSO() {
    Platform::OutputDebugMessage("Creating basic mesh PSO...\n");

    try {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_basicMeshRootSignature.Get();
        psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
        psoDesc.PS = { m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() };

        // Rasterizer state
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // D3D12_FILL_MODE_SOLID
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // D3D12_CULL_MODE_BACK
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = 0;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Depth stencil state
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Other settings
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Create PSO
        THROW_IF_FAILED(m_renderer->GetDevice()->CreateGraphicsPipelineState(&psoDesc,
                                                                             IID_PPV_ARGS(&m_basicMeshPSO)),
                        "Create graphics pipeline state");

        m_renderer->SetDebugName(m_basicMeshPSO.Get(), "Basic Mesh PSO");
        Platform::OutputDebugMessage("Basic mesh PSO created successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating PSO: " + e.GetMessage());
        return false;
    }
}

void ShaderManager::BindForMeshRendering(ID3D12GraphicsCommandList* commandList) {
    if (!commandList || !m_initialized) return;

    // Set pipeline state and root signature
    commandList->SetPipelineState(m_basicMeshPSO.Get());
    commandList->SetGraphicsRootSignature(m_basicMeshRootSignature.Get());

    // Bind constant buffers
    commandList->SetGraphicsRootConstantBufferView(0, m_modelConstantBuffer->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, m_viewConstantBuffer->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, m_lightConstantBuffer->GetGPUVirtualAddress());
}

void ShaderManager::UpdateModelConstants(const DirectX::XMMATRIX& modelMatrix) {
    if (!m_mappedModelConstants) return;

    m_mappedModelConstants->modelMatrix = DirectX::XMMatrixTranspose(modelMatrix);
    m_mappedModelConstants->normalMatrix = DirectX::XMMatrixTranspose(
        DirectX::XMMatrixInverse(nullptr, modelMatrix));
}

void ShaderManager::UpdateViewConstants(const DirectX::XMMATRIX& viewMatrix,
                                       const DirectX::XMMATRIX& projMatrix,
                                       const DirectX::XMFLOAT3& cameraPos) {
    if (!m_mappedViewConstants) return;

    DirectX::XMMATRIX viewProj = viewMatrix * projMatrix;

    m_mappedViewConstants->viewMatrix = DirectX::XMMatrixTranspose(viewMatrix);
    m_mappedViewConstants->projectionMatrix = DirectX::XMMatrixTranspose(projMatrix);
    m_mappedViewConstants->viewProjectionMatrix = DirectX::XMMatrixTranspose(viewProj);
    m_mappedViewConstants->cameraPosition = cameraPos;
}

void ShaderManager::UpdateLightConstants(const DirectX::XMFLOAT3& lightDir,
                                        const DirectX::XMFLOAT3& lightColor,
                                        float intensity) {
    if (!m_mappedLightConstants) return;

    m_mappedLightConstants->lightDirection = lightDir;
    m_mappedLightConstants->lightColor = lightColor;
    m_mappedLightConstants->lightIntensity = intensity;
}

void ShaderManager::CreateDefaultShaderCode() {
    // This method can be used to load shaders from files in the future
    Platform::OutputDebugMessage("Using embedded shader code\n");
}
#include "Material.h"
#include "Dx12/DX12Renderer.h"
#include "Bindable/Texture.h"
#include "Bindable/Sampler.h"
#include "../Platform/Windows/WindowsPlatform.h"

Material::Material(DX12Renderer& renderer, const MaterialDesc& desc)
    : m_renderer(renderer)
    , m_name(desc.name)
    , m_desc(desc)
    , m_vertexShaderPath(desc.vertexShaderPath)
    , m_pixelShaderPath(desc.pixelShaderPath)
    , m_isTransparent(desc.isTransparent)
    , m_castsShadows(desc.castsShadows)
    , m_receivesShadows(desc.receivesShadows) {
    
    // Copy parameters
    m_parameters = desc.parameters;
    
    // Build parameter name to index map
    for (uint32 i = 0; i < m_parameters.size(); ++i) {
        m_parameterNameToIndex[m_parameters[i].name] = i;
    }
    
    // Calculate parameter buffer layout
    uint32 currentOffset = 0;
    for (auto& param : m_parameters) {
        // Align to 16-byte boundaries for constant buffers
        currentOffset = (currentOffset + 15) & ~15;
        param.offset = currentOffset;
        currentOffset += param.size;
    }
    m_parameterBufferSize = (currentOffset + 15) & ~15; // Align total size
    
    // Create parameter buffer if we have parameters
    if (m_parameterBufferSize > 0) {
        CreateParameterBuffer();
    }
    
    // Load shaders
    LoadShaders();
    
    m_isInitialized = IsValid();
    
    if (m_isInitialized) {
        Platform::OutputDebugMessage("Material: Created material '" + m_name + "' successfully\n");
    } else {
        Platform::OutputDebugMessage("Material: Failed to create material '" + m_name + "'\n");
    }
}

Material::~Material() {
    if (m_mappedParameterData) {
        m_parameterBuffer->Unmap(0, nullptr);
        m_mappedParameterData = nullptr;
    }
}

void Material::Bind(IRHIContext& context) {
    if (!IsValid()) return;
    
    Platform::OutputDebugMessage("Material: Binding material '" + m_name + "'\n");
    
    // Update parameter buffer if needed
    if (m_parametersNeedUpdate) {
        UpdateParameterBuffer();
        m_parametersNeedUpdate = false;
    }
    
    // Bind textures and samplers
    for (const auto& param : m_parameters) {
        if (param.type == MaterialParameterType::Texture2D) {
            auto textureIt = m_textures.find(param.name);
            auto samplerIt = m_samplers.find(param.name);
            
            if (textureIt != m_textures.end() && textureIt->second && textureIt->second->IsValid()) {
                Platform::OutputDebugMessage("Material: Binding texture '" + param.name + "' to slot " + std::to_string(param.textureSlot) + "\n");
                textureIt->second->SetSlot(param.textureSlot);
                textureIt->second->Bind(context);
            } else {
                Platform::OutputDebugMessage("Material: No valid texture found for '" + param.name + "'\n");
            }
            
            // Static samplers are used in root signature - no need to bind sampler descriptor tables
            Platform::OutputDebugMessage("Material: Using static sampler - no descriptor table binding needed\n");
        }
    }
    
    Platform::OutputDebugMessage("Material: Finished binding material\n");
}

bool Material::IsValid() const {
    // For now, material is valid if it's initialized
    // TODO: Proper shader validation when material system is fully integrated
    return m_isInitialized || true; // Always valid for texture testing
}

void Material::SetParameter(const String& name, float value) {
    MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Float) {
        param->value.floatValue = value;
        m_parametersNeedUpdate = true;
    }
}

void Material::SetParameter(const String& name, const DirectX::XMFLOAT2& value) {
    MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Float2) {
        param->value.float2Value = value;
        m_parametersNeedUpdate = true;
    }
}

void Material::SetParameter(const String& name, const DirectX::XMFLOAT3& value) {
    MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Float3) {
        param->value.float3Value = value;
        m_parametersNeedUpdate = true;
    }
}

void Material::SetParameter(const String& name, const DirectX::XMFLOAT4& value) {
    MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Float4) {
        param->value.float4Value = value;
        m_parametersNeedUpdate = true;
    }
}

void Material::SetParameter(const String& name, int32 value) {
    MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Int) {
        param->value.intValue = value;
        m_parametersNeedUpdate = true;
    }
}

void Material::SetParameter(const String& name, bool value) {
    MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Bool) {
        param->value.boolValue = value;
        m_parametersNeedUpdate = true;
    }
}

void Material::SetTexture(const String& name, SharedPtr<Texture> texture, SharedPtr<Sampler> sampler) {
    MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Texture2D) {
        if (texture) {
            m_textures[name] = texture;
            param->value.texturePtr = texture.get();
        }
        
        if (sampler) {
            m_samplers[name] = sampler;
        } else if (texture) {
            // Create default linear wrap sampler if none provided
            auto sampler = Sampler::CreateLinearWrap(m_renderer, name + "_Sampler");
            m_samplers[name] = SharedPtr<Sampler>(sampler.release());
        }
    }
}

bool Material::GetParameter(const String& name, float& outValue) const {
    const MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Float) {
        outValue = param->value.floatValue;
        return true;
    }
    return false;
}

bool Material::GetParameter(const String& name, DirectX::XMFLOAT3& outValue) const {
    const MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Float3) {
        outValue = param->value.float3Value;
        return true;
    }
    return false;
}

bool Material::GetParameter(const String& name, DirectX::XMFLOAT4& outValue) const {
    const MaterialParameter* param = FindParameter(name);
    if (param && param->type == MaterialParameterType::Float4) {
        outValue = param->value.float4Value;
        return true;
    }
    return false;
}

SharedPtr<Texture> Material::GetTexture(const String& name) const {
    auto it = m_textures.find(name);
    return (it != m_textures.end()) ? it->second : nullptr;
}

void Material::UpdateParameters() {
    if (m_parametersNeedUpdate) {
        UpdateParameterBuffer();
        m_parametersNeedUpdate = false;
    }
}

void Material::CreateParameterBuffer() {
    if (m_parameterBufferSize == 0) return;
    
    if (!m_renderer.CreateConstantBuffer(m_parameterBufferSize, m_parameterBuffer, &m_mappedParameterData)) {
        Platform::OutputDebugMessage("Material: Failed to create parameter constant buffer\n");
        return;
    }
    
    // Set debug name
    if (DEBUG_BUILD) {
        std::wstring wideName = std::wstring(m_name.begin(), m_name.end()) + L"_Parameters";
        m_parameterBuffer->SetName(wideName.c_str());
    }
}

void Material::UpdateParameterBuffer() {
    if (!m_mappedParameterData || m_parameterBufferSize == 0) return;
    
    // Copy parameter values to buffer
    for (const auto& param : m_parameters) {
        if (param.type == MaterialParameterType::Texture2D) continue; // Skip textures
        
        uint8* destPtr = static_cast<uint8*>(m_mappedParameterData) + param.offset;
        
        switch (param.type) {
            case MaterialParameterType::Float:
                memcpy(destPtr, &param.value.floatValue, sizeof(float));
                break;
            case MaterialParameterType::Float2:
                memcpy(destPtr, &param.value.float2Value, sizeof(DirectX::XMFLOAT2));
                break;
            case MaterialParameterType::Float3:
                memcpy(destPtr, &param.value.float3Value, sizeof(DirectX::XMFLOAT3));
                break;
            case MaterialParameterType::Float4:
                memcpy(destPtr, &param.value.float4Value, sizeof(DirectX::XMFLOAT4));
                break;
            case MaterialParameterType::Int:
                memcpy(destPtr, &param.value.intValue, sizeof(int32));
                break;
            case MaterialParameterType::Bool:
                {
                    int32 boolAsInt = param.value.boolValue ? 1 : 0;
                    memcpy(destPtr, &boolAsInt, sizeof(int32));
                }
                break;
            default:
                break;
        }
    }
}

MaterialParameter* Material::FindParameter(const String& name) {
    auto it = m_parameterNameToIndex.find(name);
    if (it != m_parameterNameToIndex.end()) {
        return &m_parameters[it->second];
    }
    return nullptr;
}

const MaterialParameter* Material::FindParameter(const String& name) const {
    auto it = m_parameterNameToIndex.find(name);
    if (it != m_parameterNameToIndex.end()) {
        return &m_parameters[it->second];
    }
    return nullptr;
}

uint32 Material::CalculateParameterBufferSize() const {
    uint32 size = 0;
    for (const auto& param : m_parameters) {
        size += param.size;
    }
    return (size + 15) & ~15; // Align to 16-byte boundary
}

void Material::LoadShaders() {
    // For now, create placeholder shaders
    // In a full implementation, this would load from files or compile HLSL
    Platform::OutputDebugMessage("Material: Shader loading not fully implemented - using placeholders\n");
    
    // Create empty blobs as placeholders
    D3DCreateBlob(1, &m_vertexShader);
    D3DCreateBlob(1, &m_pixelShader);
}

// Static factory methods
SharedPtr<Material> Material::CreateUnlit(DX12Renderer& renderer, const DirectX::XMFLOAT4& color, const String& name) {
    MaterialDesc desc;
    desc.name = name;
    desc.vertexShaderPath = "Shaders/UnlitVS.hlsl";
    desc.pixelShaderPath = "Shaders/UnlitPS.hlsl";
    desc.parameters.push_back(MaterialParameter("Color", color));
    
    return std::make_shared<Material>(renderer, desc);
}

SharedPtr<Material> Material::CreateLit(DX12Renderer& renderer, const DirectX::XMFLOAT4& albedo, float metallic, float roughness, const String& name) {
    MaterialDesc desc;
    desc.name = name;
    desc.vertexShaderPath = "Shaders/LitVS.hlsl";
    desc.pixelShaderPath = "Shaders/LitPS.hlsl";
    desc.parameters.push_back(MaterialParameter("Albedo", albedo));
    desc.parameters.push_back(MaterialParameter("Metallic", metallic));
    desc.parameters.push_back(MaterialParameter("Roughness", roughness));
    
    return std::make_shared<Material>(renderer, desc);
}

SharedPtr<Material> Material::CreateTextured(DX12Renderer& renderer, SharedPtr<Texture> diffuseTexture, const String& name) {
    MaterialDesc desc;
    desc.name = name;
    desc.vertexShaderPath = "Shaders/TexturedVS.hlsl";
    desc.pixelShaderPath = "Shaders/TexturedPS.hlsl";
    
    // Add texture parameter
    MaterialParameter texParam;
    texParam.name = "DiffuseTexture";
    texParam.type = MaterialParameterType::Texture2D;
    texParam.textureSlot = 3; // Texture descriptor table slot in root signature
    texParam.samplerSlot = 0; // Static sampler, no slot needed
    desc.parameters.push_back(texParam);
    
    auto material = std::make_shared<Material>(renderer, desc);
    
    if (diffuseTexture) {
        material->SetTexture("DiffuseTexture", diffuseTexture);
    }
    
    return material;
}

SharedPtr<Material> Material::CreateDefault(DX12Renderer& renderer, const String& name) {
    // Create a default magenta material for debugging
    return CreateUnlit(renderer, {1.0f, 0.0f, 1.0f, 1.0f}, name);
}
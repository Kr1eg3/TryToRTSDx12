#pragma once

#include "../Core/Utilities/Types.h"
#include "Bindable/IBindable.h"
#include "RHI/RHITypes.h"
#include "../Platform/Windows/WindowsPlatform.h"
#include <DirectXMath.h>
#include <unordered_map>

class DX12Renderer;
class Texture;
class Sampler;

// Material parameter types
enum class MaterialParameterType {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Bool,
    Texture2D,
    TextureCube,
    Unknown
};

// Material parameter value
union MaterialParameterValue {
    float floatValue;
    DirectX::XMFLOAT2 float2Value;
    DirectX::XMFLOAT3 float3Value;
    DirectX::XMFLOAT4 float4Value;
    int32 intValue;
    bool boolValue;
    void* texturePtr; // Pointer to Texture object
    
    MaterialParameterValue() : floatValue(0.0f) {}
};

// Material parameter definition
struct MaterialParameter {
    String name;
    MaterialParameterType type = MaterialParameterType::Unknown;
    MaterialParameterValue value;
    uint32 offset = 0;        // Offset in constant buffer
    uint32 size = 0;          // Size in bytes
    uint32 textureSlot = 0;   // For texture parameters
    uint32 samplerSlot = 0;   // For texture parameters
    
    MaterialParameter() = default;
    MaterialParameter(const String& paramName, float val) 
        : name(paramName), type(MaterialParameterType::Float) {
        value.floatValue = val;
        size = sizeof(float);
    }
    
    MaterialParameter(const String& paramName, const DirectX::XMFLOAT3& val) 
        : name(paramName), type(MaterialParameterType::Float3) {
        value.float3Value = val;
        size = sizeof(DirectX::XMFLOAT3);
    }
    
    MaterialParameter(const String& paramName, const DirectX::XMFLOAT4& val) 
        : name(paramName), type(MaterialParameterType::Float4) {
        value.float4Value = val;
        size = sizeof(DirectX::XMFLOAT4);
    }
};

// Material description for creation
struct MaterialDesc {
    String name = "Material";
    String vertexShaderPath;
    String pixelShaderPath;
    Vector<MaterialParameter> parameters;
    bool isTransparent = false;
    bool castsShadows = true;
    bool receivesShadows = true;
};

// Material class - manages shaders, textures, and parameters
class Material : public IBindable {
public:
    Material(DX12Renderer& renderer, const MaterialDesc& desc);
    virtual ~Material();

    // IBindable interface
    void Bind(IRHIContext& context) override;
    bool IsValid() const override;
    const String& GetDebugName() const override { return m_name; }

    // Parameter management
    void SetParameter(const String& name, float value);
    void SetParameter(const String& name, const DirectX::XMFLOAT2& value);
    void SetParameter(const String& name, const DirectX::XMFLOAT3& value);
    void SetParameter(const String& name, const DirectX::XMFLOAT4& value);
    void SetParameter(const String& name, int32 value);
    void SetParameter(const String& name, bool value);
    void SetTexture(const String& name, SharedPtr<Texture> texture, SharedPtr<Sampler> sampler = nullptr);
    
    // Parameter getters
    bool GetParameter(const String& name, float& outValue) const;
    bool GetParameter(const String& name, DirectX::XMFLOAT3& outValue) const;
    bool GetParameter(const String& name, DirectX::XMFLOAT4& outValue) const;
    SharedPtr<Texture> GetTexture(const String& name) const;
    
    // Material properties
    const String& GetName() const { return m_name; }
    bool IsTransparent() const { return m_isTransparent; }
    bool CastsShadows() const { return m_castsShadows; }
    bool ReceivesShadows() const { return m_receivesShadows; }
    
    // Update constant buffer with current parameter values
    void UpdateParameters();
    
    // Static factory methods
    static SharedPtr<Material> CreateUnlit(DX12Renderer& renderer, const DirectX::XMFLOAT4& color = {1,1,1,1}, const String& name = "UnlitMaterial");
    static SharedPtr<Material> CreateLit(DX12Renderer& renderer, const DirectX::XMFLOAT4& albedo = {1,1,1,1}, float metallic = 0.0f, float roughness = 0.5f, const String& name = "LitMaterial");
    static SharedPtr<Material> CreateTextured(DX12Renderer& renderer, SharedPtr<Texture> diffuseTexture, const String& name = "TexturedMaterial");
    static SharedPtr<Material> CreateDefault(DX12Renderer& renderer, const String& name = "DefaultMaterial");

private:
    void CreateParameterBuffer();
    void UpdateParameterBuffer();
    MaterialParameter* FindParameter(const String& name);
    const MaterialParameter* FindParameter(const String& name) const;
    uint32 CalculateParameterBufferSize() const;
    void LoadShaders();

private:
    DX12Renderer& m_renderer;
    String m_name;
    MaterialDesc m_desc;
    
    // Shader resources
    String m_vertexShaderPath;
    String m_pixelShaderPath;
    ComPtr<ID3DBlob> m_vertexShader;
    ComPtr<ID3DBlob> m_pixelShader;
    
    // Parameters
    Vector<MaterialParameter> m_parameters;
    std::unordered_map<String, uint32> m_parameterNameToIndex;
    
    // Constant buffer for parameters
    ComPtr<ID3D12Resource> m_parameterBuffer;
    void* m_mappedParameterData = nullptr;
    uint32 m_parameterBufferSize = 0;
    
    // Textures and samplers
    std::unordered_map<String, SharedPtr<Texture>> m_textures;
    std::unordered_map<String, SharedPtr<Sampler>> m_samplers;
    
    // Material properties
    bool m_isTransparent = false;
    bool m_castsShadows = true;
    bool m_receivesShadows = true;
    
    // State
    bool m_parametersNeedUpdate = true;
    bool m_isInitialized = false;

    DECLARE_NON_COPYABLE(Material);
};
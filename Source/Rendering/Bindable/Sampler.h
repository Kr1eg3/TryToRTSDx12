#pragma once

#include "IBindable.h"
#include "../RHI/RHITypes.h"
#include "../../Core/Utilities/Types.h"
#include "../../Platform/Windows/WindowsPlatform.h"

class DX12Renderer;

class Sampler : public IBindable {
public:
    Sampler(DX12Renderer& renderer, const RHISamplerDesc& desc, const String& debugName = "Sampler");
    virtual ~Sampler();

    void Bind(IRHIContext& context) override;
    bool IsValid() const override { return m_sampler.samplerResource != nullptr; }
    const String& GetDebugName() const override { return m_debugName; }

    // Sampler-specific methods
    const RHISamplerDesc& GetDesc() const { return m_sampler.desc; }
    
    // Slot management for sampler binding
    void SetSlot(uint32 slot) { m_slot = slot; }
    uint32 GetSlot() const { return m_slot; }

    // Access to RHI sampler for advanced operations
    const RHISampler& GetRHISampler() const { return m_sampler; }

    // Static factory methods for common sampler types
    static UniquePtr<Sampler> CreateLinearWrap(DX12Renderer& renderer, const String& debugName = "LinearWrap");
    static UniquePtr<Sampler> CreateLinearClamp(DX12Renderer& renderer, const String& debugName = "LinearClamp");
    static UniquePtr<Sampler> CreatePointWrap(DX12Renderer& renderer, const String& debugName = "PointWrap");
    static UniquePtr<Sampler> CreatePointClamp(DX12Renderer& renderer, const String& debugName = "PointClamp");
    static UniquePtr<Sampler> CreateAnisotropic(DX12Renderer& renderer, uint32 maxAnisotropy = 16, const String& debugName = "Anisotropic");
    static UniquePtr<Sampler> CreateShadowComparison(DX12Renderer& renderer, const String& debugName = "ShadowComparison");

private:
    bool CreateSampler(const RHISamplerDesc& desc);
    void Cleanup();

    // Helper for format conversion
    D3D12_FILTER ConvertFilter(RHITextureFilter minFilter, RHITextureFilter magFilter, RHITextureFilter mipFilter, bool isComparison = false) const;
    D3D12_TEXTURE_ADDRESS_MODE ConvertAddressMode(RHITextureAddressMode mode) const;

private:
    DX12Renderer& m_renderer;
    RHISampler m_sampler;
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_samplerHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_samplerGpuHandle = {};
    
    uint32 m_slot = 0;
    String m_debugName;

    DECLARE_NON_COPYABLE(Sampler);
};
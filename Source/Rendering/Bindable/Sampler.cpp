#include "Sampler.h"
#include "../Dx12/DX12Renderer.h"
#include "../RHI/DX12RHIContext.h"
#include "../../Platform/Windows/WindowsPlatform.h"

Sampler::Sampler(DX12Renderer& renderer, const RHISamplerDesc& desc, const String& debugName)
    : m_renderer(renderer)
    , m_debugName(debugName.empty() ? desc.debugName : debugName) {
    
    if (!CreateSampler(desc)) {
        Platform::OutputDebugMessage("Sampler: Failed to create sampler: " + m_debugName + "\n");
    }
}

Sampler::~Sampler() {
    Cleanup();
}

void Sampler::Bind(IRHIContext& context) {
    if (!IsValid()) return;
    
    // Set sampler using GPU handle
    if (m_samplerGpuHandle.ptr != 0) {
        context.SetSampler(m_slot, &m_samplerGpuHandle);
    }
}

bool Sampler::CreateSampler(const RHISamplerDesc& desc) {
    m_sampler.desc = desc;
    
    // Create descriptor heap for sampler
    if (!m_renderer.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, 
                                       D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, m_samplerHeap)) {
        Platform::OutputDebugMessage("Sampler: Failed to create descriptor heap\n");
        return false;
    }
    
    // Get descriptor handles
    m_samplerHandle = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
    m_samplerGpuHandle = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
    
    // Create D3D12 sampler description
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = ConvertFilter(desc.minFilter, desc.magFilter, desc.mipFilter);
    samplerDesc.AddressU = ConvertAddressMode(desc.addressU);
    samplerDesc.AddressV = ConvertAddressMode(desc.addressV);
    samplerDesc.AddressW = ConvertAddressMode(desc.addressW);
    samplerDesc.MipLODBias = desc.mipLODBias;
    samplerDesc.MaxAnisotropy = desc.maxAnisotropy;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // Default for non-comparison samplers
    samplerDesc.BorderColor[0] = 0.0f;
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD = desc.minLOD;
    samplerDesc.MaxLOD = desc.maxLOD;
    
    // Create the sampler
    m_renderer.GetDevice()->CreateSampler(&samplerDesc, m_samplerHandle);
    
    // Update RHI sampler
    m_sampler.samplerResource = m_samplerHeap.Get();
    
    if (DEBUG_BUILD && !m_debugName.empty()) {
        std::wstring wideName(m_debugName.begin(), m_debugName.end());
        m_samplerHeap->SetName(wideName.c_str());
    }
    
    return true;
}

void Sampler::Cleanup() {
    m_samplerHeap.Reset();
    m_sampler.samplerResource = nullptr;
}

D3D12_FILTER Sampler::ConvertFilter(RHITextureFilter minFilter, RHITextureFilter magFilter, RHITextureFilter mipFilter, bool isComparison) const {
    // Helper function to map RHI filters to D3D12 filters
    auto getD3D12Filter = [isComparison](RHITextureFilter min, RHITextureFilter mag, RHITextureFilter mip) -> D3D12_FILTER {
        if (isComparison) {
            // Comparison filters
            if (min == RHITextureFilter::Point && mag == RHITextureFilter::Point && mip == RHITextureFilter::Point) {
                return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
            } else if (min == RHITextureFilter::Linear && mag == RHITextureFilter::Linear && mip == RHITextureFilter::Linear) {
                return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
            } else if (min == RHITextureFilter::Anisotropic || mag == RHITextureFilter::Anisotropic) {
                return D3D12_FILTER_COMPARISON_ANISOTROPIC;
            }
            // Mixed filters for comparison
            return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        } else {
            // Regular filters
            if (min == RHITextureFilter::Point && mag == RHITextureFilter::Point && mip == RHITextureFilter::Point) {
                return D3D12_FILTER_MIN_MAG_MIP_POINT;
            } else if (min == RHITextureFilter::Linear && mag == RHITextureFilter::Linear && mip == RHITextureFilter::Linear) {
                return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            } else if (min == RHITextureFilter::Anisotropic || mag == RHITextureFilter::Anisotropic) {
                return D3D12_FILTER_ANISOTROPIC;
            } else if (min == RHITextureFilter::Point && mag == RHITextureFilter::Point && mip == RHITextureFilter::Linear) {
                return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            } else if (min == RHITextureFilter::Linear && mag == RHITextureFilter::Linear && mip == RHITextureFilter::Point) {
                return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            }
            // Default to mixed linear
            return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        }
    };
    
    return getD3D12Filter(minFilter, magFilter, mipFilter);
}

D3D12_TEXTURE_ADDRESS_MODE Sampler::ConvertAddressMode(RHITextureAddressMode mode) const {
    switch (mode) {
        case RHITextureAddressMode::Wrap:   return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case RHITextureAddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case RHITextureAddressMode::Clamp:  return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case RHITextureAddressMode::Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:                            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

// Static factory methods
UniquePtr<Sampler> Sampler::CreateLinearWrap(DX12Renderer& renderer, const String& debugName) {
    RHISamplerDesc desc;
    desc.minFilter = RHITextureFilter::Linear;
    desc.magFilter = RHITextureFilter::Linear;
    desc.mipFilter = RHITextureFilter::Linear;
    desc.addressU = RHITextureAddressMode::Wrap;
    desc.addressV = RHITextureAddressMode::Wrap;
    desc.addressW = RHITextureAddressMode::Wrap;
    desc.debugName = debugName;
    
    return std::make_unique<Sampler>(renderer, desc, debugName);
}

UniquePtr<Sampler> Sampler::CreateLinearClamp(DX12Renderer& renderer, const String& debugName) {
    RHISamplerDesc desc;
    desc.minFilter = RHITextureFilter::Linear;
    desc.magFilter = RHITextureFilter::Linear;
    desc.mipFilter = RHITextureFilter::Linear;
    desc.addressU = RHITextureAddressMode::Clamp;
    desc.addressV = RHITextureAddressMode::Clamp;
    desc.addressW = RHITextureAddressMode::Clamp;
    desc.debugName = debugName;
    
    return std::make_unique<Sampler>(renderer, desc, debugName);
}

UniquePtr<Sampler> Sampler::CreatePointWrap(DX12Renderer& renderer, const String& debugName) {
    RHISamplerDesc desc;
    desc.minFilter = RHITextureFilter::Point;
    desc.magFilter = RHITextureFilter::Point;
    desc.mipFilter = RHITextureFilter::Point;
    desc.addressU = RHITextureAddressMode::Wrap;
    desc.addressV = RHITextureAddressMode::Wrap;
    desc.addressW = RHITextureAddressMode::Wrap;
    desc.debugName = debugName;
    
    return std::make_unique<Sampler>(renderer, desc, debugName);
}

UniquePtr<Sampler> Sampler::CreatePointClamp(DX12Renderer& renderer, const String& debugName) {
    RHISamplerDesc desc;
    desc.minFilter = RHITextureFilter::Point;
    desc.magFilter = RHITextureFilter::Point;
    desc.mipFilter = RHITextureFilter::Point;
    desc.addressU = RHITextureAddressMode::Clamp;
    desc.addressV = RHITextureAddressMode::Clamp;
    desc.addressW = RHITextureAddressMode::Clamp;
    desc.debugName = debugName;
    
    return std::make_unique<Sampler>(renderer, desc, debugName);
}

UniquePtr<Sampler> Sampler::CreateAnisotropic(DX12Renderer& renderer, uint32 maxAnisotropy, const String& debugName) {
    RHISamplerDesc desc;
    desc.minFilter = RHITextureFilter::Anisotropic;
    desc.magFilter = RHITextureFilter::Anisotropic;
    desc.mipFilter = RHITextureFilter::Anisotropic;
    desc.addressU = RHITextureAddressMode::Wrap;
    desc.addressV = RHITextureAddressMode::Wrap;
    desc.addressW = RHITextureAddressMode::Wrap;
    desc.maxAnisotropy = maxAnisotropy;
    desc.debugName = debugName;
    
    return std::make_unique<Sampler>(renderer, desc, debugName);
}

UniquePtr<Sampler> Sampler::CreateShadowComparison(DX12Renderer& renderer, const String& debugName) {
    RHISamplerDesc desc;
    desc.minFilter = RHITextureFilter::Linear;
    desc.magFilter = RHITextureFilter::Linear;
    desc.mipFilter = RHITextureFilter::Point;
    desc.addressU = RHITextureAddressMode::Border;
    desc.addressV = RHITextureAddressMode::Border;
    desc.addressW = RHITextureAddressMode::Border;
    desc.debugName = debugName;
    
    return std::make_unique<Sampler>(renderer, desc, debugName);
}
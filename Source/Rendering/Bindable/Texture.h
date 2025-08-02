#pragma once

#include "IBindable.h"
#include "../RHI/RHITypes.h"
#include "../../Core/Utilities/Types.h"
#include "../../Platform/Windows/WindowsPlatform.h"
#include <DirectXMath.h>

class DX12Renderer;

class Texture : public IBindable {
public:
    Texture(DX12Renderer& renderer, const RHITextureDesc& desc, const void* initialData = nullptr, const String& debugName = "Texture");
    Texture(DX12Renderer& renderer, const String& filePath, bool generateMips = true, const String& debugName = "");
    virtual ~Texture();

    void Bind(IRHIContext& context) override;
    bool IsValid() const override { return m_texture.textureResource != nullptr; }
    const String& GetDebugName() const override { return m_debugName; }

    // Texture-specific methods
    const RHITextureDesc& GetDesc() const { return m_texture.desc; }
    uint32 GetWidth() const { return m_texture.desc.width; }
    uint32 GetHeight() const { return m_texture.desc.height; }
    RHIResourceFormat GetFormat() const { return m_texture.desc.format; }
    
    // Slot management for texture binding
    void SetSlot(uint32 slot) { m_slot = slot; }
    uint32 GetSlot() const { return m_slot; }

    // Access to RHI texture for advanced operations
    const RHITexture& GetRHITexture() const { return m_texture; }

    // Utility methods
    bool LoadFromFile(const String& filePath, bool generateMips = true);
    bool CreateFromData(const RHITextureDesc& desc, const void* data);
    void UpdateData(const void* data, uint32 dataSize, uint32 mipLevel = 0);
    
    // Explicit upload methods
    bool NeedsUpload() const { return m_needsUpload; }
    void ForceUpload();

    // Static factory methods
    static UniquePtr<Texture> CreateFromFile(DX12Renderer& renderer, const String& filePath, bool generateMips = true, const String& debugName = "");
    static UniquePtr<Texture> CreateSolidColor(DX12Renderer& renderer, uint32 width, uint32 height, const DirectX::XMFLOAT4& color, const String& debugName = "SolidColorTexture");
    static UniquePtr<Texture> CreateCheckerboard(DX12Renderer& renderer, uint32 width, uint32 height, const String& debugName = "CheckerboardTexture");

private:
    bool CreateTexture(const RHITextureDesc& desc, const void* initialData);
    bool CreateShaderResourceView();
    void Cleanup();

    // Helper methods for file loading
    bool LoadDDSFromFile(const String& filePath);
    bool LoadWICFromFile(const String& filePath);
    
    // Helper for format conversion
    DXGI_FORMAT ConvertToD3D12Format(RHIResourceFormat format) const;
    RHIResourceFormat ConvertFromD3D12Format(DXGI_FORMAT format) const;
    
    // Upload texture data to GPU
    void UploadTextureData(uint64 uploadBufferSize);

private:
    DX12Renderer& m_renderer;
    RHITexture m_texture;
    ComPtr<ID3D12Resource> m_uploadBuffer;
    ComPtr<ID3D12Resource> m_d3d12Texture;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuHandle = {};
    
    uint32 m_slot = 0;
    String m_debugName;
    bool m_needsUpload = false;

    DECLARE_NON_COPYABLE(Texture);
};
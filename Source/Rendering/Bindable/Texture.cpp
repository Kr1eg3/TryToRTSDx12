#include "Texture.h"
#include "../Dx12/DX12Renderer.h"
#include "../RHI/DX12RHIContext.h"
#include "../../Platform/Windows/WindowsPlatform.h"
#include "../../Core/Utilities/TextureLoader.h"

Texture::Texture(DX12Renderer& renderer, const RHITextureDesc& desc, const void* initialData, const String& debugName)
    : m_renderer(renderer)
    , m_debugName(debugName.empty() ? desc.debugName : debugName) {
    
    if (!CreateTexture(desc, initialData)) {
        Platform::OutputDebugMessage("Texture: Failed to create texture: " + m_debugName + "\n");
    }
}

Texture::Texture(DX12Renderer& renderer, const String& filePath, bool generateMips, const String& debugName)
    : m_renderer(renderer)
    , m_debugName(debugName.empty() ? filePath : debugName) {
    
    if (!LoadFromFile(filePath, generateMips)) {
        Platform::OutputDebugMessage("Texture: Failed to load texture from file: " + filePath + "\n");
        
        // Create a fallback 1x1 pink texture
        RHITextureDesc fallbackDesc;
        fallbackDesc.width = 1;
        fallbackDesc.height = 1;
        fallbackDesc.format = RHIResourceFormat::R8G8B8A8_Unorm;
        fallbackDesc.debugName = m_debugName + "_Fallback";
        
        uint32 pinkColor = 0xFF00FFFF; // RGBA: Pink
        CreateTexture(fallbackDesc, &pinkColor);
    }
}

Texture::~Texture() {
    Cleanup();
}

void Texture::Bind(IRHIContext& context) {
    Platform::OutputDebugMessage("Texture::Bind: Starting bind for texture: " + m_debugName + "\n");
    
    if (!IsValid()) {
        Platform::OutputDebugMessage("Texture::Bind: Texture is not valid, skipping bind\n");
        return;
    }
    
    try {
        // Set our descriptor heap (only SRV for now, sampler binding is disabled)
        if (m_srvHeap) {
            auto& dx12Context = static_cast<DX12RHIContext&>(context);
            ID3D12GraphicsCommandList* commandList = dx12Context.GetCommandList();
            
            if (!commandList) {
                Platform::OutputDebugMessage("Texture::Bind: Command list is null!\n");
                return;
            }
            
            Platform::OutputDebugMessage("Texture::Bind: Setting texture descriptor heap (SRV only)\n");
            ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
            commandList->SetDescriptorHeaps(1, heaps);
            Platform::OutputDebugMessage("Texture::Bind: Descriptor heap set successfully\n");
        } else {
            Platform::OutputDebugMessage("Texture::Bind: No SRV heap available!\n");
            return;
        }
        
        // Bind texture descriptor table
        if (m_srvGpuHandle.ptr != 0) {
            Platform::OutputDebugMessage("Texture::Bind: Binding texture to slot " + std::to_string(m_slot) + 
                                        " with handle ptr: " + std::to_string(m_srvGpuHandle.ptr) + "\n");
            context.SetTexture(m_slot, &m_srvGpuHandle);
            Platform::OutputDebugMessage("Texture::Bind: Texture bound successfully\n");
        } else {
            Platform::OutputDebugMessage("Texture::Bind: GPU handle is null (ptr=0), skipping texture bind\n");
        }
    }
    catch (...) {
        Platform::OutputDebugMessage("Texture::Bind: Exception caught in Texture::Bind!\n");
    }
}

bool Texture::LoadFromFile(const String& filePath, bool generateMips) {
    // Load image data using TextureLoader
    auto imageData = TextureLoader::LoadFromFile(filePath);
    
    if (!imageData.IsValid()) {
        Platform::OutputDebugMessage("Texture: Failed to load image data from: " + filePath + "\n");
        return false;
    }
    
    // Create texture description
    RHITextureDesc desc;
    desc.width = imageData.width;
    desc.height = imageData.height;
    desc.format = RHIResourceFormat::R8G8B8A8_Unorm;
    desc.mipLevels = generateMips ? 0 : 1; // 0 means generate all mip levels
    desc.debugName = m_debugName;
    
    // Create texture with loaded data
    return CreateTexture(desc, imageData.pixels.get());
}

bool Texture::CreateFromData(const RHITextureDesc& desc, const void* data) {
    return CreateTexture(desc, data);
}

void Texture::UpdateData(const void* data, uint32 dataSize, uint32 mipLevel) {
    if (!IsValid() || !data) return;
    
    // For now, only support updating mip level 0
    if (mipLevel != 0) {
        Platform::OutputDebugMessage("Texture: UpdateData only supports mip level 0\n");
        return;
    }
    
    // Calculate expected data size
    uint32 bytesPerPixel = 4; // Assume RGBA for now
    uint32 expectedSize = m_texture.desc.width * m_texture.desc.height * bytesPerPixel;
    
    if (dataSize < expectedSize) {
        Platform::OutputDebugMessage("Texture: UpdateData - insufficient data size\n");
        return;
    }
    
    // Create upload buffer and copy data
    // This is a simplified implementation - full implementation would handle different formats
    Platform::OutputDebugMessage("Texture: UpdateData not fully implemented\n");
}

bool Texture::CreateTexture(const RHITextureDesc& desc, const void* initialData) {
    m_texture.desc = desc;
    
    // Create D3D12 texture description
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = desc.width;
    textureDesc.Height = desc.height;
    textureDesc.DepthOrArraySize = static_cast<UINT16>(desc.arraySize);
    textureDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);
    textureDesc.Format = ConvertToD3D12Format(desc.format);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    // Create heap properties
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    // Create the texture resource in common state for potential data upload
    HRESULT hr = m_renderer.GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_d3d12Texture)
    );
    
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Texture: Failed to create D3D12 texture resource\n");
        return false;
    }
    
    // Set debug name
    if (DEBUG_BUILD && !m_debugName.empty()) {
        std::wstring wideName(m_debugName.begin(), m_debugName.end());
        m_d3d12Texture->SetName(wideName.c_str());
    }
    
    // Update RHI texture
    m_texture.textureResource = m_d3d12Texture.Get();
    
    // Create SRV
    if (!CreateShaderResourceView()) {
        Platform::OutputDebugMessage("Texture: Failed to create shader resource view\n");
        return false;
    }
    
    // Upload texture data if provided
    if (initialData) {
        uint32 bytesPerPixel = 4; // Assume RGBA for now
        uint64 uploadBufferSize = desc.width * desc.height * bytesPerPixel;
        
        // Create upload buffer
        if (!m_renderer.CreateBuffer(uploadBufferSize, D3D12_HEAP_TYPE_UPLOAD, 
                                   D3D12_RESOURCE_STATE_GENERIC_READ, m_uploadBuffer)) {
            Platform::OutputDebugMessage("Texture: Failed to create upload buffer\n");
            return false;
        }
        
        // Map and copy data
        void* mappedData = nullptr;
        hr = m_uploadBuffer->Map(0, nullptr, &mappedData);
        if (SUCCEEDED(hr)) {
            memcpy(mappedData, initialData, uploadBufferSize);
            m_uploadBuffer->Unmap(0, nullptr);
            
            // Use the corrected texture upload method
            if (m_renderer.CopyUploadToTexture(m_d3d12Texture, m_uploadBuffer, 
                                              desc.width, desc.height,
                                              ConvertToD3D12Format(desc.format))) {
                m_renderer.ExecuteUploadCommands();
                Platform::OutputDebugMessage("Texture: Upload completed successfully\n");
                m_needsUpload = false;
            } else {
                Platform::OutputDebugMessage("Texture: Upload failed\n");
                m_needsUpload = true;
            }
        }
    } else {
        // If no data provided, transition to shader resource state anyway
        m_needsUpload = false;
    }
    
    return true;
}

bool Texture::CreateShaderResourceView() {
    Platform::OutputDebugMessage("Texture::CreateShaderResourceView: Starting for texture: " + m_debugName + "\n");
    
    try {
        // Create descriptor heap for SRV
        if (!m_renderer.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, 
                                           D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, m_srvHeap)) {
            Platform::OutputDebugMessage("Texture::CreateShaderResourceView: Failed to create descriptor heap\n");
            return false;
        }
        Platform::OutputDebugMessage("Texture::CreateShaderResourceView: Descriptor heap created\n");
        
        // Get descriptor handles
        m_srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        m_srvGpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        Platform::OutputDebugMessage("Texture::CreateShaderResourceView: Got descriptor handles, GPU ptr: " + 
                                    std::to_string(m_srvGpuHandle.ptr) + "\n");
        
        // Validate texture resource
        if (!m_d3d12Texture) {
            Platform::OutputDebugMessage("Texture::CreateShaderResourceView: D3D12 texture is null!\n");
            return false;
        }
        
        // Create SRV description
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = ConvertToD3D12Format(m_texture.desc.format);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = m_texture.desc.mipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        
        Platform::OutputDebugMessage("Texture::CreateShaderResourceView: Creating SRV with format: " + 
                                    std::to_string(static_cast<int>(srvDesc.Format)) + "\n");
        
        // Create the SRV
        m_renderer.GetDevice()->CreateShaderResourceView(m_d3d12Texture.Get(), &srvDesc, m_srvHandle);
        Platform::OutputDebugMessage("Texture::CreateShaderResourceView: SRV created successfully\n");
        
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("Texture::CreateShaderResourceView: Exception caught!\n");
        return false;
    }
}

void Texture::Cleanup() {
    m_srvHeap.Reset();
    m_uploadBuffer.Reset();
    m_d3d12Texture.Reset();
    m_texture.textureResource = nullptr;
}

bool Texture::LoadDDSFromFile(const String& filePath) {
    // This would use DirectXTex library to load DDS files
    Platform::OutputDebugMessage("Texture: DDS loading not implemented yet\n");
    return false;
}

bool Texture::LoadWICFromFile(const String& filePath) {
    // This would use DirectXTex library to load common image formats
    Platform::OutputDebugMessage("Texture: WIC loading not implemented yet\n");
    return false;
}

DXGI_FORMAT Texture::ConvertToD3D12Format(RHIResourceFormat format) const {
    switch (format) {
        case RHIResourceFormat::R32G32B32_Float:       return DXGI_FORMAT_R32G32B32_FLOAT;
        case RHIResourceFormat::R32G32B32A32_Float:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RHIResourceFormat::R32G32_Float:          return DXGI_FORMAT_R32G32_FLOAT;
        case RHIResourceFormat::R32_Float:             return DXGI_FORMAT_R32_FLOAT;
        case RHIResourceFormat::R8G8B8A8_Unorm:        return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RHIResourceFormat::R8G8B8A8_Unorm_sRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case RHIResourceFormat::R16_Uint:              return DXGI_FORMAT_R16_UINT;
        case RHIResourceFormat::R32_Uint:              return DXGI_FORMAT_R32_UINT;
        case RHIResourceFormat::D32_Float:             return DXGI_FORMAT_D32_FLOAT;
        case RHIResourceFormat::BC1_Unorm:             return DXGI_FORMAT_BC1_UNORM;
        case RHIResourceFormat::BC2_Unorm:             return DXGI_FORMAT_BC2_UNORM;
        case RHIResourceFormat::BC3_Unorm:             return DXGI_FORMAT_BC3_UNORM;
        case RHIResourceFormat::BC7_Unorm:             return DXGI_FORMAT_BC7_UNORM;
        default:                                       return DXGI_FORMAT_UNKNOWN;
    }
}

RHIResourceFormat Texture::ConvertFromD3D12Format(DXGI_FORMAT format) const {
    switch (format) {
        case DXGI_FORMAT_R32G32B32_FLOAT:       return RHIResourceFormat::R32G32B32_Float;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:    return RHIResourceFormat::R32G32B32A32_Float;
        case DXGI_FORMAT_R32G32_FLOAT:          return RHIResourceFormat::R32G32_Float;
        case DXGI_FORMAT_R32_FLOAT:             return RHIResourceFormat::R32_Float;
        case DXGI_FORMAT_R8G8B8A8_UNORM:        return RHIResourceFormat::R8G8B8A8_Unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return RHIResourceFormat::R8G8B8A8_Unorm_sRGB;
        case DXGI_FORMAT_R16_UINT:              return RHIResourceFormat::R16_Uint;
        case DXGI_FORMAT_R32_UINT:              return RHIResourceFormat::R32_Uint;
        case DXGI_FORMAT_D32_FLOAT:             return RHIResourceFormat::D32_Float;
        case DXGI_FORMAT_BC1_UNORM:             return RHIResourceFormat::BC1_Unorm;
        case DXGI_FORMAT_BC2_UNORM:             return RHIResourceFormat::BC2_Unorm;
        case DXGI_FORMAT_BC3_UNORM:             return RHIResourceFormat::BC3_Unorm;
        case DXGI_FORMAT_BC7_UNORM:             return RHIResourceFormat::BC7_Unorm;
        default:                               return RHIResourceFormat::Unknown;
    }
}

// Static factory methods
UniquePtr<Texture> Texture::CreateFromFile(DX12Renderer& renderer, const String& filePath, bool generateMips, const String& debugName) {
    return std::make_unique<Texture>(renderer, filePath, generateMips, debugName);
}

UniquePtr<Texture> Texture::CreateSolidColor(DX12Renderer& renderer, uint32 width, uint32 height, const DirectX::XMFLOAT4& color, const String& debugName) {
    // Use TextureLoader to create solid color
    auto imageData = TextureLoader::CreateSolidColor(width, height,
        static_cast<uint8>(color.x * 255.0f),
        static_cast<uint8>(color.y * 255.0f),
        static_cast<uint8>(color.z * 255.0f),
        static_cast<uint8>(color.w * 255.0f));
    
    RHITextureDesc desc;
    desc.width = imageData.width;
    desc.height = imageData.height;
    desc.format = RHIResourceFormat::R8G8B8A8_Unorm;
    desc.debugName = debugName;
    
    return std::make_unique<Texture>(renderer, desc, imageData.pixels.get(), debugName);
}

UniquePtr<Texture> Texture::CreateCheckerboard(DX12Renderer& renderer, uint32 width, uint32 height, const String& debugName) {
    // Use TextureLoader to create checkerboard
    auto imageData = TextureLoader::CreateTestPattern(width, height, "checkerboard");
    
    RHITextureDesc desc;
    desc.width = imageData.width;
    desc.height = imageData.height;
    desc.format = RHIResourceFormat::R8G8B8A8_Unorm;
    desc.debugName = debugName;
    
    return std::make_unique<Texture>(renderer, desc, imageData.pixels.get(), debugName);
}

void Texture::UploadTextureData(uint64 uploadBufferSize) {
    if (!m_uploadBuffer || !m_d3d12Texture) {
        Platform::OutputDebugMessage("Texture::UploadTextureData: Invalid buffers\n");
        return;
    }
    
    Platform::OutputDebugMessage("Texture::UploadTextureData: Starting upload for " + m_debugName + "\n");
    
    // Use the renderer's texture-specific copy method
    if (!m_renderer.CopyUploadToTexture(m_d3d12Texture, m_uploadBuffer, 
                                        m_texture.desc.width, m_texture.desc.height,
                                        ConvertToD3D12Format(m_texture.desc.format))) {
        Platform::OutputDebugMessage("Texture::UploadTextureData: Failed to copy upload texture\n");
        return;
    }
    
    // Execute the upload commands immediately
    m_renderer.ExecuteUploadCommands();
    
    Platform::OutputDebugMessage("Texture::UploadTextureData: Upload completed for " + m_debugName + "\n");
    
    m_needsUpload = false;
}

void Texture::ForceUpload() {
    if (!m_needsUpload) {
        Platform::OutputDebugMessage("Texture::ForceUpload: Texture doesn't need upload\n");
        return;
    }
    
    if (!m_uploadBuffer || !m_d3d12Texture) {
        Platform::OutputDebugMessage("Texture::ForceUpload: Invalid buffers\n");
        return;
    }
    
    Platform::OutputDebugMessage("Texture::ForceUpload: Force uploading texture " + m_debugName + "\n");
    
    uint32 bytesPerPixel = 4; // Assume RGBA for now
    uint64 uploadBufferSize = m_texture.desc.width * m_texture.desc.height * bytesPerPixel;
    
    UploadTextureData(uploadBufferSize);
}
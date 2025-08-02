#pragma once

#include "../../Core/Utilities/Types.h"

enum class RHIGraphicsAPI {
    DirectX12,
    DirectX11,
    Vulkan,
    OpenGL
};

enum class RHIPrimitiveTopology {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList
};

enum class RHIShaderType {
    Vertex,
    Pixel,
    Geometry,
    Hull,
    Domain,
    Compute
};

enum class RHIResourceFormat {
    R32G32B32_Float,
    R32G32B32A32_Float,
    R32G32_Float,
    R32_Float,
    R8G8B8A8_Unorm,
    R8G8B8A8_Unorm_sRGB,
    R16_Uint,
    R32_Uint,
    D32_Float,
    BC1_Unorm,       // DXT1
    BC2_Unorm,       // DXT3
    BC3_Unorm,       // DXT5
    BC7_Unorm,       // BC7
    Unknown
};

enum class RHITextureFilter {
    Point,
    Linear,
    Anisotropic
};

enum class RHITextureAddressMode {
    Wrap,
    Mirror,
    Clamp,
    Border
};

enum class RHITextureDimension {
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    Texture2DArray
};

struct RHIViewport {
    float32 x = 0.0f;
    float32 y = 0.0f;
    float32 width = 0.0f;
    float32 height = 0.0f;
    float32 minDepth = 0.0f;
    float32 maxDepth = 1.0f;
};

struct RHIRect {
    int32 left = 0;
    int32 top = 0;
    int32 right = 0;
    int32 bottom = 0;
};

struct RHIVertexBufferView {
    void* bufferResource = nullptr;
    uint64 bufferLocation = 0;
    uint32 sizeInBytes = 0;
    uint32 strideInBytes = 0;
};

struct RHIIndexBufferView {
    void* bufferResource = nullptr;
    uint64 bufferLocation = 0;
    uint32 sizeInBytes = 0;
    RHIResourceFormat format = RHIResourceFormat::R32_Uint;
};

struct RHIConstantBufferView {
    void* bufferResource = nullptr;
    uint64 bufferLocation = 0;
    uint32 sizeInBytes = 0;
};

struct RHIShader {
    void* shaderResource = nullptr;
    RHIShaderType type = RHIShaderType::Vertex;
    String entryPoint;
    Vector<uint8> bytecode;
};

struct RHITextureDesc {
    uint32 width = 1;
    uint32 height = 1;
    uint32 depth = 1;
    uint32 mipLevels = 1;
    uint32 arraySize = 1;
    RHIResourceFormat format = RHIResourceFormat::R8G8B8A8_Unorm;
    RHITextureDimension dimension = RHITextureDimension::Texture2D;
    bool generateMips = false;
    String debugName = "Texture";
};

struct RHITexture {
    void* textureResource = nullptr;
    RHITextureDesc desc;
};

struct RHISamplerDesc {
    RHITextureFilter minFilter = RHITextureFilter::Linear;
    RHITextureFilter magFilter = RHITextureFilter::Linear;
    RHITextureFilter mipFilter = RHITextureFilter::Linear;
    RHITextureAddressMode addressU = RHITextureAddressMode::Wrap;
    RHITextureAddressMode addressV = RHITextureAddressMode::Wrap;
    RHITextureAddressMode addressW = RHITextureAddressMode::Wrap;
    float32 mipLODBias = 0.0f;
    uint32 maxAnisotropy = 16;
    float32 minLOD = 0.0f;
    float32 maxLOD = 3.402823466e+38f; // FLT_MAX
    String debugName = "Sampler";
};

struct RHISampler {
    void* samplerResource = nullptr;
    RHISamplerDesc desc;
};

struct RHITextureView {
    void* textureResource = nullptr;
    void* shaderResourceView = nullptr;
    uint32 slot = 0;
};

struct RHISamplerView {
    void* samplerResource = nullptr;
    uint32 slot = 0;
};
#pragma once

#include "../../Core/Utilities/Types.h"
#include "RHITypes.h"

class IRHIContext {
public:
    virtual ~IRHIContext() = default;

    virtual void SetVertexBuffer(uint32 slot, const RHIVertexBufferView& bufferView) = 0;
    virtual void SetIndexBuffer(const RHIIndexBufferView& bufferView) = 0;
    virtual void SetConstantBuffer(uint32 rootParameterIndex, const RHIConstantBufferView& bufferView) = 0;
    
    virtual void SetVertexShader(const RHIShader& shader) = 0;
    virtual void SetPixelShader(const RHIShader& shader) = 0;
    
    virtual void SetTexture(uint32 slot, const RHITextureView& textureView) = 0;
    virtual void SetSampler(uint32 slot, const RHISamplerView& samplerView) = 0;
    
    // Overloads for direct GPU handle binding (DX12-specific)
    virtual void SetTexture(uint32 slot, void* gpuHandle) = 0;
    virtual void SetSampler(uint32 slot, void* gpuHandle) = 0;
    
    virtual void SetPrimitiveTopology(RHIPrimitiveTopology topology) = 0;
    virtual void SetViewport(const RHIViewport& viewport) = 0;
    virtual void SetScissorRect(const RHIRect& rect) = 0;
    
    virtual void DrawIndexed(uint32 indexCount, uint32 startIndexLocation = 0, int32 baseVertexLocation = 0) = 0;
    virtual void Draw(uint32 vertexCount, uint32 startVertexLocation = 0) = 0;

    virtual RHIGraphicsAPI GetAPI() const = 0;
};
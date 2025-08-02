#pragma once

#include "IRHIContext.h"
#include "../Dx12/DX12Renderer.h"
#include "../../Platform/Windows/WindowsPlatform.h"

class DX12RHIContext : public IRHIContext {
public:
    DX12RHIContext(DX12Renderer* renderer);
    virtual ~DX12RHIContext() = default;

    void SetVertexBuffer(uint32 slot, const RHIVertexBufferView& bufferView) override;
    void SetIndexBuffer(const RHIIndexBufferView& bufferView) override;
    void SetConstantBuffer(uint32 rootParameterIndex, const RHIConstantBufferView& bufferView) override;
    
    void SetVertexShader(const RHIShader& shader) override;
    void SetPixelShader(const RHIShader& shader) override;
    
    void SetTexture(uint32 slot, const RHITextureView& textureView) override;
    void SetSampler(uint32 slot, const RHISamplerView& samplerView) override;
    
    // Overloads for direct GPU handle binding
    void SetTexture(uint32 slot, void* gpuHandle) override;
    void SetSampler(uint32 slot, void* gpuHandle) override;
    
    void SetPrimitiveTopology(RHIPrimitiveTopology topology) override;
    void SetViewport(const RHIViewport& viewport) override;
    void SetScissorRect(const RHIRect& rect) override;
    
    void DrawIndexed(uint32 indexCount, uint32 startIndexLocation = 0, int32 baseVertexLocation = 0) override;
    void Draw(uint32 vertexCount, uint32 startVertexLocation = 0) override;

    RHIGraphicsAPI GetAPI() const override { return RHIGraphicsAPI::DirectX12; }

    ID3D12GraphicsCommandList* GetCommandList() const { return m_renderer->GetCommandList(); }
    ID3D12Device* GetDevice() const { return m_renderer->GetDevice(); }

private:
    D3D12_PRIMITIVE_TOPOLOGY ConvertTopology(RHIPrimitiveTopology topology) const;
    DXGI_FORMAT ConvertFormat(RHIResourceFormat format) const;

private:
    DX12Renderer* m_renderer = nullptr;
    
    DECLARE_NON_COPYABLE(DX12RHIContext);
};
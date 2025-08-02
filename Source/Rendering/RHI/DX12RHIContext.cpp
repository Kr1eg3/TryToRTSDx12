#include "DX12RHIContext.h"

DX12RHIContext::DX12RHIContext(DX12Renderer* renderer)
    : m_renderer(renderer) {
    ASSERT(renderer != nullptr, "DX12Renderer cannot be null");
}

void DX12RHIContext::SetVertexBuffer(uint32 slot, const RHIVertexBufferView& bufferView) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = bufferView.bufferLocation;
    vbView.SizeInBytes = bufferView.sizeInBytes;
    vbView.StrideInBytes = bufferView.strideInBytes;
    
    commandList->IASetVertexBuffers(slot, 1, &vbView);
}

void DX12RHIContext::SetIndexBuffer(const RHIIndexBufferView& bufferView) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = bufferView.bufferLocation;
    ibView.SizeInBytes = bufferView.sizeInBytes;
    ibView.Format = ConvertFormat(bufferView.format);
    
    commandList->IASetIndexBuffer(&ibView);
}

void DX12RHIContext::SetConstantBuffer(uint32 rootParameterIndex, const RHIConstantBufferView& bufferView) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, bufferView.bufferLocation);
}

void DX12RHIContext::SetVertexShader(const RHIShader& shader) {
}

void DX12RHIContext::SetPixelShader(const RHIShader& shader) {
}

void DX12RHIContext::SetTexture(uint32 slot, const RHITextureView& textureView) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    if (textureView.shaderResourceView) {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = *static_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(textureView.shaderResourceView);
        commandList->SetGraphicsRootDescriptorTable(slot, handle);
    }
}

void DX12RHIContext::SetSampler(uint32 slot, const RHISamplerView& samplerView) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    if (samplerView.samplerResource) {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = *static_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(samplerView.samplerResource);
        commandList->SetGraphicsRootDescriptorTable(slot, handle);
    }
}

void DX12RHIContext::SetTexture(uint32 slot, void* gpuHandle) {
    auto* commandList = GetCommandList();
    if (!commandList) {
        Platform::OutputDebugMessage("DX12RHIContext::SetTexture: Command list is null!\n");
        return;
    }
    
    if (!gpuHandle) {
        Platform::OutputDebugMessage("DX12RHIContext::SetTexture: GPU handle is null!\n");
        return;
    }
    
    try {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = *static_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(gpuHandle);
        Platform::OutputDebugMessage("DX12RHIContext: Setting texture descriptor table for slot " + std::to_string(slot) + 
                                    " with handle ptr: " + std::to_string(handle.ptr) + "\n");
        
        if (handle.ptr == 0) {
            Platform::OutputDebugMessage("DX12RHIContext::SetTexture: Warning - handle ptr is 0!\n");
            return;
        }
        
        commandList->SetGraphicsRootDescriptorTable(slot, handle);
        Platform::OutputDebugMessage("DX12RHIContext: Texture descriptor table set successfully\n");
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12RHIContext::SetTexture: Exception caught!\n");
    }
}

void DX12RHIContext::SetSampler(uint32 slot, void* gpuHandle) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    if (gpuHandle) {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = *static_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(gpuHandle);
        commandList->SetGraphicsRootDescriptorTable(slot, handle);
    }
}

void DX12RHIContext::SetPrimitiveTopology(RHIPrimitiveTopology topology) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    commandList->IASetPrimitiveTopology(ConvertTopology(topology));
}

void DX12RHIContext::SetViewport(const RHIViewport& viewport) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    D3D12_VIEWPORT vp = {};
    vp.TopLeftX = viewport.x;
    vp.TopLeftY = viewport.y;
    vp.Width = viewport.width;
    vp.Height = viewport.height;
    vp.MinDepth = viewport.minDepth;
    vp.MaxDepth = viewport.maxDepth;
    
    commandList->RSSetViewports(1, &vp);
}

void DX12RHIContext::SetScissorRect(const RHIRect& rect) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    D3D12_RECT scissorRect = {};
    scissorRect.left = rect.left;
    scissorRect.top = rect.top;
    scissorRect.right = rect.right;
    scissorRect.bottom = rect.bottom;
    
    commandList->RSSetScissorRects(1, &scissorRect);
}

void DX12RHIContext::DrawIndexed(uint32 indexCount, uint32 startIndexLocation, int32 baseVertexLocation) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    commandList->DrawIndexedInstanced(indexCount, 1, startIndexLocation, baseVertexLocation, 0);
}

void DX12RHIContext::Draw(uint32 vertexCount, uint32 startVertexLocation) {
    auto* commandList = GetCommandList();
    ASSERT(commandList != nullptr, "Command list is null");
    
    commandList->DrawInstanced(vertexCount, 1, startVertexLocation, 0);
}

D3D12_PRIMITIVE_TOPOLOGY DX12RHIContext::ConvertTopology(RHIPrimitiveTopology topology) const {
    switch (topology) {
        case RHIPrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case RHIPrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case RHIPrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case RHIPrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case RHIPrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:
            ASSERT(false, "Unknown primitive topology");
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

DXGI_FORMAT DX12RHIContext::ConvertFormat(RHIResourceFormat format) const {
    switch (format) {
        case RHIResourceFormat::R32G32B32_Float:    return DXGI_FORMAT_R32G32B32_FLOAT;
        case RHIResourceFormat::R32G32B32A32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RHIResourceFormat::R32G32_Float:       return DXGI_FORMAT_R32G32_FLOAT;
        case RHIResourceFormat::R32_Float:          return DXGI_FORMAT_R32_FLOAT;
        case RHIResourceFormat::R8G8B8A8_Unorm:     return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RHIResourceFormat::R16_Uint:           return DXGI_FORMAT_R16_UINT;
        case RHIResourceFormat::R32_Uint:           return DXGI_FORMAT_R32_UINT;
        case RHIResourceFormat::D32_Float:          return DXGI_FORMAT_D32_FLOAT;
        default:
            ASSERT(false, "Unknown resource format");
            return DXGI_FORMAT_UNKNOWN;
    }
}
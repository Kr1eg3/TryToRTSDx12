#pragma once

#include "IBindable.h"
#include "../RHI/DX12RHIContext.h"
#include "../../Platform/Windows/WindowsPlatform.h"

template<typename VertexType>
class VertexBuffer : public IBindable {
public:
    VertexBuffer(DX12Renderer& renderer, const Vector<VertexType>& vertices, const String& debugName = "VertexBuffer");
    virtual ~VertexBuffer() = default;

    void Bind(IRHIContext& context) override;
    bool IsValid() const override { return m_buffer != nullptr; }
    const String& GetDebugName() const override { return m_debugName; }

    uint32 GetVertexCount() const { return m_vertexCount; }
    uint32 GetStride() const { return sizeof(VertexType); }

private:
    bool CreateBuffer(DX12Renderer& renderer, const Vector<VertexType>& vertices);

private:
    ComPtr<ID3D12Resource> m_buffer;
    ComPtr<ID3D12Resource> m_uploadBuffer;
    RHIVertexBufferView m_bufferView;
    uint32 m_vertexCount = 0;
    
    DECLARE_NON_COPYABLE(VertexBuffer);
};

// Template implementation
template<typename VertexType>
VertexBuffer<VertexType>::VertexBuffer(DX12Renderer& renderer, const Vector<VertexType>& vertices, const String& debugName)
    : m_vertexCount(static_cast<uint32>(vertices.size())) {
    m_debugName = debugName;
    
    if (!CreateBuffer(renderer, vertices)) {
        Platform::OutputDebugMessage("Failed to create vertex buffer: " + debugName);
    }
}

template<typename VertexType>
void VertexBuffer<VertexType>::Bind(IRHIContext& context) {
    if (!IsValid()) {
        ASSERT(false, "Attempting to bind invalid vertex buffer");
        return;
    }
    
    context.SetVertexBuffer(0, m_bufferView);
}

template<typename VertexType>
bool VertexBuffer<VertexType>::CreateBuffer(DX12Renderer& renderer, const Vector<VertexType>& vertices) {
    if (vertices.empty()) {
        Platform::OutputDebugMessage("Vertex buffer is empty");
        return false;
    }

    const uint64 bufferSize = vertices.size() * sizeof(VertexType);
    
    D3D12_VERTEX_BUFFER_VIEW tempView;
    if (!renderer.CreateVertexBuffer(
        vertices.data(),
        bufferSize,
        m_buffer,
        m_uploadBuffer,
        tempView)) {
        return false;
    }
    
    m_bufferView.sizeInBytes = static_cast<uint32>(bufferSize);
    m_bufferView.strideInBytes = sizeof(VertexType);
    m_bufferView.bufferLocation = m_buffer->GetGPUVirtualAddress();
    
    renderer.SetDebugName(m_buffer.Get(), m_debugName);
    
    return true;
}
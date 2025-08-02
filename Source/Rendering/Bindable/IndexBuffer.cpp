#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(DX12Renderer& renderer, const Vector<uint32>& indices, const String& debugName)
    : m_indexCount(static_cast<uint32>(indices.size())) {
    m_debugName = debugName;
    
    if (!CreateBuffer(renderer, indices)) {
        Platform::OutputDebugMessage("Failed to create index buffer: " + debugName);
    }
}

void IndexBuffer::Bind(IRHIContext& context) {
    if (!IsValid()) {
        ASSERT(false, "Attempting to bind invalid index buffer");
        return;
    }
    
    context.SetIndexBuffer(m_bufferView);
}

bool IndexBuffer::CreateBuffer(DX12Renderer& renderer, const Vector<uint32>& indices) {
    if (indices.empty()) {
        Platform::OutputDebugMessage("Index buffer is empty");
        return false;
    }

    const uint64 bufferSize = indices.size() * sizeof(uint32);
    
    D3D12_INDEX_BUFFER_VIEW tempView;
    if (!renderer.CreateIndexBuffer(
        indices.data(),
        bufferSize,
        m_buffer,
        m_uploadBuffer,
        tempView)) {
        return false;
    }
    
    m_bufferView.sizeInBytes = static_cast<uint32>(bufferSize);
    m_bufferView.format = RHIResourceFormat::R32_Uint;
    m_bufferView.bufferLocation = m_buffer->GetGPUVirtualAddress();
    
    renderer.SetDebugName(m_buffer.Get(), m_debugName);
    
    return true;
}
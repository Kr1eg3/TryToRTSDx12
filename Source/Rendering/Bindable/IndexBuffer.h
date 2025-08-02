#pragma once

#include "IBindable.h"
#include "../RHI/DX12RHIContext.h"

class IndexBuffer : public IBindable {
public:
    IndexBuffer(DX12Renderer& renderer, const Vector<uint32>& indices, const String& debugName = "IndexBuffer");
    virtual ~IndexBuffer() = default;

    void Bind(IRHIContext& context) override;
    bool IsValid() const override { return m_buffer != nullptr; }
    const String& GetDebugName() const override { return m_debugName; }

    uint32 GetIndexCount() const { return m_indexCount; }

private:
    bool CreateBuffer(DX12Renderer& renderer, const Vector<uint32>& indices);

private:
    ComPtr<ID3D12Resource> m_buffer;
    ComPtr<ID3D12Resource> m_uploadBuffer;
    RHIIndexBufferView m_bufferView;
    uint32 m_indexCount = 0;
    
    DECLARE_NON_COPYABLE(IndexBuffer);
};
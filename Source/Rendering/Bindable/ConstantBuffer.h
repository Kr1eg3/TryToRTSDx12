#pragma once

#include "IBindable.h"
#include "../RHI/DX12RHIContext.h"
#include "../../Platform/Windows/WindowsPlatform.h"

template<typename ConstantType>
class ConstantBuffer : public IBindable {
public:
    ConstantBuffer(DX12Renderer& renderer, uint32 rootParameterIndex, const String& debugName = "ConstantBuffer");
    virtual ~ConstantBuffer() = default;

    void Bind(IRHIContext& context) override;
    bool IsValid() const override { return m_buffer != nullptr; }
    const String& GetDebugName() const override { return m_debugName; }

    void Update(const ConstantType& data);
    ConstantType* GetMappedData() { return m_mappedData; }

private:
    bool CreateBuffer(DX12Renderer& renderer);

private:
    ComPtr<ID3D12Resource> m_buffer;
    RHIConstantBufferView m_bufferView;
    ConstantType* m_mappedData = nullptr;
    uint32 m_rootParameterIndex = 0;
    
    DECLARE_NON_COPYABLE(ConstantBuffer);
};

// Template implementation
template<typename ConstantType>
ConstantBuffer<ConstantType>::ConstantBuffer(DX12Renderer& renderer, uint32 rootParameterIndex, const String& debugName)
    : m_rootParameterIndex(rootParameterIndex) {
    m_debugName = debugName;
    
    if (!CreateBuffer(renderer)) {
        Platform::OutputDebugMessage("Failed to create constant buffer: " + debugName);
    }
}

template<typename ConstantType>
void ConstantBuffer<ConstantType>::Bind(IRHIContext& context) {
    if (!IsValid()) {
        ASSERT(false, "Attempting to bind invalid constant buffer");
        return;
    }
    
    context.SetConstantBuffer(m_rootParameterIndex, m_bufferView);
}

template<typename ConstantType>
void ConstantBuffer<ConstantType>::Update(const ConstantType& data) {
    if (!IsValid() || m_mappedData == nullptr) {
        ASSERT(false, "Cannot update invalid constant buffer");
        return;
    }
    
    *m_mappedData = data;
}

template<typename ConstantType>
bool ConstantBuffer<ConstantType>::CreateBuffer(DX12Renderer& renderer) {
    constexpr uint64 alignedSize = (sizeof(ConstantType) + 255) & ~255;
    
    void* mappedData = nullptr;
    if (!renderer.CreateConstantBuffer(alignedSize, m_buffer, &mappedData)) {
        return false;
    }
    
    m_mappedData = static_cast<ConstantType*>(mappedData);
    m_bufferView.bufferLocation = m_buffer->GetGPUVirtualAddress();
    m_bufferView.sizeInBytes = static_cast<uint32>(alignedSize);
    
    renderer.SetDebugName(m_buffer.Get(), m_debugName);
    
    return true;
}
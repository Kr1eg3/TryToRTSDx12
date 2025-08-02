#include "DX12Renderer.h"
#include "../../Core/Window/Window.h"
#include "../../Platform/Windows/WindowsPlatform.h"
#include "../RHI/DX12RHIContext.h"

#include "../Mesh.h"
#include <fstream>
#include <sstream>

// Link required libraries
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

DX12Renderer::DX12Renderer() {
    Platform::OutputDebugMessage("DX12Renderer created\n");
}

DX12Renderer::~DX12Renderer() {
    Shutdown();
}

bool DX12Renderer::Initialize(Window* window, const RendererConfig& config) {
    Platform::OutputDebugMessage("Initializing DX12Renderer...\n");

    if (!window) {
        Platform::OutputDebugMessage("Error: Window is null\n");
        return false;
    }

    m_window = window;
    m_config = config;
    m_hwnd = static_cast<HWND>(window->GetNativeHandle());
    m_windowWidth = window->GetWidth();
    m_windowHeight = window->GetHeight();
    m_backBufferCount = config.backBufferCount;

    // Initialize in order
    try {
        if (m_config.enableDebugLayer) {
            EnableDebugLayer();
        }

        if (!CreateDevice()) return false;
        if (!CreateCommandQueue()) return false;
        if (!CreateSwapChain()) return false;
        if (!CreateDescriptorHeaps()) return false;
        if (!CreateRenderTargets()) return false;
        if (!CreateDepthStencil()) return false;
        if (!CreateCommandAllocators()) return false;
        if (!CreateCommandList()) return false;
        if (!CreateSynchronization()) return false;
        if (!CreateAllConstantBuffers()) return false;
        if (!CreateShaderDescriptorHeaps()) return false;

        if (m_config.enableDebugLayer) {
            SetupDebugDevice();
        }

        m_isInitialized = true;
        Platform::OutputDebugMessage("DX12Renderer initialized successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("DX12Renderer initialization failed: " + e.GetMessage());
        return false;
    }
}

void DX12Renderer::Shutdown() {
    if (!m_isInitialized) return;

    Platform::OutputDebugMessage("Shutting down DX12Renderer...\n");

    // Wait for GPU to finish
    WaitForGpu();

    // Clean up synchronization
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    // Unmap constant buffers
    for (uint32 i = 0; i < m_modelConstantBuffers.size(); ++i) {
        if (m_modelConstantBuffers[i] && m_mappedModelConstants[i]) {
            m_modelConstantBuffers[i]->Unmap(0, nullptr);
            m_mappedModelConstants[i] = nullptr;
        }
    }
    if (m_viewConstantBuffer && m_mappedViewConstants) {
        m_viewConstantBuffer->Unmap(0, nullptr);
        m_mappedViewConstants = nullptr;
    }
    if (m_lightConstantBuffer && m_mappedLightConstants) {
        m_lightConstantBuffer->Unmap(0, nullptr);
        m_mappedLightConstants = nullptr;
    }
    for (uint32 i = 0; i < m_materialConstantBuffers.size(); ++i) {
        if (m_materialConstantBuffers[i] && i < m_mappedMaterialConstants.size() && m_mappedMaterialConstants[i]) {
            m_materialConstantBuffers[i]->Unmap(0, nullptr);
            m_mappedMaterialConstants[i] = nullptr;
        }
    }

    // Release COM objects (smart pointers will handle this automatically)
    m_modelConstantBuffers.clear();
    m_mappedModelConstants.clear();
    m_viewConstantBuffer.Reset();
    m_lightConstantBuffer.Reset();
    m_materialConstantBuffers.clear();
    m_mappedMaterialConstants.clear();
    
    m_commandList.Reset();
    m_commandAllocators.clear();
    m_depthStencilBuffer.Reset();
    m_renderTargets.clear();
    m_dsvHeap.Reset();
    m_rtvHeap.Reset();
    m_srvHeap.Reset();
    m_samplerHeap.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_texturedPixelShader.Reset();
    m_swapChain.Reset();
    m_commandQueue.Reset();
    m_fence.Reset();
    m_device.Reset();
    m_debugDevice.Reset();
    m_debugController.Reset();

    m_isInitialized = false;
    Platform::OutputDebugMessage("DX12Renderer shutdown complete\n");
}

void DX12Renderer::BeginFrame() {
    ASSERT(m_isInitialized, "Renderer not initialized");

    // Reset command allocator and list for current frame
    THROW_IF_FAILED(m_commandAllocators[m_currentFrameIndex]->Reset(), "Reset command allocator");
    THROW_IF_FAILED(m_commandList->Reset(m_commandAllocators[m_currentFrameIndex].Get(), nullptr), "Reset command list");

    // Get current back buffer index
    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Transition back buffer to render target state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_currentBackBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList->ResourceBarrier(1, &barrier);

    // Set render targets
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += m_currentBackBufferIndex * m_rtvDescriptorSize;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Set viewport
    ViewportDesc viewport;
    viewport.width = static_cast<float>(m_windowWidth);
    viewport.height = static_cast<float>(m_windowHeight);
    SetViewport(viewport);
}

void DX12Renderer::EndFrame() {
    ASSERT(m_isInitialized, "Renderer not initialized");

    // Transition back buffer to present state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_currentBackBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList->ResourceBarrier(1, &barrier);

    // Close and execute command list
    THROW_IF_FAILED(m_commandList->Close(), "Close command list");

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    // Signal fence for current frame
    m_currentFenceValue++;
    THROW_IF_FAILED(m_commandQueue->Signal(m_fence.Get(), m_currentFenceValue), "Signal fence");
    m_fenceValues[m_currentFrameIndex] = m_currentFenceValue;
}

void DX12Renderer::Present() {
    ASSERT(m_isInitialized, "Renderer not initialized");

    // Present
    UINT presentFlags = 0;
    UINT syncInterval = m_config.vsyncEnabled ? 1 : 0;

    HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        Platform::OutputDebugMessage("Device removed/reset detected during Present\n");
        // Handle device removal/reset
        throw WindowsException(hr, "Present", __FILE__, __LINE__);
    }

    THROW_IF_FAILED(hr, "Present");

    // Move to next frame
    MoveToNextFrame();
}

void DX12Renderer::Clear(const ClearValues& clearValues) {
    ASSERT(m_isInitialized, "Renderer not initialized");

    // Clear render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += m_currentBackBufferIndex * m_rtvDescriptorSize;

    const float clearColor[] = {
        clearValues.color.x,
        clearValues.color.y,
        clearValues.color.z,
        clearValues.color.w
    };

    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Clear depth stencil
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_commandList->ClearDepthStencilView(dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        clearValues.depth, clearValues.stencil, 0, nullptr);
}

void DX12Renderer::SetViewport(const ViewportDesc& viewport) {
    D3D12_VIEWPORT d3dViewport = {};
    d3dViewport.TopLeftX = viewport.x;
    d3dViewport.TopLeftY = viewport.y;
    d3dViewport.Width = viewport.width;
    d3dViewport.Height = viewport.height;
    d3dViewport.MinDepth = viewport.minDepth;
    d3dViewport.MaxDepth = viewport.maxDepth;

    m_commandList->RSSetViewports(1, &d3dViewport);

    // Set scissor rect to match viewport
    D3D12_RECT scissorRect = {};
    scissorRect.left = static_cast<LONG>(viewport.x);
    scissorRect.top = static_cast<LONG>(viewport.y);
    scissorRect.right = static_cast<LONG>(viewport.x + viewport.width);
    scissorRect.bottom = static_cast<LONG>(viewport.y + viewport.height);

    m_commandList->RSSetScissorRects(1, &scissorRect);
}

void DX12Renderer::WaitForGpu() {
    if (!m_isInitialized) return;

    // Signal fence
    m_currentFenceValue++;
    THROW_IF_FAILED(m_commandQueue->Signal(m_fence.Get(), m_currentFenceValue), "Signal fence for GPU wait");

    // Wait for fence
    if (m_fence->GetCompletedValue() < m_currentFenceValue) {
        THROW_IF_FAILED(m_fence->SetEventOnCompletion(m_currentFenceValue, m_fenceEvent), "Set fence event");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DX12Renderer::Resize(uint32 width, uint32 height) {
    if (!m_isInitialized || (width == m_windowWidth && height == m_windowHeight)) {
        return;
    }

    Platform::OutputDebugMessage("Resizing renderer to " + std::to_string(width) + "x" + std::to_string(height) + "\n");

    // Wait for GPU to finish
    WaitForGpu();

    // Release render targets
    for (auto& rt : m_renderTargets) {
        rt.Reset();
    }
    m_renderTargets.clear();

    // Release depth stencil
    m_depthStencilBuffer.Reset();

    // Resize swap chain
    THROW_IF_FAILED(m_swapChain->ResizeBuffers(
        m_backBufferCount, width, height, m_backBufferFormat, 0), "Resize swap chain buffers");

    // Update dimensions
    m_windowWidth = width;
    m_windowHeight = height;

    // Recreate render targets and depth stencil
    CreateRenderTargets();
    CreateDepthStencil();

    Platform::OutputDebugMessage("Renderer resize complete\n");
}

void DX12Renderer::SetDebugName(void* resource, const String& name) {
    if (!resource || !m_config.enableDebugLayer) return;

    WString wName = Platform::StringToWString(name);
    static_cast<ID3D12Object*>(resource)->SetName(wName.c_str());
}

uint64 DX12Renderer::GetGpuMemoryUsage() const {
    if (!m_device) return 0;

    DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo = {};
    ComPtr<IDXGIAdapter3> adapter;

    // Get adapter from device
    LUID luid = m_device->GetAdapterLuid();
    ComPtr<IDXGIFactory4> factory;
    if (SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
        if (SUCCEEDED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)))) {
            if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo))) {
                return memoryInfo.CurrentUsage;
            }
        }
    }

    return 0;
}

bool DX12Renderer::CreateDevice() {
    Platform::OutputDebugMessage("Creating D3D12 device...\n");

    // Create DXGI factory
    ComPtr<IDXGIFactory4> factory;
    UINT factoryFlags = 0;
    if (m_config.enableDebugLayer) {
        factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
    }

    THROW_IF_FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)), "Create DXGI factory");

    // Find hardware adapter
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0;
         SUCCEEDED(factory->EnumAdapters1(adapterIndex, &adapter));
         ++adapterIndex) {

        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // Try to create device with this adapter
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)))) {
            Platform::OutputDebugMessage("D3D12 device created successfully\n");
            SetDebugName(m_device.Get(), "Main Device");
            return true;
        }
    }

    Platform::OutputDebugMessage("Failed to find compatible D3D12 adapter\n");
    return false;
}

bool DX12Renderer::CreateCommandQueue() {
    Platform::OutputDebugMessage("Creating command queue...\n");

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    THROW_IF_FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)),
                    "Create command queue");

    SetDebugName(m_commandQueue.Get(), "Main Command Queue");
    return true;
}

bool DX12Renderer::CreateSwapChain() {
    Platform::OutputDebugMessage("Creating swap chain...\n");

    // Create DXGI factory for swap chain
    ComPtr<IDXGIFactory4> factory;
    THROW_IF_FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "Create DXGI factory for swap chain");

    // Describe swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_windowWidth;
    swapChainDesc.Height = m_windowHeight;
    swapChainDesc.Format = m_backBufferFormat;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = m_backBufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    // Create swap chain
    ComPtr<IDXGISwapChain1> swapChain1;
    THROW_IF_FAILED(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        m_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1), "Create swap chain");

    // Get IDXGISwapChain3 interface
    THROW_IF_FAILED(swapChain1.As(&m_swapChain), "Query IDXGISwapChain3");

    // Disable fullscreen transitions
    THROW_IF_FAILED(factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER),
                    "Disable fullscreen transitions");

    return true;
}

bool DX12Renderer::CreateDescriptorHeaps() {
    Platform::OutputDebugMessage("Creating descriptor heaps...\n");

    CreateRtvDescriptorHeap();
    CreateDsvDescriptorHeap();

    return true;
}

void DX12Renderer::CreateRtvDescriptorHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = m_backBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;

    THROW_IF_FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
                    "Create RTV descriptor heap");

    SetDebugName(m_rtvHeap.Get(), "RTV Descriptor Heap");

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void DX12Renderer::CreateDsvDescriptorHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;

    THROW_IF_FAILED(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)),
                    "Create DSV descriptor heap");

    SetDebugName(m_dsvHeap.Get(), "DSV Descriptor Heap");

    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

bool DX12Renderer::CreateRenderTargets() {
    Platform::OutputDebugMessage("Creating render targets...\n");

    m_renderTargets.resize(m_backBufferCount);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (uint32 i = 0; i < m_backBufferCount; ++i) {
        // Get back buffer from swap chain
        THROW_IF_FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])),
                        "Get swap chain buffer " + std::to_string(i));

        // Create RTV
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);

        SetDebugName(m_renderTargets[i].Get(), "Back Buffer " + std::to_string(i));

        // Move to next descriptor
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    return true;
}

bool DX12Renderer::CreateDepthStencil() {
    Platform::OutputDebugMessage("Creating depth stencil buffer...\n");

    // Create depth stencil texture
    D3D12_RESOURCE_DESC depthStencilDesc = {};
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = m_windowWidth;
    depthStencilDesc.Height = m_windowHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = m_depthStencilFormat;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optimizedClearValue = {};
    optimizedClearValue.Format = m_depthStencilFormat;
    optimizedClearValue.DepthStencil.Depth = 1.0f;
    optimizedClearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 0;
    heapProps.VisibleNodeMask = 0;

    THROW_IF_FAILED(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &optimizedClearValue,
        IID_PPV_ARGS(&m_depthStencilBuffer)), "Create depth stencil buffer");

    SetDebugName(m_depthStencilBuffer.Get(), "Depth Stencil Buffer");

    // Create depth stencil view
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = m_depthStencilFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);

    return true;
}

bool DX12Renderer::CreateCommandAllocators() {
    Platform::OutputDebugMessage("Creating command allocators...\n");

    m_commandAllocators.resize(m_backBufferCount);

    for (uint32 i = 0; i < m_backBufferCount; ++i) {
        THROW_IF_FAILED(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])),
            "Create command allocator " + std::to_string(i));

        SetDebugName(m_commandAllocators[i].Get(), "Command Allocator " + std::to_string(i));
    }

    return true;
}

bool DX12Renderer::CreateCommandList() {
    Platform::OutputDebugMessage("Creating command list...\n");

    THROW_IF_FAILED(m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)), "Create command list");

    SetDebugName(m_commandList.Get(), "Main Command List");

    // Close command list (it starts in recording state)
    THROW_IF_FAILED(m_commandList->Close(), "Close initial command list");

    return true;
}

bool DX12Renderer::CreateSynchronization() {
    Platform::OutputDebugMessage("Creating synchronization objects...\n");

    // Create fence
    THROW_IF_FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)),
                    "Create fence");

    SetDebugName(m_fence.Get(), "Main Fence");

    // Initialize fence values
    m_fenceValues.resize(m_backBufferCount, 0);
    m_currentFenceValue = 0;

    // Create fence event
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()), "Create fence event");
    }

    return true;
}

void DX12Renderer::MoveToNextFrame() {
    // Move to next frame
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_backBufferCount;

    // Wait for the next frame's command allocator to be available
    WaitForFrame(m_currentFrameIndex);
}

void DX12Renderer::WaitForFrame(uint32 frameIndex) {
    if (m_fence->GetCompletedValue() < m_fenceValues[frameIndex]) {
        THROW_IF_FAILED(m_fence->SetEventOnCompletion(m_fenceValues[frameIndex], m_fenceEvent),
                        "Set fence event for frame wait");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DX12Renderer::EnableDebugLayer() {
    Platform::OutputDebugMessage("Enabling D3D12 debug layer...\n");

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)))) {
        m_debugController->EnableDebugLayer();

        if (m_config.enableGpuValidation) {
            ComPtr<ID3D12Debug1> debugController1;
            if (SUCCEEDED(m_debugController.As(&debugController1))) {
                debugController1->SetEnableGPUBasedValidation(TRUE);
                Platform::OutputDebugMessage("GPU-based validation enabled\n");
            }
        }
    } else {
        Platform::OutputDebugMessage("Warning: Failed to enable debug layer\n");
    }
}

void DX12Renderer::SetupDebugDevice() {
    if (SUCCEEDED(m_device.As(&m_debugDevice))) {
        if (m_config.enableBreakOnError) {
            //m_debugDevice->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            //m_debugDevice->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            //m_debugDevice->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
            Platform::OutputDebugMessage("Debug device break on error enabled\n");
        }

        // Filter out unnecessary messages
        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        };

        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumSeverities = ARRAY_SIZE(severities);
        filter.DenyList.pSeverityList = severities;
        filter.DenyList.NumIDs = ARRAY_SIZE(denyIds);
        filter.DenyList.pIDList = denyIds;

        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(m_device.As(&infoQueue))) {
            infoQueue->PushStorageFilter(&filter);
        }
    }
}

bool DX12Renderer::CreateBuffer(uint64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
                                ComPtr<ID3D12Resource>& buffer, const void* data,
                                ComPtr<ID3D12Resource>* uploadBuffer) {
    try {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = heapType;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = size;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // For default heap, start in COMMON state (D3D12 requirement)
        D3D12_RESOURCE_STATES creationState = (heapType == D3D12_HEAP_TYPE_DEFAULT) ?
            D3D12_RESOURCE_STATE_COMMON : initialState;

        THROW_IF_FAILED(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            creationState,
            nullptr,
            IID_PPV_ARGS(&buffer)), "Create buffer");

        // If data is provided and we need an upload buffer, create it but don't copy yet
        if (data && heapType == D3D12_HEAP_TYPE_DEFAULT && uploadBuffer) {
            D3D12_HEAP_PROPERTIES uploadHeapProps = {};
            uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            THROW_IF_FAILED(m_device->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(uploadBuffer->GetAddressOf())), "Create upload buffer");

            // Map and copy data to upload buffer
            void* mappedData;
            D3D12_RANGE readRange = { 0, 0 };
            THROW_IF_FAILED(uploadBuffer->Get()->Map(0, &readRange, &mappedData), "Map upload buffer");
            memcpy(mappedData, data, size);
            uploadBuffer->Get()->Unmap(0, nullptr);

            // NOTE: We don't copy from upload to default buffer here because command list might be closed
            // This will be done later when the command list is in recording state
        }

        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating buffer: " + e.GetMessage());
        return false;
    }
}

// New method to copy upload buffer to default buffer (call this when command list is recording)
bool DX12Renderer::CopyUploadToDefaultBuffer(ComPtr<ID3D12Resource>& defaultBuffer,
                                             ComPtr<ID3D12Resource>& uploadBuffer,
                                             uint64 size, D3D12_RESOURCE_STATES finalState) {
    if (!defaultBuffer || !uploadBuffer) {
        return false;
    }

    try {
        // Transition default buffer to copy destination
        D3D12_RESOURCE_BARRIER barrierToCopyDest = {};
        barrierToCopyDest.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierToCopyDest.Transition.pResource = defaultBuffer.Get();
        barrierToCopyDest.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrierToCopyDest.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrierToCopyDest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &barrierToCopyDest);

        // Copy from upload buffer to default buffer
        m_commandList->CopyBufferRegion(defaultBuffer.Get(), 0, uploadBuffer.Get(), 0, size);

        // Transition to final state
        if (finalState != D3D12_RESOURCE_STATE_COPY_DEST) {
            D3D12_RESOURCE_BARRIER barrierToFinal = {};
            barrierToFinal.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierToFinal.Transition.pResource = defaultBuffer.Get();
            barrierToFinal.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrierToFinal.Transition.StateAfter = finalState;
            barrierToFinal.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            m_commandList->ResourceBarrier(1, &barrierToFinal);
        }

        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error copying upload to default buffer: " + e.GetMessage());
        return false;
    }
}

bool DX12Renderer::CopyUploadToTexture(ComPtr<ID3D12Resource>& textureResource,
                                       ComPtr<ID3D12Resource>& uploadBuffer,
                                       uint32 width, uint32 height, DXGI_FORMAT format) {
    if (!textureResource || !uploadBuffer) {
        Platform::OutputDebugMessage("CopyUploadToTexture: Invalid resources\n");
        return false;
    }

    try {
        Platform::OutputDebugMessage("CopyUploadToTexture: Starting texture upload\n");
        
        // Ensure command list is ready for recording
        HRESULT hr = m_commandAllocators[m_currentFrameIndex]->Reset();
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("CopyUploadToTexture: Failed to reset command allocator\n");
            return false;
        }
        
        hr = m_commandList->Reset(m_commandAllocators[m_currentFrameIndex].Get(), nullptr);
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("CopyUploadToTexture: Failed to reset command list\n");
            return false;
        }

        // Transition texture to copy destination
        D3D12_RESOURCE_BARRIER barrierToCopyDest = {};
        barrierToCopyDest.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierToCopyDest.Transition.pResource = textureResource.Get();
        barrierToCopyDest.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrierToCopyDest.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrierToCopyDest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &barrierToCopyDest);

        // Setup texture copy locations
        D3D12_TEXTURE_COPY_LOCATION dest = {};
        dest.pResource = textureResource.Get();
        dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = 0;
        src.PlacedFootprint.Footprint.Format = format;
        src.PlacedFootprint.Footprint.Width = width;
        src.PlacedFootprint.Footprint.Height = height;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = (width * 4 + 255) & ~255; // Align to 256 bytes

        // Copy texture data
        m_commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);

        // Transition texture to shader resource
        D3D12_RESOURCE_BARRIER barrierToShaderResource = {};
        barrierToShaderResource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrierToShaderResource.Transition.pResource = textureResource.Get();
        barrierToShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrierToShaderResource.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrierToShaderResource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &barrierToShaderResource);

        // Close command list before execution
        hr = m_commandList->Close();
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("CopyUploadToTexture: Failed to close command list\n");
            return false;
        }

        Platform::OutputDebugMessage("CopyUploadToTexture: Texture upload completed\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error copying upload to texture: " + e.GetMessage());
        return false;
    }
}

void DX12Renderer::ExecuteUploadCommands() {
    Platform::OutputDebugMessage("DX12Renderer: Executing upload commands\n");
    ExecuteCommandListAndWait();
}

bool DX12Renderer::CreateVertexBuffer(const void* data, uint64 size, ComPtr<ID3D12Resource>& vertexBuffer,
                                     ComPtr<ID3D12Resource>& uploadBuffer, D3D12_VERTEX_BUFFER_VIEW& bufferView) {
    if (!CreateBuffer(size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                     vertexBuffer, data, &uploadBuffer)) {
        return false;
    }

    bufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    bufferView.StrideInBytes = sizeof(Vertex);
    bufferView.SizeInBytes = static_cast<UINT>(size);

    return true;
}

bool DX12Renderer::CreateIndexBuffer(const void* data, uint64 size, ComPtr<ID3D12Resource>& indexBuffer,
                                    ComPtr<ID3D12Resource>& uploadBuffer, D3D12_INDEX_BUFFER_VIEW& bufferView) {
    if (!CreateBuffer(size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
                     indexBuffer, data, &uploadBuffer)) {
        return false;
    }

    bufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    bufferView.Format = DXGI_FORMAT_R32_UINT;
    bufferView.SizeInBytes = static_cast<UINT>(size);

    return true;
}

bool DX12Renderer::CreateConstantBuffer(uint64 size, ComPtr<ID3D12Resource>& constantBuffer, void** mappedData) {
    // Align size to 256 bytes (required for constant buffers)
    uint64 alignedSize = (size + 255) & ~255;

    if (!CreateBuffer(alignedSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ,
                     constantBuffer, nullptr, nullptr)) {
        return false;
    }

    if (mappedData) {
        D3D12_RANGE readRange = { 0, 0 };
        THROW_IF_FAILED(constantBuffer->Map(0, &readRange, mappedData), "Map constant buffer");
    }

    return true;
}

bool DX12Renderer::CompileShader(const String& source, const String& entryPoint, const String& target,
                                ComPtr<ID3DBlob>& shaderBlob) {
    try {
        ComPtr<ID3DBlob> errorBlob;

        UINT compileFlags = 0;
        #ifdef _DEBUG
            compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        #endif

        HRESULT hr = D3DCompile(
            source.c_str(),
            source.length(),
            nullptr,
            nullptr,
            nullptr,
            entryPoint.c_str(),
            target.c_str(),
            compileFlags,
            0,
            &shaderBlob,
            &errorBlob
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                String errorMsg = "Shader compilation failed: ";
                errorMsg += static_cast<const char*>(errorBlob->GetBufferPointer());
                Platform::OutputDebugMessage(errorMsg);
            }
            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        Platform::OutputDebugMessage("Error compiling shader: " + String(e.what()));
        return false;
    }
}

bool DX12Renderer::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors,
                                       D3D12_DESCRIPTOR_HEAP_FLAGS flags, ComPtr<ID3D12DescriptorHeap>& heap) {
    try {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = type;
        heapDesc.NumDescriptors = numDescriptors;
        heapDesc.Flags = flags;
        heapDesc.NodeMask = 0;

        THROW_IF_FAILED(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)),
                        "Create descriptor heap");

        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating descriptor heap: " + e.GetMessage());
        return false;
    }
}

uint32 DX12Renderer::GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const {
    return m_device->GetDescriptorHandleIncrementSize(type);
}

DX12RHIContext* DX12Renderer::CreateRHIContext() {
    return new DX12RHIContext(this);
}

void DX12Renderer::DestroyRHIContext(DX12RHIContext* context) {
    delete context;
}

void DX12Renderer::ExecuteCommandListAndWait() {
    // Close command list
    THROW_IF_FAILED(m_commandList->Close(), "Close command list for execute and wait");

    // Execute command list
    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    // Wait for completion
    WaitForGpu();

    // Reset command list
    THROW_IF_FAILED(m_commandAllocators[m_currentFrameIndex]->Reset(), "Reset command allocator after wait");
    THROW_IF_FAILED(m_commandList->Reset(m_commandAllocators[m_currentFrameIndex].Get(), nullptr),
                    "Reset command list after wait");
}

bool DX12Renderer::CreateRootSignatures() {
    Platform::OutputDebugMessage("DX12Renderer: Creating Root Signatures...\n");
    
    if (!CreateBasicMeshRootSignature()) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create basic mesh root signature\n");
        return false;
    }
    
    if (!CreateTexturedMeshRootSignature()) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create textured mesh root signature\n");
        return false;
    }
    
    Platform::OutputDebugMessage("DX12Renderer: Root Signatures created successfully\n");
    return true;
}

bool DX12Renderer::CreateAllPipelineStates() {
    Platform::OutputDebugMessage("DX12Renderer: Creating all Pipeline State Objects...\n");
    
    // Create basic mesh PSO
    if (!CreateBasicMeshPSO(m_basicMeshRootSignature.Get(), 
                           m_vertexShader.Get(), 
                           m_pixelShader.Get())) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create basic mesh PSO\n");
        return false;
    }
    
    // Create wireframe mesh PSO
    if (!CreateWireframeMeshPSO(m_basicMeshRootSignature.Get(), 
                               m_vertexShader.Get(), 
                               m_pixelShader.Get())) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create wireframe mesh PSO\n");
        return false;
    }
    
    // Create textured mesh PSO
    if (!CreateTexturedMeshPSO(m_texturedMeshRootSignature.Get(), 
                              m_vertexShader.Get(), 
                              m_texturedPixelShader.Get())) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create textured mesh PSO\n");
        return false;
    }
    
    // Create textured wireframe mesh PSO
    if (!CreateTexturedWireframeMeshPSO(m_texturedMeshRootSignature.Get(), 
                                       m_vertexShader.Get(), 
                                       m_texturedPixelShader.Get())) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create textured wireframe mesh PSO\n");
        return false;
    }
    
    // Create emissive mesh PSO (if emissive shader is available)
    if (m_emissivePixelShader) {
        if (!CreateEmissiveMeshPSO(m_basicMeshRootSignature.Get(), 
                                  m_vertexShader.Get(), 
                                  m_emissivePixelShader.Get())) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create emissive mesh PSO\n");
            return false;
        }
        
        // Create emissive wireframe mesh PSO
        if (!CreateEmissiveWireframeMeshPSO(m_basicMeshRootSignature.Get(), 
                                           m_vertexShader.Get(), 
                                           m_emissivePixelShader.Get())) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create emissive wireframe mesh PSO\n");
            return false;
        }
    } else {
        Platform::OutputDebugMessage("DX12Renderer: Emissive pixel shader not available, skipping emissive PSOs\n");
    }
    
    Platform::OutputDebugMessage("DX12Renderer: All Pipeline State Objects created successfully\n");
    return true;
}

bool DX12Renderer::CreateBasicMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader) {
    Platform::OutputDebugMessage("DX12Renderer: Creating basic mesh PSO...\n");
    
    try {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = rootSignature;
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.RasterizerState.DepthBias = 0;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Depth stencil state
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Other settings
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_backBufferFormat;
        psoDesc.DSVFormat = m_depthStencilFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Create PSO
        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_basicMeshPSO));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create basic mesh PSO\n");
            return false;
        }

        SetDebugName(m_basicMeshPSO.Get(), "Basic Mesh PSO");
        Platform::OutputDebugMessage("DX12Renderer: Basic mesh PSO created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateBasicMeshPSO\n");
        return false;
    }
}

bool DX12Renderer::CreateWireframeMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader) {
    Platform::OutputDebugMessage("DX12Renderer: Creating wireframe mesh PSO...\n");
    
    try {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO description (same as basic but wireframe)
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = rootSignature;
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // Wireframe mode
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;      // Disable culling for wireframe
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.RasterizerState.DepthBias = 0;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Depth stencil state
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Other settings
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_backBufferFormat;
        psoDesc.DSVFormat = m_depthStencilFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Create PSO
        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_wireframeMeshPSO));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create wireframe mesh PSO\n");
            return false;
        }

        SetDebugName(m_wireframeMeshPSO.Get(), "Wireframe Mesh PSO");
        Platform::OutputDebugMessage("DX12Renderer: Wireframe mesh PSO created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateWireframeMeshPSO\n");
        return false;
    }
}

bool DX12Renderer::CreateTexturedMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader) {
    Platform::OutputDebugMessage("DX12Renderer: Creating textured mesh PSO...\n");
    
    try {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = rootSignature;
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.RasterizerState.DepthBias = 0;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Depth stencil state
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Other settings
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_backBufferFormat;
        psoDesc.DSVFormat = m_depthStencilFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Create PSO
        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_texturedMeshPSO));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create textured mesh PSO\n");
            return false;
        }

        SetDebugName(m_texturedMeshPSO.Get(), "Textured Mesh PSO");
        Platform::OutputDebugMessage("DX12Renderer: Textured mesh PSO created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateTexturedMeshPSO\n");
        return false;
    }
}

bool DX12Renderer::CreateTexturedWireframeMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader) {
    Platform::OutputDebugMessage("DX12Renderer: Creating textured wireframe mesh PSO...\n");
    
    try {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO description (textured but wireframe)
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = rootSignature;
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // Wireframe mode
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;      // Disable culling for wireframe
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.RasterizerState.DepthBias = 0;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Depth stencil state
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Other settings
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_backBufferFormat;
        psoDesc.DSVFormat = m_depthStencilFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Create PSO
        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_texturedWireframeMeshPSO));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create textured wireframe mesh PSO\n");
            return false;
        }

        SetDebugName(m_texturedWireframeMeshPSO.Get(), "Textured Wireframe Mesh PSO");
        Platform::OutputDebugMessage("DX12Renderer: Textured wireframe mesh PSO created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateTexturedWireframeMeshPSO\n");
        return false;
    }
}

bool DX12Renderer::CreateBasicMeshRootSignature() {
    Platform::OutputDebugMessage("DX12Renderer: Creating basic mesh root signature...\n");

    try {
        // Root parameters for constant buffers only
        D3D12_ROOT_PARAMETER rootParameters[4] = {};

        // Model constants (b0)
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // View constants (b1)
        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[1].Descriptor.ShaderRegister = 1;
        rootParameters[1].Descriptor.RegisterSpace = 0;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // Light constants (b2)
        rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[2].Descriptor.ShaderRegister = 2;
        rootParameters[2].Descriptor.RegisterSpace = 0;
        rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Material constants (b3)
        rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[3].Descriptor.ShaderRegister = 3;
        rootParameters[3].Descriptor.RegisterSpace = 0;
        rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Root signature description
        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = 4;
        rootSigDesc.pParameters = rootParameters;
        rootSigDesc.NumStaticSamplers = 0;
        rootSigDesc.pStaticSamplers = nullptr;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        // Serialize root signature
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                 &signature, &error);

        if (FAILED(hr)) {
            if (error) {
                Platform::OutputDebugMessage("DX12Renderer: Basic root signature serialization error: " +
                                            String(static_cast<const char*>(error->GetBufferPointer())));
            }
            return false;
        }

        // Create root signature
        hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(),
                                          signature->GetBufferSize(),
                                          IID_PPV_ARGS(&m_basicMeshRootSignature));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create basic mesh root signature\n");
            return false;
        }

        SetDebugName(m_basicMeshRootSignature.Get(), "Basic Mesh Root Signature");
        Platform::OutputDebugMessage("DX12Renderer: Basic mesh root signature created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateBasicMeshRootSignature\n");
        return false;
    }
}

bool DX12Renderer::CreateTexturedMeshRootSignature() {
    Platform::OutputDebugMessage("DX12Renderer: Creating textured mesh root signature...\n");

    try {
        // Root parameters for constant buffers, textures, and samplers
        D3D12_ROOT_PARAMETER rootParameters[4] = {};

        // Model constants (b0)
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // View constants (b1)
        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[1].Descriptor.ShaderRegister = 1;
        rootParameters[1].Descriptor.RegisterSpace = 0;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // Light constants (b2)
        rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[2].Descriptor.ShaderRegister = 2;
        rootParameters[2].Descriptor.RegisterSpace = 0;
        rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Static descriptor range for texture (t0)
        static D3D12_DESCRIPTOR_RANGE textureRange = {};
        textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        textureRange.NumDescriptors = 1;
        textureRange.BaseShaderRegister = 0;
        textureRange.RegisterSpace = 0;
        textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[3].DescriptorTable.pDescriptorRanges = &textureRange;
        rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Use static sampler instead of descriptor table
        static D3D12_STATIC_SAMPLER_DESC staticSampler = {};
        staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.MipLODBias = 0.0f;
        staticSampler.MaxAnisotropy = 1;
        staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        staticSampler.MinLOD = 0.0f;
        staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
        staticSampler.ShaderRegister = 0;
        staticSampler.RegisterSpace = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Root signature description
        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = 4;
        rootSigDesc.pParameters = rootParameters;
        rootSigDesc.NumStaticSamplers = 1;
        rootSigDesc.pStaticSamplers = &staticSampler;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        // Serialize root signature
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                 &signature, &error);

        if (FAILED(hr)) {
            if (error) {
                Platform::OutputDebugMessage("DX12Renderer: Textured root signature serialization error: " +
                                            String(static_cast<const char*>(error->GetBufferPointer())));
            }
            return false;
        }

        // Create root signature
        hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(),
                                          signature->GetBufferSize(),
                                          IID_PPV_ARGS(&m_texturedMeshRootSignature));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create textured mesh root signature\n");
            return false;
        }

        SetDebugName(m_texturedMeshRootSignature.Get(), "Textured Mesh Root Signature");
        Platform::OutputDebugMessage("DX12Renderer: Textured mesh root signature created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateTexturedMeshRootSignature\n");
        return false;
    }
}

bool DX12Renderer::CreateAllConstantBuffers() {
    Platform::OutputDebugMessage("Creating constant buffers...\n");

    try {
        // Create multiple model constant buffers for rendering multiple objects
        m_modelConstantBuffers.resize(MAX_OBJECTS);
        m_mappedModelConstants.resize(MAX_OBJECTS);

        for (uint32 i = 0; i < MAX_OBJECTS; ++i) {
            if (!CreateConstantBuffer(sizeof(ModelConstants),
                                     m_modelConstantBuffers[i],
                                     reinterpret_cast<void**>(&m_mappedModelConstants[i]))) {
                Platform::OutputDebugMessage("Failed to create model constant buffer " + std::to_string(i) + "\n");
                return false;
            }
            SetDebugName(m_modelConstantBuffers[i].Get(),
                       "Model Constants Buffer " + std::to_string(i));
        }

        // Create view constants buffer
        if (!CreateConstantBuffer(sizeof(ViewConstants), m_viewConstantBuffer,
                                 reinterpret_cast<void**>(&m_mappedViewConstants))) {
            Platform::OutputDebugMessage("Failed to create view constant buffer\n");
            return false;
        }
        SetDebugName(m_viewConstantBuffer.Get(), "View Constants Buffer");

        // Create light constants buffer
        if (!CreateConstantBuffer(sizeof(LightConstants), m_lightConstantBuffer,
                                 reinterpret_cast<void**>(&m_mappedLightConstants))) {
            Platform::OutputDebugMessage("Failed to create light constant buffer\n");
            return false;
        }
        SetDebugName(m_lightConstantBuffer.Get(), "Light Constants Buffer");

        // Create multiple material constant buffers for rendering multiple objects
        m_materialConstantBuffers.resize(MAX_OBJECTS);
        m_mappedMaterialConstants.resize(MAX_OBJECTS);

        for (uint32 i = 0; i < MAX_OBJECTS; ++i) {
            if (!CreateConstantBuffer(sizeof(MaterialConstants),
                                     m_materialConstantBuffers[i],
                                     reinterpret_cast<void**>(&m_mappedMaterialConstants[i]))) {
                Platform::OutputDebugMessage("Failed to create material constant buffer " + std::to_string(i) + "\n");
                return false;
            }
            SetDebugName(m_materialConstantBuffers[i].Get(),
                       "Material Constants Buffer " + std::to_string(i));
        }

        Platform::OutputDebugMessage("Constant buffers created successfully (Model buffers: " +
                                    std::to_string(MAX_OBJECTS) + ")\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating constant buffers: " + e.GetMessage());
        return false;
    }
}

uint32 DX12Renderer::AllocateObjectIndex() {
    uint32 objectIndex = m_currentObjectIndex;
    m_currentObjectIndex = (m_currentObjectIndex + 1) % MAX_OBJECTS;
    return objectIndex;
}

void DX12Renderer::UpdateModelConstants(const DirectX::XMMATRIX& modelMatrix, uint32 objectIndex) {
    if (objectIndex >= MAX_OBJECTS || !m_mappedModelConstants[objectIndex]) {
        Platform::OutputDebugMessage("Invalid objectIndex for UpdateModelConstants: " +
                                    std::to_string(objectIndex) + "\n");
        return;
    }

    m_mappedModelConstants[objectIndex]->modelMatrix = DirectX::XMMatrixTranspose(modelMatrix);
    // Normal matrix should be inverse transpose of the upper-left 3x3 of model matrix
    // Since we transpose for HLSL, we need: transpose(inverse(transpose(modelMatrix))) = inverse(modelMatrix)
    m_mappedModelConstants[objectIndex]->normalMatrix = DirectX::XMMatrixInverse(nullptr, modelMatrix);
}

void DX12Renderer::UpdateViewConstants(const DirectX::XMMATRIX& viewMatrix,
                                      const DirectX::XMMATRIX& projMatrix,
                                      const DirectX::XMFLOAT3& cameraPos) {
    if (!m_mappedViewConstants) return;

    DirectX::XMMATRIX viewProj = viewMatrix * projMatrix;

    m_mappedViewConstants->viewMatrix = DirectX::XMMatrixTranspose(viewMatrix);
    m_mappedViewConstants->projectionMatrix = DirectX::XMMatrixTranspose(projMatrix);
    m_mappedViewConstants->viewProjectionMatrix = DirectX::XMMatrixTranspose(viewProj);
    m_mappedViewConstants->cameraPosition = cameraPos;
}

void DX12Renderer::UpdateLightConstants(const DirectX::XMFLOAT3& lightPos,
                                       const DirectX::XMFLOAT3& lightColor,
                                       float intensity) {
    if (!m_mappedLightConstants) return;

    m_mappedLightConstants->lightPosition = lightPos;
    m_mappedLightConstants->lightColor = lightColor;
    m_mappedLightConstants->lightIntensity = intensity;
}

void DX12Renderer::UpdateMaterialConstants(const DirectX::XMFLOAT3& baseColor, uint32 objectIndex,
                                          float metallic, float roughness) {
    if (objectIndex >= MAX_OBJECTS || objectIndex >= m_mappedMaterialConstants.size() || !m_mappedMaterialConstants[objectIndex]) {
        Platform::OutputDebugMessage("Invalid objectIndex for UpdateMaterialConstants: " +
                                    std::to_string(objectIndex) + "\n");
        return;
    }

    m_mappedMaterialConstants[objectIndex]->baseColor = baseColor;
    m_mappedMaterialConstants[objectIndex]->metallic = metallic;
    m_mappedMaterialConstants[objectIndex]->roughness = roughness;
}

ID3D12Resource* DX12Renderer::GetModelConstantBuffer(uint32 objectIndex) const {
    if (objectIndex >= MAX_OBJECTS || objectIndex >= m_modelConstantBuffers.size()) {
        return nullptr;
    }
    return m_modelConstantBuffers[objectIndex].Get();
}

ID3D12Resource* DX12Renderer::GetMaterialConstantBuffer(uint32 objectIndex) const {
    if (objectIndex >= MAX_OBJECTS || objectIndex >= m_materialConstantBuffers.size()) {
        return nullptr;
    }
    return m_materialConstantBuffers[objectIndex].Get();
}

bool DX12Renderer::InitializeRenderingPipeline() {
    Platform::OutputDebugMessage("DX12Renderer: Initializing rendering pipeline...\n");

    // Load and compile shaders first
    if (!CreateBasicMeshShaders()) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create basic mesh shaders\n");
        return false;
    }

    // Create root signatures
    if (!CreateRootSignatures()) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create root signatures\n");
        return false;
    }

    // Create PSOs using compiled shaders
    if (!CreateAllPipelineStates()) {
        Platform::OutputDebugMessage("DX12Renderer: Failed to create pipeline states\n");
        return false;
    }

    Platform::OutputDebugMessage("DX12Renderer: Rendering pipeline initialized successfully\n");
    return true;
}

bool DX12Renderer::CreateShaderDescriptorHeaps() {
    Platform::OutputDebugMessage("Creating shader descriptor heaps in DX12Renderer...\n");

    try {
        // Create SRV descriptor heap (for textures)
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = 1024; // Support up to 1024 textures
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        srvHeapDesc.NodeMask = 0;

        THROW_IF_FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc,
                                                       IID_PPV_ARGS(&m_srvHeap)),
                        "Create SRV descriptor heap");
        
        SetDebugName(m_srvHeap.Get(), "SRV Descriptor Heap");
        m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Create Sampler descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.NumDescriptors = 256; // Support up to 256 samplers
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        samplerHeapDesc.NodeMask = 0;

        THROW_IF_FAILED(m_device->CreateDescriptorHeap(&samplerHeapDesc,
                                                       IID_PPV_ARGS(&m_samplerHeap)),
                        "Create Sampler descriptor heap");
        
        SetDebugName(m_samplerHeap.Get(), "Sampler Descriptor Heap");
        m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        Platform::OutputDebugMessage("Shader descriptor heaps created successfully in DX12Renderer\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating shader descriptor heaps: " + e.GetMessage());
        return false;
    }
}

uint32 DX12Renderer::AllocateSRVDescriptor() {
    if (m_currentSrvIndex >= 1024) {
        Platform::OutputDebugMessage("DX12Renderer: SRV descriptor heap is full!\n");
        return 0;
    }
    return m_currentSrvIndex++;
}

uint32 DX12Renderer::AllocateSamplerDescriptor() {
    if (m_currentSamplerIndex >= 256) {
        Platform::OutputDebugMessage("DX12Renderer: Sampler descriptor heap is full!\n");
        return 0;
    }
    return m_currentSamplerIndex++;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Renderer::GetSRVCPUHandle(uint32 index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * m_srvDescriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12Renderer::GetSRVGPUHandle(uint32 index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += index * m_srvDescriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Renderer::GetSamplerCPUHandle(uint32 index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * m_samplerDescriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12Renderer::GetSamplerGPUHandle(uint32 index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += index * m_samplerDescriptorSize;
    return handle;
}

bool DX12Renderer::LoadShaderSource(const String& filePath, String& shaderSource) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Platform::OutputDebugMessage("Failed to open shader file: " + filePath + "\n");
        return false;
    }

    // Read the entire file into string
    std::stringstream buffer;
    buffer << file.rdbuf();
    shaderSource = buffer.str();
    
    if (shaderSource.empty()) {
        Platform::OutputDebugMessage("Shader file is empty: " + filePath + "\n");
        return false;
    }

    Platform::OutputDebugMessage("Loaded shader from file: " + filePath + " (" + 
                                std::to_string(shaderSource.length()) + " bytes)\n");
    return true;
}

bool DX12Renderer::CreateBasicMeshShaders() {
    Platform::OutputDebugMessage("Creating basic mesh shaders from files...\n");

    // Load shaders from files
    String vertexShaderCode;
    String pixelShaderCode;
    String texturedPixelShaderCode;
    String emissivePixelShaderCode;

    if (!LoadShaderSource("../../Shaders/BasicMesh.vs.hlsl", vertexShaderCode)) {
        Platform::OutputDebugMessage("Failed to load vertex shader from file\n");
        return false;
    }

    if (!LoadShaderSource("../../Shaders/BasicMesh.ps.hlsl", pixelShaderCode)) {
        Platform::OutputDebugMessage("Failed to load pixel shader from file\n");
        return false;
    }

    if (!LoadShaderSource("../../Shaders/TexturedMesh.ps.hlsl", texturedPixelShaderCode)) {
        Platform::OutputDebugMessage("Failed to load textured pixel shader from file\n");
        return false;
    }

    if (!LoadShaderSource("../../Shaders/EmissiveMesh.ps.hlsl", emissivePixelShaderCode)) {
        Platform::OutputDebugMessage("Failed to load emissive pixel shader from file\n");
        return false;
    }
    Platform::OutputDebugMessage("Emissive pixel shader loaded successfully\n");

    // Compile shaders
    if (!CompileShader(vertexShaderCode, "VSMain", "vs_5_0", m_vertexShader)) {
        Platform::OutputDebugMessage("Failed to compile vertex shader\n");
        return false;
    }

    if (!CompileShader(pixelShaderCode, "PSMain", "ps_5_0", m_pixelShader)) {
        Platform::OutputDebugMessage("Failed to compile pixel shader\n");
        return false;
    }

    if (!CompileShader(texturedPixelShaderCode, "PSMain", "ps_5_0", m_texturedPixelShader)) {
        Platform::OutputDebugMessage("Failed to compile textured pixel shader\n");
        return false;
    }

    if (!CompileShader(emissivePixelShaderCode, "PSMain", "ps_5_0", m_emissivePixelShader)) {
        Platform::OutputDebugMessage("Failed to compile emissive pixel shader\n");
        return false;
    }
    Platform::OutputDebugMessage("Emissive pixel shader compiled successfully\n");

    Platform::OutputDebugMessage("Basic mesh shaders compiled successfully from files\n");
    return true;
}

void DX12Renderer::BindForMeshRendering(ID3D12GraphicsCommandList* commandList, uint32 objectIndex) {
    if (!commandList || !m_isInitialized) {
        Platform::OutputDebugMessage("Invalid parameters for BindForMeshRendering: objectIndex=" +
                                    std::to_string(objectIndex) + "\n");
        return;
    }

    // Choose PSO based on wireframe mode
    ID3D12PipelineState* pso = m_wireframeMode ? GetWireframeMeshPSO() : GetBasicMeshPSO();
    commandList->SetPipelineState(pso);
    commandList->SetGraphicsRootSignature(GetBasicMeshRootSignature());

    // Bind constant buffers
    ID3D12Resource* modelCB = GetModelConstantBuffer(objectIndex);
    if (modelCB) {
        commandList->SetGraphicsRootConstantBufferView(0, modelCB->GetGPUVirtualAddress());
    }
    commandList->SetGraphicsRootConstantBufferView(1, GetViewConstantBuffer()->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, GetLightConstantBuffer()->GetGPUVirtualAddress());
    ID3D12Resource* materialCB = GetMaterialConstantBuffer(objectIndex);
    if (materialCB) {
        commandList->SetGraphicsRootConstantBufferView(3, materialCB->GetGPUVirtualAddress());
    }
}

void DX12Renderer::BindForTexturedMeshRendering(ID3D12GraphicsCommandList* commandList, uint32 objectIndex) {
    Platform::OutputDebugMessage("BindForTexturedMeshRendering: Starting with objectIndex=" + std::to_string(objectIndex) + "\n");
    
    if (!GetTexturedMeshRootSignature() || !commandList) {
        Platform::OutputDebugMessage("BindForTexturedMeshRendering: Missing root signature or command list\n");
        return;
    }
    
    // Check if textured PSOs exist
    if (!GetTexturedMeshPSO() || !GetTexturedWireframeMeshPSO()) {
        Platform::OutputDebugMessage("BindForTexturedMeshRendering: Missing textured PSOs, falling back to basic rendering\n");
        BindForMeshRendering(commandList, objectIndex);
        return;
    }
    
    // Validate constant buffers
    if (!GetModelConstantBuffer(objectIndex) ||
        !GetViewConstantBuffer() ||
        !GetLightConstantBuffer()) {
        Platform::OutputDebugMessage("BindForTexturedMeshRendering: Missing constant buffers, falling back to basic rendering\n");
        BindForMeshRendering(commandList, objectIndex);
        return;
    }
    
    Platform::OutputDebugMessage("BindForTexturedMeshRendering: Setting root signature\n");
    // Set textured root signature and PSO
    commandList->SetGraphicsRootSignature(GetTexturedMeshRootSignature());
    
    Platform::OutputDebugMessage("BindForTexturedMeshRendering: Setting PSO\n");
    if (m_wireframeMode) {
        commandList->SetPipelineState(GetTexturedWireframeMeshPSO());
    } else {
        commandList->SetPipelineState(GetTexturedMeshPSO());
    }
    
    Platform::OutputDebugMessage("BindForTexturedMeshRendering: Binding constant buffers\n");
    // Bind constant buffers
    commandList->SetGraphicsRootConstantBufferView(0,
        GetModelConstantBuffer(objectIndex)->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1,
        GetViewConstantBuffer()->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2,
        GetLightConstantBuffer()->GetGPUVirtualAddress());
    ID3D12Resource* materialCB = GetMaterialConstantBuffer(objectIndex);
    if (materialCB) {
        commandList->SetGraphicsRootConstantBufferView(3, materialCB->GetGPUVirtualAddress());
    }
    
    Platform::OutputDebugMessage("BindForTexturedMeshRendering: Completed successfully\n");
}

void DX12Renderer::BindForEmissiveMeshRendering(ID3D12GraphicsCommandList* commandList, uint32 objectIndex) {
    Platform::OutputDebugMessage("BindForEmissiveMeshRendering: Starting with objectIndex=" + std::to_string(objectIndex) + "\n");
    
    if (!commandList || !m_isInitialized) {
        Platform::OutputDebugMessage("Invalid parameters for BindForEmissiveMeshRendering: objectIndex=" +
                                    std::to_string(objectIndex) + "\n");
        return;
    }

    // Choose PSO based on wireframe mode - use emissive PSOs
    ID3D12PipelineState* pso = m_wireframeMode ? GetEmissiveWireframeMeshPSO() : GetEmissiveMeshPSO();
    if (pso) {
        Platform::OutputDebugMessage("BindForEmissiveMeshRendering: Using emissive PSO\n");
        commandList->SetPipelineState(pso);
    } else {
        Platform::OutputDebugMessage("BindForEmissiveMeshRendering: Emissive PSO not available, falling back to basic\n");
        pso = m_wireframeMode ? GetWireframeMeshPSO() : GetBasicMeshPSO();
        commandList->SetPipelineState(pso);
    }
    
    // Use basic mesh root signature (same constants layout)
    commandList->SetGraphicsRootSignature(GetBasicMeshRootSignature());

    // Bind constant buffers (same as basic mesh)
    ID3D12Resource* modelCB = GetModelConstantBuffer(objectIndex);
    if (modelCB) {
        commandList->SetGraphicsRootConstantBufferView(0, modelCB->GetGPUVirtualAddress());
    }
    commandList->SetGraphicsRootConstantBufferView(1, GetViewConstantBuffer()->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, GetLightConstantBuffer()->GetGPUVirtualAddress());
    ID3D12Resource* materialCB = GetMaterialConstantBuffer(objectIndex);
    if (materialCB) {
        commandList->SetGraphicsRootConstantBufferView(3, materialCB->GetGPUVirtualAddress());
    }
}

bool DX12Renderer::CreateEmissiveMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader) {
    Platform::OutputDebugMessage("DX12Renderer: Creating emissive mesh PSO...\n");
    
    try {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = rootSignature;
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.RasterizerState.DepthBias = 0;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Depth stencil state
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Other settings
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_backBufferFormat;
        psoDesc.DSVFormat = m_depthStencilFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Create PSO
        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_emissiveMeshPSO));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create emissive mesh PSO\n");
            return false;
        }

        SetDebugName(m_emissiveMeshPSO.Get(), "Emissive Mesh PSO");
        Platform::OutputDebugMessage("DX12Renderer: Emissive mesh PSO created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateEmissiveMeshPSO\n");
        return false;
    }
}

bool DX12Renderer::CreateEmissiveWireframeMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader) {
    Platform::OutputDebugMessage("DX12Renderer: Creating emissive wireframe mesh PSO...\n");
    
    try {
        // Input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // PSO description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = rootSignature;
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // Wireframe mode
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.RasterizerState.DepthBias = 0;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Depth stencil state
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Other settings
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_backBufferFormat;
        psoDesc.DSVFormat = m_depthStencilFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Create PSO
        HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_emissiveWireframeMeshPSO));
        if (FAILED(hr)) {
            Platform::OutputDebugMessage("DX12Renderer: Failed to create emissive wireframe mesh PSO\n");
            return false;
        }

        SetDebugName(m_emissiveWireframeMeshPSO.Get(), "Emissive Wireframe Mesh PSO");
        Platform::OutputDebugMessage("DX12Renderer: Emissive wireframe mesh PSO created successfully\n");
        return true;
    }
    catch (...) {
        Platform::OutputDebugMessage("DX12Renderer: Exception in CreateEmissiveWireframeMeshPSO\n");
        return false;
    }
}

// Factory method implementation
UniquePtr<Renderer> Renderer::Create() {
    return std::make_unique<DX12Renderer>();
}
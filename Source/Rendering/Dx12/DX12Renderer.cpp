#include "DX12Renderer.h"
#include "../../Core/Window/Window.h"
#include "../../Platform/Windows/WindowsPlatform.h"

// Link required libraries
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

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

    // Release COM objects (smart pointers will handle this automatically)
    m_commandList.Reset();
    m_commandAllocators.clear();
    m_depthStencilBuffer.Reset();
    m_renderTargets.clear();
    m_dsvHeap.Reset();
    m_rtvHeap.Reset();
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

// Factory method implementation
UniquePtr<Renderer> Renderer::Create() {
    return std::make_unique<DX12Renderer>();
}
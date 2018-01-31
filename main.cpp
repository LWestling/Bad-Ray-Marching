#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <SimpleMath.h>
#include <time.h>
#include <SDL.h>
#include <string>

#define TOI(x) assert(SUCCEEDED(x))
#define Rel(x) if(x) x->Release();

#define WIN_TITLE "DX12 Training"
#define WIN_WIDTH 800
#define WIN_HEIGHT 600

#define RES_W 1920
#define RES_H 1080

#define VS_NAME L"vs.hlsl"
#define VS_TARGET "vs_5_0"
#define VS_MAIN "main"

#define PS_NAME L"ps.hlsl"
#define PS_TARGET "ps_5_0"
#define PS_MAIN "main"

#define NO_ADAPTER -2 

// SDL And common stuff
const int FRAME_COUNT = 2;
int gFrameIndex = 0;
int gRTVSize;
bool gRunning;
SDL_Window *gWindow = nullptr;
    
// Dev & Factory
ID3D12Device *gDevice = nullptr;
IDXGIFactory5 *gFactory = nullptr;

/* Command stuff */
ID3D12CommandQueue *gCmdQue = nullptr;
ID3D12GraphicsCommandList *gCmdList = nullptr;
ID3D12CommandAllocator *gCmdAll = nullptr;
ID3D12RootSignature *gRootSign = nullptr;

/* Pipeline */
ID3D12PipelineState *gPipelineState = nullptr;

/* Fencing */
ID3D12Fence *gFence = nullptr;
UINT gFenceValue = 0;
HANDLE gFenceEvent;

// unsorted :Sk
ID3D12DescriptorHeap *gRenderTargetHeap = nullptr;
ID3D12Resource *gRenderTarget[FRAME_COUNT];
D3D12_VIEWPORT gViewport;
D3D12_RECT gClip;
IDXGISwapChain1 *gSwapChain = nullptr;
IDXGIAdapter *gAdapter;

// debug
ID3D12Debug *gDebugController;

// triangle
const int VERTEX_COUNT = 6;
float gTriangle[] = {   
                        -1.f,   1.f,   0.1f,
                        0.f,   1.f,
                        1.f,   -1.f,   0.1f,
                        1.f,   0.f,
                        -1.f,  -1.f,   0.1f,
                        0.f,   0.f,
                        -1.f,  1.f,    0.1f,
                        0.f,   1.f,
                        1.f,   1.f,    0.1f,
                        1.f,   1.f,
                        1.f,   -1.f,   0.1f,
                        1.f,   0.f,
                    };
ID3D12Resource *gTriangleRes;
D3D12_VERTEX_BUFFER_VIEW gTriangleView;

// CS
struct Position {
    DirectX::SimpleMath::Vector4 pos, look;
    DirectX::SimpleMath::Matrix cam, view;
} gCamera;
struct TimeConst {
    float time;
    DirectX::SimpleMath::Vector3 stuff;
} gTime;
ID3D12Resource *gCameraRes;
ID3D12Resource *gTimeRes;
DirectX::SimpleMath::Vector3 gEye = DirectX::SimpleMath::Vector3(0.f, 0.f, -10.f);
DirectX::SimpleMath::Vector3 gDir = DirectX::SimpleMath::Vector3(0.f, 0.f, 1.f);

ID3D12DescriptorHeap *gDescHeap;

// extra stuff
float timeStart;

int main(int argc, char * argv[]);

void SetupWindow();
int SetupDevice();
void SetupSwapChain();
void SetupDebugging();
void SetupCommandQueueAndFencing();
void CreateResouces();
void SetupTODO();
void CreateRootSignature();
void CreateShadersAndPipeline();
void CreateVertexBuffer();
void UploadDataToGpu();
void RenderLoop();
void Render();
void ReleaseMemory();

void SetupConstantBuffer();
void UpdateConstantBuffers();

void HandleSdlEvent(SDL_Event e);

void WaitForGpu();

int main(int argc, char* argv[]) {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    SetupWindow();
    SetupDebugging();
    if (SetupDevice() != 0)
        assert(false); // lul
#if defined(_DEBUG)
    SetupDebugging();
#endif
    SetupCommandQueueAndFencing();
    SetupSwapChain();
    CreateResouces();
    //SetupTODO();
    CreateRootSignature();
    CreateShadersAndPipeline();
    CreateVertexBuffer();
    UploadDataToGpu();
    SetupConstantBuffer();

    RenderLoop();

    ReleaseMemory();
    
    return 0;
}

void SetupWindow()
{
    SDL_Init(SDL_INIT_VIDEO);
    gWindow = SDL_CreateWindow(WIN_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, 0);
}

int SetupDevice()
{
    int dxgiFactoryFlags = 0;
    #if defined(_DEBUG)
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG; // dont like it, MOVE
    #endif
    // create factory
    TOI(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&gFactory)));

    // create device
    for (int i = 0;; i++) {
        gAdapter = nullptr;
        if (DXGI_ERROR_NOT_FOUND == gFactory->EnumAdapters(0, &gAdapter))
            return NO_ADAPTER;
        if (SUCCEEDED(D3D12CreateDevice(gAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&gDevice))))
            break;
        Rel(gAdapter)
    }

    return 0;
}

void SetupSwapChain() {
    // create swapchain
    DXGI_SWAP_CHAIN_DESC1 swapDesc;
    ZeroMemory(&swapDesc, sizeof(swapDesc));
    swapDesc.Width              = WIN_WIDTH;
    swapDesc.Height             = WIN_HEIGHT;
    swapDesc.Scaling            = DXGI_SCALING_STRETCH;
    swapDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount        = FRAME_COUNT;
    swapDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.SampleDesc.Count   = 1;
    swapDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;

    TOI(gFactory->CreateSwapChainForHwnd(gCmdQue, GetActiveWindow(), &swapDesc, nullptr, nullptr, &gSwapChain));

    ZeroMemory(&gViewport, sizeof(gViewport));
    ZeroMemory(&gClip, sizeof(gClip));

    gViewport.Height   = gClip.bottom     = WIN_HEIGHT;
    gViewport.Width    = gClip.right      = WIN_WIDTH;
    gViewport.MaxDepth = 1.f;
}

void SetupDebugging() {
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&gDebugController))))
    {
        gDebugController->EnableDebugLayer();
    }
}

void SetupCommandQueueAndFencing()
{
    // Create cmd que 
    D3D12_COMMAND_QUEUE_DESC queDesc;
    ZeroMemory(&queDesc, sizeof(queDesc));
    queDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
    queDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    gDevice->CreateCommandQueue(&queDesc, IID_PPV_ARGS(&gCmdQue));

    // create cmd allocator & list
    TOI(gDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gCmdAll)));
    TOI(gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gCmdAll, nullptr, IID_PPV_ARGS(&gCmdList)));
    gCmdList->Close();

    // create the fence
    gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gFence));
    gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void CreateResouces()
{
    // Render target memory
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
    ZeroMemory(&heapDesc, sizeof(heapDesc));
    heapDesc.NumDescriptors = FRAME_COUNT; // swappers chagen todo
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    // Get cpu handle and size
    TOI(gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gRenderTargetHeap)));
    gRTVSize = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE cdh = gRenderTargetHeap->GetCPUDescriptorHandleForHeapStart();

    // Get render target
    for (int i = 0; i < FRAME_COUNT; i++) {
        gSwapChain->GetBuffer(i, IID_PPV_ARGS(&gRenderTarget[i])); // get ptr from swap chain
        gDevice->CreateRenderTargetView(gRenderTarget[i], nullptr, cdh); // create render target to the ptr
        cdh.ptr += gRTVSize; // offset memory by size of render target
    }
}

void SetupTODO()
{
    gCmdAll->Reset();
    gCmdList->Reset(gCmdAll, nullptr);
    gCmdList->RSSetViewports(1, &gViewport);
    gCmdList->RSSetScissorRects(1, &gClip);
    gCmdList->Close();

    ID3D12CommandList *exc[] = { gCmdList };
    gCmdQue->ExecuteCommandLists(1, exc);
    WaitForGpu();
}

void CreateRootSignature()
{
    /* Parameter for the shader stuff */
    D3D12_DESCRIPTOR_RANGE range[1];
    ZeroMemory(&range, sizeof(range));
    range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    range[0].BaseShaderRegister = 0; // register(b0), b0, 0
    range[0].RegisterSpace = 0; // register(b0, space0), space0, 0. Space 0 is automaticly the space selected when no is available?
    range[0].NumDescriptors = 2; // two 
    range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // the table
    D3D12_ROOT_DESCRIPTOR_TABLE table;
    table.NumDescriptorRanges = ARRAYSIZE(range);
    table.pDescriptorRanges = range;

    // 1 parameter in the table
    D3D12_ROOT_PARAMETER para[1];
    ZeroMemory(&para, sizeof(para));
    para[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    para[0].DescriptorTable = table;
    para[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // create the root sign
    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    desc.NumParameters = ARRAYSIZE(para);
    desc.pParameters = para;
    
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;

    ID3DBlob *blob = nullptr;
    D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, nullptr);
    gDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&gRootSign));
    Rel(blob);
}

void CreateShadersAndPipeline()
{
    // compiling shaders is very fun
    ID3DBlob *vsBlob;
    D3DCompileFromFile(VS_NAME, nullptr, nullptr, VS_MAIN, VS_TARGET, D3DCOMPILE_DEBUG, 0, &vsBlob, nullptr);
    ID3DBlob *psBlob;
    D3DCompileFromFile(PS_NAME, nullptr, nullptr, PS_MAIN, PS_TARGET, D3DCOMPILE_DEBUG, 0, &psBlob, nullptr);

    D3D12_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 3 * sizeof(float), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_INPUT_LAYOUT_DESC layoutDesc;
    layoutDesc.NumElements = ARRAYSIZE(inputLayoutDesc);
    layoutDesc.pInputElementDescs = inputLayoutDesc;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc;
    ZeroMemory(&pipelineDesc, sizeof(pipelineDesc));

    // shaders
    pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
    pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize();
    pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();

    // Root sign, input layout & topology
    pipelineDesc.pRootSignature = gRootSign;
    pipelineDesc.InputLayout = layoutDesc;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineDesc.NumRenderTargets = 1;

    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = UINT_MAX;

    pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pipelineDesc.DepthStencilState.DepthEnable = false;

    D3D12_RENDER_TARGET_BLEND_DESC defBDesc = {
        false, false,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL
    };
    for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        pipelineDesc.BlendState.RenderTarget[i] = defBDesc;

    gDevice->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&gPipelineState));
}

void CreateVertexBuffer()
{
    D3D12_HEAP_PROPERTIES heapProp;
    ZeroMemory(&heapProp, sizeof(heapProp));
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc;
    ZeroMemory(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeof(gTriangle);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    TOI(gDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&gTriangleRes)
    ));

    gTriangleView.BufferLocation = gTriangleRes->GetGPUVirtualAddress();
    gTriangleView.SizeInBytes = sizeof(gTriangle);
    gTriangleView.StrideInBytes = sizeof(gTriangle) / VERTEX_COUNT;
}

void UploadDataToGpu()
{
    void *data = nullptr;
    D3D12_RANGE readRange = { 0, 0 }; // if end <= start then no read
    gTriangleRes->Map(0, &readRange, &data);
    memcpy(data, gTriangle, sizeof(gTriangle));
    gTriangleRes->Unmap(0, nullptr); // nullptr because everything is written
}

void RenderLoop()
{
    gRunning = true;
    SDL_Event sdlEvent;
    UINT64 test;

    while (gRunning) {
        test = clock();
        while (SDL_PollEvent(&sdlEvent)) {
            if (sdlEvent.type == SDL_QUIT)
                gRunning = false;
            else
                HandleSdlEvent(sdlEvent);
        }

        Render();
        WaitForGpu();
        UpdateConstantBuffers();
        SDL_SetWindowTitle(gWindow, (std::string("DX12 Noob Stuff MS: ") + std::to_string(clock() - test) + " F: " + std::to_string(gFrameIndex)).c_str());
    }
}

void ResourceBarrierTrans(ID3D12Resource *res, D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER bDesc;
    ZeroMemory(&bDesc, sizeof(bDesc));
    bDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bDesc.Transition.pResource = res;
    bDesc.Transition.StateBefore = before;
    bDesc.Transition.StateAfter = after;
    bDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    gCmdList->ResourceBarrier(1, &bDesc);
}

void Render()
{
    gCmdAll->Reset();
    gCmdList->Reset(gCmdAll, gPipelineState);

    auto cpuDescHandle = gRenderTargetHeap->GetCPUDescriptorHandleForHeapStart();
    cpuDescHandle.ptr += gRTVSize * gFrameIndex;

    float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };

    // root sign
    gCmdList->SetGraphicsRootSignature(gRootSign);

    ID3D12DescriptorHeap* heaps[] = { gDescHeap };
    gCmdList->SetDescriptorHeaps(ARRAYSIZE(heaps), heaps);
    gCmdList->SetGraphicsRootDescriptorTable(0, gDescHeap->GetGPUDescriptorHandleForHeapStart());

    ResourceBarrierTrans(
        gRenderTarget[gFrameIndex],
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    gCmdList->RSSetViewports(1, &gViewport);
    gCmdList->RSSetScissorRects(1, &gClip);
    gCmdList->OMSetRenderTargets(1, &cpuDescHandle, true, nullptr);

    gCmdList->ClearRenderTargetView(cpuDescHandle, clearColor, 0, nullptr);

    gCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    gCmdList->IASetVertexBuffers(0, 1, &gTriangleView);
    gCmdList->DrawInstanced(6, 1, 0, 0);

    ResourceBarrierTrans(
        gRenderTarget[gFrameIndex],
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );

    gCmdList->Close();
    ID3D12CommandList *exc[] = { gCmdList };
    gCmdQue->ExecuteCommandLists(ARRAYSIZE(exc), exc);

    DXGI_PRESENT_PARAMETERS pp = {}; //nada
    gSwapChain->Present1(0, 0, &pp);

    gFrameIndex++;
    gFrameIndex %= FRAME_COUNT;
}

void ReleaseMemory() {
    if (gWindow) {
        SDL_DestroyWindow(gWindow);
        gWindow = nullptr;
    }

    Rel(gRenderTargetHeap);
    Rel(gFence);
    Rel(gCmdAll);
    Rel(gCmdList);
    Rel(gCmdQue);
    Rel(gSwapChain);
    Rel(gDevice);
    Rel(gFactory);
    for (int i = 0; i < FRAME_COUNT; i++)
        Rel(gRenderTarget[i]);
    Rel(gDebugController);
    Rel(gRootSign);
}

void SetupConstantBuffer()
{
    using namespace DirectX::SimpleMath;
    gCamera.pos = Vector4::Zero;
    gCamera.look = Vector4::Zero;

    // desc heap desc
    D3D12_DESCRIPTOR_HEAP_DESC dhDesc;
    ZeroMemory(&dhDesc, sizeof(dhDesc));
    dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    dhDesc.NumDescriptors = 2; // two constant buffer

    gDevice->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(&gDescHeap));

    // memory properties
    D3D12_HEAP_PROPERTIES heapProp;
    ZeroMemory(&heapProp, sizeof(heapProp));
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    // resource desc
    D3D12_RESOURCE_DESC resDesc;
    ZeroMemory(&resDesc, sizeof(resDesc));
    resDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    resDesc.DepthOrArraySize = 1;
    resDesc.Height = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Width = 1024 * 64;

    // creation
    TOI(gDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&gCameraRes)
    ));
    TOI(gDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&gTimeRes)
    ));

    UINT size = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cdh = gDescHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_CONSTANT_BUFFER_VIEW_DESC cDesc;
    ZeroMemory(&cDesc, sizeof(cDesc));
    cDesc.BufferLocation = gCameraRes->GetGPUVirtualAddress();
    cDesc.SizeInBytes = (sizeof(gCamera) + 255) & ~255;	// CB size is required to be 256-byte aligned.
    gDevice->CreateConstantBufferView(&cDesc, cdh);

    cdh.ptr += size;

    cDesc.BufferLocation = gTimeRes->GetGPUVirtualAddress();
    cDesc.SizeInBytes = (sizeof(gTime) + 255) & ~255;	// CB size is required to be 256-byte aligned.
    gDevice->CreateConstantBufferView(&cDesc, cdh);

    UpdateConstantBuffers();
}

void UpdateConstantBuffers()
{
    gTime.time = (SDL_GetTicks() - timeStart) * 0.0004f;
    gCamera.cam = DirectX::SimpleMath::Matrix::CreateLookAt(
        { gEye.x, gEye.y, gEye.z },
        { gDir.x, gDir.y, gDir.z },
        { 0.f, 1.f, 0.f }
    );
    gCamera.view = DirectX::SimpleMath::Matrix::CreatePerspective(WIN_WIDTH, WIN_HEIGHT, 0.1f, 100.f);

    D3D12_RANGE range = { 0.f, 0.f };
    void *data;

    gCameraRes->Map(0, &range, &data);
    memcpy(data, &gCamera, sizeof(gCamera));
    gCameraRes->Unmap(0, nullptr);

    void *data2;
    gTimeRes->Map(0, &range, &data2);
    memcpy(data2, &gTime, sizeof(gTime));
    gTimeRes->Unmap(0, nullptr);
}

void HandleSdlEvent(SDL_Event e)
{
    using namespace DirectX::SimpleMath;
    if (e.type == SDL_KEYDOWN)
    {
        switch (e.key.keysym.sym)
        {
        case SDLK_UP:
            gEye.y += 0.3f;
            break;
        case SDLK_DOWN:
            gEye.y -= 0.3f;
            break;
        case SDLK_d:
        case SDLK_RIGHT:
            gEye.x += 0.3f;
            break;
        case SDLK_a:
        case SDLK_LEFT:
            gEye.x -= 0.3f;
            break;
        case SDLK_w:
            gEye.z += 0.3f;
            break;
        case SDLK_s:
            gEye.z -= 0.3f;
            break;
        case SDLK_h:
            gDir.x += 0.01f;
            break;
        case SDLK_g:
            gDir.x -= 0.01f;
            break;
        case SDLK_t:
            gDir.y += 0.01f;
            break;
        case SDLK_b:
            gDir.y -= 0.01f;
            break;
        case SDLK_o:
            timeStart = SDL_GetTicks();
            break;
        }
    }

    if (e.type == SDL_MOUSEMOTION) {
        if (e.motion.state == SDL_PRESSED) {
            Vector2 lastPos = Vector2(e.motion.xrel, e.motion.yrel);
            gDir = Vector3(gDir.x, lastPos.x * 0.01f + gDir.y, lastPos.y * 0.01f + gDir.z);
        }
    }

    gCamera.pos = Vector4(gEye.x, gEye.y, gEye.z, 0.f);
    gCamera.look = Vector4(gDir.x, gDir.y, gDir.z, 0.f);
}

void WaitForGpu()
{
    const UINT startVal = gFenceValue;
    gCmdQue->Signal(gFence, gFenceValue);
    gFenceValue++;

    // Wait until the previous frame is finished.
    if (gFence->GetCompletedValue() < startVal)
    {
        gFence->SetEventOnCompletion(startVal, gFenceEvent);
        WaitForSingleObject(gFenceEvent, INFINITE);
    }
}

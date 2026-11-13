#if !defined(_WIN32)
#error This validation executable is Windows-only.
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
using Microsoft::WRL::ComPtr;
static void hr(HRESULT h,const char* what){if(FAILED(h)){std::fprintf(stderr,"FAIL: %s (0x%08lx)\n",what,(unsigned long)h);std::exit(2);}}
static void waitForFence(ID3D12Fence* fence, UINT64 value, HANDLE event, const char* what) {
    const UINT64 completed = fence->GetCompletedValue();
    if (completed == UINT64_MAX) {
        std::fprintf(stderr, "FAIL: device removed while waiting for %s\n", what);
        std::exit(2);
    }
    if (completed >= value) return;
    hr(fence->SetEventOnCompletion(value, event), what);
    const DWORD wait = WaitForSingleObject(event, INFINITE);
    if (wait != WAIT_OBJECT_0) {
        std::fprintf(stderr, "FAIL: wait for %s returned %lu (GetLastError=%lu)\n", what,
                     static_cast<unsigned long>(wait), static_cast<unsigned long>(GetLastError()));
        std::exit(2);
    }
    if (fence->GetCompletedValue() == UINT64_MAX) {
        std::fprintf(stderr, "FAIL: device removed after %s\n", what);
        std::exit(2);
    }
}
static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE t){D3D12_HEAP_PROPERTIES p{};p.Type=t;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;}
static D3D12_RESOURCE_DESC buffer(UINT64 n, D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=n;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;return d;}
static ComPtr<ID3D12Resource> makeBuffer(ID3D12Device* d,UINT64 n,D3D12_HEAP_TYPE h,D3D12_RESOURCE_STATES s,D3D12_RESOURCE_FLAGS f=D3D12_RESOURCE_FLAG_NONE){ComPtr<ID3D12Resource> r;auto hp=heap(h);auto desc=buffer(n,f);hr(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,s,nullptr,IID_PPV_ARGS(&r)),"CreateCommittedResource");return r;}
int main(int argc,char**argv){
    if(argc!=2){std::fprintf(stderr,"usage: anvil_d3d12_compute_validation <compute_validation.comp.dxil>\n");return 64;}
    // Debug layer and GPU-based validation must be enabled before device creation.
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))){debug->EnableDebugLayer();ComPtr<ID3D12Debug1> debug1;if(SUCCEEDED(debug.As(&debug1)))debug1->SetEnableGPUBasedValidation(TRUE);}
    ComPtr<IDXGIFactory6> factory;hr(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"CreateDXGIFactory2");
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i=0;;++i){
        ComPtr<IDXGIAdapter1> candidate;
        const HRESULT enumResult=factory->EnumAdapters1(i,&candidate);
        if(enumResult==DXGI_ERROR_NOT_FOUND)break;
        hr(enumResult,"EnumAdapters1");
        DXGI_ADAPTER_DESC1 desc{};candidate->GetDesc1(&desc);
        if(!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){adapter=candidate;break;}
    }
    bool usingWarp=false;
    if(!adapter){
        const HRESULT warpResult=factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        if(FAILED(warpResult)){std::fprintf(stderr,"SKIP: no hardware adapter or WARP D3D12 runtime\n");return 77;}
        usingWarp=true;
    }
    ComPtr<ID3D12Device> device;hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"D3D12CreateDevice");
    DXGI_ADAPTER_DESC1 selectedDesc{};adapter->GetDesc1(&selectedDesc);
    char adapterName[256]{};WideCharToMultiByte(CP_UTF8,0,selectedDesc.Description,-1,adapterName,sizeof(adapterName),nullptr,nullptr);
    std::printf("D3D12 adapter: %s%s\n",adapterName,usingWarp?" (WARP software adapter)":"");
    ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(device.As(&info))){info->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,TRUE);info->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,TRUE);}
    std::ifstream in(argv[1],std::ios::binary);if(!in){std::fprintf(stderr,"FAIL: cannot open DXIL: %s\n",argv[1]);return 2;}std::vector<char> dxil((std::istreambuf_iterator<char>(in)),{});if(dxil.empty())return 2;
    D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;range.NumDescriptors=1;range.BaseShaderRegister=0;range.RegisterSpace=3;range.OffsetInDescriptorsFromTableStart=0;
    D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable.NumDescriptorRanges=1;params[0].DescriptorTable.pDescriptorRanges=&range;params[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants={0,0,2};params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rsd{};rsd.NumParameters=2;rsd.pParameters=params;rsd.Flags=D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ComPtr<ID3DBlob> rsBlob,errors;hr(D3D12SerializeRootSignature(&rsd,D3D_ROOT_SIGNATURE_VERSION_1,&rsBlob,&errors),"D3D12SerializeRootSignature");ComPtr<ID3D12RootSignature> root;hr(device->CreateRootSignature(0,rsBlob->GetBufferPointer(),rsBlob->GetBufferSize(),IID_PPV_ARGS(&root)),"CreateRootSignature(UAV)");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={dxil.data(),dxil.size()};ComPtr<ID3D12PipelineState> pso;hr(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso)),"CreateComputePipelineState");
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;ComPtr<ID3D12DescriptorHeap> descriptors;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&descriptors)),"CreateDescriptorHeap(UAV)");
    constexpr UINT count=128;const UINT bytes=count*sizeof(UINT);
    // Initialize the DEFAULT-heap UAV deterministically through an UPLOAD staging copy.
    auto output=makeBuffer(device.Get(),bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto initial=makeBuffer(device.Get(),bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* initMap=nullptr;hr(initial->Map(0,nullptr,&initMap),"Map(initial data)");auto* initialValues=static_cast<UINT*>(initMap);for(UINT i=0;i<count;++i)initialValues[i]=i;initial->Unmap(0,nullptr);
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"CreateCommandQueue");
    ComPtr<ID3D12CommandAllocator> alloc;hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)),"CreateCommandAllocator");ComPtr<ID3D12GraphicsCommandList> list;hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list)),"CreateCommandList");
    list->CopyBufferRegion(output.Get(),0,initial.Get(),0,bytes);D3D12_RESOURCE_BARRIER initBarrier{};initBarrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;initBarrier.Transition.pResource=output.Get();initBarrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;initBarrier.Transition.StateAfter=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;initBarrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;list->ResourceBarrier(1,&initBarrier);hr(list->Close(),"Close initialization list");ID3D12CommandList* initLists[]={list.Get()};queue->ExecuteCommandLists(1,initLists);
    ComPtr<ID3D12Fence> initFence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&initFence)),"Create initialization fence");HANDLE initEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!initEvent){std::fprintf(stderr,"FAIL: CreateEvent(initialization) (GetLastError=%lu)\n",static_cast<unsigned long>(GetLastError()));return 2;}hr(queue->Signal(initFence.Get(),1),"Signal initialization fence");waitForFence(initFence.Get(),1,initEvent,"initialization fence");CloseHandle(initEvent);
    // Reset allocator/list only after the initialization submission has completed.
    hr(alloc->Reset(),"Reset command allocator");hr(list->Reset(alloc.Get(),nullptr),"Reset command list");
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.Buffer.NumElements=count;uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;device->CreateUnorderedAccessView(output.Get(),nullptr,&uav,descriptors->GetCPUDescriptorHandleForHeapStart());
    // Dispatch arguments are exactly the 12-byte D3D12_DISPATCH_ARGUMENTS structure.
    auto args=makeBuffer(device.Get(),sizeof(D3D12_DISPATCH_ARGUMENTS),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);D3D12_DISPATCH_ARGUMENTS dispatchArgs{2,1,1};void* mapped=nullptr;hr(args->Map(0,nullptr,&mapped),"Map(indirect args)");std::memcpy(mapped,&dispatchArgs,sizeof(dispatchArgs));args->Unmap(0,nullptr);
    auto readback=makeBuffer(device.Get(),bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    ID3D12DescriptorHeap* heaps[]={descriptors.Get()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(pso.Get());list->SetComputeRootDescriptorTable(0,descriptors->GetGPUDescriptorHandleForHeapStart());
    UINT constants[2]={count,7};list->SetComputeRoot32BitConstants(1,2,constants,0);
    // First half: direct Dispatch. Explicit UAV barrier orders the subsequent indirect compute write.
    list->Dispatch(1,1,1);D3D12_RESOURCE_BARRIER uavBarrier{};uavBarrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;uavBarrier.UAV.pResource=output.Get();list->ResourceBarrier(1,&uavBarrier);
    // Second half: ExecuteIndirect dispatches two groups (128 threads); shader bounds-checks elementCount.
    D3D12_INDIRECT_ARGUMENT_DESC iad{};iad.Type=D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;D3D12_COMMAND_SIGNATURE_DESC csd{};csd.ByteStride=sizeof(D3D12_DISPATCH_ARGUMENTS);csd.NumArgumentDescs=1;csd.pArgumentDescs=&iad;ComPtr<ID3D12CommandSignature> signature;hr(device->CreateCommandSignature(&csd,nullptr,IID_PPV_ARGS(&signature)),"CreateCommandSignature(DISPATCH)");
    list->ExecuteIndirect(signature.Get(),1,args.Get(),0,nullptr,0);
    uavBarrier.UAV.pResource=output.Get();list->ResourceBarrier(1,&uavBarrier);
    D3D12_RESOURCE_BARRIER transition{};transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;transition.Transition.pResource=output.Get();transition.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;transition.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;transition.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;list->ResourceBarrier(1,&transition);list->CopyBufferRegion(readback.Get(),0,output.Get(),0,bytes);hr(list->Close(),"Close command list");ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"CreateFence");HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event){std::fprintf(stderr,"FAIL: CreateEvent(readback) (GetLastError=%lu)\n",static_cast<unsigned long>(GetLastError()));return 2;}hr(queue->Signal(fence.Get(),1),"Signal fence");waitForFence(fence.Get(),1,event,"compute/readback fence");CloseHandle(event);
    const UINT* values=nullptr;D3D12_RANGE rr{0,bytes};hr(readback->Map(0,&rr,(void**)&values),"Map(readback)");bool ok=true;for(UINT i=0;i<count;++i){UINT expected=i<64u?((i*3u+7u)*3u+7u):(i*3u+7u); if(values[i]!=expected){std::fprintf(stderr,"FAIL: value[%u]=%u expected=%u\n",i,values[i],expected);ok=false;break;}}D3D12_RANGE wr{0,0};readback->Unmap(0,&wr);
    if(info){const UINT64 n=info->GetNumStoredMessagesAllowedByRetrievalFilter();for(UINT64 i=0;i<n;++i){SIZE_T sz=0;info->GetMessage(i,nullptr,&sz);std::vector<unsigned char> b(sz);auto*m=reinterpret_cast<D3D12_MESSAGE*>(b.data());if(SUCCEEDED(info->GetMessage(i,m,&sz))&&m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::fprintf(stderr,"D3D12 validation: %s\n",m->pDescription);ok=false;}}}
    if(!ok)return 1;
    std::puts("PASS: D3D12 compute direct Dispatch + ExecuteIndirect + UAV barriers + readback validation");
    std::puts("PASS: no D3D12 error/corruption messages");
    return 0;
}

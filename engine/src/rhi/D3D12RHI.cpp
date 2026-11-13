// ANVIL RHI - Direct3D 12 backend.
// Windows-only. Uses upload heaps for buffers and default heaps for textures; command lists
// retain referenced resources until the frame fence has completed.
#if !defined(_WIN32)
#error D3D12RHI.cpp must only be compiled on Windows.
#endif
#include "anvil/rhi/RHI.h"
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <climits>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace anvil::rhi::dx12 {
namespace {
constexpr UINT kFrames = 2, kMaxDescriptors = 8192, kFrameDescriptors = 8192;
inline void check(HRESULT hr, const char* what) {
    if (FAILED(hr)) throw std::runtime_error(std::string("D3D12: ") + what + " failed (HRESULT " + std::to_string(static_cast<unsigned long>(hr)) + ")");
}
inline UINT alignUp(UINT n, UINT a) {
    if (a == 0 || (a & (a - 1)) != 0) throw std::invalid_argument("D3D12: alignment must be a non-zero power of two");
    if (n > UINT_MAX - (a - 1)) throw std::overflow_error("D3D12: aligned size overflows UINT");
    return (n + a - 1) & ~(a - 1);
}
D3D12_RESOURCE_STATES stateFor(TextureUsage u, TextureFormat f) {
    if (hasUsage(u, TextureUsage::DepthStencil)) return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if (hasUsage(u, TextureUsage::RenderTarget)) return D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (hasUsage(u, TextureUsage::TransferDst)) return D3D12_RESOURCE_STATE_COPY_DEST;
    (void)f; return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}
DXGI_FORMAT formatOf(TextureFormat f) {
    switch(f) {
    case TextureFormat::R8: return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::RG8: return DXGI_FORMAT_R8G8_UNORM;
    case TextureFormat::RGB8: case TextureFormat::RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::SRGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::BGRA8: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::SBGRA8: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case TextureFormat::R16F: return DXGI_FORMAT_R16_FLOAT;
    case TextureFormat::RG16F: return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R32F: return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::Depth24: return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::Depth32F: return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::RGB10A2: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case TextureFormat::R11G11B10F: return DXGI_FORMAT_R11G11B10_FLOAT;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}
bool depthFormat(TextureFormat f) { return f==TextureFormat::Depth24 || f==TextureFormat::Depth32F || f==TextureFormat::Depth24Stencil8; }
UINT bytesPerPixel(TextureFormat f) {
    switch(f) {
    case TextureFormat::R8: return 1; case TextureFormat::RG8: case TextureFormat::R16F: return 2;
    case TextureFormat::RGB8: return 3; case TextureFormat::RGBA16F: return 8;
    case TextureFormat::RG16F: return 4; default: return 4;
    }
}
D3D12_COMPARISON_FUNC compareOf(CompareOp c) {
    switch(c) { case CompareOp::Never:return D3D12_COMPARISON_FUNC_NEVER; case CompareOp::Less:return D3D12_COMPARISON_FUNC_LESS;
    case CompareOp::Equal:return D3D12_COMPARISON_FUNC_EQUAL; case CompareOp::LessEqual:return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareOp::Greater:return D3D12_COMPARISON_FUNC_GREATER; case CompareOp::NotEqual:return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CompareOp::GreaterEqual:return D3D12_COMPARISON_FUNC_GREATER_EQUAL; default:return D3D12_COMPARISON_FUNC_ALWAYS; }
}
D3D12_TEXTURE_ADDRESS_MODE addressOf(TextureWrap w) {
    switch(w) { case TextureWrap::ClampToEdge:return D3D12_TEXTURE_ADDRESS_MODE_CLAMP; case TextureWrap::ClampToBorder:return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    case TextureWrap::MirroredRepeat:return D3D12_TEXTURE_ADDRESS_MODE_MIRROR; default:return D3D12_TEXTURE_ADDRESS_MODE_WRAP; }
}
D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType(PrimitiveTopology t) {
    switch(t) { case PrimitiveTopology::LineList:return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case PrimitiveTopology::PointList:return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; default:return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }
}
DXGI_FORMAT vertexFormat(VertexFormat f) {
    switch(f) { case VertexFormat::Float:return DXGI_FORMAT_R32_FLOAT; case VertexFormat::Float2:return DXGI_FORMAT_R32G32_FLOAT;
    case VertexFormat::Float3:return DXGI_FORMAT_R32G32B32_FLOAT; case VertexFormat::Float4:return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case VertexFormat::UByte4Norm:return DXGI_FORMAT_R8G8B8A8_UNORM; case VertexFormat::UInt:return DXGI_FORMAT_R32_UINT; }
    return DXGI_FORMAT_UNKNOWN;
}
D3D12_RESOURCE_DESC bufferDesc(UINT64 size) {
    D3D12_RESOURCE_DESC d{}; d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; d.Width=size; d.Height=1; d.DepthOrArraySize=1;
    d.MipLevels=1; d.SampleDesc.Count=1; d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR; return d;
}
D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE t) {
    D3D12_HEAP_PROPERTIES p{}; p.Type=t; p.CPUPageProperty=D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    p.MemoryPoolPreference=D3D12_MEMORY_POOL_UNKNOWN; p.CreationNodeMask=1; p.VisibleNodeMask=1; return p;
}
struct Core;
class Texture;
class Swapchain;
struct Core : std::enable_shared_from_this<Core> {
    ComPtr<IDXGIFactory6> factory; ComPtr<IDXGIAdapter1> adapter; ComPtr<ID3D12Device> dev;
    ComPtr<ID3D12CommandQueue> queue; ComPtr<ID3D12Fence> fence; HANDLE fenceEvent=nullptr;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12DescriptorHeap> srvCpu, samplerCpu, rtvHeap, dsvHeap;
    UINT srvInc=0, samplerInc=0, rtvInc=0, dsvInc=0, nextSrv=0, nextSampler=0, nextRtv=0, nextDsv=0;
    struct Frame { ComPtr<ID3D12CommandAllocator> alloc; ComPtr<ID3D12DescriptorHeap> srv; ComPtr<ID3D12DescriptorHeap> sampler; UINT srvUsed=0,samplerUsed=0; UINT64 fenceValue=0; };
    Frame frames[kFrames]; UINT frame=0; UINT64 nextFence=1; DeviceCapabilities caps;
    bool tearing=false;
    ~Core(){ if(fenceEvent) CloseHandle(fenceEvent); }
    void init(const DeviceCreateInfo& info) {
        UINT flags=0;
        if(info.validation) { ComPtr<ID3D12Debug> dbg; if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) { dbg->EnableDebugLayer(); flags|=DXGI_CREATE_FACTORY_DEBUG; } }
        check(CreateDXGIFactory2(flags,IID_PPV_ARGS(&factory)),"CreateDXGIFactory2");
        ComPtr<IDXGIAdapter1> chosen; SIZE_T best=0; int index=0, wanted=info.preferredAdapter;
        for(UINT i=0;;++i) { ComPtr<IDXGIAdapter1> a; if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 d{}; a->GetDesc1(&d); if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) { if(!info.allowSoftwareAdapter) continue; }
            if(wanted>=0 && index++!=wanted) continue;
            if(wanted>=0) { chosen=a; break; }
            if(d.DedicatedVideoMemory>=best) { best=d.DedicatedVideoMemory; chosen=a; }
        }
        if(!chosen && info.allowSoftwareAdapter) check(factory->EnumWarpAdapter(IID_PPV_ARGS(&chosen)),"EnumWarpAdapter");
        if(!chosen) throw std::runtime_error("D3D12: no suitable adapter found (enable allowSoftwareAdapter for WARP)");
        adapter=chosen; check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)),"D3D12CreateDevice"); D3D12_FEATURE_DATA_SHADER_MODEL sm{D3D_SHADER_MODEL_6_0}; check(dev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&sm,sizeof(sm)),"CheckFeatureSupport(SM6.0)"); if(sm.HighestShaderModel<D3D_SHADER_MODEL_6_0)throw std::runtime_error("D3D12: Shader Model 6.0 is required for DXIL shaders");
        D3D12_COMMAND_QUEUE_DESC q{}; q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT; check(dev->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"CreateCommandQueue");
        check(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"CreateFence");
        fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!fenceEvent) throw std::runtime_error("D3D12: CreateEvent failed");
        auto makeHeap=[&](D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,D3D12_DESCRIPTOR_HEAP_FLAGS flags,ComPtr<ID3D12DescriptorHeap>& out) {
            D3D12_DESCRIPTOR_HEAP_DESC d{}; d.Type=type; d.NumDescriptors=count; d.Flags=flags; check(dev->CreateDescriptorHeap(&d,IID_PPV_ARGS(&out)),"CreateDescriptorHeap"); };
        makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,kMaxDescriptors,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,srvCpu);
        makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,256,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,samplerCpu);
        makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,2048,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,rtvHeap);
        makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,2048,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,dsvHeap);
        srvInc=dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        samplerInc=dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        rtvInc=dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsvInc=dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        for(auto& f:frames) { check(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.alloc)),"CreateCommandAllocator");
            makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,kFrameDescriptors,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,f.srv);
            makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,2048,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,f.sampler); }
        createRoot();
        DXGI_ADAPTER_DESC1 ad{}; adapter->GetDesc1(&ad); caps.backend=Backend::Direct3D12;
        char name[256]{}; WideCharToMultiByte(CP_UTF8,0,ad.Description,-1,name,sizeof(name),nullptr,nullptr); caps.deviceName=name; caps.apiVersion="Direct3D 12 / Shader Model 6";
        caps.framesInFlight=kFrames; caps.discreteGpu=(ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)==0; caps.dedicatedVideoMemory=ad.DedicatedVideoMemory;
        D3D12_FEATURE_DATA_D3D12_OPTIONS opt{}; if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&opt,sizeof(opt)))) caps.maxTextureSize=D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        ComPtr<IDXGIFactory5> f5; if(SUCCEEDED(factory.As(&f5))) f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&tearing,sizeof(tearing));
    }
    void createRoot() {
        D3D12_DESCRIPTOR_RANGE ranges[3]{};
        ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_CBV,kMaxUniformSlots,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
        ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,kMaxTextureSlots,0,1,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
        ranges[2]={D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,kMaxTextureSlots,0,1,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
        D3D12_ROOT_PARAMETER p[4]{};
        for(int i=0;i<3;++i){p[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;p[i].DescriptorTable.NumDescriptorRanges=1;p[i].DescriptorTable.pDescriptorRanges=&ranges[i];p[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;}
        p[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;p[3].Constants={4,0,kMaxPushConstantBytes/4};p[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC d{};d.NumParameters=4;d.pParameters=p;d.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> blob,err; HRESULT hr=D3D12SerializeRootSignature(&d,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&err);
        if(FAILED(hr)) throw std::runtime_error(err?static_cast<const char*>(err->GetBufferPointer()):"D3D12: root signature serialization failed");
        check(dev->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"CreateRootSignature");
    }
    D3D12_CPU_DESCRIPTOR_HANDLE cpu(ID3D12DescriptorHeap* h,UINT i,UINT inc) const { auto x=h->GetCPUDescriptorHandleForHeapStart();x.ptr+=SIZE_T(i)*inc;return x; }
    D3D12_GPU_DESCRIPTOR_HANDLE gpu(ID3D12DescriptorHeap* h,UINT i,UINT inc) const { auto x=h->GetGPUDescriptorHandleForHeapStart();x.ptr+=UINT64(i)*inc;return x; }
    void wait(UINT64 value){ if(value){const UINT64 completed=fence->GetCompletedValue();if(completed==UINT64_MAX){const HRESULT reason=dev->GetDeviceRemovedReason();throw std::runtime_error("D3D12: device removed (GetDeviceRemovedReason HRESULT "+std::to_string(static_cast<unsigned long>(reason))+")");}if(completed<value){check(fence->SetEventOnCompletion(value,fenceEvent),"SetEventOnCompletion");WaitForSingleObject(fenceEvent,INFINITE);const UINT64 after=fence->GetCompletedValue();if(after==UINT64_MAX){const HRESULT reason=dev->GetDeviceRemovedReason();throw std::runtime_error("D3D12: device removed while waiting (GetDeviceRemovedReason HRESULT "+std::to_string(static_cast<unsigned long>(reason))+")");}}} }
    void beginFrame(){ auto& f=frames[frame];wait(f.fenceValue);f.srvUsed=f.samplerUsed=0; }
    void finishFrame(){ auto& f=frames[frame];f.fenceValue=nextFence++;check(queue->Signal(fence.Get(),f.fenceValue),"Signal");frame=(frame+1)%kFrames; }
    UINT allocSrv(UINT count){auto& f=frames[frame];if(count>kFrameDescriptors-f.srvUsed)throw std::runtime_error("D3D12: per-frame SRV/CBV descriptor heap exhausted");UINT base=f.srvUsed;f.srvUsed+=count;return base;}
    UINT allocSampler(UINT count){auto& f=frames[frame];constexpr UINT capacity=2048;if(count>capacity-f.samplerUsed)throw std::runtime_error("D3D12: per-frame sampler heap exhausted");UINT base=f.samplerUsed;f.samplerUsed+=count;return base;}
};
class Buffer final : public rhi::Buffer {
public:
    Buffer(std::shared_ptr<Core> c,const BufferDesc& d,const void* initial):c_(std::move(c)),size_(d.size),dynamic_(d.dynamic) {
        if(!size_||size_>UINT_MAX-255u)throw std::invalid_argument("D3D12: invalid or unalignable buffer size");
        if(d.usage==BufferUsage::Storage||d.usage==BufferUsage::Readback||d.usage==BufferUsage::Indirect)throw UnsupportedByBackend("D3D12: Storage, Readback and Indirect buffers are not implemented by this backend");
        stride_=alignUp(static_cast<UINT>(size_),256);UINT64 allocation=dynamic_?static_cast<UINT64>(stride_)*kFrames:stride_;
        auto hp=heapProps(D3D12_HEAP_TYPE_UPLOAD);auto rd=bufferDesc(allocation);
        check(c_->dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&res_)),"CreateCommittedResource(buffer)");
        check(res_->Map(0,nullptr,reinterpret_cast<void**>(&mapped_)),"Map(buffer)");
        if(dynamic_){shadow_.assign(size_,0);if(initial)std::memcpy(shadow_.data(),initial,size_);version_=1;}
        else { if(initial)std::memcpy(mapped_,initial,size_); else std::memset(mapped_,0,size_); }
    }
    ~Buffer() override {if(res_)res_->Unmap(0,nullptr);}
    size_t size()const noexcept override{return size_;}
    void update(size_t off,const void* data,size_t bytes)override{
        if(off>size_||bytes>size_-off)throw std::out_of_range("D3D12: buffer update out of range");if(!bytes)return;
        if(!data)throw std::invalid_argument("D3D12: null buffer update source with non-zero size");
        if(dynamic_){std::memcpy(shadow_.data()+off,data,bytes);++version_;sync();}
        else {c_->wait(c_->frames[0].fenceValue);c_->wait(c_->frames[1].fenceValue);std::memcpy(mapped_+off,data,bytes);}
    }
    D3D12_GPU_VIRTUAL_ADDRESS address()const{if(dynamic_)const_cast<Buffer*>(this)->sync();return res_->GetGPUVirtualAddress()+(dynamic_?static_cast<UINT64>(c_->frame)*stride_:0);}
    ID3D12Resource* get()const{return res_.Get();}
private:
    void sync(){if(!dynamic_)return;UINT f=c_->frame;if(slotVersion_[f]==version_)return;std::memcpy(mapped_+static_cast<size_t>(f)*stride_,shadow_.data(),size_);slotVersion_[f]=version_;}
    std::shared_ptr<Core> c_;size_t size_;bool dynamic_=false;UINT stride_=0;ComPtr<ID3D12Resource> res_;uint8_t* mapped_=nullptr;
    std::vector<uint8_t> shadow_;uint64_t version_=0;std::array<uint64_t,kFrames>slotVersion_{};
};
class Texture final : public rhi::Texture {
public:
    Texture(std::shared_ptr<Core> c,const TextureDesc& d,const void* initial):c_(std::move(c)),w_(d.width),h_(d.height),fmt_(d.format),usage_(d.usage){
        if(w_<=0||h_<=0||formatOf(fmt_)==DXGI_FORMAT_UNKNOWN)throw std::invalid_argument("D3D12: invalid texture description");
        if(d.levels<1||d.levels>UINT16_MAX||d.samples<1)throw std::invalid_argument("D3D12: invalid mip/sample count");
        int maxMipLevels=1;for(int extent=std::max(w_,h_);extent>1;extent>>=1)++maxMipLevels;
        if(d.levels>maxMipLevels)throw std::invalid_argument("D3D12: mip count exceeds the complete texture mip chain");
        if(d.samples>1&&(d.levels!=1||initial))throw UnsupportedByBackend("D3D12: multisampled textures require exactly one mip and cannot use initial-data uploads");
        if(hasUsage(usage_,TextureUsage::Storage))throw UnsupportedByBackend("D3D12: storage textures/UAV binding are not implemented by this backend");
        if(initial&&d.levels>1)throw UnsupportedByBackend("D3D12: initial upload of multiple mip levels is not supported; provide mip 0 only with a single-level texture");
        D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w_;rd.Height=h_;rd.DepthOrArraySize=1;rd.MipLevels=static_cast<UINT16>(d.levels);
        rd.Format=depthFormat(fmt_)?(fmt_==TextureFormat::Depth32F?DXGI_FORMAT_R32_TYPELESS:DXGI_FORMAT_R24G8_TYPELESS):formatOf(fmt_);rd.SampleDesc.Count=static_cast<UINT>(std::max(1,d.samples));rd.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
        rd.Flags=depthFormat(fmt_)?D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL:D3D12_RESOURCE_FLAG_NONE;
        if(hasUsage(usage_,TextureUsage::RenderTarget)&&!depthFormat(fmt_))rd.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto hp=heapProps(D3D12_HEAP_TYPE_DEFAULT);auto st=stateFor(usage_,fmt_);
        D3D12_CLEAR_VALUE cv{};cv.Format=formatOf(fmt_);cv.DepthStencil.Depth=1.0f;
        check(c_->dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,initial?D3D12_RESOURCE_STATE_COPY_DEST:st,depthFormat(fmt_)?&cv:nullptr,IID_PPV_ARGS(&res_)),"CreateCommittedResource(texture)");
        if(hasUsage(usage_,TextureUsage::Sampled)){if(c_->nextSrv>=kMaxDescriptors)throw std::runtime_error("D3D12: SRV heap exhausted");srvIndex_=c_->nextSrv++;
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Format=depthFormat(fmt_)?(fmt_==TextureFormat::Depth32F?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R24_UNORM_X8_TYPELESS):rd.Format;sd.ViewDimension=rd.SampleDesc.Count>1?D3D12_SRV_DIMENSION_TEXTURE2DMS:D3D12_SRV_DIMENSION_TEXTURE2D;if(rd.SampleDesc.Count>1)sd.Texture2DMS.UnusedField_NothingToDefine=0;else sd.Texture2D.MipLevels=rd.MipLevels;
            c_->dev->CreateShaderResourceView(res_.Get(),&sd,c_->cpu(c_->srvCpu.Get(),srvIndex_,c_->srvInc));}
        if(hasUsage(usage_,TextureUsage::RenderTarget)&&!depthFormat(fmt_)){if(c_->nextRtv>=2048)throw std::runtime_error("D3D12: RTV descriptor heap exhausted");rtvIndex_=c_->nextRtv++;c_->dev->CreateRenderTargetView(res_.Get(),nullptr,c_->cpu(c_->rtvHeap.Get(),rtvIndex_,c_->rtvInc));}
        if(depthFormat(fmt_)){if(c_->nextDsv>=2048)throw std::runtime_error("D3D12: DSV descriptor heap exhausted");dsvIndex_=c_->nextDsv++;D3D12_DEPTH_STENCIL_VIEW_DESC dd{};dd.Format=formatOf(fmt_);dd.ViewDimension=rd.SampleDesc.Count>1?D3D12_DSV_DIMENSION_TEXTURE2DMS:D3D12_DSV_DIMENSION_TEXTURE2D;
            c_->dev->CreateDepthStencilView(res_.Get(),&dd,c_->cpu(c_->dsvHeap.Get(),dsvIndex_,c_->dsvInc));}
        state_=initial?D3D12_RESOURCE_STATE_COPY_DEST:st;
        if(initial){upload(initial,bytesPerPixel(fmt_)*static_cast<size_t>(w_)*h_);}
    }
    Texture(std::shared_ptr<Core> c,ComPtr<ID3D12Resource> external,int w,int h,TextureFormat f):c_(std::move(c)),res_(std::move(external)),w_(w),h_(h),fmt_(f),usage_(TextureUsage::RenderTarget),state_(D3D12_RESOURCE_STATE_PRESENT),isSwapchain_(true) {
        if(c_->nextRtv>=2048)throw std::runtime_error("D3D12: RTV descriptor heap exhausted");rtvIndex_=c_->nextRtv++;c_->dev->CreateRenderTargetView(res_.Get(),nullptr,c_->cpu(c_->rtvHeap.Get(),rtvIndex_,c_->rtvInc));
    }
    int width()const noexcept override{return w_;}int height()const noexcept override{return h_;}TextureFormat format()const noexcept override{return fmt_;}
    uintptr_t nativeHandle()const noexcept override{return 0;}
    void transition(ID3D12GraphicsCommandList* cmd,D3D12_RESOURCE_STATES to){if(state_==to)return;D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=res_.Get();b.Transition.StateBefore=state_;b.Transition.StateAfter=to;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;cmd->ResourceBarrier(1,&b);state_=to;}
    void transition(D3D12_RESOURCE_STATES to);
    void upload(const void* src,size_t bytes){
        if(!src)throw std::invalid_argument("D3D12: null texture upload source");
        const size_t bpp=bytesPerPixel(fmt_);if(static_cast<size_t>(w_)>SIZE_MAX/static_cast<size_t>(h_)/bpp)throw std::overflow_error("D3D12: texture upload size overflow");
        const size_t required=static_cast<size_t>(w_)*static_cast<size_t>(h_)*bpp;
        if(bytes<required)throw std::invalid_argument("D3D12: texture upload source is smaller than mip 0");
        auto rd=res_->GetDesc(); UINT64 total=0; D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
        c_->dev->GetCopyableFootprints(&rd,0,1,0,&fp,nullptr,nullptr,&total);
        auto hp=heapProps(D3D12_HEAP_TYPE_UPLOAD); auto bd=bufferDesc(total); ComPtr<ID3D12Resource> up;
        check(c_->dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&up)),"texture upload buffer");
        std::vector<uint8_t> expanded; const uint8_t* pixels=static_cast<const uint8_t*>(src);
        UINT bpp=bytesPerPixel(fmt_);
        if(fmt_==TextureFormat::RGB8){expanded.resize(static_cast<size_t>(w_)*h_*4);for(size_t i=0,n=static_cast<size_t>(w_)*h_;i<n;++i){expanded[i*4]=pixels[i*3];expanded[i*4+1]=pixels[i*3+1];expanded[i*4+2]=pixels[i*3+2];expanded[i*4+3]=255;}pixels=expanded.data();bpp=4;}
        void* mapped=nullptr;check(up->Map(0,nullptr,&mapped),"Map(texture staging)");
        for(int y=0;y<h_;++y)std::memcpy(static_cast<uint8_t*>(mapped)+fp.Offset+static_cast<size_t>(y)*fp.Footprint.RowPitch,pixels+static_cast<size_t>(y)*w_*bpp,static_cast<size_t>(w_)*bpp);
        up->Unmap(0,nullptr);
        ComPtr<ID3D12CommandAllocator> a;ComPtr<ID3D12GraphicsCommandList> l;
        check(c_->dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a)),"upload allocator");
        check(c_->dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a.Get(),nullptr,IID_PPV_ARGS(&l)),"upload list");
        D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=res_.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION sl{};sl.pResource=up.Get();sl.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;sl.PlacedFootprint=fp;
        l->CopyTextureRegion(&dst,0,0,0,&sl,nullptr);
        D3D12_RESOURCE_BARRIER bar{};bar.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;bar.Transition.pResource=res_.Get();
        bar.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;bar.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        bar.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;l->ResourceBarrier(1,&bar);
        check(l->Close(),"close upload list");ID3D12CommandList* lists[]={l.Get()};c_->queue->ExecuteCommandLists(1,lists);
        UINT64 fv=c_->nextFence++;check(c_->queue->Signal(c_->fence.Get(),fv),"signal upload");c_->wait(fv);
        state_=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv()const{return c_->cpu(c_->rtvHeap.Get(),rtvIndex_,c_->rtvInc);}
    D3D12_CPU_DESCRIPTOR_HANDLE dsv()const{return c_->cpu(c_->dsvHeap.Get(),dsvIndex_,c_->dsvInc);}
    UINT srv()const{return srvIndex_;}
    ID3D12Resource* get()const{return res_.Get();}
    std::shared_ptr<Core> c_;ComPtr<ID3D12Resource> res_;int w_,h_;TextureFormat fmt_;TextureUsage usage_;D3D12_RESOURCE_STATES state_=D3D12_RESOURCE_STATE_COMMON;
    UINT srvIndex_=UINT_MAX,rtvIndex_=UINT_MAX,dsvIndex_=UINT_MAX;bool isSwapchain_=false;
};
void Texture::transition(D3D12_RESOURCE_STATES to){ // upload-time transition with an isolated command list
    if(state_==to)return;ComPtr<ID3D12CommandAllocator>a;ComPtr<ID3D12GraphicsCommandList>l;
    check(c_->dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a)),"transition allocator");
    check(c_->dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a.Get(),nullptr,IID_PPV_ARGS(&l)),"transition list");
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=res_.Get();b.Transition.StateBefore=state_;b.Transition.StateAfter=to;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;l->ResourceBarrier(1,&b);
    check(l->Close(),"close transition");ID3D12CommandList* ls[]={l.Get()};c_->queue->ExecuteCommandLists(1,ls);UINT64 fv=c_->nextFence++;check(c_->queue->Signal(c_->fence.Get(),fv),"signal transition");c_->wait(fv);state_=to;
}
class Sampler final:public rhi::Sampler{
public:Sampler(std::shared_ptr<Core>c,const SamplerDesc&d):c_(std::move(c)){if(c_->nextSampler>=256)throw std::runtime_error("D3D12: sampler heap exhausted");idx_=c_->nextSampler++;
 D3D12_SAMPLER_DESC s{};const bool anisotropic=d.maxAnisotropy>1.0f;const bool minLinear=d.minFilter!=TextureFilter::Nearest;const bool magLinear=d.magFilter!=TextureFilter::Nearest;const bool mipLinear=d.minFilter==TextureFilter::LinearMipmapLinear;
 const unsigned filterIndex=(minLinear?4u:0u)|(magLinear?2u:0u)|(mipLinear?1u:0u);
 static constexpr D3D12_FILTER filters[8]={D3D12_FILTER_MIN_MAG_MIP_POINT,D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR,D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,D3D12_FILTER_MIN_MAG_MIP_LINEAR};
 static constexpr D3D12_FILTER comparisonFilters[8]={D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR,D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR,D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR};
 if(anisotropic)s.Filter=d.compare?D3D12_FILTER_COMPARISON_ANISOTROPIC:D3D12_FILTER_ANISOTROPIC;else s.Filter=d.compare?comparisonFilters[filterIndex]:filters[filterIndex];
 s.AddressU=addressOf(d.wrapU);s.AddressV=addressOf(d.wrapV);s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.MipLODBias=0;s.MaxAnisotropy=static_cast<UINT>(std::clamp(d.maxAnisotropy,1.0f,16.0f));s.ComparisonFunc=compareOf(d.compareOp);
 s.BorderColor[0]=s.BorderColor[1]=s.BorderColor[2]=d.whiteBorder?1.0f:0.0f;s.BorderColor[3]=d.whiteBorder?1.0f:0.0f;s.MinLOD=0;s.MaxLOD=D3D12_FLOAT32_MAX;c_->dev->CreateSampler(&s,c_->cpu(c_->samplerCpu.Get(),idx_,c_->samplerInc));}
 UINT index()const{return idx_;}private:std::shared_ptr<Core>c_;UINT idx_=0;
};
class Shader final:public rhi::Shader{public:explicit Shader(const ShaderDesc&d){if(d.vertexDxil.empty()||d.fragmentDxil.empty())throw std::runtime_error("D3D12: ShaderDesc needs vertexDxil + fragmentDxil (use loadShaderDesc)");vs=d.vertexDxil;ps=d.fragmentDxil;}std::vector<uint8_t>vs,ps;};
class Pipeline final:public rhi::Pipeline{
public:Pipeline(std::shared_ptr<Core>c,const std::shared_ptr<rhi::Shader>&s,const PipelineDesc&d):c_(std::move(c)),desc_(d){
 auto sh=std::dynamic_pointer_cast<Shader>(s);if(!sh)throw std::invalid_argument("D3D12: pipeline shader backend mismatch");
 std::vector<D3D12_INPUT_ELEMENT_DESC> input;for(auto&a:d.vertexLayout.attributes){D3D12_INPUT_ELEMENT_DESC x{};x.SemanticName="TEXCOORD";x.SemanticIndex=a.location;x.Format=vertexFormat(a.format);x.InputSlot=a.binding;x.AlignedByteOffset=a.offset;x.InputSlotClass=D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;x.InstanceDataStepRate=0;for(const auto& b:d.vertexLayout.bindings)if(b.binding==a.binding&&b.perInstance){x.InputSlotClass=D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;x.InstanceDataStepRate=1;}input.push_back(x);}
 D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=c_->root.Get();p.VS={sh->vs.data(),sh->vs.size()};p.PS={sh->ps.data(),sh->ps.size()};p.InputLayout={input.data(),static_cast<UINT>(input.size())};
 p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=!d.cull?D3D12_CULL_MODE_NONE:(d.cullMode==CullMode::Front?D3D12_CULL_MODE_FRONT:D3D12_CULL_MODE_BACK);
 p.RasterizerState.FrontCounterClockwise=d.frontFaceCCW;p.RasterizerState.DepthBias=static_cast<INT>(d.depthBiasConstant);p.RasterizerState.SlopeScaledDepthBias=d.depthBiasSlope;p.RasterizerState.DepthClipEnable=TRUE;
 p.BlendState.AlphaToCoverageEnable=FALSE;p.BlendState.IndependentBlendEnable=FALSE;
 auto&b=p.BlendState.RenderTarget[0];b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
 if(d.blend){b.BlendEnable=TRUE;b.BlendOp=D3D12_BLEND_OP_ADD;b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.SrcBlend=D3D12_BLEND_ONE;b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;
 if(d.blendMode==BlendMode::Alpha){b.SrcBlend=D3D12_BLEND_SRC_ALPHA;b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;}else if(d.blendMode==BlendMode::Additive){b.SrcBlend=D3D12_BLEND_SRC_ALPHA;b.DestBlend=D3D12_BLEND_ONE;}}
 p.DepthStencilState.DepthEnable=d.depthTest&&d.depthFormat!=TextureFormat::Undefined;p.DepthStencilState.DepthWriteMask=d.depthWrite?D3D12_DEPTH_WRITE_MASK_ALL:D3D12_DEPTH_WRITE_MASK_ZERO;
 p.DepthStencilState.DepthFunc=compareOf(d.depthCompare);p.DepthStencilState.StencilEnable=FALSE;p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=topologyType(d.topology);
 p.NumRenderTargets=static_cast<UINT>(d.colorFormats.size());if(p.NumRenderTargets>D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)throw std::invalid_argument("D3D12: too many color targets");
 for(UINT i=0;i<p.NumRenderTargets;++i)p.RTVFormats[i]=formatOf(d.colorFormats[i]);p.DSVFormat=d.depthFormat==TextureFormat::Undefined?DXGI_FORMAT_UNKNOWN:formatOf(d.depthFormat);
 p.SampleDesc.Count=static_cast<UINT>(std::max(1,d.samples));check(c_->dev->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pso_)),"CreateGraphicsPipelineState");
 }ID3D12PipelineState*get()const{return pso_.Get();}PipelineDesc desc_;
private:std::shared_ptr<Core>c_;ComPtr<ID3D12PipelineState>pso_;
};
class CommandList final:public rhi::CommandList{
public:explicit CommandList(std::shared_ptr<Core>c):c_(std::move(c)){for(auto&f:frames_)check(c_->dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f)),"command allocator");
 check(c_->dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames_[0].Get(),nullptr,IID_PPV_ARGS(&list_)),"command list");list_->Close();}
 void begin()override{keep_[c_->frame].clear();auto&f=frames_[c_->frame];check(f->Reset(),"allocator reset");check(list_->Reset(f.Get(),nullptr),"command list reset");recording_=true;inPass_=false;pipeline_.reset();swapTargets_.clear();vb_.reset();ib_.reset();for(auto&x:tex_)x.reset();for(auto&x:smp_)x.reset();for(auto&x:ubo_)x.reset();pc_.fill(0);}
 void end()override{require();if(inPass_)endRenderPass();for(auto& t:swapTargets_)t->transition(list_.Get(),D3D12_RESOURCE_STATE_PRESENT);check(list_->Close(),"command list close");recording_=false;}
 void setPipeline(const std::shared_ptr<rhi::Pipeline>&p)override{require();pipeline_=std::dynamic_pointer_cast<Pipeline>(p);if(!pipeline_)throw std::invalid_argument("D3D12: pipeline backend mismatch");list_->SetPipelineState(pipeline_->get());list_->SetGraphicsRootSignature(c_->root.Get());keep_[c_->frame].push_back(pipeline_);}
 void bindVertexBuffer(const std::shared_ptr<rhi::Buffer>&b,unsigned binding)override{require();auto x=std::dynamic_pointer_cast<Buffer>(b);if(!x)throw std::invalid_argument("D3D12: vertex buffer backend mismatch");UINT stride=0;if(pipeline_)for(const auto& b:pipeline_->desc_.vertexLayout.bindings)if(b.binding==binding){stride=b.stride;break;}D3D12_VERTEX_BUFFER_VIEW v{x->address(),static_cast<UINT>(x->size()),stride};if(!v.StrideInBytes)throw std::runtime_error("D3D12: vertex stride missing from pipeline layout");list_->IASetVertexBuffers(binding,1,&v);keep_[c_->frame].push_back(x);}
 void bindIndexBuffer(const std::shared_ptr<rhi::Buffer>&b)override{require();auto x=std::dynamic_pointer_cast<Buffer>(b);if(!x)throw std::invalid_argument("D3D12: index buffer backend mismatch");D3D12_INDEX_BUFFER_VIEW v{x->address(),static_cast<UINT>(x->size()),DXGI_FORMAT_R32_UINT};list_->IASetIndexBuffer(&v);keep_[c_->frame].push_back(x);}
 void bindTexture(unsigned slot,const std::shared_ptr<rhi::Texture>&t,const std::shared_ptr<rhi::Sampler>&s)override{require();if(slot>=kMaxTextureSlots)throw std::out_of_range("D3D12: texture slot out of range");tex_[slot]=std::dynamic_pointer_cast<Texture>(t);if(tex_[slot])tex_[slot]->transition(list_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);smp_[slot]=std::dynamic_pointer_cast<Sampler>(s);if(t&&!tex_[slot])throw std::invalid_argument("D3D12: texture backend mismatch");if(s&&!smp_[slot])throw std::invalid_argument("D3D12: sampler backend mismatch");if(tex_[slot])keep_[c_->frame].push_back(tex_[slot]);if(smp_[slot])keep_[c_->frame].push_back(smp_[slot]);}
 void bindUniformBuffer(unsigned slot,const std::shared_ptr<rhi::Buffer>&b,size_t offset,size_t range)override{require();if(slot>=kMaxUniformSlots)throw std::out_of_range("D3D12: uniform slot out of range");auto x=std::dynamic_pointer_cast<Buffer>(b);if(!x)throw std::invalid_argument("D3D12: uniform buffer backend mismatch");if(offset>x->size()||(range&&range>x->size()-offset)||(offset%256)!=0)throw std::out_of_range("D3D12: invalid CBV range/alignment");ubo_[slot]=x;uboOffset_[slot]=offset;uboRange_[slot]=range?range:x->size()-offset;keep_[c_->frame].push_back(x);}
 void pushConstants(const void*p,uint32_t bytes,uint32_t offset)override{require();if(bytes>kMaxPushConstantBytes||offset>kMaxPushConstantBytes||bytes>kMaxPushConstantBytes-offset)throw std::out_of_range("D3D12: push constants exceed 128 bytes");std::memcpy(pc_.data()+offset,p,bytes);}
 void setViewport(float x,float y,float w,float h,float mn,float mx)override{require();D3D12_VIEWPORT v{x,y,w,h,mn,mx};list_->RSSetViewports(1,&v);}
 void setScissor(int x,int y,uint32_t w,uint32_t h)override{require();D3D12_RECT r{x,y,x+static_cast<LONG>(w),y+static_cast<LONG>(h)};list_->RSSetScissorRects(1,&r);}
 void beginRenderPass(const RenderPassDesc&d)override{require();if(inPass_)throw std::logic_error("D3D12: nested render pass");if(d.colors.size()>8)throw std::invalid_argument("D3D12: too many color attachments");pass_=d;std::array<D3D12_CPU_DESCRIPTOR_HANDLE,8>rt{};UINT n=0;
 for(auto&a:d.colors){auto t=std::dynamic_pointer_cast<Texture>(a.texture);if(!t||t->rtvIndex_==UINT_MAX)throw std::invalid_argument("D3D12: invalid color attachment");t->transition(list_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET);rt[n++]=t->rtv();if(a.load==LoadOp::Clear)list_->ClearRenderTargetView(rt[n-1],a.clear,0,nullptr);else if(a.load==LoadOp::DontCare){}keep_[c_->frame].push_back(t);if(t->isSwapchain_)swapTargets_.push_back(t);}
 D3D12_CPU_DESCRIPTOR_HANDLE*dp=nullptr;D3D12_CPU_DESCRIPTOR_HANDLE dh{};if(d.depth.texture){auto t=std::dynamic_pointer_cast<Texture>(d.depth.texture);if(!t||t->dsvIndex_==UINT_MAX)throw std::invalid_argument("D3D12: invalid depth attachment");t->transition(list_.Get(),D3D12_RESOURCE_STATE_DEPTH_WRITE);dh=t->dsv();dp=&dh;if(d.depth.load==LoadOp::Clear)list_->ClearDepthStencilView(dh,D3D12_CLEAR_FLAG_DEPTH,d.depth.clearDepth,0,0,nullptr);keep_[c_->frame].push_back(t);}
 list_->OMSetRenderTargets(n,rt.data(),FALSE,dp);
 UINT vw=0,vh=0;if(!d.colors.empty()){auto t=std::dynamic_pointer_cast<Texture>(d.colors[0].texture);vw=static_cast<UINT>(t->width());vh=static_cast<UINT>(t->height());}
 else if(d.depth.texture){auto t=std::dynamic_pointer_cast<Texture>(d.depth.texture);vw=static_cast<UINT>(t->width());vh=static_cast<UINT>(t->height());}
 if(vw&&vh){D3D12_VIEWPORT vp{0.0f,0.0f,static_cast<float>(vw),static_cast<float>(vh),0.0f,1.0f};D3D12_RECT sc{0,0,static_cast<LONG>(vw),static_cast<LONG>(vh)};list_->RSSetViewports(1,&vp);list_->RSSetScissorRects(1,&sc);}
 inPass_=true;}
 void endRenderPass()override{require();if(!inPass_)throw std::logic_error("D3D12: endRenderPass without begin");for(auto&a:pass_.colors)if(a.store==StoreOp::DontCare){}if(pass_.depth.texture&&pass_.depth.store==StoreOp::DontCare){}inPass_=false;}
 void draw(uint32_t n,uint32_t first)override{flushBindings();list_->IASetPrimitiveTopology(topology());list_->DrawInstanced(n,1,first,0);}
 void drawIndexed(uint32_t n,uint32_t first)override{flushBindings();list_->IASetPrimitiveTopology(topology());list_->DrawIndexedInstanced(n,1,first,0,0);}
 void drawInstanced(uint32_t n,uint32_t instances,uint32_t first,uint32_t firstInstance)override{flushBindings();list_->IASetPrimitiveTopology(topology());list_->DrawInstanced(n,instances,first,firstInstance);}
 void drawIndexedInstanced(uint32_t n,uint32_t instances,uint32_t first,int32_t base,uint32_t firstInstance)override{flushBindings();list_->IASetPrimitiveTopology(topology());list_->DrawIndexedInstanced(n,instances,first,base,firstInstance);}
 void generateMipmaps(const std::shared_ptr<rhi::Texture>&)override{throw UnsupportedByBackend("D3D12: runtime mip generation is not implemented; supply prefiltered mip levels");}
 ID3D12GraphicsCommandList*get()const{return list_.Get();}
private:
 void require(){if(!recording_)throw std::logic_error("D3D12: command list is not recording");}
 D3D12_PRIMITIVE_TOPOLOGY topology()const{if(!pipeline_)return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;switch(pipeline_->desc_.topology){case PrimitiveTopology::TriangleStrip:return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;case PrimitiveTopology::LineList:return D3D_PRIMITIVE_TOPOLOGY_LINELIST;case PrimitiveTopology::PointList:return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;default:return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;}}
 void flushBindings(){require();auto&f=c_->frames[c_->frame];ID3D12DescriptorHeap* heaps[]={f.srv.Get(),f.sampler.Get()};list_->SetDescriptorHeaps(2,heaps);
 UINT cb=c_->allocSrv(kMaxUniformSlots);for(UINT i=0;i<kMaxUniformSlots;++i){if(ubo_[i]){D3D12_CONSTANT_BUFFER_VIEW_DESC d{};d.BufferLocation=ubo_[i]->address()+uboOffset_[i];d.SizeInBytes=alignUp(static_cast<UINT>(uboRange_[i]),256);c_->dev->CreateConstantBufferView(&d,c_->cpu(f.srv.Get(),cb+i,c_->srvInc));}else{c_->dev->CreateConstantBufferView(nullptr,c_->cpu(f.srv.Get(),cb+i,c_->srvInc));}}
 list_->SetGraphicsRootDescriptorTable(0,c_->gpu(f.srv.Get(),cb,c_->srvInc));
 UINT srv=c_->allocSrv(kMaxTextureSlots);UINT sm=c_->allocSampler(kMaxTextureSlots);for(UINT i=0;i<kMaxTextureSlots;++i){if(tex_[i]&&tex_[i]->srvIndex_!=UINT_MAX)c_->dev->CopyDescriptorsSimple(1,c_->cpu(f.srv.Get(),srv+i,c_->srvInc),c_->cpu(c_->srvCpu.Get(),tex_[i]->srvIndex_,c_->srvInc),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
 else{D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;c_->dev->CreateShaderResourceView(nullptr,&d,c_->cpu(f.srv.Get(),srv+i,c_->srvInc));}
 if(smp_[i])c_->dev->CopyDescriptorsSimple(1,c_->cpu(f.sampler.Get(),sm+i,c_->samplerInc),c_->cpu(c_->samplerCpu.Get(),smp_[i]->index(),c_->samplerInc),D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
 else {D3D12_SAMPLER_DESC sd{};sd.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sd.MaxAnisotropy=1;sd.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sd.MinLOD=0;sd.MaxLOD=D3D12_FLOAT32_MAX;c_->dev->CreateSampler(&sd,c_->cpu(f.sampler.Get(),sm+i,c_->samplerInc));}}
 list_->SetGraphicsRootDescriptorTable(1,c_->gpu(f.srv.Get(),srv,c_->srvInc));list_->SetGraphicsRootDescriptorTable(2,c_->gpu(f.sampler.Get(),sm,c_->samplerInc));list_->SetGraphicsRoot32BitConstants(3,kMaxPushConstantBytes/4,pc_.data(),0);}
 std::shared_ptr<Core>c_;ComPtr<ID3D12CommandAllocator>frames_[kFrames];ComPtr<ID3D12GraphicsCommandList>list_;bool recording_=false,inPass_=false;
 std::shared_ptr<Pipeline>pipeline_;std::shared_ptr<Buffer>vb_,ib_;std::array<std::shared_ptr<Texture>,kMaxTextureSlots>tex_{};std::array<std::shared_ptr<Sampler>,kMaxTextureSlots>smp_{};
 std::array<std::shared_ptr<Buffer>,kMaxUniformSlots>ubo_{};std::array<size_t,kMaxUniformSlots>uboOffset_{},uboRange_{};std::array<uint32_t,kMaxPushConstantBytes/4>pc_{};RenderPassDesc pass_;std::array<std::vector<std::shared_ptr<void>>,kFrames>keep_;std::vector<std::shared_ptr<Texture>>swapTargets_;
};
class Swapchain final:public rhi::Swapchain{
public:Swapchain(std::shared_ptr<Core>c,const SwapchainDesc&d):c_(std::move(c)),desc_(d){desc_.imageCount=std::clamp(d.imageCount,2u,3u);auto*w=static_cast<GLFWwindow*>(d.nativeWindow);if(!w)throw std::invalid_argument("D3D12: SwapchainDesc.nativeWindow must be a GLFWwindow*");HWND hwnd=glfwGetWin32Window(w);if(!hwnd)throw std::runtime_error("D3D12: GLFW window has no HWND");
 DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=std::max(1,d.width);sd.Height=std::max(1,d.height);sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=desc_.imageCount;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.Flags=c_->tearing?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
 ComPtr<IDXGISwapChain1>s;check(c_->factory->CreateSwapChainForHwnd(c_->queue.Get(),hwnd,&sd,nullptr,nullptr,&s),"CreateSwapChainForHwnd");check(s.As(&sc_),"Query IDXGISwapChain3");c_->factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);fmt_=TextureFormat::RGBA8;createBuffers();}
 ~Swapchain()override{try{c_->wait(c_->frames[0].fenceValue);c_->wait(c_->frames[1].fenceValue);}catch(...){}}
 void resize(int w,int h)override{if(w<=0||h<=0)return;c_->wait(c_->frames[0].fenceValue);c_->wait(c_->frames[1].fenceValue);buffers_.clear();check(sc_->ResizeBuffers(desc_.imageCount,w,h,DXGI_FORMAT_R8G8B8A8_UNORM,c_->tearing?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0),"ResizeBuffers");createBuffers();}
 TextureFormat format()const noexcept override{return fmt_;}int width()const noexcept override{return w_;}int height()const noexcept override{return h_;}
 std::shared_ptr<rhi::Texture> acquire()override{if(w_<=0||h_<=0||buffers_.empty())return {};index_=sc_->GetCurrentBackBufferIndex();if(index_>=buffers_.size())throw std::runtime_error("D3D12: swapchain returned an invalid back-buffer index");return buffers_[index_];}
 void present()override{UINT flags=(!desc_.vsync&&c_->tearing)?DXGI_PRESENT_ALLOW_TEARING:0;HRESULT hr=sc_->Present(desc_.vsync?1:0,flags);if(FAILED(hr)){if(hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET||hr==DXGI_ERROR_DEVICE_HUNG){HRESULT reason=c_->dev->GetDeviceRemovedReason();throw std::runtime_error("D3D12: Present failed; device removal reason HRESULT "+std::to_string(static_cast<unsigned long>(reason)));}check(hr,"Present");}}
private:void createBuffers(){DXGI_SWAP_CHAIN_DESC1 d{};sc_->GetDesc1(&d);w_=d.Width;h_=d.Height;buffers_.clear();for(UINT i=0;i<d.BufferCount;++i){ComPtr<ID3D12Resource>r;check(sc_->GetBuffer(i,IID_PPV_ARGS(&r)),"GetBuffer");buffers_.push_back(std::make_shared<Texture>(c_,r,w_,h_,fmt_));}}
 std::shared_ptr<Core>c_;SwapchainDesc desc_;ComPtr<IDXGISwapChain3>sc_;std::vector<std::shared_ptr<Texture>>buffers_;TextureFormat fmt_=TextureFormat::RGBA8;int w_=0,h_=0;UINT index_=0;
};
class Device final:public rhi::Device{
public:explicit Device(std::shared_ptr<Core>c):c_(std::move(c)){}~Device()override{try{waitIdle();}catch(...){}}const DeviceCapabilities&capabilities()const noexcept override{return c_->caps;}
 std::shared_ptr<rhi::Buffer>createBuffer(const BufferDesc&d,const void*p)override{return std::make_shared<Buffer>(c_,d,p);}
 std::shared_ptr<rhi::Texture>createTexture2D(const TextureDesc&d,const void*p)override{return std::make_shared<Texture>(c_,d,p);}
 std::shared_ptr<rhi::Sampler>createSampler(const SamplerDesc&d)override{return std::make_shared<Sampler>(c_,d);}
 std::shared_ptr<rhi::Shader>createShader(const ShaderDesc&d)override{return std::make_shared<Shader>(d);}
 std::shared_ptr<rhi::Pipeline>createPipeline(const std::shared_ptr<rhi::Shader>&s,const PipelineDesc&d)override{return std::make_shared<Pipeline>(c_,s,d);}
 std::unique_ptr<rhi::CommandList>createCommandList()override{return std::make_unique<CommandList>(c_);}
 std::unique_ptr<rhi::Swapchain>createSwapchain(const SwapchainDesc&d)override{return std::make_unique<Swapchain>(c_,d);}
 void beginFrame()override{c_->beginFrame();}
 void submit(rhi::CommandList&l)override{auto*x=dynamic_cast<CommandList*>(&l);if(!x)throw std::invalid_argument("D3D12: command list backend mismatch");ID3D12CommandList*ls[]={x->get()};c_->queue->ExecuteCommandLists(1,ls);c_->finishFrame();}
 void endFrame()override{}
 void waitIdle()override{check(c_->queue->Signal(c_->fence.Get(),c_->nextFence),"Signal idle");c_->wait(c_->nextFence++);}
private:std::shared_ptr<Core>c_;
};
} // namespace
std::unique_ptr<Device> createD3D12Device(const DeviceCreateInfo&info){auto c=std::make_shared<Core>();c->init(info);return std::make_unique<Device>(std::move(c));}
} // namespace anvil::rhi::dx12
namespace anvil::rhi { std::unique_ptr<Device> createD3D12Device(const DeviceCreateInfo&info){return dx12::createD3D12Device(info);} }

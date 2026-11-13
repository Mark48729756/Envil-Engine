#include "anvil/rhi/OpenGLRHI.h"
#include "anvil/Core.h"
#include <algorithm>
#include <limits>
#include <vector>
#include <string>
#include <stdexcept>

namespace anvil::rhi::gl {
namespace {
GLenum bufferTarget(BufferUsage u) { switch (u) { case BufferUsage::Index: return GL_ELEMENT_ARRAY_BUFFER; case BufferUsage::Uniform: return GL_UNIFORM_BUFFER; case BufferUsage::Storage: return GL_SHADER_STORAGE_BUFFER; case BufferUsage::Indirect: return GL_DISPATCH_INDIRECT_BUFFER; default: return GL_ARRAY_BUFFER; } }
GLenum textureFormat(TextureFormat f) { switch (f) { case TextureFormat::R8:return GL_R8; case TextureFormat::RG8:return GL_RG8; case TextureFormat::RGB8:return GL_RGB8; case TextureFormat::SRGBA8:return GL_SRGB8_ALPHA8; case TextureFormat::R16F:return GL_R16F; case TextureFormat::RG16F:return GL_RG16F; case TextureFormat::RGBA16F:return GL_RGBA16F; case TextureFormat::R32F:return GL_R32F; case TextureFormat::Depth24:return GL_DEPTH_COMPONENT24; case TextureFormat::Depth32F:return GL_DEPTH_COMPONENT32F; case TextureFormat::RG32F:return GL_RG32F; case TextureFormat::RGBA32F:return GL_RGBA32F; case TextureFormat::R16:return GL_R16; case TextureFormat::RG16:return GL_RG16; case TextureFormat::RGBA16:return GL_RGBA16; case TextureFormat::R32UI:return GL_R32UI; case TextureFormat::RGBA8Snorm:return GL_RGBA8_SNORM; default:return GL_RGBA8; } }
GLenum externalFormat(TextureFormat f) { switch (f) { case TextureFormat::R32UI:return GL_RED_INTEGER; case TextureFormat::R8: case TextureFormat::R16F: case TextureFormat::R32F: case TextureFormat::R16:return GL_RED; case TextureFormat::RG8: case TextureFormat::RG16F: case TextureFormat::RG32F: case TextureFormat::RG16:return GL_RG; case TextureFormat::RGB8:return GL_RGB; case TextureFormat::Depth24: case TextureFormat::Depth32F:return GL_DEPTH_COMPONENT; default:return GL_RGBA; } }
GLenum dataType(TextureFormat f) {
    switch (f) {
        case TextureFormat::R16F: case TextureFormat::RG16F: case TextureFormat::RGBA16F: case TextureFormat::R32F: case TextureFormat::Depth32F:
        case TextureFormat::RG32F: case TextureFormat::RGBA32F: return GL_FLOAT;
        case TextureFormat::R16: case TextureFormat::RG16: case TextureFormat::RGBA16: return GL_UNSIGNED_SHORT;
        case TextureFormat::R32UI: return GL_UNSIGNED_INT;
        case TextureFormat::RGBA8Snorm: return GL_BYTE;
        default: return GL_UNSIGNED_BYTE;
    }
}
// Formats usable as image2D on GL (core image-unit format list); everything else is rejected for TextureUsage::Storage.
bool storageCapable(TextureFormat f) {
    switch (f) { case TextureFormat::R8: case TextureFormat::RG8: case TextureFormat::RGBA8: case TextureFormat::R16F: case TextureFormat::RG16F: case TextureFormat::RGBA16F:
                 case TextureFormat::R32F: case TextureFormat::RG32F: case TextureFormat::RGBA32F: case TextureFormat::R16: case TextureFormat::RG16: case TextureFormat::RGBA16:
                 case TextureFormat::R32UI: case TextureFormat::RGBA8Snorm: return true; default: return false; }
}
GLenum filter(TextureFilter f) { switch(f) { case TextureFilter::Nearest:return GL_NEAREST; case TextureFilter::LinearMipmapLinear:return GL_LINEAR_MIPMAP_LINEAR; default:return GL_LINEAR; } }
GLenum wrap(TextureWrap w) { switch(w) { case TextureWrap::ClampToEdge:return GL_CLAMP_TO_EDGE; case TextureWrap::MirroredRepeat:return GL_MIRRORED_REPEAT; default:return GL_REPEAT; } }
GLuint compileStage(GLenum type, const std::string& source, const std::string& label) {
    GLuint s = glCreateShader(type); const char* p = source.c_str(); GLint len = static_cast<GLint>(source.size()); glShaderSource(s, 1, &p, &len); glCompileShader(s);
    GLint ok = GL_FALSE; glGetShaderiv(s, GL_COMPILE_STATUS, &ok); if (!ok) { GLint n=0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &n); std::string log(static_cast<std::size_t>(std::max(n,1)), '\0'); glGetShaderInfoLog(s,n,nullptr,log.data()); glDeleteShader(s); throw std::runtime_error("RHI shader compile failed ("+label+"): "+log); } return s;
}
}
Buffer::Buffer(const BufferDesc& d,const void* data):size_(d.size),target_(bufferTarget(d.usage)),dynamic_(d.dynamic),usage_(d.usage) { if (!size_) throw std::invalid_argument("RHI buffer size must be > 0"); glCreateBuffers(1,&id_); glNamedBufferStorage(id_,static_cast<GLsizeiptr>(size_),data,dynamic_?GL_DYNAMIC_STORAGE_BIT:0); }
Buffer::~Buffer(){ if(id_) glDeleteBuffers(1,&id_); }
void Buffer::update(std::size_t off,const void* data,std::size_t bytes){ if(!dynamic_) throw std::logic_error("Cannot update immutable RHI buffer"); if(!data || off>size_ || bytes>size_-off) throw std::out_of_range("RHI buffer update range invalid"); glNamedBufferSubData(id_,static_cast<GLintptr>(off),static_cast<GLsizeiptr>(bytes),data); }
Texture::Texture(const TextureDesc& d,const void* data):width_(d.width),height_(d.height),format_(d.format),usage_(d.usage),levels_(d.levels){ if(width_<=0||height_<=0||d.levels<=0) throw std::invalid_argument("Invalid RHI texture dimensions/levels");
    if(hasUsage(d.usage,TextureUsage::Storage)){ if(!storageCapable(d.format)) throw std::invalid_argument("OpenGL: format cannot be used as a storage image (TextureUsage::Storage)"); if(d.samples>1) throw std::invalid_argument("OpenGL: storage textures cannot be multisampled"); } glCreateTextures(GL_TEXTURE_2D,1,&id_); glTextureStorage2D(id_,d.levels,textureFormat(d.format),width_,height_); if(data) glTextureSubImage2D(id_,0,0,0,width_,height_,externalFormat(d.format),dataType(d.format),data); }
Texture::~Texture(){if(id_)glDeleteTextures(1,&id_);}
GLenum Texture::internalFormat() const noexcept { return textureFormat(format_); }
Sampler::Sampler(const SamplerDesc& d){glCreateSamplers(1,&id_);glSamplerParameteri(id_,GL_TEXTURE_MIN_FILTER,filter(d.minFilter));glSamplerParameteri(id_,GL_TEXTURE_MAG_FILTER,filter(d.magFilter));glSamplerParameteri(id_,GL_TEXTURE_WRAP_S,wrap(d.wrapU));glSamplerParameteri(id_,GL_TEXTURE_WRAP_T,wrap(d.wrapV));}
Sampler::~Sampler(){if(id_)glDeleteSamplers(1,&id_);}
Shader::Shader(const ShaderDesc& d){if(!d.computeSource.empty()){buildCompute(d);return;}GLuint v=compileStage(GL_VERTEX_SHADER,d.vertexSource,d.label); GLuint f=0; try{f=compileStage(GL_FRAGMENT_SHADER,d.fragmentSource,d.label);id_=glCreateProgram();glAttachShader(id_,v);glAttachShader(id_,f);glLinkProgram(id_);GLint ok=GL_FALSE;glGetProgramiv(id_,GL_LINK_STATUS,&ok);if(!ok){GLint n=0;glGetProgramiv(id_,GL_INFO_LOG_LENGTH,&n);std::string log(static_cast<std::size_t>(std::max(n,1)),'\0');glGetProgramInfoLog(id_,n,nullptr,log.data());throw std::runtime_error("RHI shader link failed ("+d.label+"): "+log);}}catch(...){if(id_)glDeleteProgram(id_);glDeleteShader(v);if(f)glDeleteShader(f);throw;}glDetachShader(id_,v);glDetachShader(id_,f);glDeleteShader(v);glDeleteShader(f);}
Shader::~Shader(){if(id_)glDeleteProgram(id_);}
void Shader::buildCompute(const ShaderDesc& d){
    if(!d.vertexSource.empty()||!d.fragmentSource.empty()) throw std::invalid_argument("RHI shader: a Shader is either vertex+fragment or compute, not both");
    if(!GLAD_GL_VERSION_4_3) throw UnsupportedByBackend("OpenGL: compute shaders need OpenGL 4.3");
    GLuint c=compileStage(GL_COMPUTE_SHADER,d.computeSource,d.label);
    id_=glCreateProgram(); glAttachShader(id_,c); glLinkProgram(id_);
    GLint ok=GL_FALSE; glGetProgramiv(id_,GL_LINK_STATUS,&ok);
    if(!ok){ GLint n=0; glGetProgramiv(id_,GL_INFO_LOG_LENGTH,&n); std::string log(static_cast<std::size_t>(std::max(n,1)),'\0'); glGetProgramInfoLog(id_,n,nullptr,log.data());
             glDeleteProgram(id_); id_=0; glDeleteShader(c); throw std::runtime_error("RHI compute shader link failed ("+d.label+"): "+log); }
    glDetachShader(id_,c); glDeleteShader(c); compute_=true;
}
Pipeline::Pipeline(const std::shared_ptr<rhi::Shader>& s,const PipelineDesc& d):shader_(s),desc_(d){auto gls=std::dynamic_pointer_cast<Shader>(s);if(!gls)throw std::invalid_argument("Pipeline shader belongs to another RHI backend");if(gls->isCompute())throw std::invalid_argument("OpenGL: createPipeline got a compute shader (use createComputePipeline)");program_=gls->id();}
ComputePipeline::ComputePipeline(const std::shared_ptr<rhi::Shader>& s,const ComputePipelineDesc& d){
    if(!GLAD_GL_VERSION_4_3) throw UnsupportedByBackend("OpenGL: compute needs OpenGL 4.3");
    shader_=std::dynamic_pointer_cast<Shader>(s);
    if(!shader_||!shader_->isCompute()) throw std::invalid_argument("OpenGL: createComputePipeline needs a shader created from ShaderDesc::computeSource");
    if(d.pushConstantBytes>0) throw UnsupportedByBackend("OpenGL: compute push constants are not supported; pass the data in a uniform buffer (set 0 / binding N)");
    program_=shader_->id();
    GLint ls[3]={1,1,1}; glGetProgramiv(program_,GL_COMPUTE_WORK_GROUP_SIZE,ls);
    GLint inv=0; glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,&inv);
    for(int i=0;i<3;++i){ GLint m=0; glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,static_cast<GLuint>(i),&m); if(ls[i]<=0||ls[i]>m) throw std::invalid_argument("OpenGL: local workgroup size exceeds GL_MAX_COMPUTE_WORK_GROUP_SIZE"); local_[i]=static_cast<std::uint32_t>(ls[i]); }
    if(static_cast<std::uint64_t>(local_[0])*local_[1]*local_[2]>static_cast<std::uint64_t>(inv)) throw std::invalid_argument("OpenGL: local workgroup size exceeds GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS");
}
CommandList::CommandList(){
    computeOk_=GLAD_GL_VERSION_4_3;
    if(computeOk_){ for(int i=0;i<3;++i) glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,static_cast<GLuint>(i),&maxCount_[i]);
        glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,&ssboAlign_); glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&maxSsboRange_); }
}
void CommandList::requireRecording() const { if(!recording_) throw std::logic_error("CommandList::begin must be called first"); }
void CommandList::requireCompute() const { if(!computeOk_) throw UnsupportedByBackend("OpenGL: compute needs OpenGL 4.3"); }
void CommandList::begin(){
    recording_=true;
    if(computeOk_){ for(unsigned i=0;i<kMaxStorageImageSlots;++i) if(simg_[i].tex) glBindImageTexture(i,0,0,GL_FALSE,0,GL_READ_ONLY,GL_R32F);
                    for(unsigned i=0;i<kMaxStorageBufferSlots;++i) if(sbuf_[i].buf) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,0); }
    sbuf_={}; simg_={}; tex_={}; cpipeline_.reset(); openBuf_.clear(); openImg_.clear(); openWrite_=false;   // bindings of a previous list do not leak into this one
}
void CommandList::end(){ requireRecording(); flushAccesses(true); recording_=false; }
void CommandList::setPipeline(const std::shared_ptr<rhi::Pipeline>& p){if(!recording_)throw std::logic_error("CommandList::begin must be called first");pipeline_=std::dynamic_pointer_cast<Pipeline>(p);if(!pipeline_)throw std::invalid_argument("Pipeline belongs to another RHI backend");glUseProgram(pipeline_->program());program_=pipeline_->program();const auto& d=pipeline_->desc();d.depthTest?glEnable(GL_DEPTH_TEST):glDisable(GL_DEPTH_TEST);glDepthMask(d.depthWrite?GL_TRUE:GL_FALSE);d.blend?glEnable(GL_BLEND):glDisable(GL_BLEND);if(d.blend)glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);d.cull?glEnable(GL_CULL_FACE):glDisable(GL_CULL_FACE);}
void CommandList::bindVertexBuffer(const std::shared_ptr<rhi::Buffer>& b,unsigned binding){auto p=std::dynamic_pointer_cast<Buffer>(b);if(!p)throw std::invalid_argument("Vertex buffer backend mismatch");(void)binding;glBindBuffer(GL_ARRAY_BUFFER,p->id());}
void CommandList::bindIndexBuffer(const std::shared_ptr<rhi::Buffer>& b){auto p=std::dynamic_pointer_cast<Buffer>(b);if(!p)throw std::invalid_argument("Index buffer backend mismatch");glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,p->id());}
void CommandList::bindTexture(unsigned unit,const std::shared_ptr<rhi::Texture>& t,const std::shared_ptr<rhi::Sampler>& s){auto tex=std::dynamic_pointer_cast<Texture>(t);if(!tex)throw std::invalid_argument("Texture backend mismatch");glBindTextureUnit(unit,tex->id());if(unit<kMaxTextureSlots)tex_[unit]=tex;if(s){auto smp=std::dynamic_pointer_cast<Sampler>(s);if(!smp)throw std::invalid_argument("Sampler backend mismatch");glBindSampler(unit,smp->id());}}
void CommandList::draw(std::uint32_t n,std::uint32_t first){if(!recording_)throw std::logic_error("CommandList is not recording");flushAccesses(false);useGraphicsProgram();glDrawArrays(GL_TRIANGLES,static_cast<GLint>(first),static_cast<GLsizei>(n));}
void CommandList::drawIndexed(std::uint32_t n,std::uint32_t first){if(!recording_)throw std::logic_error("CommandList is not recording");flushAccesses(false);useGraphicsProgram();glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(n),GL_UNSIGNED_INT,reinterpret_cast<const void*>(static_cast<std::uintptr_t>(first)*sizeof(std::uint32_t)));}
void CommandList::useGraphicsProgram(){ if(pipeline_&&program_!=pipeline_->program()){ glUseProgram(pipeline_->program()); program_=pipeline_->program(); } }
void CommandList::bindStorageTexture(unsigned slot,const std::shared_ptr<rhi::Texture>& t,std::uint32_t mip,std::uint32_t baseLayer,std::uint32_t layerCount){
    requireRecording(); requireCompute();
    if(slot>=kMaxStorageImageSlots) throw std::out_of_range("OpenGL: storage image slot out of range");
    auto gt=std::dynamic_pointer_cast<Texture>(t);
    if(!gt){ if(t) throw std::invalid_argument("Texture backend mismatch"); glBindImageTexture(slot,0,0,GL_FALSE,0,GL_READ_ONLY,GL_R32F); simg_[slot]={}; return; }
    if(!hasUsage(gt->usage(),TextureUsage::Storage)) throw std::invalid_argument("OpenGL: bindStorageTexture needs a texture created with TextureUsage::Storage");
    if(mip>=static_cast<std::uint32_t>(gt->levels())) throw std::out_of_range("OpenGL: storage image mip is outside the texture");
    if(baseLayer!=0||layerCount>1) throw UnsupportedByBackend("OpenGL: only plain 2D storage images are supported (baseLayer 0, layerCount 1)");
    glBindImageTexture(slot,gt->id(),static_cast<GLint>(mip),GL_FALSE,0,GL_READ_WRITE,gt->internalFormat());
    simg_[slot]={gt,mip};
}
void CommandList::bindStorageBuffer(unsigned slot,const std::shared_ptr<rhi::Buffer>& b,std::size_t offset,std::size_t range){
    requireRecording(); requireCompute();
    if(slot>=kMaxStorageBufferSlots) throw std::out_of_range("OpenGL: storage buffer slot out of range");
    auto gb=std::dynamic_pointer_cast<Buffer>(b);
    if(!gb){ if(b) throw std::invalid_argument("Buffer backend mismatch"); glBindBufferBase(GL_SHADER_STORAGE_BUFFER,slot,0); sbuf_[slot]={}; return; }
    if(gb->usage()!=BufferUsage::Storage&&gb->usage()!=BufferUsage::Indirect) throw std::invalid_argument("OpenGL: bindStorageBuffer needs a buffer created with BufferUsage::Storage (or Indirect)");
    if(ssboAlign_>0&&offset%static_cast<std::size_t>(ssboAlign_)!=0) throw std::invalid_argument("OpenGL: storage buffer offset must be a multiple of GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT");
    if(offset>=gb->size()) throw std::out_of_range("OpenGL: storage buffer offset is outside the buffer");
    const std::size_t r=range?range:gb->size()-offset;
    if(offset+r>gb->size()) throw std::out_of_range("OpenGL: storage buffer range is outside the buffer");
    if(maxSsboRange_>0&&r>static_cast<std::size_t>(maxSsboRange_)) throw std::invalid_argument("OpenGL: storage buffer range exceeds GL_MAX_SHADER_STORAGE_BLOCK_SIZE");
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,slot,gb->id(),static_cast<GLintptr>(offset),static_cast<GLsizeiptr>(r));
    sbuf_[slot]={gb,offset,r};
}
void CommandList::setComputePipeline(const std::shared_ptr<rhi::ComputePipeline>& p){
    requireRecording(); requireCompute();
    cpipeline_=std::dynamic_pointer_cast<ComputePipeline>(p);
    if(!cpipeline_) throw std::invalid_argument("OpenGL: setComputePipeline needs a compute pipeline of this backend");
    glUseProgram(cpipeline_->program()); program_=cpipeline_->program();
}
void CommandList::dispatch(std::uint32_t x,std::uint32_t y,std::uint32_t z){
    requireRecording(); requireCompute();
    if(x>static_cast<std::uint32_t>(maxCount_[0])||y>static_cast<std::uint32_t>(maxCount_[1])||z>static_cast<std::uint32_t>(maxCount_[2])) throw std::invalid_argument("OpenGL: dispatch exceeds GL_MAX_COMPUTE_WORK_GROUP_COUNT");
    if(x==0||y==0||z==0) return;
    prepareDispatch(nullptr,0);
    glDispatchCompute(x,y,z);
}
void CommandList::dispatchIndirect(const std::shared_ptr<rhi::Buffer>& b,std::size_t offset){
    requireRecording(); requireCompute();
    auto ib=std::dynamic_pointer_cast<Buffer>(b);
    if(!ib||ib->usage()!=BufferUsage::Indirect) throw std::invalid_argument("OpenGL: dispatchIndirect needs a buffer created with BufferUsage::Indirect");
    if(offset%4!=0||offset+3*sizeof(std::uint32_t)>ib->size()) throw std::out_of_range("OpenGL: dispatchIndirect offset is misaligned or outside the buffer");
    prepareDispatch(ib,offset);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER,ib->id());
    glDispatchComputeIndirect(static_cast<GLintptr>(offset));
}
void CommandList::prepareDispatch(const std::shared_ptr<Buffer>& indirect,std::size_t indirectOffset){
    if(!cpipeline_) throw std::logic_error("OpenGL: dispatch without a compute pipeline");
    // A texture cannot be sampled and written through an image unit in the same dispatch (undefined feedback in GL).
    for(unsigned i=0;i<kMaxStorageImageSlots;++i){
        if(!simg_[i].tex) continue;
        for(unsigned u=0;u<kMaxTextureSlots;++u)
            if(tex_[u]==simg_[i].tex) throw std::logic_error("OpenGL: texture is bound as sampled (slot "+std::to_string(u)+") and as storage image (slot "+std::to_string(i)+"); unbind the sampled slot first");
    }
    if(program_!=cpipeline_->program()){ glUseProgram(cpipeline_->program()); program_=cpipeline_->program(); }   // a draw may have switched the program
    trackAccesses(indirect,indirectOffset);
}
void CommandList::trackAccesses(const std::shared_ptr<Buffer>& indirect,std::size_t indirectOffset){
    std::vector<BufAccess> acc; std::vector<ImgAccess> iacc;
    for(const auto& sb:sbuf_) if(sb.buf) acc.push_back({sb.buf->id(),sb.offset,sb.offset+sb.range,true});
    if(indirect) acc.push_back({indirect->id(),indirectOffset,indirectOffset+3*sizeof(std::uint32_t),false});
    for(const auto& si:simg_) if(si.tex) iacc.push_back({si.tex->id(),si.mip,1,true});
    for(const auto& t:tex_) if(t) iacc.push_back({t->id(),0,static_cast<std::uint32_t>(std::max(1,t->levels())),false});
    bool conflict=openBuf_.size()+acc.size()+openImg_.size()+iacc.size()>256;   // bounded bookkeeping: past the cap just synchronise
    for(const auto& n:acc){ if(conflict) break; for(const auto& o:openBuf_) if(o.buf==n.buf&&n.b<o.e&&o.b<n.e&&(o.write||n.write)){conflict=true;break;} }
    for(const auto& n:iacc){ if(conflict) break; for(const auto& o:openImg_) if(o.tex==n.tex&&n.m0<o.m0+o.mc&&o.m0<n.m0+n.mc&&(o.write||n.write)){conflict=true;break;} }
    if(conflict){
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT|GL_COMMAND_BARRIER_BIT);
        openBuf_.clear(); openImg_.clear(); openWrite_=false;
    }
    for(const auto& n:acc){ openBuf_.push_back(n); openWrite_=openWrite_||n.write; }
    for(const auto& n:iacc){ openImg_.push_back(n); openWrite_=openWrite_||n.write; }
}
// Compute writes become visible to everything else (vertex / index / indirect / uniform / texture / framebuffer / transfers) before the next draw
// and at the end of the list. Nothing is emitted when only reads are open.
void CommandList::flushAccesses(bool endOfList){
    if(openWrite_) glMemoryBarrier(GL_ALL_BARRIER_BITS);
    if(openWrite_||endOfList){ openBuf_.clear(); openImg_.clear(); openWrite_=false; }
}
Device::Device(){caps_.backend=Backend::OpenGL45;const auto* name=glGetString(GL_RENDERER);const auto* version=glGetString(GL_VERSION);caps_.deviceName=name?reinterpret_cast<const char*>(name):"Unknown OpenGL device";caps_.apiVersion=version?reinterpret_cast<const char*>(version):"Unknown";glGetIntegerv(GL_MAX_TEXTURE_SIZE,&caps_.maxTextureSize);glGetIntegerv(GL_MAX_SAMPLES,&caps_.maxSamples);caps_.debugOutput=GLAD_GL_VERSION_4_3;caps_.directStateAccess=GLAD_GL_VERSION_4_5;
    caps_.compute=GLAD_GL_VERSION_4_3;caps_.storageImages=GLAD_GL_VERSION_4_3;
    if(caps_.compute){for(int i=0;i<3;++i){GLint v=0;glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,static_cast<GLuint>(i),&v);caps_.maxComputeWorkGroupSize[i]=static_cast<std::uint32_t>(std::max(v,0));}GLint inv=0;glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,&inv);caps_.maxComputeInvocations=static_cast<std::uint32_t>(std::max(inv,0));}}
std::shared_ptr<rhi::Buffer> Device::createBuffer(const BufferDesc&d,const void*p){return std::make_shared<Buffer>(d,p);} std::shared_ptr<rhi::Texture> Device::createTexture2D(const TextureDesc&d,const void*p){return std::make_shared<Texture>(d,p);} std::shared_ptr<rhi::Sampler> Device::createSampler(const SamplerDesc&d){return std::make_shared<Sampler>(d);} std::shared_ptr<rhi::Shader> Device::createShader(const ShaderDesc&d){return std::make_shared<Shader>(d);} std::shared_ptr<rhi::Pipeline> Device::createPipeline(const std::shared_ptr<rhi::Shader>&s,const PipelineDesc&d){return std::make_shared<Pipeline>(s,d);} std::unique_ptr<rhi::CommandList> Device::createCommandList(){return std::make_unique<CommandList>();}
std::shared_ptr<rhi::ComputePipeline> Device::createComputePipeline(const std::shared_ptr<rhi::Shader>&s,const ComputePipelineDesc&d){return std::make_shared<ComputePipeline>(s,d);}
}
namespace anvil::rhi { std::unique_ptr<Device> createOpenGL45Device(){return std::make_unique<gl::Device>();} }

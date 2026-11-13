#include "anvil/Import.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <cctype>
#include <limits>
#include <functional>
#include <unordered_set>
#ifdef ANVIL_HAS_ZLIB
#include <zlib.h>
#endif
namespace anvil {
namespace {
std::uint16_t u16(const std::vector<unsigned char>& b,std::size_t p){return p<=b.size()&&b.size()-p>=2?std::uint16_t(b[p]|(b[p+1]<<8)):0;}
std::uint32_t u32(const std::vector<unsigned char>& b,std::size_t p){return p<=b.size()&&b.size()-p>=4?std::uint32_t(b[p]|(b[p+1]<<8)|(b[p+2]<<16)|(b[p+3]<<24)):0;}
std::uint32_t computeCrc32(const unsigned char* data,std::size_t size){std::uint32_t crc=0xffffffffu;for(std::size_t i=0;i<size;++i){crc^=data[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&(~(crc&1u)+1u));}return ~crc;}
std::string lower(std::string s){for(char&c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
AssetKind classify(const std::filesystem::path& p){auto e=lower(p.extension().string());
 if(e==".png"||e==".jpg"||e==".jpeg"||e==".tga"||e==".bmp"||e==".tif"||e==".tiff"||e==".webp"||e==".hdr"||e==".exr")return AssetKind::Texture;
 if(e==".fbx"||e==".obj"||e==".gltf"||e==".glb"||e==".dae"||e==".3ds"||e==".ply")return AssetKind::Model;
 if(e==".wav"||e==".ogg"||e==".mp3"||e==".flac"||e==".aiff")return AssetKind::Audio;
 if(e==".anim"||e==".animation"||e==".animset")return AssetKind::Animation;
 if(e==".mat"||e==".material"||e==".mtl")return AssetKind::Material;
 if(e==".unity"||e==".tscn"||e==".scn"||e==".scene"||e==".umap")return AssetKind::Scene;
 return AssetKind::Other;
}
bool selected(AssetKind k,const MigrationOptions&o){switch(k){case AssetKind::Texture:return o.copyTextures;case AssetKind::Model:return o.copyModels;case AssetKind::Audio:return o.copyAudio;case AssetKind::Animation:return o.copyAnimations;case AssetKind::Material:return o.copyMaterials;case AssetKind::Scene:return o.copyScenes;default:return false;}}
void countAsset(ImportReport&r,AssetKind k){switch(k){case AssetKind::Texture:++r.textureCount;break;case AssetKind::Model:++r.modelCount;break;case AssetKind::Audio:++r.audioCount;break;case AssetKind::Animation:++r.animationCount;break;case AssetKind::Material:++r.materialCount;break;case AssetKind::Scene:++r.sceneCount;break;default:break;}}
bool extractZipEntry(const std::vector<unsigned char>&zip,const ImportEntry&e,std::vector<unsigned char>&out,std::string&error,std::uint64_t maxEntryBytes=64ull*1024*1024){
 const std::size_t h=e.localHeaderOffset;if(e.generalPurposeFlags&1u){error="Encrypted ZIP entries are not supported";return false;}if(h>zip.size()||zip.size()-h<30||u32(zip,h)!=0x04034b50){error="Invalid ZIP local-file header";return false;}if((u16(zip,h+6)&1u)||u16(zip,h+8)!=e.compressionMethod){error="ZIP local and central entry flags/method do not match or entry is encrypted";return false;}
 const std::size_t data=h+30+u16(zip,h+26)+u16(zip,h+28);if(data>zip.size()||e.compressedSize>zip.size()-data||e.uncompressedSize>maxEntryBytes){error="ZIP entry size or offset is invalid or exceeds configured safety limit";return false;}
 const auto method=e.compressionMethod;const auto*src=zip.data()+data;const auto compressed=static_cast<std::size_t>(e.compressedSize);
 if(method==0){if(compressed!=e.uncompressedSize){error="Stored ZIP entry has inconsistent sizes";return false;}out.assign(src,src+compressed);if(computeCrc32(out.data(),out.size())!=e.crc32){out.clear();error="ZIP entry CRC mismatch";return false;}return true;}
#ifdef ANVIL_HAS_ZLIB
 if(method==8){out.resize(static_cast<std::size_t>(e.uncompressedSize));z_stream stream{};stream.next_in=const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));stream.avail_in=static_cast<uInt>(compressed);stream.next_out=reinterpret_cast<Bytef*>(out.data());stream.avail_out=static_cast<uInt>(out.size());if(inflateInit2(&stream,-MAX_WBITS)!=Z_OK){error="zlib initialization failed";return false;}int status=inflate(&stream,Z_FINISH);inflateEnd(&stream);if(status!=Z_STREAM_END||stream.total_out!=out.size()){out.clear();error="Deflate decompression failed or produced an unexpected size";return false;}if(computeCrc32(out.data(),out.size())!=e.crc32){out.clear();error="ZIP entry CRC mismatch";return false;}return true;}
#endif
 error="Unsupported ZIP compression method: "+std::to_string(method);return false;
}
std::string readUtf8(const std::vector<unsigned char>&b,std::size_t&pos,std::size_t end){
 if(pos>=end)return {};
 auto readLen=[&](){if(pos>=end)return std::size_t(0);unsigned char a=b[pos++];if(a&0x80){if(pos>=end)return std::size_t(0);return (std::size_t(a&0x7f)<<8)|b[pos++];}return std::size_t(a);};
 (void)readLen();std::size_t bytes=readLen();if(bytes>end-pos)return {};std::string v(reinterpret_cast<const char*>(b.data()+pos),bytes);pos+=bytes;if(pos<end&&b[pos]==0)++pos;return v;
}
std::vector<std::string> parseStringPool(const std::vector<unsigned char>&b,std::size_t p,std::size_t end){
 std::vector<std::string> out;if(p+28>end)return out;auto count=u32(b,p+8),flags=u32(b,p+16),stringsStart=u32(b,p+20);if(count>100000||p+28ull+count*4ull>end||stringsStart<28ull+count*4ull||p+stringsStart>end)return out;bool utf8=(flags&0x100)!=0;
 for(std::uint32_t i=0;i<count;++i){auto off=u32(b,p+28+static_cast<std::size_t>(i)*4);std::size_t q=p+stringsStart+off;if(q>=end){out.emplace_back();continue;}if(utf8)out.push_back(readUtf8(b,q,end));else{auto units=u16(b,q);q+=2;if(units>32768||q+static_cast<std::size_t>(units)*2>end){out.emplace_back();continue;}std::string v;for(unsigned n=0;n<units;++n){std::uint16_t cp=u16(b,q+n*2);if(cp<0x80)v.push_back(static_cast<char>(cp));else if(cp<0x800){v.push_back(static_cast<char>(0xc0|(cp>>6)));v.push_back(static_cast<char>(0x80|(cp&0x3f)));}else{v.push_back(static_cast<char>(0xe0|(cp>>12)));v.push_back(static_cast<char>(0x80|((cp>>6)&0x3f)));v.push_back(static_cast<char>(0x80|(cp&0x3f)));}}out.push_back(std::move(v));}}
 return out;
}
bool parseAndroidManifest(const std::vector<unsigned char>&b,ImportReport&r){
 if(b.size()<8||u16(b,0)!=0x0003||u32(b,4)>b.size()){r.warnings.push_back("AndroidManifest.xml is not a valid binary Android XML document.");return false;}
 const std::size_t end=u32(b,4);std::vector<std::string> strings;bool sawManifest=false;
 for(std::size_t p=u16(b,2);p+8<=end;){auto type=u16(b,p);auto header=u16(b,p+2);auto size=u32(b,p+4);if(size<8||p+size>end||header>size){r.warnings.push_back("Malformed Android XML chunk bounds.");return false;}
  if(type==0x0001)strings=parseStringPool(b,p,p+size);
  else if(type==0x0102&&header>=16&&p+36<=end&&!strings.empty()){
   auto nameIndex=u32(b,p+20);if(nameIndex>=strings.size()){p+=size;continue;}const auto&tag=strings[nameIndex];auto attrStart=u16(b,p+24),attrSize=u16(b,p+26),attrCount=u16(b,p+28);std::size_t attrs=p+16+attrStart;
   if(attrSize<20||attrCount>4096||attrs+static_cast<std::size_t>(attrCount)*attrSize>p+size){p+=size;continue;}
   std::string package;
   for(unsigned i=0;i<attrCount;++i){std::size_t a=attrs+static_cast<std::size_t>(i)*attrSize;auto ni=u32(b,a+4);auto raw=u32(b,a+8);auto dataType=b[a+15];auto data=u32(b,a+16);std::string value;if(raw!=0xffffffffu&&raw<strings.size())value=strings[raw];else if(dataType==3&&data<strings.size())value=strings[data];
    if(ni>=strings.size())continue;
    const auto&an=strings[ni];if(tag=="manifest"&&an=="package")package=value;
    if((tag=="uses-permission"||tag=="uses-permission-sdk-23"||tag=="uses-permission-sdk-m")&&an=="name"&&!value.empty())r.permissions.push_back(value);
    if(tag=="uses-sdk"&&an=="minSdkVersion")r.minSdkVersion=static_cast<int>(data);
    if(tag=="uses-sdk"&&an=="targetSdkVersion")r.targetSdkVersion=static_cast<int>(data);
   }
   if(tag=="manifest"){sawManifest=true;if(!package.empty())r.packageName=package;}
  }
  p+=size;
 }
 std::sort(r.permissions.begin(),r.permissions.end());r.permissions.erase(std::unique(r.permissions.begin(),r.permissions.end()),r.permissions.end());if(!sawManifest)r.warnings.push_back("Binary manifest did not contain a manifest start element.");return sawManifest;
}

}
ImportReport ImportAnalyzer::analyze(const std::filesystem::path& p){
 ImportReport r; r.source=p;
 std::error_code ec; if(!std::filesystem::exists(p,ec)||(!std::filesystem::is_regular_file(p,ec)&&!std::filesystem::is_directory(p,ec))){r.errors.push_back("Input is not an existing file or project directory");return r;}
 auto ext=lower(p.extension().string());
 if(ext==".exe"||ext==".dll"){
  r.kind=ImportKind::WindowsPE; std::ifstream f(p,std::ios::binary); std::vector<unsigned char>b((std::istreambuf_iterator<char>(f)),{});
  if(b.size()<64||b[0]!='M'||b[1]!='Z'){r.errors.push_back("Invalid DOS/PE image");return r;}
  auto off=u32(b,0x3c); if(off+24>b.size()||u32(b,off)!=0x00004550){r.errors.push_back("PE signature missing or truncated");return r;}
  r.machine=u16(b,off+4); r.timestamp=u32(b,off+8);
  const std::uint16_t sectionCount=u16(b,off+6),optionalSize=u16(b,off+20);
  const std::size_t optional=static_cast<std::size_t>(off)+24;
  const auto optionalMagic=u16(b,optional);const bool pe64=optionalMagic==0x20b;
  const std::size_t directory=optional+(pe64?112:96);
  if(optionalMagic!=0x10b&&optionalMagic!=0x20b)r.warnings.push_back("Unknown PE optional-header magic; import parsing may be incomplete.");
  const std::size_t sectionTable=optional+optionalSize;
  auto rvaOffset=[&](std::uint32_t rva)->std::size_t{
   for(std::uint16_t si=0;si<sectionCount;++si){std::size_t sh=sectionTable+static_cast<std::size_t>(si)*40;if(sh+40>b.size())break;
    auto virtualSize=u32(b,sh+8),virtualAddress=u32(b,sh+12),rawSize=u32(b,sh+16),rawOffset=u32(b,sh+20);
    auto span=std::max(virtualSize,rawSize);if(rva>=virtualAddress&&std::uint64_t(rva)<std::uint64_t(virtualAddress)+span){auto delta=rva-virtualAddress;if(delta<rawSize&&std::uint64_t(rawOffset)+delta<b.size())return static_cast<std::size_t>(std::uint64_t(rawOffset)+delta);}
   }return std::string::npos;
  };
  if(optional+optionalSize<=b.size()&&directory+16<=optional+optionalSize){
   auto importRva=u32(b,directory+8),importSize=u32(b,directory+12);auto imp=rvaOffset(importRva);
   if(importRva&&imp!=std::string::npos){for(std::size_t n=0;n<importSize/20+1&&imp+20<=b.size();++n,imp+=20){auto nameRva=u32(b,imp+12);if(!nameRva&&!u32(b,imp)&&!u32(b,imp+4)&&!u32(b,imp+8)&&!u32(b,imp+16))break;auto no=rvaOffset(nameRva);if(no==std::string::npos)continue;std::string dll;for(std::size_t q=no;q<b.size()&&b[q]&&q-no<260;++q)dll.push_back(static_cast<char>(b[q]));if(!dll.empty())r.libraries.push_back(dll);}}
   else r.warnings.push_back("PE import directory is absent or cannot be mapped from RVA to file offset.");
   // Delay-load imports (IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT, directory 13).
   if(directory+14*8<=optional+optionalSize){auto delayRva=u32(b,directory+13*8),delaySize=u32(b,directory+13*8+4);auto delayOffset=rvaOffset(delayRva);if(delayRva&&delayOffset!=std::string::npos){for(std::size_t n=0;n<std::min<std::size_t>(delaySize/32+1,4096)&&delayOffset+32<=b.size();++n,delayOffset+=32){auto attrs=u32(b,delayOffset),nameRva=u32(b,delayOffset+4);if(!attrs&&!nameRva&&!u32(b,delayOffset+8)&&!u32(b,delayOffset+12)&&!u32(b,delayOffset+16)&&!u32(b,delayOffset+20)&&!u32(b,delayOffset+24)&&!u32(b,delayOffset+28))break;if(!(attrs&1u)){r.warnings.push_back("Delay-load descriptor uses VA fields; conversion to RVA is not implemented");continue;}auto no=rvaOffset(nameRva);if(no==std::string::npos)continue;std::string dll;for(std::size_t q=no;q<b.size()&&b[q]&&q-no<260;++q)dll.push_back(static_cast<char>(b[q]));if(!dll.empty())r.libraries.push_back(dll);}}}
   // Traverse the PE resource directory and retain exact byte ranges for embedded resources.
   if(directory+3*8<=optional+optionalSize){auto resourceRva=u32(b,directory+2*8);auto resourceBase=rvaOffset(resourceRva);if(resourceRva&&resourceBase!=std::string::npos){
    auto resourceLabel=[&](std::uint32_t value,bool named)->std::string{if(!named)return std::to_string(value&0xffffu);std::size_t q=resourceBase+(value&0x7fffffffu);if(q+2>b.size())return "named";auto len=u16(b,q);q+=2;if(len>128||q+static_cast<std::size_t>(len)*2>b.size())return "named";std::string text;for(unsigned i=0;i<len;++i){auto ch=u16(b,q+i*2);text.push_back(ch<128?static_cast<char>(ch):'_');}return text.empty()?"named":text;};
    std::unordered_set<std::uint32_t> visited;std::function<void(std::uint32_t,int,std::uint32_t,std::uint32_t)> walk;
    walk=[&](std::uint32_t relative,int depth,std::uint32_t type,std::uint32_t name){if(depth>2||!visited.insert(relative).second)return;std::size_t dirOff=resourceBase+relative;if(dirOff>b.size()||b.size()-dirOff<16)return;std::uint32_t count=std::uint32_t(u16(b,dirOff+12))+u16(b,dirOff+14);if(count>4096||dirOff+16ull+count*8ull>b.size())return;for(std::uint32_t i=0;i<count&&r.embeddedResources.size()<100000;++i){auto e=dirOff+16+static_cast<std::size_t>(i)*8;auto nameField=u32(b,e),child=u32(b,e+4);auto label=resourceLabel(nameField,(nameField&0x80000000u)!=0);std::uint32_t nextType=depth==0?((nameField&0x80000000u)?0x80000000u|static_cast<std::uint32_t>(std::hash<std::string>{}(label)&0x7fffffffu):nameField&0xffffu):type;std::uint32_t nextName=depth==1?((nameField&0x80000000u)?0x80000000u|static_cast<std::uint32_t>(std::hash<std::string>{}(label)&0x7fffffffu):nameField&0xffffu):name;
     if(depth<2&&(child&0x80000000u)){walk(child&0x7fffffffu,depth+1,nextType,nextName);continue;}if(depth!=2||(child&0x80000000u))continue;std::size_t dataEntry=resourceBase+(child&0x7fffffffu);if(dataEntry>b.size()||b.size()-dataEntry<16)continue;auto dataRva=u32(b,dataEntry),dataSize=u32(b,dataEntry+4);auto dataOffset=rvaOffset(dataRva);if(dataSize==0||dataOffset==std::string::npos||dataOffset>b.size()||dataSize>b.size()-dataOffset)continue;auto language=std::uint32_t(nameField&0xffffu);EmbeddedResource resource;resource.typeId=type;resource.nameId=name;resource.languageId=language;resource.dataOffset=static_cast<std::uint32_t>(dataOffset);resource.size=dataSize;resource.path="Resources/PE/type_"+std::to_string(type)+"_name_"+std::to_string(name)+"_lang_"+std::to_string(language)+".bin";r.embeddedResources.push_back(std::move(resource));
    }};walk(0,0,0,0);
   }}

  }else r.warnings.push_back("PE optional header is truncated; import directory was not inspected.");
  switch(r.machine){case 0x14c:r.architecture="x86";break;case 0x8664:r.architecture="x86_64";break;case 0xaa64:r.architecture="ARM64";break;case 0x1c0:r.architecture="ARM";break;default:r.architecture="Unknown";r.warnings.push_back("Unrecognized PE machine type");}
  for(const auto&lib:r.libraries){auto name=lower(lib);if(name.find("d3d12")!=std::string::npos||name.find("d3d11")!=std::string::npos||name.find("dxgi")!=std::string::npos)r.graphicsApis.push_back("Direct3D");if(name.find("vulkan")!=std::string::npos)r.graphicsApis.push_back("Vulkan");if(name.find("opengl")!=std::string::npos||name=="opengl32.dll")r.graphicsApis.push_back("OpenGL");if(name.find("gles")!=std::string::npos)r.graphicsApis.push_back("OpenGL ES");}
  std::sort(r.libraries.begin(),r.libraries.end());r.libraries.erase(std::unique(r.libraries.begin(),r.libraries.end()),r.libraries.end());
  std::sort(r.graphicsApis.begin(),r.graphicsApis.end());r.graphicsApis.erase(std::unique(r.graphicsApis.begin(),r.graphicsApis.end()),r.graphicsApis.end());
  r.warnings.push_back("Imported DLLs include normal and RVA-based delay-load imports; transitive dependency resolution is not included.");
  return r;
 }
 if(ext==".apk"||ext==".zip"){
  std::ifstream f(p,std::ios::binary); std::vector<unsigned char>b((std::istreambuf_iterator<char>(f)),{});
  r.kind=ext==".apk"?ImportKind::AndroidAPK:ImportKind::ZipArchive;
  // ZIP End Of Central Directory and central directory records.
  std::size_t eocd=std::string::npos, begin=b.size()>65557?b.size()-65557:0;
  for(std::size_t i=b.size()>=22?b.size()-22:0;;--i){if(u32(b,i)==0x06054b50){eocd=i;break;}if(i==begin||i==0)break;}
  if(eocd==std::string::npos){r.errors.push_back("ZIP end-of-central-directory record not found");return r;}if(eocd+22>b.size()||std::uint64_t(eocd)+22+u16(b,eocd+20)!=b.size()){r.errors.push_back("ZIP end-of-central-directory comment length is invalid");return r;}
  if(u16(b,eocd+4)!=0||u16(b,eocd+6)!=0||u16(b,eocd+8)!=u16(b,eocd+10)){r.errors.push_back("Multi-disk ZIP/APK archives are not supported");return r;}
  auto count=u16(b,eocd+10); auto cdSize=u32(b,eocd+12); auto pos=u32(b,eocd+16);
  if(count==0xffffu||cdSize==0xffffffffu||pos==0xffffffffu){r.errors.push_back("ZIP64 APK archives are not supported by this importer");return r;}
  if(pos>b.size()||cdSize>b.size()-pos||std::uint64_t(pos)+cdSize>eocd){r.errors.push_back("ZIP central directory bounds are invalid");return r;}
  unsigned parsedEntries=0;for(;parsedEntries<count&&pos+46<=b.size()&&u32(b,pos)==0x02014b50;++parsedEntries){
   auto flags=u16(b,pos+8);auto nl=u16(b,pos+28),el=u16(b,pos+30),cl=u16(b,pos+32);auto compressed=u32(b,pos+20),sz=u32(b,pos+24);auto method=u16(b,pos+10);auto local=u32(b,pos+42);auto crc=u32(b,pos+16);
   if(pos>b.size()||b.size()-pos<46||std::uint64_t(pos)+46+nl+el+cl>std::uint64_t(b.size())||std::uint64_t(pos)+46+nl+el+cl>std::uint64_t(u32(b,eocd+16))+cdSize){r.errors.push_back("Malformed ZIP central directory entry");return r;}
   std::string name(reinterpret_cast<const char*>(b.data()+pos+46),nl);
   r.entries.push_back({name,sz,AssetKind::Other,compressed,method,local,crc,flags});
   if(name.rfind("lib/",0)==0&&name.size()>3&&name.find(".so")!=std::string::npos){r.libraries.push_back(name);auto slash=name.find('/',4);if(slash!=std::string::npos)r.abis.push_back(name.substr(4,slash-4));}
   pos+=46+nl+el+cl;
  }
  if(parsedEntries!=count||pos!=std::uint64_t(u32(b,eocd+16))+cdSize){r.errors.push_back("ZIP central directory entry count or size does not match EOCD");return r;}
  if(ext==".apk"){
   auto manifest=std::find_if(r.entries.begin(),r.entries.end(),[](const ImportEntry&e){return e.path=="AndroidManifest.xml";});
   if(manifest==r.entries.end())r.warnings.push_back("AndroidManifest.xml not found in archive.");
   else {std::vector<unsigned char> xml;std::string why;if(extractZipEntry(b,*manifest,xml,why))parseAndroidManifest(xml,r);else r.warnings.push_back("Cannot decode AndroidManifest.xml: "+why);}
   if(r.libraries.empty())r.warnings.push_back("No native .so libraries found in the ZIP central directory.");
   std::sort(r.abis.begin(),r.abis.end());r.abis.erase(std::unique(r.abis.begin(),r.abis.end()),r.abis.end());
   r.warnings.push_back("DEX/resources are enumerated but not decompiled or reconstructed into an editable source project.");
#ifndef ANVIL_HAS_ZLIB
   r.warnings.push_back("This build has no zlib support; deflated APK entries cannot be decompressed for manifest inspection.");
#endif
  }
  return r;
 }
 if(std::filesystem::is_directory(p,ec)){
  if(std::filesystem::exists(p/"ProjectSettings/ProjectVersion.txt")){r.kind=ImportKind::UnityProject;std::ifstream v(p/"ProjectSettings/ProjectVersion.txt");std::getline(v,r.engineVersion);}
  else {
   bool unreal=std::filesystem::exists(p/"Config/DefaultEngine.ini");
   for(std::filesystem::directory_iterator it(p,ec),end;!ec&&it!=end;it.increment(ec))if(it->path().extension()==".uproject")unreal=true;
   if(unreal)r.kind=ImportKind::UnrealProject;
   else if(std::filesystem::exists(p/"project.godot"))r.kind=ImportKind::GodotProject;
   else if(std::filesystem::exists(p/"CMakeLists.txt")||std::filesystem::exists(p/"anvil.project"))r.kind=ImportKind::AnvilProject;
   else r.warnings.push_back("Directory does not match a recognized project signature.");
  }
  for(std::filesystem::recursive_directory_iterator it(p,std::filesystem::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){
   if(it->is_symlink(ec)){it.disable_recursion_pending();continue;}if(!it->is_regular_file(ec))continue;
   auto rel=std::filesystem::relative(it->path(),p,ec);if(ec)break;auto k=classify(rel);auto size=it->file_size(ec);if(ec){ec.clear();size=0;}
   r.entries.push_back({rel.generic_string(),size,k});countAsset(r,k);
  }
  if(ec)r.warnings.push_back("Some project paths could not be enumerated: "+ec.message());
  r.completeSourceAvailable=true;
  if(r.kind==ImportKind::UnityProject||r.kind==ImportKind::UnrealProject||r.kind==ImportKind::GodotProject)r.warnings.push_back("Engine-specific scenes, materials, shaders and serialized objects require format-aware conversion; listed files are not converted automatically.");
  return r;
 }
 r.warnings.push_back("Unsupported extension; no data was modified.");
 return r;
}
namespace {
bool safeArchivePath(const std::string& name,std::filesystem::path& relative){
 if(name.empty()||name.front()=='/'||name.find('\\')!=std::string::npos||name.find('\0')!=std::string::npos||name.find(':')!=std::string::npos)return false;
 std::filesystem::path p(name);
 if(p.is_absolute()||p.has_root_name()||p.has_root_directory())return false;
 for(const auto&part:p)if(part==".."||part=="."||part.empty())return false;
 relative=p;return true;
}
bool commitStaging(const std::filesystem::path& staging,const std::filesystem::path& destination,std::string& error){
 std::error_code ec;if(std::filesystem::exists(destination,ec)){error="Destination already exists; import never overwrites user data";return false;}
 std::filesystem::rename(staging,destination,ec);if(ec){error="Cannot commit import directory: "+ec.message();return false;}return true;
}
void rollbackStaging(const std::filesystem::path& staging){std::error_code ignored;std::filesystem::remove_all(staging,ignored);}
}
MigrationResult PackageImporter::importApk(const std::filesystem::path& apk,const std::filesystem::path& destination,const PackageImportOptions& options){
 MigrationResult out;out.destination=destination;std::error_code ec;
 if(lower(apk.extension().string())!=".apk"||!std::filesystem::is_regular_file(apk,ec)){out.errors.push_back("APK import requires an existing .apk file");return out;}
 auto report=ImportAnalyzer::analyze(apk);if(!report.errors.empty()){out.errors=report.errors;return out;}
 if(report.kind!=ImportKind::AndroidAPK){out.errors.push_back("Input is not a recognized Android APK ZIP package");return out;}
 if(std::filesystem::exists(destination,ec)){out.errors.push_back("Destination already exists; choose a new empty destination");return out;}
 auto fileSize=std::filesystem::file_size(apk,ec);if(ec||fileSize>options.maxArchiveBytes){out.errors.push_back("APK exceeds configured archive size limit or cannot be read");return out;}
 std::ifstream input(apk,std::ios::binary);std::vector<unsigned char> zip((std::istreambuf_iterator<char>(input)),{});if(!input.eof()&&input.fail()){out.errors.push_back("Failed to read APK bytes");return out;}
 auto staging=destination;staging += ".anvil-staging";if(std::filesystem::exists(staging,ec)){out.errors.push_back("Import staging path already exists");return out;}if(!std::filesystem::create_directories(staging,ec)||ec){out.errors.push_back("Cannot create import staging directory: "+ec.message());return out;}
 std::uint64_t total=0;bool failed=false;
 for(const auto&e:report.entries){
  const bool dex=e.path.size()>=4&&e.path.substr(e.path.size()-4)==".dex";
  const bool androidResource=e.path.rfind("res/",0)==0||e.path=="resources.arsc"||e.path=="AndroidManifest.xml";
  const bool native=e.path.rfind("lib/",0)==0&&e.path.find(".so")!=std::string::npos;
  if(((dex||androidResource)&&!options.includeDexAndResources)||(native&&!options.includeNativeLibraries)||(e.path.rfind("assets/",0)==0&&!options.includeAssets))continue;
  std::filesystem::path rel;if(!safeArchivePath(e.path,rel)){out.warnings.push_back("Skipped unsafe archive path: "+e.path);continue;}
  if(e.uncompressedSize>options.maxEntryBytes||e.uncompressedSize>options.maxTotalBytes-total){failed=true;out.errors.push_back("APK extraction limit exceeded at entry: "+e.path);break;}
  if(e.path.empty()||e.path.back()=='/')continue;
  std::vector<unsigned char> bytes;std::string why;if(!extractZipEntry(zip,e,bytes,why,options.maxEntryBytes)){failed=true;out.errors.push_back("Cannot extract "+e.path+": "+why);break;}
  auto target=staging/rel;std::filesystem::create_directories(target.parent_path(),ec);if(ec){failed=true;out.errors.push_back("Cannot create package subdirectory: "+ec.message());break;}
  if(std::filesystem::exists(target,ec)){failed=true;out.errors.push_back("Duplicate APK entry path rejected: "+e.path);break;}
  std::ofstream output(target,std::ios::binary|std::ios::trunc);if(!output||(!bytes.empty()&&!output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))){failed=true;out.errors.push_back("Cannot write extracted entry: "+e.path);break;}
  ++out.copiedFiles;out.copiedBytes+=bytes.size();total+=bytes.size();
 }
 if(!failed){std::ofstream meta(staging/"ANVIL_APK_IMPORT.txt");if(!meta){failed=true;out.errors.push_back("Cannot write APK import report");}else{meta<<"ANVIL APK IMPORT 1\nsource="<<apk.filename().generic_string()<<"\npackage="<<report.packageName<<"\nmin_sdk="<<report.minSdkVersion<<"\ntarget_sdk="<<report.targetSdkVersion<<"\nfiles="<<out.copiedFiles<<"\nbytes="<<out.copiedBytes<<"\n";for(const auto&abi:report.abis)meta<<"abi="<<abi<<"\n";for(const auto&w:report.warnings)meta<<"warning="<<w<<"\n";}}
 if(!failed){std::string why;if(!commitStaging(staging,destination,why)){failed=true;out.errors.push_back(why);}}
 if(failed){rollbackStaging(staging);out.copiedFiles=out.copiedBytes=0;return out;}
 out.warnings=report.warnings;out.warnings.push_back("APK package files were extracted byte-for-byte. DEX was not decompiled; Android resources were not converted into editable ANVIL assets.");out.success=true;return out;
}
MigrationResult PackageImporter::importWindowsExecutable(const std::filesystem::path& exe,const std::filesystem::path& destination,const PackageImportOptions& options){
 MigrationResult out;out.destination=destination;std::error_code ec;
 if(lower(exe.extension().string())!=".exe"||!std::filesystem::is_regular_file(exe,ec)){out.errors.push_back("Windows import requires an existing .exe file");return out;}
 auto report=ImportAnalyzer::analyze(exe);if(!report.errors.empty()){out.errors=report.errors;return out;}if(report.kind!=ImportKind::WindowsPE){out.errors.push_back("Input is not a valid Windows PE executable");return out;}
 if(std::filesystem::exists(destination,ec)){out.errors.push_back("Destination already exists; choose a new empty destination");return out;}
 auto staging=destination;staging += ".anvil-staging";if(std::filesystem::exists(staging,ec)){out.errors.push_back("Import staging path already exists");return out;}if(!std::filesystem::create_directories(staging,ec)||ec){out.errors.push_back("Cannot create import staging directory: "+ec.message());return out;}
 auto copyOne=[&](const std::filesystem::path& from,const std::filesystem::path& relative)->bool{
  if(!std::filesystem::is_regular_file(from,ec)||ec){ec.clear();return true;}if(std::filesystem::is_symlink(from,ec)||ec){ec.clear();out.warnings.push_back("Skipped symlink or unreadable file: "+from.filename().string());return true;}
  auto size=std::filesystem::file_size(from,ec);if(ec){ec.clear();return true;}if(size>options.maxEntryBytes||size>options.maxTotalBytes-out.copiedBytes){out.errors.push_back("Executable bundle import size limit exceeded at: "+from.filename().string());return false;}
  auto target=staging/relative;std::filesystem::create_directories(target.parent_path(),ec);if(ec){out.errors.push_back("Cannot create bundle directory: "+ec.message());return false;}
  std::filesystem::copy_file(from,target,std::filesystem::copy_options::none,ec);if(ec){out.errors.push_back("Cannot copy "+from.string()+": "+ec.message());return false;}++out.copiedFiles;out.copiedBytes+=size;return true;
 };
 bool ok=copyOne(exe,exe.filename());auto parent=exe.parent_path();
 if(ok&&out.copiedFiles==0){out.errors.push_back("Primary executable could not be copied into the bundle");ok=false;}
 if(ok&&!report.embeddedResources.empty()){
  std::ifstream peStream(exe,std::ios::binary);std::vector<unsigned char> peBytes((std::istreambuf_iterator<char>(peStream)),{});
  for(const auto&resource:report.embeddedResources){if(!ok)break;if(resource.size>options.maxEntryBytes||resource.size>options.maxTotalBytes-out.copiedBytes){out.errors.push_back("Embedded PE resource exceeds configured import limit: "+resource.path);ok=false;break;}if(resource.dataOffset>peBytes.size()||resource.size>peBytes.size()-resource.dataOffset){out.errors.push_back("Embedded PE resource points outside executable: "+resource.path);ok=false;break;}auto target=staging/resource.path;std::filesystem::create_directories(target.parent_path(),ec);if(ec){out.errors.push_back("Cannot create PE resource directory: "+ec.message());ok=false;break;}std::ofstream resourceFile(target,std::ios::binary);if(!resourceFile.write(reinterpret_cast<const char*>(peBytes.data()+resource.dataOffset),resource.size)){out.errors.push_back("Cannot extract PE resource: "+resource.path);ok=false;break;}++out.copiedFiles;out.copiedBytes+=resource.size;}
 }

 if(ok&&options.includeAdjacentDependencies){
  std::vector<std::string> wanted;for(const auto&dll:report.libraries)wanted.push_back(lower(dll));
  for(const auto&dll:wanted){if(!ok)break;auto candidate=parent/dll;if(!std::filesystem::exists(candidate,ec)){for(std::filesystem::directory_iterator it(parent,ec),end;!ec&&it!=end;it.increment(ec))if(lower(it->path().filename().string())==dll){candidate=it->path();break;}}
   if(std::filesystem::exists(candidate,ec))ok=copyOne(candidate,candidate.filename());else out.warnings.push_back("Imported dependency not found beside executable: "+dll);
  }
  // Adjacent debug symbols and configuration files are useful for diagnostics/runtime setup.
  if(ok){auto pdb=exe.parent_path()/(exe.stem().string()+".pdb");if(std::filesystem::exists(pdb,ec))ok=copyOne(pdb,pdb.filename());}
  if(ok){for(std::filesystem::directory_iterator it(parent,ec),end;!ec&&it!=end;it.increment(ec)){if(!it->is_regular_file(ec)||it->is_symlink(ec))continue;auto extension=lower(it->path().extension().string());if(extension==".ini"||extension==".cfg"||extension==".json"||extension==".xml"||extension==".toml"||extension==".yaml"||extension==".yml"){if(!copyOne(it->path(),it->path().filename())){ok=false;break;}}}}
  // Preserve common runtime data/configuration directories when they sit beside the executable.
  const std::vector<std::string> dirs={"Data","data","Assets","assets","Content","content","Plugins","plugins"};
  for(const auto&name:dirs){if(!ok)break;auto dir=parent/name;if(!std::filesystem::is_directory(dir,ec)){ec.clear();continue;}for(std::filesystem::recursive_directory_iterator it(dir,std::filesystem::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){if(it->is_symlink(ec)){it.disable_recursion_pending();continue;}if(!it->is_regular_file(ec))continue;auto rel=std::filesystem::path(name)/std::filesystem::relative(it->path(),dir,ec);if(ec){ec.clear();continue;}if(!copyOne(it->path(),rel)){ok=false;break;}}}
 }
 if(ok){std::ofstream meta(staging/"ANVIL_WINDOWS_IMPORT.txt");if(!meta){ok=false;out.errors.push_back("Cannot write executable import report");}else{meta<<"ANVIL WINDOWS PE IMPORT 1\nsource="<<exe.filename().generic_string()<<"\narchitecture="<<report.architecture<<"\nmachine="<<report.machine<<"\nfiles="<<out.copiedFiles<<"\nbytes="<<out.copiedBytes<<"\n";for(const auto&dll:report.libraries)meta<<"import="<<dll<<"\n";for(const auto&api:report.graphicsApis)meta<<"graphics_api="<<api<<"\n";for(const auto&w:report.warnings)meta<<"warning="<<w<<"\n";}}
 if(ok){std::string why;if(!commitStaging(staging,destination,why)){ok=false;out.errors.push_back(why);}}
 if(!ok){rollbackStaging(staging);out.copiedFiles=out.copiedBytes=0;return out;}
 out.warnings=report.warnings;out.warnings.push_back("This is a redistributable file snapshot, not recovered source code. System DLLs, delay-loaded dependencies, registry state, installers and external data may still be required.");out.success=true;return out;
}
MigrationResult MigrationWizard::migrateDirectory(const std::filesystem::path& source,const std::filesystem::path& destination,const MigrationOptions& options){
 MigrationResult out;out.destination=destination;std::error_code ec;
 if(!std::filesystem::is_directory(source,ec)){out.errors.push_back("Migration source must be an existing project directory");return out;}
 auto report=ImportAnalyzer::analyze(source);if(!report.errors.empty()){out.errors=report.errors;return out;}
 if(report.kind==ImportKind::Unknown){out.errors.push_back("Source directory is not a recognized project");return out;}
 if(std::filesystem::exists(destination,ec)){out.errors.push_back("Migration destination already exists");return out;}
 auto staging=destination;staging += ".anvil-staging";if(std::filesystem::exists(staging,ec)){out.errors.push_back("Migration staging directory already exists");return out;}
 if(!std::filesystem::create_directories(staging,ec)||ec){out.errors.push_back("Cannot create migration staging directory: "+ec.message());return out;}
 bool failed=false;
 for(const auto&e:report.entries){auto rel=std::filesystem::path(e.path);if(rel.empty()||rel.is_absolute()){out.warnings.push_back("Skipped unsafe relative path: "+e.path);continue;}for(const auto&part:rel)if(part==".."||part=="."){failed=true;out.errors.push_back("Unsafe project path rejected: "+e.path);break;}if(failed)break;
  if(!selected(e.assetKind,options))continue;
  auto from=source/rel,to=staging/"Imported"/rel;std::filesystem::create_directories(to.parent_path(),ec);if(ec){failed=true;out.errors.push_back("Cannot create asset directory: "+ec.message());break;}
  if(std::filesystem::is_symlink(from,ec)||ec){ec.clear();out.warnings.push_back("Skipped a symlink or unreadable source file: "+e.path);continue;}
  auto size=std::filesystem::file_size(from,ec);if(ec){ec.clear();out.warnings.push_back("Skipped unreadable file: "+e.path);continue;}
  std::filesystem::copy_file(from,to,std::filesystem::copy_options::none,ec);if(ec){failed=true;out.errors.push_back("Copy failed for "+e.path+": "+ec.message());break;}++out.copiedFiles;out.copiedBytes+=size;
 }
 if(!failed){std::ofstream meta(staging/"migration.report");if(!meta){failed=true;out.errors.push_back("Cannot write migration report");}else meta<<"ANVIL_MIGRATION 1\nsource="<<source.generic_string()<<"\nsource_kind="<<static_cast<int>(report.kind)<<"\ncopied_files="<<out.copiedFiles<<"\ncopied_bytes="<<out.copiedBytes<<"\n";}
 if(failed){std::filesystem::remove_all(staging,ec);out.copiedFiles=0;out.copiedBytes=0;return out;}
 std::filesystem::rename(staging,destination,ec);if(ec){out.errors.push_back("Cannot commit migration: "+ec.message());std::filesystem::remove_all(staging,ec);out.copiedFiles=0;out.copiedBytes=0;return out;}
 if(report.sceneCount){if(!options.copyScenes)out.warnings.push_back("Scene/level files were detected but not copied because scene conversion is not implemented.");else out.warnings.push_back("Scene/level files were copied as opaque source files; no scene conversion was performed.");}
 out.warnings.insert(out.warnings.end(),report.warnings.begin(),report.warnings.end());out.success=true;return out;
}
}

#include "anvil/Import.h"
#include "anvil/ProjectHub.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <array>
#include <tuple>
#ifdef ANVIL_HAS_ZLIB
#include <zlib.h>
#endif

static void put16(std::vector<unsigned char>&b,std::size_t p,std::uint16_t v){b[p]=static_cast<unsigned char>(v);b[p+1]=static_cast<unsigned char>(v>>8);}
static void put32(std::vector<unsigned char>&b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b[p+i]=static_cast<unsigned char>(v>>(i*8));}
static void append16(std::vector<unsigned char>&b,std::uint16_t v){auto p=b.size();b.resize(p+2);put16(b,p,v);}
static void append32(std::vector<unsigned char>&b,std::uint32_t v){auto p=b.size();b.resize(p+4);put32(b,p,v);}
static std::uint32_t portableCrc32(const unsigned char* data,std::size_t size){std::uint32_t crc=0xffffffffu;for(std::size_t i=0;i<size;++i){crc^=data[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&(~(crc&1u)+1u));}return ~crc;}
static std::vector<unsigned char> makeAxml(){
 std::vector<std::string> ss={"manifest","package","com.example.anvil","uses-permission","name","android.permission.INTERNET","uses-sdk","minSdkVersion","targetSdkVersion"};
 std::vector<unsigned char> pool(28+ss.size()*4);put16(pool,0,1);put16(pool,2,28);put32(pool,8,static_cast<std::uint32_t>(ss.size()));put32(pool,16,0x100);put32(pool,20,static_cast<std::uint32_t>(pool.size()));
 // Rebuild offsets and UTF-8 strings after the offset table.
 for(std::size_t i=0;i<ss.size();++i){put32(pool,28+i*4,static_cast<std::uint32_t>(pool.size()- (28+ss.size()*4)));auto n=ss[i].size();pool.push_back(static_cast<unsigned char>(n));pool.push_back(static_cast<unsigned char>(n));pool.insert(pool.end(),ss[i].begin(),ss[i].end());pool.push_back(0);}
 put32(pool,4,static_cast<std::uint32_t>(pool.size()));
 auto tag=[&](std::uint32_t name,const std::vector<std::array<std::uint32_t,3>>&attrs){std::vector<unsigned char>x(36+attrs.size()*20);put16(x,0,0x0102);put16(x,2,16);put32(x,4,static_cast<std::uint32_t>(x.size()));put32(x,20,name);put16(x,24,20);put16(x,26,20);put16(x,28,static_cast<std::uint16_t>(attrs.size()));for(std::size_t i=0;i<attrs.size();++i){auto a=36+i*20;put32(x,a,0xffffffffu);put32(x,a+4,attrs[i][0]);put32(x,a+8,attrs[i][1]);put16(x,a+12,8);x[a+14]=0;x[a+15]=static_cast<unsigned char>(attrs[i][2]>>24);put32(x,a+16,attrs[i][2]&0x00ffffffu);}return x;};
 auto manifest=tag(0,{{1,2,0x03000002}});auto permission=tag(3,{{4,5,0x03000005}});auto sdk=tag(6,{{7,0xffffffffu,0x10000021},{8,0xffffffffu,0x10000023}});
 std::vector<unsigned char> xml(8);put16(xml,0,3);put16(xml,2,8);xml.insert(xml.end(),pool.begin(),pool.end());xml.insert(xml.end(),manifest.begin(),manifest.end());xml.insert(xml.end(),permission.begin(),permission.end());xml.insert(xml.end(),sdk.begin(),sdk.end());auto end=xml.size();xml.resize(end+8);put16(xml,end,0x0103);put16(xml,end+2,8);put32(xml,end+4,8);put32(xml,4,static_cast<std::uint32_t>(xml.size()));return xml;
}
static std::vector<unsigned char> makeZip(const std::vector<std::tuple<std::string,std::vector<unsigned char>,bool>>&files){std::vector<unsigned char>z;struct C{std::string name;std::uint32_t local,compressed,uncompressed,crc;std::uint16_t method;};std::vector<C> centralEntries;for(const auto&f:files){const auto&name=std::get<0>(f);const auto&data=std::get<1>(f);std::vector<unsigned char>payload=data;std::uint16_t method=0;
#ifdef ANVIL_HAS_ZLIB
 if(std::get<2>(f)){payload.resize(compressBound(static_cast<uLong>(data.size())));z_stream stream{};stream.next_in=const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));stream.avail_in=static_cast<uInt>(data.size());stream.next_out=reinterpret_cast<Bytef*>(payload.data());stream.avail_out=static_cast<uInt>(payload.size());assert(deflateInit2(&stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY)==Z_OK);assert(deflate(&stream,Z_FINISH)==Z_STREAM_END);payload.resize(stream.total_out);deflateEnd(&stream);method=8;}
#endif
 auto local=static_cast<std::uint32_t>(z.size());append32(z,0x04034b50);append16(z,20);append16(z,0);append16(z,method);append16(z,0);append16(z,0);
 auto crc=portableCrc32(data.data(),data.size());
 append32(z,crc);append32(z,static_cast<std::uint32_t>(payload.size()));append32(z,static_cast<std::uint32_t>(data.size()));append16(z,static_cast<std::uint16_t>(name.size()));append16(z,0);z.insert(z.end(),name.begin(),name.end());z.insert(z.end(),payload.begin(),payload.end());centralEntries.push_back({name,local,static_cast<std::uint32_t>(payload.size()),static_cast<std::uint32_t>(data.size()),crc,method});}auto cdStart=static_cast<std::uint32_t>(z.size());for(const auto&e:centralEntries){append32(z,0x02014b50);append16(z,20);append16(z,20);append16(z,0);append16(z,e.method);append16(z,0);append16(z,0);append32(z,e.crc);append32(z,e.compressed);append32(z,e.uncompressed);append16(z,static_cast<std::uint16_t>(e.name.size()));append16(z,0);append16(z,0);append16(z,0);append16(z,0);append32(z,0);append32(z,e.local);z.insert(z.end(),e.name.begin(),e.name.end());}auto cdSize=static_cast<std::uint32_t>(z.size())-cdStart;append32(z,0x06054b50);append16(z,0);append16(z,0);append16(z,static_cast<std::uint16_t>(centralEntries.size()));append16(z,static_cast<std::uint16_t>(centralEntries.size()));append32(z,cdSize);append32(z,cdStart);append16(z,0);return z;}

int main(){
 namespace fs=std::filesystem; auto root=fs::temp_directory_path()/"anvil_m104_m105_test";
 std::error_code ec; fs::remove_all(root,ec); fs::create_directories(root);
 anvil::ProjectConfig cfg; cfg.name="Regression Project"; cfg.target=anvil::TargetPlatform::Android; cfg.targets={anvil::TargetPlatform::Windows,anvil::TargetPlatform::Android};
 std::string error; assert(anvil::ProjectHub::create(root/"project",cfg,&error));
 auto projects=anvil::ProjectHub::discover(root); assert(projects.size()==1); assert(projects[0].name==cfg.name);assert(projects[0].target.find("Windows")!=std::string::npos&&projects[0].target.find("Android")!=std::string::npos);
 assert(anvil::ProjectHub::validate(root/"project"));assert(fs::exists(root/"project/Config/Platforms/Windows.ini")&&fs::exists(root/"project/Config/Platforms/Android.ini"));auto recents=root/"recent.txt";assert(anvil::ProjectHub::addRecent(recents,root/"project"));assert(anvil::ProjectHub::recent(recents).size()==1);
 assert(!anvil::ProjectHub::create(root/"project",cfg,&error));
 auto exe=root/"sample.exe"; std::vector<unsigned char>b(256); b[0]='M';b[1]='Z'; b[0x3c]=0x40;
 b[0x40]='P';b[0x41]='E'; b[0x44]=0x64;b[0x45]=0x86;
 std::ofstream f(exe,std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()),b.size()); f.close();
 // A bounded synthetic PE image with one import descriptor (d3d12.dll).
 b.assign(0x400,0);b[0]='M';b[1]='Z';b[0x3c]=0x40;b[0x40]='P';b[0x41]='E';b[0x44]=0x64;b[0x45]=0x86;b[0x46]=1;b[0x54]=0xe0;b[0x58]=0x0b;b[0x59]=0x02;
 const std::size_t section=0x40+24+0xe0;b[section+8]=0x00;b[section+9]=0x02;b[section+12]=0x00;b[section+13]=0x10;b[section+16]=0x00;b[section+17]=0x02;b[section+20]=0x00;b[section+21]=0x02;
 const std::size_t importDir=0x40+24+112;b[importDir+8]=0x00;b[importDir+9]=0x10;b[importDir+12]=0x28;
 b[0x20c]=0x30;b[0x20d]=0x10;const char dll[]="d3d12.dll";std::copy(dll,dll+sizeof(dll),b.begin()+0x230);
 // Minimal PE resource tree: RT_RCDATA / ID 1 / language 1033 -> four raw bytes.
 b[importDir+16]=0x40;b[importDir+17]=0x10;b[importDir+20]=0x80;
 b[0x24e]=1; b[0x250]=10; b[0x254]=0x20;b[0x257]=0x80;
 b[0x26e]=1; b[0x270]=1; b[0x274]=0x40;b[0x277]=0x80;
 b[0x28e]=1; b[0x290]=0x09;b[0x291]=0x04;b[0x294]=0x60;
 b[0x2a0]=0x80;b[0x2a1]=0x11;b[0x2a4]=4;
 b[0x380]='D';b[0x381]='A';b[0x382]='T';b[0x383]='A';
 std::ofstream pe(exe,std::ios::binary|std::ios::trunc);pe.write(reinterpret_cast<const char*>(b.data()),b.size());pe.close();
 auto report=anvil::ImportAnalyzer::analyze(exe); assert(report.kind==anvil::ImportKind::WindowsPE); assert(report.architecture=="x86_64");assert(!report.libraries.empty()&&report.libraries[0]=="d3d12.dll");assert(!report.graphicsApis.empty()&&report.graphicsApis[0]=="Direct3D");assert(report.embeddedResources.size()==1&&report.embeddedResources[0].size==4);
 std::ofstream(root/"d3d12.dll",std::ios::binary)<<"DLL";fs::create_directories(root/"Assets");std::ofstream(root/"Assets/data.bin",std::ios::binary)<<"ASSET";auto exeDest=root/"exe_import";auto exeImport=anvil::PackageImporter::importWindowsExecutable(exe,exeDest);assert(exeImport.success&&fs::exists(exeDest/"sample.exe"));assert(fs::exists(exeDest/"d3d12.dll"));assert(fs::exists(exeDest/"Assets/data.bin"));assert(fs::exists(exeDest/"Resources/PE/type_10_name_1_lang_1033.bin"));
 auto missing=anvil::ImportAnalyzer::analyze(root/"missing.apk"); assert(!missing.errors.empty());
 auto apk=root/"sample.apk";auto zip=makeZip({{"AndroidManifest.xml",makeAxml(),true},{"lib/arm64-v8a/libgame.so",{'S','O'},false},{"assets/data.bin",{'A','S','S','E','T'},true},{"../escaped.txt",{'X'},false}});std::ofstream apkOut(apk,std::ios::binary);apkOut.write(reinterpret_cast<const char*>(zip.data()),zip.size());apkOut.close();auto apkReport=anvil::ImportAnalyzer::analyze(apk);assert(apkReport.kind==anvil::ImportKind::AndroidAPK);
 auto apkDest=root/"apk_import";auto apkImport=anvil::PackageImporter::importApk(apk,apkDest);assert(apkImport.success&&apkImport.copiedFiles==3);assert(!fs::exists(root/"escaped.txt"));assert(fs::exists(apkDest/"AndroidManifest.xml"));assert(fs::exists(apkDest/"lib/arm64-v8a/libgame.so"));assert(fs::exists(apkDest/"assets/data.bin"));anvil::PackageImportOptions noAssets;noAssets.includeAssets=false;auto apkNoAssets=anvil::PackageImporter::importApk(apk,root/"apk_no_assets",noAssets);assert(apkNoAssets.success&&apkNoAssets.copiedFiles==2&&!fs::exists(root/"apk_no_assets/assets/data.bin"));
 auto corruptZip=zip;auto payloadPos=std::search(corruptZip.begin(),corruptZip.end(),std::begin("SO") ,std::end("SO")-1);assert(payloadPos!=corruptZip.end());*payloadPos='X';auto corruptApk=root/"corrupt.apk";std::ofstream corruptOut(corruptApk,std::ios::binary);corruptOut.write(reinterpret_cast<const char*>(corruptZip.data()),corruptZip.size());corruptOut.close();auto corruptResult=anvil::PackageImporter::importApk(corruptApk,root/"corrupt_import");assert(!corruptResult.success&&!fs::exists(root/"corrupt_import"));assert(apkReport.packageName=="com.example.anvil");assert(apkReport.permissions.size()==1&&apkReport.permissions[0]=="android.permission.INTERNET");assert(apkReport.minSdkVersion==33&&apkReport.targetSdkVersion==35);assert(apkReport.abis.size()==1&&apkReport.abis[0]=="arm64-v8a");
 // Project scanner classifies concrete assets and migration copies only selected supported files.
 auto unity=root/"UnityGame";fs::create_directories(unity/"ProjectSettings");std::ofstream(unity/"ProjectSettings/ProjectVersion.txt")<<"2022.3.10f1\n";
 fs::create_directories(unity/"Assets/Textures");std::ofstream(unity/"Assets/Textures/test.png",std::ios::binary)<<"PNGDATA";
 fs::create_directories(unity/"Assets/Models");std::ofstream(unity/"Assets/Models/test.obj")<<"o test\n";
 fs::create_directories(unity/"Assets/Scenes");std::ofstream(unity/"Assets/Scenes/main.unity")<<"serialized scene";
 auto scan=anvil::ImportAnalyzer::analyze(unity);assert(scan.kind==anvil::ImportKind::UnityProject);assert(scan.textureCount==1&&scan.modelCount==1&&scan.sceneCount==1);assert(scan.engineVersion=="2022.3.10f1");
 auto migrated=root/"Migrated";auto migration=anvil::MigrationWizard::migrateDirectory(unity,migrated);assert(migration.success&&migration.copiedFiles==2);assert(fs::exists(migrated/"Imported/Assets/Textures/test.png"));assert(!fs::exists(migrated/"Imported/Assets/Scenes/main.unity"));
 auto failed=anvil::MigrationWizard::migrateDirectory(unity,migrated);assert(!failed.success);
 fs::remove_all(root,ec); std::cout<<"Import/ProjectHub tests passed\n";
}

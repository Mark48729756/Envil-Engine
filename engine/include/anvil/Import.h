#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace anvil {
enum class ImportKind { Unknown, AndroidAPK, WindowsPE, UnityProject, UnrealProject, GodotProject, AnvilProject, ZipArchive };
enum class AssetKind { Texture, Model, Audio, Animation, Material, Scene, Other };
struct ImportEntry { std::string path; std::uint64_t uncompressedSize=0; AssetKind assetKind=AssetKind::Other; std::uint64_t compressedSize=0; std::uint16_t compressionMethod=0; std::uint32_t localHeaderOffset=0; std::uint32_t crc32=0; std::uint16_t generalPurposeFlags=0; };
struct EmbeddedResource { std::string path; std::uint32_t dataOffset=0; std::uint32_t size=0; std::uint32_t typeId=0; std::uint32_t nameId=0; std::uint32_t languageId=0; };
struct ImportReport {
 ImportKind kind=ImportKind::Unknown; std::filesystem::path source;
 std::string architecture, packageName, engineVersion;
 std::vector<std::string> abis; int minSdkVersion=0,targetSdkVersion=0;
 std::uint16_t machine=0; std::uint32_t timestamp=0;
 std::vector<std::string> permissions, libraries, graphicsApis, warnings, errors;
 std::vector<ImportEntry> entries; std::vector<EmbeddedResource> embeddedResources;
 std::uint64_t textureCount=0,modelCount=0,audioCount=0,animationCount=0,materialCount=0,sceneCount=0;
 bool completeSourceAvailable=false;
};
struct MigrationOptions { bool copyTextures=true,copyModels=true,copyAudio=true,copyAnimations=true,copyMaterials=true,copyScenes=false; };
struct MigrationResult { bool success=false; std::filesystem::path destination; std::uint64_t copiedFiles=0,copiedBytes=0; std::vector<std::string> warnings,errors; };
struct PackageImportOptions {
 std::uint64_t maxTotalBytes=4ull*1024*1024*1024;
 std::uint64_t maxArchiveBytes=4ull*1024*1024*1024;
 std::uint64_t maxEntryBytes=512ull*1024*1024;
 bool includeNativeLibraries=true;
 bool includeDexAndResources=true;
 bool includeAssets=true;
 bool includeAdjacentDependencies=true;
};
class ImportAnalyzer { public: static ImportReport analyze(const std::filesystem::path& path); };
class PackageImporter {
public:
 // APK import extracts the original package tree; it does not decompile DEX or rebuild source code.
 static MigrationResult importApk(const std::filesystem::path& apk,const std::filesystem::path& destination,const PackageImportOptions& options={});
 // EXE import creates a runnable distribution snapshot from the EXE and discoverable sibling files.
 // It does not recover source code or dependencies that are absent from the source directory.
 static MigrationResult importWindowsExecutable(const std::filesystem::path& exe,const std::filesystem::path& destination,const PackageImportOptions& options={});
};
class MigrationWizard { public: static MigrationResult migrateDirectory(const std::filesystem::path& source,const std::filesystem::path& destination,const MigrationOptions& options={}); };
}

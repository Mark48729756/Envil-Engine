#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace anvil {
enum class TargetPlatform { Windows, Android, IOS, PlayStation, Xbox, NintendoSwitch };
struct ProjectConfig { std::string name="NewProject"; TargetPlatform target=TargetPlatform::Windows; std::vector<TargetPlatform> targets; std::string templateName="3D"; };
struct ProjectRecord { std::string name; std::filesystem::path path; std::string templateName,target; };
class ProjectHub {
public:
 static bool create(const std::filesystem::path& directory,const ProjectConfig& config,std::string* error=nullptr);
 static std::vector<ProjectRecord> discover(const std::filesystem::path& root);
 static bool validate(const std::filesystem::path& project,std::string* error=nullptr);
 static bool addRecent(const std::filesystem::path& recentFile,const std::filesystem::path& project,std::string* error=nullptr);
 static std::vector<ProjectRecord> recent(const std::filesystem::path& recentFile);
 static std::vector<std::string> templates();
 static const char* platformName(TargetPlatform platform);
};
}

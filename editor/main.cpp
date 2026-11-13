// ANVIL Editor - Milestone 2
// Unity-style docking editor: Hierarchy / Viewport / Inspector / Project / Console / Render Settings,
// SVG icons (rasterized with nanosvg), move/rotate/scale gizmos, click-picking, Play mode with scene snapshot/restore.
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <anvil/Application.h>
#include <anvil/Core.h>
#include <anvil/Scene.h>
#include <anvil/SceneSerializer.h>
#include <anvil/Cutscene.h>
#include <anvil/OpenWorld.h>
#include <anvil/RoadNetwork.h>
#include <unordered_set>
#include <future>
#include <chrono>
#include <map>
#include <memory>
#include <unordered_map>
#include <anvil/Texture2D.h>
#include <anvil/AssetDatabase.h>
#include <anvil/ProjectHub.h>
#include <anvil/Import.h>
#include <anvil/Controller.h>
#include <anvil/UI.h>
#include "Icons.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

using namespace anvil;
namespace fs = std::filesystem;

static std::vector<std::string> gConsole;
static void consoleSink(const char* s) {
    gConsole.emplace_back(s);
    if (gConsole.size() > 2000) gConsole.erase(gConsole.begin(), gConsole.begin() + 500);
}

static ImVec4 hex(unsigned rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, a);
}
static ImTextureID texId(unsigned t) { return (ImTextureID)(intptr_t)t; }

static void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();

    // Neutral graphite editor theme inspired by professional DCC/game editors.
    // Keep the interface low-contrast; reserve color for semantic status only.
    s.WindowRounding = 0.0f;
    s.ChildRounding = 2.0f;
    s.FrameRounding = 2.0f;
    s.PopupRounding = 2.0f;
    s.GrabRounding = 2.0f;
    s.TabRounding = 2.0f;
    s.ScrollbarRounding = 4.0f;

    s.WindowPadding = ImVec2(8.0f, 8.0f);
    s.FramePadding = ImVec2(6.0f, 4.0f);
    s.ItemSpacing = ImVec2(8.0f, 5.0f);
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;
    s.TabBarBorderSize = 1.0f;
    s.IndentSpacing = 14.0f;
    s.ScrollbarSize = 12.0f;
    s.SeparatorTextBorderSize = 1.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = hex(0xD0D0D0);
    c[ImGuiCol_TextDisabled]          = hex(0x858585);
    c[ImGuiCol_WindowBg]              = hex(0x242424);
    c[ImGuiCol_ChildBg]               = hex(0x202020);
    c[ImGuiCol_PopupBg]               = hex(0x292929, 0.98f);
    c[ImGuiCol_Border]                = hex(0x3A3A3A);
    c[ImGuiCol_BorderShadow]          = hex(0x000000, 0.0f);

    c[ImGuiCol_FrameBg]               = hex(0x343434);
    c[ImGuiCol_FrameBgHovered]        = hex(0x414141);
    c[ImGuiCol_FrameBgActive]         = hex(0x494949);
    c[ImGuiCol_TitleBg]               = hex(0x1D1D1D);
    c[ImGuiCol_TitleBgActive]         = hex(0x292929);
    c[ImGuiCol_TitleBgCollapsed]      = hex(0x1D1D1D);
    c[ImGuiCol_MenuBarBg]             = hex(0x292929);

    c[ImGuiCol_ScrollbarBg]           = hex(0x1D1D1D);
    c[ImGuiCol_ScrollbarGrab]         = hex(0x484848);
    c[ImGuiCol_ScrollbarGrabHovered]  = hex(0x5A5A5A);
    c[ImGuiCol_ScrollbarGrabActive]   = hex(0x686868);

    c[ImGuiCol_CheckMark]             = hex(0xC0C0C0);
    c[ImGuiCol_SliderGrab]            = hex(0x929292);
    c[ImGuiCol_SliderGrabActive]      = hex(0xB0B0B0);
    c[ImGuiCol_Button]                = hex(0x383838);
    c[ImGuiCol_ButtonHovered]         = hex(0x484848);
    c[ImGuiCol_ButtonActive]          = hex(0x555555);

    c[ImGuiCol_Header]                = hex(0x414141);
    c[ImGuiCol_HeaderHovered]         = hex(0x4B4B4B);
    c[ImGuiCol_HeaderActive]          = hex(0x555555);
    c[ImGuiCol_Separator]             = hex(0x3A3A3A);
    c[ImGuiCol_SeparatorHovered]      = hex(0x666666);
    c[ImGuiCol_SeparatorActive]       = hex(0x777777);

    c[ImGuiCol_ResizeGrip]            = hex(0x555555, 0.25f);
    c[ImGuiCol_ResizeGripHovered]     = hex(0x777777, 0.65f);
    c[ImGuiCol_ResizeGripActive]      = hex(0x999999, 0.85f);

    c[ImGuiCol_Tab]                   = hex(0x292929);
    c[ImGuiCol_TabHovered]            = hex(0x484848);
    c[ImGuiCol_TabSelected]           = hex(0x3A3A3A);
    c[ImGuiCol_TabSelectedOverline]   = hex(0xA0A0A0);
    c[ImGuiCol_TabDimmed]             = hex(0x222222);
    c[ImGuiCol_TabDimmedSelected]     = hex(0x303030);
    c[ImGuiCol_DockingPreview]        = hex(0xA0A0A0, 0.22f);
    c[ImGuiCol_DockingEmptyBg]        = hex(0x181818);

    c[ImGuiCol_NavCursor]             = hex(0xA0A0A0, 0.65f);
    c[ImGuiCol_NavWindowingHighlight] = hex(0xD0D0D0, 0.65f);
    c[ImGuiCol_NavWindowingDimBg]     = hex(0x000000, 0.35f);
    c[ImGuiCol_ModalWindowDimBg]      = hex(0x000000, 0.55f);
}

class Editor : public Application {
public:
    Editor() : Application(1600, 900, "ANVIL Editor") {}
    ~Editor() override {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        icons::shutdown();
        logSink() = nullptr;
    }

protected:
    // ------------------------------------------------------------------ lifecycle
    void onInit() override {
        logSink() = consoleSink;
        icons::init();
        renderer_ = std::make_unique<Renderer>(1280, 720);
        renderer_->presentToScreen = false;
        controllers_ = std::make_unique<ControllerSystem>(window_.native());
        buildDefaultUIDocument();
        scene_.initGpuResources(ANVIL_ROOT_DIR);
        assetDb_.scan(ANVIL_ROOT_DIR);
        cam_.position = {0.0f, 4.5f, 16.0f};
        cam_.pitch = -12.0f;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        io.IniFilename = "anvil_editor.ini";
        ImGui_ImplGlfw_InitForOpenGL(window_.native(), true);
        ImGui_ImplOpenGL3_Init("#version 450");
        {
            const char* cands[] = {ANVIL_ROOT_DIR "/assets/fonts/LiberationSans-Regular.ttf", "assets/fonts/LiberationSans-Regular.ttf",
                                   "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
            for (const char* c : cands) if (fs::exists(c)) { if (io.Fonts->AddFontFromFileTTF(c, 15.0f)) break; }
        }
        applyTheme();
        ImGui::GetStyle().FontSizeBase = 15.0f;

        if (const char* n = std::getenv("ANVIL_SELECT")) for (Entity& e : scene_.entities) if (e.name == n) selected_ = e.id;   // test hook
        float az, el;
        sunToAngles(az, el);
        sunAz_ = az; sunEl_ = el;
        ANVIL_LOG("ANVIL Editor ready. RMB + WASD to fly, LMB to select, drag gizmo axes to transform.");
    }

    void onUpdate(float dt) override {
        frameMs_ = frameMs_ * 0.95f + dt * 1000.0f * 0.05f;
        updateCutscene(dt);
        updateWorldStreaming();
        updateTraffic(dt);
        if (playing_ && !paused_) {
            playTime_ += dt;
            scene_.updateOrbits(playTime_);
            scene_.stepPhysics(dt);
        }
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) gizmoMode_ = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) gizmoMode_ = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) gizmoMode_ = 2;
            if (selected_ && ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelected();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) saveProjectScene();
            if (selected_ && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) selected_ = scene_.duplicate(selected_).id;
            if (selected_ && ImGui::IsKeyPressed(ImGuiKey_F)) focusSelected();
        }
        ++frameNo_;
        if (const char* a = std::getenv("ANVIL_AUTOEXIT")) { if (frameNo_ > std::atoi(a) * 10) window_.close(); }
    }

    void onRender() override {
        // keep the render target the size of the Viewport panel (UI built last frame decided it)
        int vw = std::max(16, (int)vpSize_.x), vh = std::max(16, (int)vpSize_.y);
        if (vw != renderer_->width() || vh != renderer_->height()) renderer_->resize(vw, vh);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawUI();
        ImGui::Render();

        scene_.collect(items_, lights_);
        addFarProxies();
        addRoadAndTrafficItems();
        renderer_->render(cam_, items_, lights_, scene_.sun);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, window_.width(), window_.height());
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (const char* shot = std::getenv("ANVIL_SHOT")) {
            if (frameNo_ == 20) {
                int w = window_.width(), h = window_.height();
                std::vector<unsigned char> px((size_t)w * h * 3);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
                if (FILE* f = std::fopen(shot, "wb")) {
                    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
                    for (int y = h - 1; y >= 0; --y) std::fwrite(&px[(size_t)y * w * 3], 1, (size_t)w * 3, f);
                    std::fclose(f);
                }
                window_.close();
            }
        }
    }

private:
    void drawControllerDebugger() {
        if (!showControllerDebugger_ || !controllers_) return;
        if (!ImGui::Begin("Controller Debugger", &showControllerDebugger_)) { ImGui::End(); return; }
        const auto devices=controllers_->connected();
        if(captureDeviceId_>=0){
            const auto captured=controllers_->pollInputCapture();
            if(captured.kind!=ControllerCaptureKind::None){
                const int target=captureTargetLogical_[captureDeviceId_];
                if(captured.kind==ControllerCaptureKind::Button){
                    for(int physical=0;physical<64;++physical)
                        if(controllers_->buttonMapping(captureDeviceId_,physical)==target)
                            controllers_->setButtonMapping(captureDeviceId_,physical,-1);
                    controllers_->setButtonMapping(captureDeviceId_,captured.index,target);
                    controllerCaptureStatus_="Captured physical button "+std::to_string(captured.index)+" for "+ControllerSystem::buttonName(target);
                } else {
                    if(controllers_->beginInputCapture(captureDeviceId_))
                        controllerCaptureStatus_="Axis movement ignored for button binding; press a button instead";
                    else {controllerCaptureStatus_="Capture ended";captureDeviceId_=-1;}
                }
                if(captured.kind==ControllerCaptureKind::Button)captureDeviceId_=-1;
            } else if(!controllers_->inputCapturePending()){
                controllerCaptureStatus_="Capture ended (device disconnected)";
                captureDeviceId_=-1;
            }
        }
        if(devices.empty())ImGui::TextDisabled("No game controllers detected. Connect a USB or Bluetooth controller and refresh.");
        if(!controllerCaptureStatus_.empty())ImGui::TextWrapped("%s",controllerCaptureStatus_.c_str());
        for(const auto&d:devices){
            ImGui::PushID(d.id);
            if(ImGui::CollapsingHeader((d.name+"  ["+std::to_string(d.id)+"]").c_str(),ImGuiTreeNodeFlags_DefaultOpen)){
                ImGui::Text("GUID: %s",d.guid.c_str());ImGui::Text("Mapping: %s",d.mapping.empty()?"Raw HID/joystick":d.mapping.c_str());
                ImGui::Text("Physical controls: %d buttons, %d axes",d.buttonCount,d.axisCount);
                ImGui::TextDisabled("Transport, VID/PID and battery are not exposed by the active GLFW API.");
                auto state=controllers_->state(d.id);
                ImGui::Text("Buttons");
                for(int b=0;b<std::min(d.buttonCount,64);++b){bool down=b<15?state.buttons[static_cast<std::size_t>(b)]:state.physicalButtons[static_cast<std::size_t>(b)];ImGui::PushID(b);ImGui::Checkbox(b<15?ControllerSystem::buttonName(b):("Button "+std::to_string(b)).c_str(),&down);ImGui::PopID();if(b%4!=3&&b+1<d.buttonCount)ImGui::SameLine();}
                ImGui::Separator();ImGui::Text("Button bindings");
                for(int logical=0;logical<15;++logical){
                    int physical=-1;
                    for(int candidate=0;candidate<64;++candidate)
                        if(controllers_->buttonMapping(d.id,candidate)==logical){physical=candidate;break;}
                    ImGui::Text("%s  <-  %s",ControllerSystem::buttonName(logical),physical>=0?("Button "+std::to_string(physical)).c_str():"Unbound");
                    ImGui::SameLine();ImGui::PushID(logical);
                    if(ImGui::SmallButton("Rebind")){
                        if(controllers_->beginInputCapture(d.id)){captureDeviceId_=d.id;captureTargetLogical_[d.id]=logical;controllerCaptureStatus_="Press a controller button or move an axis...";}
                        else controllerCaptureStatus_="Could not start capture: controller is unavailable";
                    }
                    ImGui::PopID();
                }
                if(captureDeviceId_==d.id&&ImGui::SmallButton("Cancel capture")){controllers_->cancelInputCapture();captureDeviceId_=-1;controllerCaptureStatus_="Capture cancelled";}
                if(ImGui::Button("Reset button mappings"))controllers_->resetButtonMappings(d.id);
                ImGui::Separator();ImGui::Text("Axes");
                for(int a=0;a<std::min(d.axisCount,16);++a){
                    float v=a<6?state.axes[static_cast<std::size_t>(a)]:state.physicalAxes[static_cast<std::size_t>(a)];
                    ImGui::PushID(a);ImGui::Text("Axis %d",a);ImGui::SameLine(80);ImGui::ProgressBar((v+1.0f)*0.5f,ImVec2(-1,0)," ");
                    AxisSettings settings=controllers_->axisSettings(d.id,a);bool changed=false;
                    changed|=ImGui::SliderFloat("Deadzone",&settings.deadzone,0.0f,0.95f,"%.2f");
                    changed|=ImGui::SliderFloat("Sensitivity",&settings.sensitivity,0.0f,4.0f,"%.2f");
                    changed|=ImGui::Checkbox("Invert axis",&settings.inverted);
                    if(changed)controllers_->setAxisSettings(d.id,a,settings);
                    ImGui::PopID();
                }
                if(ImGui::Button("Save controller profile")){
                    std::string safe=d.guid;for(char&ch:safe)if(!std::isalnum(static_cast<unsigned char>(ch)))ch='_';
                    const fs::path folder=fs::current_path()/"ControllerProfiles";std::error_code ec;fs::create_directories(folder,ec);
                    std::string error;if(!ec&&controllers_->saveProfile(folder/(safe+".profile"),d.guid,&error))controllerCaptureStatus_="Profile saved: "+(safe+".profile");else controllerCaptureStatus_="Profile save failed: "+(ec?ec.message():error);
                }
                ImGui::SameLine();
                if(ImGui::Button("Load controller profile")){
                    std::string safe=d.guid;for(char&ch:safe)if(!std::isalnum(static_cast<unsigned char>(ch)))ch='_';
                    std::string error;if(controllers_->loadProfile(fs::current_path()/"ControllerProfiles"/(safe+".profile"),d.guid,&error))controllerCaptureStatus_="Profile loaded";else controllerCaptureStatus_="Profile load failed: "+error;
                }
                if(ImGui::Button("Test vibration")){rumbleActive_[d.id]=true;controllers_->setRumble(d.id,0.65f,0.35f);}ImGui::SameLine();if(ImGui::Button("Stop vibration")){rumbleActive_[d.id]=false;controllers_->setRumble(d.id,0.0f,0.0f);}
                ImGui::TextDisabled("Only features confirmed by the GLFW backend are exposed. Advanced vendor APIs are not active.");
            }
            ImGui::PopID();
        }
        ImGui::End();
    }

    void drawProjectHub() {
        if (!showHub_) return;
        ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("ANVIL Project Hub", &showHub_, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
        if (ImGui::BeginTabBar("ProjectHubTabs")) {
            if (ImGui::BeginTabItem("Проекты")) {
                ImGui::TextDisabled("Create a native ANVIL project or open an existing ANVIL project folder.");
                ImGui::Separator();
                ImGui::Text("Project name"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##hubProjectName", hubProjectName_, sizeof(hubProjectName_));
                ImGui::Text("Project directory"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##hubProjectPath", hubProjectPath_, sizeof(hubProjectPath_));
                const char* templates[] = {"Blank", "2D", "3D"}; ImGui::Combo("Template", &hubTemplate_, templates, 3);
                const char* platforms[] = {"Windows", "Android", "iOS", "PlayStation", "Xbox", "Nintendo Switch"};
                ImGui::Text("Target platforms");
                for (int i=0;i<6;++i) { ImGui::SameLine(i%3==0?0:ImGui::GetCursorPosX()+10); ImGui::Checkbox(platforms[i], &hubTargets_[i]); }
                if (ImGui::Button("Create Project", ImVec2(160, 32))) {
                    ProjectConfig cfg; cfg.name=hubProjectName_; cfg.templateName=templates[std::clamp(hubTemplate_,0,2)];
                    const TargetPlatform values[]={TargetPlatform::Windows,TargetPlatform::Android,TargetPlatform::IOS,TargetPlatform::PlayStation,TargetPlatform::Xbox,TargetPlatform::NintendoSwitch};
                    for(int i=0;i<6;++i)if(hubTargets_[i])cfg.targets.push_back(values[i]);
                    if(cfg.targets.empty())cfg.targets.push_back(TargetPlatform::Windows);cfg.target=cfg.targets.front();
                    std::string error;fs::path dest=hubProjectPath_;if(ProjectHub::create(dest,cfg,&error)){ProjectHub::addRecent(fs::current_path()/".anvil"/"recent-projects.txt",dest);projDir_=dest.string();assetDb_.scan(dest);hubStatus_="Project created: "+dest.string();showHub_=false;}else hubStatus_="Create failed: "+error;
                }
                ImGui::SameLine(); if(ImGui::Button("Open ANVIL Project",ImVec2(180,32))){std::string error;fs::path p=hubOpenPath_;if(ProjectHub::validate(p,&error)){projDir_=p.string();assetDb_.scan(p);ProjectHub::addRecent(fs::current_path()/".anvil"/"recent-projects.txt",p);hubStatus_="Opened project: "+p.string();showHub_=false;}else hubStatus_="Open failed: "+error;}
                ImGui::Text("Project path to open");ImGui::SetNextItemWidth(-1);ImGui::InputText("##hubOpenPath",hubOpenPath_,sizeof(hubOpenPath_));
                ImGui::Separator();ImGui::Text("Recent Projects");
                auto recent=ProjectHub::recent(fs::current_path()/".anvil"/"recent-projects.txt");
                if(recent.empty())ImGui::TextDisabled("No recent ANVIL projects.");
                for(const auto&r:recent){if(ImGui::Selectable((r.name+"  —  "+r.path.string()).c_str())){projDir_=r.path.string();assetDb_.scan(r.path);showHub_=false;}}
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Import / Migration")) {
                ImGui::TextWrapped("Import APK packages, inspect Windows PE executables, or migrate supported raw assets from Unity, Unreal, Godot and ANVIL directories. APK/EXE import preserves available package/runtime bytes; it does not recover source code.");
                ImGui::Text("Source file or project directory");ImGui::SetNextItemWidth(-1);ImGui::InputText("##hubImportSource",hubImportSource_,sizeof(hubImportSource_));
                ImGui::Text("Import destination directory");ImGui::SetNextItemWidth(-1);ImGui::InputText("##hubImportDest",hubImportDest_,sizeof(hubImportDest_));
                if(ImGui::Button("Analyze Input",ImVec2(135,30))){hubImportReport_=ImportAnalyzer::analyze(hubImportSource_);hubImportSummary_=std::string("Kind: ")+std::to_string(static_cast<int>(hubImportReport_.kind))+" | entries "+std::to_string(hubImportReport_.entries.size())+" | embedded PE resources "+std::to_string(hubImportReport_.embeddedResources.size());}
                ImGui::SameLine();if(ImGui::Button("Extract APK Package",ImVec2(175,30))){hubImportReport_=ImportAnalyzer::analyze(hubImportSource_);auto result=PackageImporter::importApk(hubImportSource_,hubImportDest_);hubImportSummary_=result.success?"APK extracted: "+std::to_string(result.copiedFiles)+" files, "+std::to_string(result.copiedBytes)+" bytes":"APK import failed: "+(result.errors.empty()?std::string("unknown error"):result.errors.front());}
                ImGui::SameLine();if(ImGui::Button("Import Windows EXE",ImVec2(175,30))){hubImportReport_=ImportAnalyzer::analyze(hubImportSource_);auto result=PackageImporter::importWindowsExecutable(hubImportSource_,hubImportDest_);hubImportSummary_=result.success?"EXE runtime bundle created: "+std::to_string(result.copiedFiles)+" files, "+std::to_string(result.copiedBytes)+" bytes":"EXE import failed: "+(result.errors.empty()?std::string("unknown error"):result.errors.front());}
                if(ImGui::Button("Migrate Supported Project Assets",ImVec2(245,30))){auto result=MigrationWizard::migrateDirectory(hubImportSource_,hubImportDest_);hubImportSummary_=result.success?"Migration complete: "+std::to_string(result.copiedFiles)+" files, "+std::to_string(result.copiedBytes)+" bytes":"Migration failed: "+(result.errors.empty()?std::string("unknown error"):result.errors.front());if(result.success){projDir_=hubImportDest_;assetDb_.scan(projDir_);}}
                if(hubImportReport_.kind==ImportKind::AndroidAPK){ImGui::Text("Android package: %s | min SDK %d | target SDK %d | ABI count %zu",hubImportReport_.packageName.c_str(),hubImportReport_.minSdkVersion,hubImportReport_.targetSdkVersion,hubImportReport_.abis.size());}
                if(hubImportReport_.kind==ImportKind::WindowsPE){ImGui::Text("PE architecture: %s | imported DLLs: %zu | embedded resources: %zu",hubImportReport_.architecture.c_str(),hubImportReport_.libraries.size(),hubImportReport_.embeddedResources.size());}
                if(!hubImportSummary_.empty())ImGui::TextWrapped("%s",hubImportSummary_.c_str());
                for(const auto&w:hubImportReport_.warnings)ImGui::BulletText("%s",w.c_str());
                for(const auto&e:hubImportReport_.errors)ImGui::TextColored(hex(0xE08080),"%s",e.c_str());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if(!hubStatus_.empty())ImGui::TextWrapped("%s",hubStatus_.c_str());
        ImGui::End();
    }


    // ------------------------------------------------------------------ roads + traffic
    fs::path roadsPath() const { return fs::path(projectRoot_) / "Assets" / "World" / "roads.net"; }
    bool roadClick(const glm::vec3& ro, const glm::vec3& rd) {
        if (rd.y > -1e-4f) return false;
        const float t = -ro.y / rd.y; if (t < 0) return false;
        const glm::vec3 hit = ro + rd * t; uint32_t n = roads_.nearestNode(hit, 3.0f);
        if (!n) n = roads_.addNode({hit.x, 0.0f, hit.z});
        if (roadChain_ && roadChain_ != n) roads_.addSegment(roadChain_, n, roadFwd_, roadBack_, roadLane_, roadSpeedKmh_ / 3.6f);
        roadChain_ = n; return true;
    }
    void drawRoadsOverlay(ImDrawList* dl, const glm::mat4& vp) {
        if (!showRoads_) return;
        for (const auto& sg : roads_.segments) {
            const RoadNode *a = roads_.node(sg.a), *b = roads_.node(sg.b); ImVec2 pa, pb; if (!a || !b) continue;
            if (project(vp, a->pos + glm::vec3(0, 0.15f, 0), pa) && project(vp, b->pos + glm::vec3(0, 0.15f, 0), pb)) dl->AddLine(pa, pb, IM_COL32(255, 190, 60, 230), 2.0f);
        }
        for (const auto& n : roads_.nodes) { ImVec2 p; if (project(vp, n.pos + glm::vec3(0, 0.15f, 0), p)) dl->AddCircleFilled(p, n.id == roadChain_ ? 7.0f : 4.5f, n.id == roadChain_ ? IM_COL32(90, 230, 120, 255) : IM_COL32(240, 240, 240, 255)); }
    }
    void updateTraffic(float dt) {
        if (!trafficOn_ || roads_.segments.empty()) { if (!traffic_.agents.empty()) traffic_.clear(); return; }
        if (world_.enabled) {
            traffic_.allowed = [this](const glm::vec3& p) { int cx, cz; worldCellOf(p, world_.cellSize, cx, cz); auto it = wCells_.find(cellKey(cx, cz)); return it == wCells_.end() || it->second.state == 2; };
            traffic_.update(roads_, dt, cam_.position, world_.loadRadius * world_.cellSize * 0.9f, world_.unloadRadius * world_.cellSize);
        } else { traffic_.allowed = nullptr; traffic_.update(roads_, dt, cam_.position, 150.0f, 200.0f); }
    }
    void addRoadAndTrafficItems() {
        const Mesh* cube = scene_.cubeMesh(); if (!cube) return;
        auto box = [&](glm::vec3 c, glm::vec3 half, float yaw, glm::vec3 col, float rough) {
            DrawItem it; it.mesh = cube; it.model = glm::translate(glm::mat4(1.0f), c) * glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 1, 0)) * glm::scale(glm::mat4(1.0f), half);
            it.material.albedo = col; it.material.roughness = rough; items_.push_back(it); };
        for (const auto& sg : roads_.segments) {
            const RoadNode *a = roads_.node(sg.a), *b = roads_.node(sg.b); if (!a || !b) continue;
            const glm::vec3 d = roads_.dir(sg), r = RoadNetwork::rightOf(d); const float len = roads_.length(sg), W = (sg.lanesFwd + sg.lanesBack) * sg.laneWidth, off = (sg.lanesFwd - sg.lanesBack) * sg.laneWidth * 0.5f;
            glm::vec3 c = (a->pos + b->pos) * 0.5f + r * off; c.y = 0.08f; box(c, {W * 0.5f, 0.02f, len * 0.5f}, std::atan2(d.x, d.z), {0.11f, 0.11f, 0.12f}, 0.95f);
        }
        for (const auto& n : roads_.nodes) {
            float hw = 0; for (const auto& sg : roads_.segments) if (sg.a == n.id || sg.b == n.id) hw = std::max(hw, std::max(sg.lanesFwd, sg.lanesBack) * sg.laneWidth);
            if (hw > 0) box({n.pos.x, 0.0805f, n.pos.z}, {hw, 0.02f, hw}, 0.0f, {0.11f, 0.11f, 0.12f}, 0.95f);
        }
        const glm::vec3 pal[6] = {{0.75f, 0.1f, 0.1f}, {0.1f, 0.2f, 0.6f}, {0.85f, 0.85f, 0.85f}, {0.1f, 0.1f, 0.1f}, {0.8f, 0.7f, 0.15f}, {0.3f, 0.45f, 0.3f}};
        for (const auto& ag : traffic_.agents) {
            const float yaw = std::atan2(ag.heading.x, ag.heading.z);
            if (!ag.ped) box(ag.pos + glm::vec3(0, 0.85f, 0), {0.9f, 0.75f, 2.1f}, yaw, pal[ag.color % 6], 0.35f);
            else box(ag.pos + glm::vec3(0, 0.9f, 0), {0.25f, 0.9f, 0.25f}, yaw, pal[(ag.color + 2) % 6] * 0.7f, 0.8f);
        }
    }
    void drawRoadsWindow() {
        if (!showRoads_) return;
        ImGui::SetNextWindowSize(ImVec2(400, 520), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Дороги и трафик", &showRoads_)) { ImGui::End(); return; }
        ImGui::Checkbox("Рисовать дорогу (клик во вьюпорте по земле)", &roadTool_);
        if (roadTool_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) roadChain_ = 0;
        ImGui::SetNextItemWidth(120); ImGui::SliderInt("Полос вперёд", &roadFwd_, 0, 4);
        ImGui::SetNextItemWidth(120); ImGui::SliderInt("Полос назад", &roadBack_, 0, 4);
        ImGui::SetNextItemWidth(120); ImGui::DragFloat("Ширина полосы (м)", &roadLane_, 0.05f, 2.5f, 5.0f);
        ImGui::SetNextItemWidth(120); ImGui::DragFloat("Лимит (км/ч)", &roadSpeedKmh_, 1.0f, 10.0f, 130.0f);
        if (ImGui::Button("Закончить цепочку (Esc)")) roadChain_ = 0;
        ImGui::SameLine(); if (ImGui::Button("Удалить все дороги")) { roads_ = RoadNetwork(); traffic_.clear(); roadChain_ = 0; }
        ImGui::TextDisabled("Клик рядом с узлом (3 м) присоединяет к нему. Направление «вперёд» — от первого узла ко второму, движение правостороннее.");
        ImGui::Separator();
        ImGui::Checkbox("Симуляция трафика и пешеходов", &trafficOn_);
        ImGui::SetNextItemWidth(120); ImGui::SliderInt("Макс. машин", &traffic_.maxCars, 0, 200);
        ImGui::SetNextItemWidth(120); ImGui::SliderInt("Макс. пешеходов", &traffic_.maxPeds, 0, 300);
        ImGui::Text("Узлов: %zu   сегментов: %zu   машин: %d   пешеходов: %d", roads_.nodes.size(), roads_.segments.size(), traffic_.carCount(), traffic_.pedCount());
        ImGui::TextDisabled(world_.enabled ? "Агенты появляются только в загруженных ячейках и исчезают вместе с ними." : "Стриминг выключен: агенты живут в радиусе 150 м от камеры.");
        ImGui::Separator();
        ImGui::BeginChild("segList", ImVec2(0, 0), true);
        for (size_t i = 0; i < roads_.segments.size() && i < 200; ++i) {
            auto& sg = roads_.segments[i]; ImGui::PushID(int(sg.id));
            ImGui::Text("#%u  %d↑ %d↓  %.0f м", sg.id, sg.lanesFwd, sg.lanesBack, roads_.length(sg)); ImGui::SameLine();
            if (ImGui::SmallButton("x")) { roads_.removeSegment(sg.id); ImGui::PopID(); break; }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::End();
    }

    // ------------------------------------------------------------------ open world
    struct ProxyBox { glm::vec3 center{0}, half{1}, color{0.6f}; };
    struct WCell { int cx = 0, cz = 0, state = 0; /*0 on disk, 1 loading, 2 loaded, 3 failed*/ std::vector<uint32_t> ids; std::vector<ProxyBox> proxy; std::future<std::unique_ptr<Scene>> fut; };
    static long long cellKey(int cx, int cz) { return (static_cast<long long>(cx) << 32) ^ static_cast<unsigned>(cz); }
    fs::path cellPath(int cx, int cz) const { return fs::path(projectRoot_) / "Assets" / "World" / ("cell_" + std::to_string(cx) + "_" + std::to_string(cz) + ".scene"); }
    fs::path proxyPath(int cx, int cz) const { fs::path p = cellPath(cx, cz); p.replace_extension(".proxy"); return p; }
    // far-LOD proxy: 4x4 skyline boxes per cell (max height, footprint coverage, average colour)
    void buildProxy(WCell& c, const std::vector<const Entity*>& list) {
        constexpr int S = 4; const float cs = world_.cellSize, sub = cs / S;
        struct Acc { float top = -1e9f, bottom = 1e9f, area = 0; glm::vec3 col{0}; int n = 0; } acc[S * S];
        for (const Entity* e : list) {
            if (e->primitive == Primitive::None || e->primitive == Primitive::Plane) continue;
            const glm::vec3 pos = glm::vec3(scene_.worldModel(e->id)[3]);
            int ix = std::clamp(int(std::floor((pos.x - c.cx * cs) / sub)), 0, S - 1), iz = std::clamp(int(std::floor((pos.z - c.cz * cs) / sub)), 0, S - 1);
            Acc& a = acc[iz * S + ix]; a.top = std::max(a.top, pos.y + e->scale.y); a.bottom = std::min(a.bottom, pos.y - e->scale.y);
            a.area += 4.0f * e->scale.x * e->scale.z; a.col += e->material.albedo; ++a.n;
        }
        c.proxy.clear();
        for (int iz = 0; iz < S; ++iz) for (int ix = 0; ix < S; ++ix) {
            const Acc& a = acc[iz * S + ix]; if (!a.n) continue;
            float cover = std::sqrt(std::min(1.0f, a.area / (sub * sub))), h = std::max(a.top, 0.5f), b = std::max(a.bottom, 0.0f), hy = std::max((h - b) * 0.5f, 0.25f);
            ProxyBox box; box.center = {c.cx * cs + (ix + 0.5f) * sub, b + hy, c.cz * cs + (iz + 0.5f) * sub};
            box.half = {sub * 0.5f * cover, hy, sub * 0.5f * cover}; box.color = a.col / float(a.n); c.proxy.push_back(box);
        }
        std::ofstream f(proxyPath(c.cx, c.cz)); for (auto& x : c.proxy) f << x.center.x << ' ' << x.center.y << ' ' << x.center.z << ' ' << x.half.x << ' ' << x.half.y << ' ' << x.half.z << ' ' << x.color.r << ' ' << x.color.g << ' ' << x.color.b << "\n";
    }
    void loadProxy(WCell& c) {
        c.proxy.clear(); std::ifstream f(proxyPath(c.cx, c.cz)); ProxyBox b;
        while (f >> b.center.x >> b.center.y >> b.center.z >> b.half.x >> b.half.y >> b.half.z >> b.color.r >> b.color.g >> b.color.b) c.proxy.push_back(b);
    }
    void addFarProxies() {
        if (!world_.enabled || world_.farRadius <= 0 || !scene_.cubeMesh()) return;
        int cx, cz; worldCellOf(cam_.position, world_.cellSize, cx, cz);
        for (auto& [k, c] : wCells_) {
            if ((c.state != 0 && c.state != 1) || worldCellDistance(cx, cz, c.cx, c.cz) > world_.farRadius) continue;
            for (const ProxyBox& b : c.proxy) { DrawItem it; it.mesh = scene_.cubeMesh(); it.model = glm::translate(glm::mat4(1.0f), b.center) * glm::scale(glm::mat4(1.0f), b.half); it.material.albedo = b.color; it.material.roughness = 0.85f; items_.push_back(it); }
        }
    }
    uint32_t rootOf(uint32_t id) {
        for (int g = 0; g < 32; ++g) { Entity* e = scene_.find(id); if (!e || !e->parentId) return id; id = e->parentId; }
        return id;
    }
    bool saveEntitiesTo(const std::vector<const Entity*>& list, const fs::path& path) {
        Scene tmp; uint32_t mx = 0;
        for (const Entity* e : list) { tmp.entities.push_back(*e); mx = std::max(mx, e->id); }
        tmp.setNextId(mx + 1); tmp.sun = scene_.sun;
        std::error_code ec; fs::create_directories(path.parent_path(), ec);
        std::string err; return SceneSerializer::save(tmp, path, &err);
    }
    // copy a freshly loaded cell scene into the live scene with new ids (parents remapped)
    void integrateCell(WCell& c, std::unique_ptr<Scene> sc) {
        c.ids.clear();
        if (!sc) { c.state = 3; return; }
        std::unordered_map<uint32_t, uint32_t> remap;
        for (const Entity& src : sc->entities) {
            Entity& n = scene_.add(src.name); const uint32_t nid = n.id;
            n = src; n.id = nid; remap[src.id] = nid; c.ids.push_back(nid);
        }
        for (uint32_t id : c.ids) if (Entity* n = scene_.find(id)) { if (n->parentId) { auto it = remap.find(n->parentId); n->parentId = it != remap.end() ? it->second : 0; } }
        c.state = 2;
    }
    void unloadCell(WCell& c) {
        if (!playing_) {
            std::vector<const Entity*> list; for (uint32_t id : c.ids) if (const Entity* e = scene_.find(id)) list.push_back(e);
            saveEntitiesTo(list, cellPath(c.cx, c.cz)); buildProxy(c, list);
        }
        for (uint32_t id : c.ids) { if (selected_ == id) selected_ = 0; scene_.remove(id); }
        c.ids.clear(); c.state = 0;
    }
    void loadAllSync() {
        for (auto& [k, c] : wCells_) {
            if (c.state == 1 && c.fut.valid()) integrateCell(c, c.fut.get());
            else if (c.state == 0) { auto sc = std::make_unique<Scene>(); std::string err; integrateCell(c, SceneSerializer::load(*sc, cellPath(c.cx, c.cz), &err) ? std::move(sc) : nullptr); }
        }
    }
    void scanWorldCells() {
        wCells_.clear();
        std::error_code ec; fs::path dir = fs::path(projectRoot_) / "Assets" / "World";
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            int x = 0, z = 0; std::string n = it->path().filename().string();
            if (std::sscanf(n.c_str(), "cell_%d_%d.scene", &x, &z) == 2) { WCell& c = wCells_[cellKey(x, z)]; c.cx = x; c.cz = z; c.state = 0; loadProxy(c); }
        }
    }
    // writes main.scene (+ per-cell files when streaming is on)
    bool saveWorldAndScene(const fs::path& mainScene, std::string* err) {
        std::error_code ec; fs::create_directories(mainScene.parent_path(), ec);
        if (!world_.enabled) {
            loadAllSync();
            for (auto& [k, c] : wCells_) { fs::remove(cellPath(c.cx, c.cz), ec); fs::remove(proxyPath(c.cx, c.cz), ec); }
            wCells_.clear();
            return SceneSerializer::save(scene_, mainScene, err);
        }
        world_.clampValues();
        std::map<long long, std::vector<const Entity*>> groups; std::map<long long, std::pair<int, int>> coords; std::vector<const Entity*> resident;
        for (const Entity& e : scene_.entities) {
            const Entity* r = scene_.find(rootOf(e.id));
            if (!r || r->primitive == Primitive::Plane) { resident.push_back(&e); continue; }
            int cx, cz; worldCellOf(r->position, world_.cellSize, cx, cz); long long k = cellKey(cx, cz);
            groups[k].push_back(&e); coords[k] = {cx, cz};
        }
        for (auto& [k, list] : groups) {
            if (!saveEntitiesTo(list, cellPath(coords[k].first, coords[k].second))) { if (err) *err = "cannot write cell file"; return false; }
            WCell& c = wCells_[k]; c.cx = coords[k].first; c.cz = coords[k].second; c.state = 2; c.ids.clear();
            for (const Entity* e : list) c.ids.push_back(e->id);
            buildProxy(c, list);
        }
        for (auto it = wCells_.begin(); it != wCells_.end();) {   // loaded cells that became empty: drop their file
            if (it->second.state == 2 && !groups.count(it->first)) { fs::remove(cellPath(it->second.cx, it->second.cz), ec); fs::remove(proxyPath(it->second.cx, it->second.cz), ec); it = wCells_.erase(it); } else ++it;
        }
        return saveEntitiesTo(resident, mainScene);
    }
    void buildOpenWorld() {
        Entity& g = scene_.add("Ground"); g.primitive = Primitive::Plane; g.scale = {40.0f, 1.0f, 40.0f};
        g.material = {{0.38f, 0.40f, 0.36f}, 0.9f, 0.0f}; g.physics.enabled = true; g.physics.motion = physics::MotionType::Static;
        Entity& l = scene_.add("Point Light 1"); l.isLight = true; l.lightColor = {1.0f, 0.95f, 0.85f}; l.lightIntensity = 150.0f; l.position = {0, 12, 0};
        generateTestCity(24, 30.0f);
        world_.enabled = true; world_.cellSize = 128.0f; world_.loadRadius = 3; world_.unloadRadius = 4;
        cam_.position = {0.0f, 30.0f, 60.0f}; cam_.pitch = -20.0f;
    }
    // procedural placeholder blocks (NOT art) to test streaming: n x n grid, spacing metres
    void generateTestCity(int n, float spacing) {
        unsigned seed = 1337u; auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return float(seed >> 8) / float(1 << 24); };
        for (int ix = -n; ix <= n; ++ix) for (int iz = -n; iz <= n; ++iz) {
            if ((ix + 100) % 4 == 0 || (iz + 100) % 4 == 0) continue;   // streets
            Entity& b = scene_.add("Block"); b.primitive = Primitive::Cube;
            float h = 2.0f + rnd() * 13.0f, w = 3.5f + rnd() * 4.0f;   // half extents (unit cube half = 1)
            b.scale = {w, h, w}; b.position = {ix * spacing, h, iz * spacing};
            float c = 0.35f + rnd() * 0.4f; b.material = {{c, c, c * 1.05f}, 0.7f, 0.0f};
        }
    }
    void updateWorldStreaming() {
        if (projectRoot_.empty()) return;
        using namespace std::chrono_literals;
        if (!world_.enabled) {   // streaming off: bring everything back into the scene
            bool any = false; for (auto& [k, c] : wCells_) if (c.state == 0 || c.state == 1) any = true;
            if (any) loadAllSync();
            return;
        }
        if (playing_) return;    // bodies are created at Play start: residency is frozen while playing
        world_.clampValues();
        int cx, cz; worldCellOf(cam_.position, world_.cellSize, cx, cz);
        int inflight = 0, integrated = 0, unloaded = 0;
        for (auto& [k, c] : wCells_) if (c.state == 1) ++inflight;
        for (auto& [k, c] : wCells_)
            if (c.state == 1 && c.fut.valid() && integrated < 2 && c.fut.wait_for(0s) == std::future_status::ready) { integrateCell(c, c.fut.get()); ++integrated; --inflight; }
        for (int dz = -world_.loadRadius; dz <= world_.loadRadius && inflight < 4; ++dz)
            for (int dx = -world_.loadRadius; dx <= world_.loadRadius && inflight < 4; ++dx) {
                auto it = wCells_.find(cellKey(cx + dx, cz + dz)); if (it == wCells_.end() || it->second.state != 0) continue;
                WCell& c = it->second; fs::path path = cellPath(c.cx, c.cz);
                c.fut = std::async(std::launch::async, [path]() -> std::unique_ptr<Scene> {
                    auto sc = std::make_unique<Scene>(); std::string err;
                    return SceneSerializer::load(*sc, path, &err) ? std::move(sc) : nullptr; });
                c.state = 1; ++inflight;
            }
        for (auto& [k, c] : wCells_)
            if (c.state == 2 && unloaded < 2 && worldCellDistance(cx, cz, c.cx, c.cz) > world_.unloadRadius) { unloadCell(c); ++unloaded; }
    }
    void drawWorldWindow() {
        if (!showWorld_) return;
        ImGui::SetNextWindowSize(ImVec2(440, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Открытый мир", &showWorld_)) { ImGui::End(); return; }
        if (projectRoot_.empty()) ImGui::TextColored(hex(0xE0C080), "Создай или открой проект: ячейки хранятся в его папке Assets/World.");
        ImGui::Checkbox("Стриминг ячеек включён", &world_.enabled);
        ImGui::BeginDisabled(!wCells_.empty());
        ImGui::SetNextItemWidth(160); ImGui::DragFloat("Размер ячейки (м)", &world_.cellSize, 1.0f, 16.0f, 4096.0f);
        ImGui::EndDisabled();
        if (!wCells_.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Менять нельзя, пока есть ячейки. Выключи стриминг и сохрани проект.");
        ImGui::SetNextItemWidth(160); ImGui::SliderInt("Радиус загрузки (ячеек)", &world_.loadRadius, 1, 16);
        ImGui::SetNextItemWidth(160); ImGui::SliderInt("Радиус выгрузки (ячеек)", &world_.unloadRadius, 2, 20);
        ImGui::SetNextItemWidth(160); ImGui::SliderInt("Дальний LOD (ячеек, 0 = выкл)", &world_.farRadius, 0, 24);
        world_.clampValues();
        if (ImGui::Button("Добавить тестовый город")) generateTestCity(24, 30.0f);
        ImGui::SameLine(); if (ImGui::Button("Загрузить все ячейки")) loadAllSync();
        ImGui::SameLine(); if (ImGui::Button("Сохранить мир")) saveProjectScene();
        int loaded = 0, loading = 0; for (auto& [k, c] : wCells_) { if (c.state == 2) ++loaded; if (c.state == 1) ++loading; }
        ImGui::Text("Объектов в памяти: %zu", scene_.entities.size());
        ImGui::Text("Ячеек: %zu   загружено: %d   грузится: %d", wCells_.size(), loaded, loading);
        int cx, cz; worldCellOf(cam_.position, world_.cellSize, cx, cz); ImGui::Text("Камера в ячейке (%d, %d)", cx, cz);
        std::unordered_set<long long> hasEntities;
        for (const Entity& e : scene_.entities) { int ex, ez; worldCellOf(e.position, world_.cellSize, ex, ez); hasEntities.insert(cellKey(ex, ez)); }
        const int R = world_.unloadRadius + 1, N = 2 * R + 1; float cs = std::min(ImGui::GetContentRegionAvail().x / N, 18.0f);
        ImVec2 o = ImGui::GetCursorScreenPos(); ImGui::Dummy(ImVec2(cs * N, cs * N)); ImDrawList* d = ImGui::GetWindowDrawList();
        for (int dz = -R; dz <= R; ++dz) for (int dx = -R; dx <= R; ++dx) {
            long long key = cellKey(cx + dx, cz + dz); auto it = wCells_.find(key); ImU32 col = IM_COL32(32, 32, 36, 255);
            if (it != wCells_.end()) col = it->second.state == 2 ? IM_COL32(70, 170, 90, 255) : it->second.state == 1 ? IM_COL32(220, 200, 70, 255) : it->second.state == 3 ? IM_COL32(200, 60, 60, 255) : IM_COL32(200, 130, 50, 255);
            else if (hasEntities.count(key)) col = IM_COL32(70, 120, 200, 255);
            ImVec2 a(o.x + (dx + R) * cs, o.y + (dz + R) * cs); d->AddRectFilled(a, ImVec2(a.x + cs - 1, a.y + cs - 1), col);
            if (dx == 0 && dz == 0) d->AddRect(a, ImVec2(a.x + cs - 1, a.y + cs - 1), IM_COL32(255, 255, 255, 255), 0, 0, 2.0f);
        }
        ImGui::TextDisabled("Зелёный — в памяти, жёлтый — грузится, оранжевый — на диске, синий — не сохранено, красный — ошибка файла.");
        ImGui::TextDisabled("Каждая ячейка — файл Assets/World/cell_X_Z.scene. Во время Play загрузка/выгрузка приостанавливается. Земля-плоскость всегда в основной сцене.");
        ImGui::End();
    }

    // ------------------------------------------------------------------ cutscene editor
    Entity* entityByName(const std::string& n) { for (Entity& e : scene_.entities) if (e.name == n) return &e; return nullptr; }
    fs::path cutPath() const { fs::path root = projectRoot_.empty() ? fs::path(ANVIL_ROOT_DIR) : fs::path(projectRoot_); return root / "Assets" / "Cutscenes" / (cut_.name + ".cutscene"); }
    void cutBegin() {
        cutSavedCam_ = cam_; cutSavedXf_.clear();
        for (auto& tr : cut_.tracks) if (Entity* e = entityByName(tr.entity)) { CutXformKey k; k.pos = e->position; k.rot = e->rotation; k.scale = e->scale; cutSavedXf_.push_back({tr.entity, k}); }
        cutPreview_ = true; cutPrev_ = cutTime_;
    }
    void cutEnd() {
        cutPreview_ = false; cutPlay_ = false; cam_ = cutSavedCam_;
        for (auto& [n, k] : cutSavedXf_) if (Entity* e = entityByName(n)) { e->position = k.pos; e->rotation = k.rot; e->scale = k.scale; }
        cutSavedXf_.clear();
    }
    void updateCutscene(float dt) {
        if (!cutPreview_) return;
        if (cutPlay_) {
            cutTime_ += dt;
            if (cutTime_ >= cut_.duration) { if (cutLoop_) cutTime_ = std::fmod(cutTime_, cut_.duration); else { cutTime_ = cut_.duration; cutPlay_ = false; } }
        }
        CamPose cp;
        if (cut_.evalCamera(cutTime_, cp)) { cam_.position = cp.pos + cut_.shakeOffset(cutTime_); cam_.yaw = cp.yaw; cam_.pitch = cp.pitch; cam_.fovY = cp.fov; }
        for (auto& tr : cut_.tracks) { CutXformKey k; if (Entity* e = entityByName(tr.entity)) if (Cutscene::evalTrack(tr, cutTime_, k)) { e->position = k.pos; e->rotation = k.rot; e->scale = k.scale; } }
        for (auto& ev : cut_.events) if (ev.type == CutEventType::Sound && ev.t > cutPrev_ && ev.t <= cutTime_) ANVIL_LOG("[Cutscene] sound: %s", ev.text.c_str());
        cutPrev_ = cutTime_;
    }
    void drawCutsceneOverlay(ImDrawList* dl) {
        if (!cutPreview_) return;
        ImVec2 a = vpMin_, b = ImVec2(vpMin_.x + vpSize_.x, vpMin_.y + vpSize_.y);
        if (cut_.letterbox) {
            float h = vpSize_.y * 0.115f;
            dl->AddRectFilled(a, ImVec2(b.x, a.y + h), IM_COL32(0, 0, 0, 255));
            dl->AddRectFilled(ImVec2(a.x, b.y - h), b, IM_COL32(0, 0, 0, 255));
        }
        if (const CutEvent* sub = cut_.activeSubtitle(cutTime_)) {
            const float fs = ImGui::GetFontSize() * 1.35f; ImFont* font = ImGui::GetFont();
            std::string line = sub->speaker.empty() ? sub->text : sub->speaker + ": " + sub->text;
            ImVec2 sz = ImGui::CalcTextSize(line.c_str()); sz.x *= 1.35f; sz.y *= 1.35f;
            float y = b.y - (cut_.letterbox ? vpSize_.y * 0.115f : 0.0f) - sz.y - 18.0f;
            ImVec2 pos(a.x + (vpSize_.x - sz.x) * 0.5f, y);
            dl->AddText(font, fs, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 220), line.c_str());
            dl->AddText(font, fs, pos, IM_COL32(245, 245, 245, 255), line.c_str());
        }
        float fa = cut_.fadeAlpha(cutTime_);
        if (fa > 0.001f) dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, int(fa * 255)));
    }
    void drawCutsceneEditor() {
        if (!showCut_) return;
        ImGui::SetNextWindowSize(ImVec2(980, 520), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Редактор кат-сцен", &showCut_)) { ImGui::End(); return; }
        // --- header
        char nm[64]; std::snprintf(nm, sizeof(nm), "%s", cut_.name.c_str());
        ImGui::SetNextItemWidth(180); if (ImGui::InputText("Название", nm, sizeof(nm))) cut_.name = nm;
        ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::DragFloat("Длит. (с)", &cut_.duration, 0.1f, 0.5f, 3600.0f, "%.1f");
        ImGui::SameLine(); ImGui::Checkbox("Кинорамка", &cut_.letterbox);
        ImGui::SameLine(); if (ImGui::Button("Сохранить")) { std::string e; cut_.sortAll(); cutStatus_ = cut_.save(cutPath(), &e) ? "Сохранено: " + cutPath().string() : "Ошибка: " + e; }
        ImGui::SameLine(); if (ImGui::Button("Загрузить")) { std::string e; if (cut_.load(cutPath(), &e)) { cutStatus_ = "Загружено"; cutTime_ = 0; cutSelCam_ = -1; } else cutStatus_ = "Ошибка: " + e; }
        ImGui::SameLine(); if (ImGui::Button("Новая")) { if (cutPreview_) cutEnd(); cut_ = Cutscene(); cutTime_ = 0; cutSelCam_ = -1; }
        // --- transport
        bool pv = cutPreview_;
        if (ImGui::Checkbox("Предпросмотр", &pv)) { if (pv) cutBegin(); else cutEnd(); }
        ImGui::SameLine(); if (ImGui::Button(cutPlay_ ? "Пауза" : "Играть")) { if (!cutPreview_) cutBegin(); if (!cutPlay_ && cutTime_ >= cut_.duration) cutTime_ = 0; cutPlay_ = !cutPlay_; }
        ImGui::SameLine(); if (ImGui::Button("Стоп")) { cutPlay_ = false; cutTime_ = 0; if (cutPreview_) cutEnd(); }
        ImGui::SameLine(); ImGui::Checkbox("Цикл", &cutLoop_);
        ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##cutTime", &cutTime_, 0.0f, cut_.duration, "%.2f с")) { if (!cutPreview_) cutBegin(); cutPlay_ = false; }
        if (!cutPreview_) ImGui::TextDisabled("Подсказка: выключи предпросмотр, поставь камеру во вьюпорте (ПКМ+WASD), потом жми «Ключ камеры» на нужном времени.");

        // --- timeline
        {
            const float labelW = 130, rowH = 22, rulerH = 20;
            const int rows = 2 + int(cut_.tracks.size());
            ImVec2 p = ImGui::GetCursorScreenPos(); float W = ImGui::GetContentRegionAvail().x, H = rulerH + rows * rowH + 4;
            ImGui::InvisibleButton("##timeline", ImVec2(W, H));
            ImDrawList* d = ImGui::GetWindowDrawList();
            float x0 = p.x + labelW, x1 = p.x + W - 6, span = std::max(x1 - x0, 10.0f);
            auto X = [&](float t) { return x0 + span * (t / cut_.duration); };
            d->AddRectFilled(p, ImVec2(p.x + W, p.y + H), IM_COL32(28, 28, 32, 255));
            float step = cut_.duration > 60 ? 10.0f : (cut_.duration > 20 ? 5.0f : 1.0f);
            for (float t = 0; t <= cut_.duration + 0.001f; t += step) {
                d->AddLine(ImVec2(X(t), p.y + rulerH - 6), ImVec2(X(t), p.y + H), IM_COL32(60, 60, 66, 255));
                char b[16]; std::snprintf(b, sizeof(b), "%.0f", t); d->AddText(ImVec2(X(t) + 3, p.y + 2), IM_COL32(150, 150, 150, 255), b);
            }
            auto rowY = [&](int r) { return p.y + rulerH + r * rowH; };
            auto diamond = [&](float x, float y, ImU32 c) { d->AddQuadFilled(ImVec2(x, y - 6), ImVec2(x + 6, y), ImVec2(x, y + 6), ImVec2(x - 6, y), c); };
            d->AddText(ImVec2(p.x + 6, rowY(0) + 3), IM_COL32(220, 220, 220, 255), "Камера");
            for (auto& k : cut_.cam) diamond(X(k.t), rowY(0) + rowH / 2, IM_COL32(255, 190, 60, 255));
            for (size_t i = 0; i < cut_.tracks.size(); ++i) {
                int r = 1 + int(i); d->AddText(ImVec2(p.x + 6, rowY(r) + 3), IM_COL32(190, 220, 255, 255), cut_.tracks[i].entity.c_str());
                for (auto& k : cut_.tracks[i].keys) diamond(X(k.t), rowY(r) + rowH / 2, IM_COL32(90, 190, 255, 255));
            }
            int er = 1 + int(cut_.tracks.size()); d->AddText(ImVec2(p.x + 6, rowY(er) + 3), IM_COL32(220, 220, 220, 255), "События");
            const ImU32 ec[4] = {IM_COL32(120, 200, 120, 230), IM_COL32(200, 200, 200, 230), IM_COL32(220, 120, 90, 230), IM_COL32(180, 120, 230, 230)};
            for (auto& e : cut_.events) d->AddRectFilled(ImVec2(X(e.t), rowY(er) + 3), ImVec2(std::max(X(e.t + e.dur), X(e.t) + 4), rowY(er) + rowH - 3), ec[int(e.type)], 3.0f);
            d->AddLine(ImVec2(X(cutTime_), p.y), ImVec2(X(cutTime_), p.y + H), IM_COL32(255, 70, 70, 255), 2.0f);
            if (ImGui::IsItemActive() && ImGui::GetIO().MousePos.x > x0) {
                cutTime_ = std::clamp((ImGui::GetIO().MousePos.x - x0) / span * cut_.duration, 0.0f, cut_.duration);
                if (!cutPreview_) cutBegin(); cutPlay_ = false;
            }
        }
        ImGui::Separator();
        // --- details
        ImGui::BeginChild("cutDetails", ImVec2(0, 0), false);
        if (ImGui::BeginTabBar("cutTabs")) {
            if (ImGui::BeginTabItem("Камера")) {
                if (ImGui::Button("Ключ камеры (из вьюпорта)")) {
                    CutCamKey k; k.t = cutTime_; k.pos = cutPreview_ ? cutSavedCam_.position : cam_.position; k.yaw = cutPreview_ ? cutSavedCam_.yaw : cam_.yaw;
                    k.pitch = cutPreview_ ? cutSavedCam_.pitch : cam_.pitch; k.fov = cutPreview_ ? cutSavedCam_.fovY : cam_.fovY;
                    cut_.cam.push_back(k); cut_.sortAll(); cutSelCam_ = -1;
                }
                ImGui::SameLine(); ImGui::TextDisabled("%zu ключей", cut_.cam.size());
                for (int i = 0; i < int(cut_.cam.size()); ++i) {
                    ImGui::PushID(i); auto& k = cut_.cam[i]; char lb[64]; std::snprintf(lb, sizeof(lb), "Ключ %d  @ %.2f с", i + 1, k.t);
                    if (ImGui::Selectable(lb, cutSelCam_ == i)) { cutSelCam_ = i; cutTime_ = k.t; }
                    ImGui::PopID();
                }
                if (cutSelCam_ >= 0 && cutSelCam_ < int(cut_.cam.size())) {
                    auto& k = cut_.cam[cutSelCam_]; ImGui::Separator();
                    ImGui::DragFloat("Время", &k.t, 0.02f, 0.0f, cut_.duration); ImGui::DragFloat3("Позиция", &k.pos.x, 0.05f);
                    ImGui::DragFloat("Yaw", &k.yaw, 0.2f); ImGui::DragFloat("Pitch", &k.pitch, 0.2f, -89, 89); ImGui::SliderFloat("FOV", &k.fov, 15, 110);
                    ImGui::Checkbox("Плавное смягчение", &k.smooth);
                    if (ImGui::Button("Взять вид из ключа")) { cam_.position = k.pos; cam_.yaw = k.yaw; cam_.pitch = k.pitch; cam_.fovY = k.fov; }
                    ImGui::SameLine(); if (ImGui::Button("Обновить из вьюпорта")) { k.pos = cam_.position; k.yaw = cam_.yaw; k.pitch = cam_.pitch; k.fov = cam_.fovY; }
                    ImGui::SameLine(); if (ImGui::Button("Удалить ключ")) { cut_.cam.erase(cut_.cam.begin() + cutSelCam_); cutSelCam_ = -1; }
                    else cut_.sortAll();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Объекты")) {
                Entity* sel = nullptr; for (Entity& e : scene_.entities) if (e.id == selected_) sel = &e;
                if (!sel) ImGui::TextDisabled("Выбери объект в иерархии.");
                else {
                    ImGui::Text("Выбран: %s", sel->name.c_str());
                    if (ImGui::Button("Добавить дорожку для выбранного")) {
                        bool has = false; for (auto& t : cut_.tracks) if (t.entity == sel->name) has = true;
                        if (!has) cut_.tracks.push_back({sel->name, {}});
                    }
                }
                for (int ti = 0; ti < int(cut_.tracks.size()); ++ti) {
                    auto& tr = cut_.tracks[ti]; ImGui::PushID(ti);
                    if (ImGui::CollapsingHeader((tr.entity + "##t").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (Entity* e = entityByName(tr.entity)) {
                            if (ImGui::Button("Ключ (текущая поза объекта)")) { CutXformKey k; k.t = cutTime_; k.pos = e->position; k.rot = e->rotation; k.scale = e->scale; tr.keys.push_back(k); cut_.sortAll(); }
                        } else ImGui::TextColored(hex(0xE08080), "Объект не найден в сцене");
                        ImGui::SameLine(); if (ImGui::Button("Удалить дорожку")) { cut_.tracks.erase(cut_.tracks.begin() + ti); ImGui::PopID(); break; }
                        for (int ki = 0; ki < int(tr.keys.size()); ++ki) {
                            ImGui::PushID(ki); ImGui::Text("@ %.2f с  pos(%.1f, %.1f, %.1f)", tr.keys[ki].t, tr.keys[ki].pos.x, tr.keys[ki].pos.y, tr.keys[ki].pos.z);
                            ImGui::SameLine(); if (ImGui::SmallButton("x")) { tr.keys.erase(tr.keys.begin() + ki); ImGui::PopID(); break; }
                            ImGui::PopID();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::TextDisabled("Предпросмотр двигает объекты; при остановке позы возвращаются.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("События")) {
                const char* types[] = {"Субтитр / диалог", "Затемнение", "Тряска камеры", "Звук (путь к файлу)"};
                ImGui::SetNextItemWidth(200); ImGui::Combo("Тип", &cutEvType_, types, 4);
                ImGui::SetNextItemWidth(120); ImGui::DragFloat("Длительность", &cutEvDur_, 0.05f, 0.05f, 600.0f);
                if (cutEvType_ == 0) { ImGui::SetNextItemWidth(200); ImGui::InputText("Говорящий", cutEvSpeaker_, sizeof(cutEvSpeaker_)); ImGui::SetNextItemWidth(-1); ImGui::InputText("Текст", cutEvText_, sizeof(cutEvText_)); }
                if (cutEvType_ == 1) { int m = cutEvValue_ > 0.5f ? 1 : 0; const char* fm[] = {"Из чёрного (появление)", "В чёрное (затухание)"}; ImGui::SetNextItemWidth(220); if (ImGui::Combo("Режим", &m, fm, 2)) cutEvValue_ = float(m); }
                if (cutEvType_ == 2) { ImGui::SetNextItemWidth(120); ImGui::SliderFloat("Сила", &cutEvValue_, 0.1f, 5.0f); }
                if (cutEvType_ == 3) { ImGui::SetNextItemWidth(-1); ImGui::InputText("Файл", cutEvText_, sizeof(cutEvText_)); }
                if (ImGui::Button("Добавить на текущее время")) {
                    CutEvent e; e.type = CutEventType(cutEvType_); e.t = cutTime_; e.dur = cutEvDur_; e.value = cutEvValue_;
                    e.speaker = cutEvSpeaker_; e.text = cutEvText_; cut_.events.push_back(e); cut_.sortAll();
                }
                ImGui::Separator();
                for (int i = 0; i < int(cut_.events.size()); ++i) {
                    auto& e = cut_.events[i]; ImGui::PushID(i);
                    ImGui::Text("%.2f–%.2f  [%s]  %s%s%s", e.t, e.t + e.dur, types[int(e.type)], e.speaker.c_str(), e.speaker.empty() ? "" : ": ", e.text.c_str());
                    ImGui::SameLine(); if (ImGui::SmallButton("x")) { cut_.events.erase(cut_.events.begin() + i); ImGui::PopID(); break; }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (!cutStatus_.empty()) ImGui::TextWrapped("%s", cutStatus_.c_str());
        ImGui::EndChild();
        ImGui::End();
    }

    // ------------------------------------------------------------------ start menu
    void buildEmptyPlatform() {
        Entity& ground = scene_.add("Ground");
        ground.primitive = Primitive::Plane;
        ground.material = {{0.55f, 0.55f, 0.58f}, 0.8f, 0.0f};
        ground.physics.enabled = true; ground.physics.motion = physics::MotionType::Static;
        Entity& sun = scene_.add("Point Light 1");
        sun.isLight = true; sun.lightColor = {1.0f, 0.95f, 0.85f}; sun.lightIntensity = 150.0f;
        sun.position = {4.0f, 8.0f, 4.0f};
    }
    void saveProjectScene() {
        if (projectRoot_.empty()) { startStatus_ = "No project open"; showStart_ = true; return; }
        world_.save(fs::path(projectRoot_) / "Config" / "world.ini"); roads_.save(roadsPath());
        std::string err; fs::path f = fs::path(projectRoot_) / "Assets" / "Scenes" / "main.scene";
        std::error_code ec; fs::create_directories(f.parent_path(), ec);
        if (saveWorldAndScene(f, &err)) ANVIL_LOG("Project saved: %s", f.string().c_str());
        else ANVIL_LOG("Save failed: %s", err.c_str());
    }
    void openProjectAt(const fs::path& p) {
        std::string err;
        if (!ProjectHub::validate(p, &err)) { startStatus_ = "Open failed: " + err; return; }
        scene_.stopPhysics(); scene_.entities.clear(); selected_ = 0; wCells_.clear(); world_ = OpenWorldSettings();
        world_.load(p / "Config" / "world.ini");
        fs::path f = p / "Assets" / "Scenes" / "main.scene";
        if (fs::exists(f)) { if (!SceneSerializer::load(scene_, f, &err)) { startStatus_ = "Scene load failed: " + err; return; } }
        else buildEmptyPlatform();
        projDir_ = projectRoot_ = p.string(); scanWorldCells(); roads_ = RoadNetwork(); roads_.load(roadsPath()); traffic_.clear(); roadChain_ = 0; assetDb_.scan(p);
        ProjectHub::addRecent(fs::current_path() / ".anvil" / "recent-projects.txt", p);
        showStart_ = false; startStatus_.clear();
    }
    void drawStartMenu() {
        if (!showStart_) return;
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(760.0f, vp->Size.x - 40), std::min(540.0f, vp->Size.y - 40)), ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        const ImGuiWindowFlags fl = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        if (!ImGui::Begin("ANVIL — Start", nullptr, fl)) { ImGui::End(); return; }
        ImGui::SetWindowFontScale(1.5f); ImGui::TextUnformatted("ANVIL Engine"); ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        if (ImGui::BeginTabBar("StartTabs")) {
            if (ImGui::BeginTabItem("Создать")) {
                ImGui::Spacing(); ImGui::TextUnformatted("Выбери сцену:");
                const char* names[3] = {"Пустая платформа", "Платформа со сферами", "Открытый мир"};
                const char* descs[3] = {"Только земля и свет", "Сферы, ящики, куб, цветные огни", "Большая земля, стриминг ячеек, тестовый город"};
                for (int i = 0; i < 3; ++i) {
                    ImGui::PushID(i);
                    if (ImGui::Selectable("##tpl", startTemplate_ == i, 0, ImVec2(0, 52))) startTemplate_ = i;
                    ImVec2 mn = ImGui::GetItemRectMin();
                    ImGui::GetWindowDrawList()->AddText(ImVec2(mn.x + 10, mn.y + 7), IM_COL32(235, 235, 235, 255), names[i]);
                    ImGui::GetWindowDrawList()->AddText(ImVec2(mn.x + 10, mn.y + 28), IM_COL32(160, 160, 160, 255), descs[i]);
                    ImGui::PopID();
                }
                ImGui::Spacing(); ImGui::TextUnformatted("Название:");
                ImGui::SetNextItemWidth(-1); ImGui::InputText("##startName", startName_, sizeof(startName_));
                ImGui::Spacing();
                if (ImGui::Button("Создать", ImVec2(200, 38))) {
                    ProjectConfig cfg; cfg.name = startName_; cfg.templateName = "3D";
                    fs::path dest = fs::path("Projects") / startName_; std::string err;
                    if (cfg.name.empty()) startStatus_ = "Введи название";
                    else if (ProjectHub::create(dest, cfg, &err)) {
                        scene_.stopPhysics(); scene_.entities.clear(); selected_ = 0;
                        wCells_.clear(); world_ = OpenWorldSettings(); roads_ = RoadNetwork(); traffic_.clear(); roadChain_ = 0;
                        if (startTemplate_ == 0) buildEmptyPlatform(); else if (startTemplate_ == 2) buildOpenWorld(); else buildDefaultScene();
                        projDir_ = projectRoot_ = dest.string(); saveProjectScene();
                        assetDb_.scan(dest);
                        ProjectHub::addRecent(fs::current_path() / ".anvil" / "recent-projects.txt", dest);
                        showStart_ = false; startStatus_.clear();
                    } else startStatus_ = "Ошибка: " + err;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Проекты")) {
                ImGui::Spacing();
                std::vector<ProjectRecord> list = ProjectHub::recent(fs::current_path() / ".anvil" / "recent-projects.txt");
                for (auto& r : ProjectHub::discover("Projects")) {
                    bool dup = false; for (auto& q : list) if (q.path == r.path) dup = true;
                    if (!dup) list.push_back(r);
                }
                if (list.empty()) ImGui::TextDisabled("Пока нет проектов. Создай первый во вкладке «Создать».");
                ImGui::BeginChild("projList", ImVec2(0, -60), true);
                for (auto& r : list) {
                    ImGui::PushID(r.path.string().c_str());
                    if (ImGui::Selectable((r.name + "   —   " + r.path.string()).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 34))) {
                        if (ImGui::IsMouseDoubleClicked(0)) openProjectAt(r.path);
                    }
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80); if (ImGui::SmallButton("Открыть")) openProjectAt(r.path);
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::TextDisabled("Двойной клик или «Открыть» — продолжить разработку.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (!startStatus_.empty()) ImGui::TextColored(hex(0xE08080), "%s", startStatus_.c_str());
        if (!projectRoot_.empty()) { ImGui::SameLine(ImGui::GetWindowWidth() - 90); if (ImGui::SmallButton("Закрыть")) showStart_ = false; }
        ImGui::End();
    }

    // ------------------------------------------------------------------ scene setup
    void buildDefaultScene() {
        Entity& ground = scene_.add("Ground");
        ground.primitive = Primitive::Plane;
        ground.material = {{0.55f, 0.55f, 0.58f}, 0.8f, 0.0f};
        ground.physics.enabled = true; ground.physics.motion = physics::MotionType::Static;

        for (int i = 0; i < 7; ++i) {
            Entity& s = scene_.add("Sphere " + std::to_string(i + 1));
            s.primitive = Primitive::Sphere;
            s.position = {(i - 3) * 2.4f, 1.0f, 0.0f};
            s.material = {i % 2 ? glm::vec3(0.95f, 0.64f, 0.54f) : glm::vec3(0.8f, 0.1f, 0.1f), 0.05f + 0.15f * i, i < 4 ? 0.0f : 1.0f};
            s.position.y = 1.0f + 2.5f * (i % 3);                         // drop from different heights when Play is pressed
            s.physics.enabled = true; s.physics.restitution = 0.35f; s.physics.friction = 0.5f;
        }
        Entity& cube = scene_.add("Cube");
        cube.primitive = Primitive::Cube;
        cube.position = {0.0f, 1.0f, -5.0f};
        cube.scale = {1.5f, 1.5f, 1.5f};
        cube.material = {{0.2f, 0.45f, 0.8f}, 0.35f, 0.0f};
        cube.physics.enabled = true; cube.physics.mass = 8.0f;
        for (int i = 0; i < 6; ++i) {   // tumbling crates: proves full 3D rotation
            Entity& crate = scene_.add("Crate " + std::to_string(i + 1));
            crate.primitive = Primitive::Cube;
            crate.scale = glm::vec3(0.5f);
            crate.position = {-3.0f + (i % 3) * 0.9f, 6.0f + (i / 3) * 1.4f, -2.0f + 0.35f * (i % 2)};
            crate.rotation = {20.0f * i, 35.0f * i, 10.0f * i};
            crate.material = {{0.75f, 0.55f, 0.3f}, 0.7f, 0.0f};
            crate.physics.enabled = true; crate.physics.friction = 0.7f;
        }

        const glm::vec3 cols[3] = {{1.0f, 0.4f, 0.3f}, {0.3f, 1.0f, 0.5f}, {0.4f, 0.5f, 1.0f}};
        for (int i = 0; i < 3; ++i) {
            Entity& l = scene_.add("Point Light " + std::to_string(i + 1));
            l.isLight = true;
            l.lightColor = cols[i];
            l.lightIntensity = 120.0f;
            l.orbit = true;
            l.orbitCenter = {0.0f, 3.5f + i * 1.5f, 3.0f};
            l.orbitRadius = 9.0f;
            l.orbitPhase = i * 2.094f;
            l.position = l.orbitCenter + glm::vec3(std::cos(l.orbitPhase) * l.orbitRadius, 0.0f, std::sin(l.orbitPhase) * l.orbitRadius * 0.5f);
        }
    }

    Entity& createEntity(Primitive p, const char* name, bool light = false) {
        Entity& e = scene_.add(name);
        e.primitive = p;
        e.isLight = light;
        e.position = cam_.position + cam_.forward() * 6.0f;
        if (p == Primitive::Plane) e.scale = {0.1f, 1.0f, 0.1f};
        selected_ = e.id;
        ANVIL_LOG("Created %s", name);
        return e;
    }

    void deleteSelected() {
        if (Entity* e = scene_.find(selected_)) ANVIL_LOG("Deleted %s", e->name.c_str());
        scene_.remove(selected_);
        selected_ = 0;
    }

    void focusSelected() {
        if (Entity* e = scene_.find(selected_)) {
            const glm::mat4 world = scene_.worldModel(e->id);
            const glm::vec3 center(world[3]);
            const glm::vec3 scale(glm::length(glm::vec3(world[0])), glm::length(glm::vec3(world[1])), glm::length(glm::vec3(world[2])));
            float r = std::max(2.0f, glm::length(scale) * 2.0f);
            cam_.position = center - cam_.forward() * r * 2.0f;
        }
    }

    void sunToAngles(float& az, float& el) const {
        glm::vec3 d = -glm::normalize(scene_.sun.direction);
        el = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
        az = glm::degrees(std::atan2(d.z, d.x));
    }
    void anglesToSun() {
        float az = glm::radians(sunAz_), el = glm::radians(sunEl_);
        scene_.sun.direction = -glm::vec3(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az));
    }

    // ------------------------------------------------------------------ small ui helpers
    void icon(Icon i, float size = 16.0f, ImVec4 tint = ImVec4(0.82f, 0.82f, 0.82f, 1.0f)) {
        ImGui::ImageWithBg(texId(icons::texture(i)), ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tint);
    }
    bool iconButton(const char* id, Icon i, bool active, const char* tip, ImVec4 tint = ImVec4(0.85f, 0.85f, 0.85f, 1.0f)) {
        ImGui::PushStyleColor(ImGuiCol_Button, active ? hex(0x555555) : hex(0x000000, 0.0f));
        bool hit = ImGui::ImageButton(id, texId(icons::texture(i)), ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tint);
        ImGui::PopStyleColor();
        if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        return hit;
    }
    static Icon iconFor(const Entity& e) {
        if (e.isLight) return Icon::Light;
        switch (e.primitive) {
            case Primitive::Sphere: return Icon::Sphere;
            case Primitive::Cube:   return Icon::Cube;
            case Primitive::Plane:  return Icon::Plane;
            default:                return Icon::Hierarchy;
        }
    }
    void panelTitle(Icon i, const char* label) {
        icon(i, 14.0f, hex(0x9AA0A6)); ImGui::SameLine(0, 6); ImGui::TextDisabled("%s", label);
        ImGui::Separator();
    }

    // ------------------------------------------------------------------ ui
    void drawUI() {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        drawMenuBar();
        drawToolbar(vp);
        drawStatusBar(vp);

        ImGuiID dock = ImGui::GetID("AnvilDock");
        ImGui::DockSpaceOverViewport(dock, vp);
        if (!layoutBuilt_) { layoutBuilt_ = true; if (!ImGui::DockBuilderGetNode(dock) || ImGui::DockBuilderGetNode(dock)->IsLeafNode()) buildLayout(dock, vp); }

        drawHierarchy();
        drawViewport();
        drawInspector();
        drawRenderSettings();
        drawProject();
        drawConsole();
        drawCodeEditor();
        drawUIDesigner();
        drawProjectHub();
        drawStartMenu();
        drawCutsceneEditor();
        drawWorldWindow();
        drawRoadsWindow();
        drawControllerDebugger();
    }

    void buildLayout(ImGuiID dock, ImGuiViewport* vp) {
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
        ImGuiID center = dock, left, right, bottom;
        left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.17f, nullptr, &center);
        right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, nullptr, &center);
        bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("UI Inspector", right);
        ImGui::DockBuilderDockWindow("Render Settings", right);
        ImGui::DockBuilderDockWindow("Project", bottom);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Code Editor", bottom);
        ImGui::DockBuilderDockWindow("UI Designer", bottom);
        ImGui::DockBuilderFinish(dock);
    }

    void drawMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            icon(Icon::Anvil, 18.0f, hex(0xC8C8C8));
            ImGui::SameLine(0, 6);
            ImGui::TextColored(hex(0xD0D0D0), "ANVIL");
            ImGui::SameLine(0, 14);
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Project", "Ctrl+S")) saveProjectScene();
                if (ImGui::MenuItem("Start Menu")) showStart_ = true;
                ImGui::MenuItem("Редактор кат-сцен", nullptr, &showCut_);
                ImGui::MenuItem("Открытый мир (стриминг)", nullptr, &showWorld_);
                ImGui::MenuItem("Дороги и трафик", nullptr, &showRoads_);
                if (ImGui::MenuItem("New Scene")) { scene_.stopPhysics(); wCells_.clear(); scene_.entities.clear(); selected_ = 0; ANVIL_LOG("New scene"); }
                if (ImGui::MenuItem("Reset Demo Scene")) { scene_.stopPhysics(); wCells_.clear(); scene_.entities.clear(); selected_ = 0; buildDefaultScene(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Project Hub", "Ctrl+Shift+P")) showHub_ = true;
                if (ImGui::MenuItem("Exit", "Alt+F4")) window_.close();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, selected_ != 0)) selected_ = scene_.duplicate(selected_).id;
                if (ImGui::MenuItem("Delete", "Del", false, selected_ != 0)) deleteSelected();
                if (ImGui::MenuItem("Focus Selected", "F", false, selected_ != 0)) focusSelected();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("GameObject")) { createMenuItems(); ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Reset Layout")) layoutBuilt_ = false, forceLayout_ = true;
                if (ImGui::MenuItem("Controller Debugger")) showControllerDebugger_ = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        if (forceLayout_) { forceLayout_ = false; buildLayout(ImGui::GetID("AnvilDock"), ImGui::GetMainViewport()); layoutBuilt_ = true; }
    }

    void createMenuItems() {
        if (ImGui::MenuItem("Cube"))        createEntity(Primitive::Cube, "Cube");
        if (ImGui::MenuItem("Sphere"))      createEntity(Primitive::Sphere, "Sphere");
        if (ImGui::MenuItem("Plane"))       createEntity(Primitive::Plane, "Plane");
        if (ImGui::MenuItem("Point Light")) createEntity(Primitive::None, "Point Light", true);
        if (ImGui::MenuItem("Empty"))       createEntity(Primitive::None, "Empty");
    }

    void drawToolbar(ImGuiViewport* vp) {
        ImGuiWindowFlags f = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, hex(0x1F1F1F));
        if (ImGui::BeginViewportSideBar("##toolbar", vp, ImGuiDir_Up, 34.0f, f)) {
            // Transform tools
            if (iconButton("##move", Icon::Move, gizmoMode_ == 0, "Move (W)")) gizmoMode_ = 0;
            ImGui::SameLine();
            if (iconButton("##rot", Icon::Rotate, gizmoMode_ == 1, "Rotate (E)")) gizmoMode_ = 1;
            ImGui::SameLine();
            if (iconButton("##scl", Icon::Scale, gizmoMode_ == 2, "Scale (R)")) gizmoMode_ = 2;

            // centered play controls
            float w = 3 * 30.0f;
            ImGui::SameLine(vp->WorkSize.x * 0.5f - w * 0.5f);
            if (iconButton("##play", playing_ ? Icon::Stop : Icon::Play, playing_, playing_ ? "Stop (restores the scene)" : "Play", playing_ ? hex(0xFF8A80) : hex(0x8FE388))) togglePlay();
            ImGui::SameLine();
            if (iconButton("##pause", Icon::Pause, paused_ && playing_, "Pause") && playing_) paused_ = !paused_;
            ImGui::SameLine();
            if (iconButton("##step", Icon::Step, false, "Step one frame") && playing_) { playTime_ += 1.0f / 60.0f; scene_.updateOrbits(playTime_); scene_.stepPhysics(1.0f / 60.0f); }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void togglePlay() {
        if (!playing_) {
            snapshot_ = scene_.entities;
            playing_ = true; paused_ = false; playTime_ = 0.0f;
            scene_.startPhysics();   // builds one backend body per Rigidbody (Jolt when compiled in)
            ANVIL_LOG("Play mode: scene snapshot saved, physics: %s (%zu bodies)", scene_.physicsBackendName(), scene_.physicsBodyCount());
        } else {
            scene_.stopPhysics();    // bodies must go before the snapshot replaces the entities
            scene_.entities = snapshot_;
            playing_ = false; paused_ = false;
            ANVIL_LOG("Stopped: scene restored");
        }
    }

    void drawStatusBar(ImGuiViewport* vp) {
        ImGuiWindowFlags f = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, hex(0x1F1F1F));
        if (ImGui::BeginViewportSideBar("##status", vp, ImGuiDir_Down, ImGui::GetFrameHeight(), f)) {
            if (ImGui::BeginMenuBar()) {
                const std::string& last = gConsole.empty() ? std::string("Ready") : gConsole.back();
                ImGui::TextDisabled("%s", last.c_str());
                char buf[96];
                std::snprintf(buf, sizeof buf, "%.1f ms  |  %d x %d  |  %s", frameMs_, renderer_->width(), renderer_->height(), playing_ ? (paused_ ? "PAUSED" : "PLAYING") : "EDIT");
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(buf).x - 14);
                ImGui::TextDisabled("%s", buf);
                ImGui::EndMenuBar();
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    // ------------------------------------------------------------------ hierarchy
    void drawHierarchyNode(uint32_t id) {
        Entity* e = scene_.find(id);
        if (!e) return;
        const auto children = scene_.childrenOf(id);
        ImGui::PushID(static_cast<int>(id));
        const bool leaf = children.empty();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
        if (leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selected_ == id) flags |= ImGuiTreeNodeFlags_Selected;
        const bool open = ImGui::TreeNodeEx("node", flags, "%s", e->name.c_str());
        if (ImGui::IsItemClicked()) selected_ = id;
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("ANVIL_ENTITY", &id, sizeof(id));
            ImGui::TextUnformatted(e->name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ANVIL_ENTITY")) {
                const uint32_t dragged = *static_cast<const uint32_t*>(payload->Data);
                if (scene_.setParent(dragged, id)) ANVIL_LOG("Reparented entity %u under %u", dragged, id);
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem()) {
            selected_ = id;
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) selected_ = scene_.duplicate(id).id;
            if (ImGui::MenuItem("Unparent", nullptr, false, e->parentId != 0)) scene_.setParent(id, 0);
            if (ImGui::MenuItem("Focus", "F")) focusSelected();
            if (ImGui::MenuItem("Delete", "Del")) deleteSelected();
            ImGui::EndPopup();
        }
        if (open && !leaf) {
            for (uint32_t child : children) drawHierarchyNode(child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void drawHierarchy() {
        ImGui::Begin("Hierarchy");
        if (ImGui::Button("+ Create")) ImGui::OpenPopup("create");
        if (ImGui::BeginPopup("create")) { createMenuItems(); ImGui::EndPopup(); }
        ImGui::Separator();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ANVIL_ENTITY")) {
                scene_.setParent(*static_cast<const uint32_t*>(payload->Data), 0);
            }
            ImGui::EndDragDropTarget();
        }
        icon(Icon::Folder, 14.0f, hex(0xE8C56A)); ImGui::SameLine(0, 6);
        ImGui::TextUnformatted(playing_ ? "Demo Scene (playing)" : "Demo Scene");
        ImGui::Indent();
        const auto roots = scene_.childrenOf(0);
        for (uint32_t id : roots) drawHierarchyNode(id);
        ImGui::Unindent();
        ImGui::End();
    }

    // ------------------------------------------------------------------ inspector
    bool vec3Row(const char* label, glm::vec3& v, float speed, float reset) {
        bool changed = false;
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(84);
        ImGui::SetNextItemWidth(-30);
        changed |= ImGui::DragFloat3("##v", &v.x, speed, 0.0f, 0.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::SmallButton("R")) { v = glm::vec3(reset); changed = true; }
        ImGui::PopID();
        return changed;
    }

    void drawInspector() {
        ImGui::Begin("Inspector");
        Entity* e = scene_.find(selected_);
        if (!e) { ImGui::TextDisabled("Nothing selected."); ImGui::TextDisabled("Click an object in the Viewport or Hierarchy."); ImGui::End(); return; }

        icon(iconFor(*e), 18.0f, e->isLight ? hex(0xF2D16B) : hex(0x8CB4E0));
        ImGui::SameLine();
        ImGui::Checkbox("##active", &e->active);
        ImGui::SameLine();
        char name[128];
        std::snprintf(name, sizeof name, "%s", e->name.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##name", name, sizeof name)) e->name = name;

        ImGui::SeparatorText("Hierarchy");
        ImGui::TextDisabled("Parent");
        ImGui::SameLine(84);
        ImGui::SetNextItemWidth(-1);
        const char* parentLabel = "<Scene Root>";
        if (Entity* parent = scene_.find(e->parentId)) parentLabel = parent->name.c_str();
        if (ImGui::BeginCombo("##parent", parentLabel)) {
            if (ImGui::Selectable("<Scene Root>", e->parentId == 0)) scene_.setParent(e->id, 0);
            for (const Entity& candidate : scene_.entities) {
                if (candidate.id == e->id) continue;
                ImGui::PushID(static_cast<int>(candidate.id));
                if (ImGui::Selectable(candidate.name.c_str(), e->parentId == candidate.id)) scene_.setParent(e->id, candidate.id);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::SeparatorText("Transform (Local)");
        vec3Row("Position", e->position, 0.05f, 0.0f);
        vec3Row("Rotation", e->rotation, 0.5f, 0.0f);
        vec3Row("Scale", e->scale, 0.02f, 1.0f);

        if (e->primitive != Primitive::None) {
            ImGui::SeparatorText("Mesh Renderer");
            static const char* kPrims[] = {"None", "Sphere", "Cube", "Plane", "OBJ Model"};
            int p = (int)e->primitive;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##mesh", &p, kPrims, 5)) e->primitive = (Primitive)p;
            if (e->primitive == Primitive::Model) {
                char modelPath[512]; std::snprintf(modelPath, sizeof modelPath, "%s", e->modelPath.c_str());
                if (ImGui::InputText("OBJ Path", modelPath, sizeof modelPath)) { e->modelPath=modelPath; std::string err; std::filesystem::path resolved(modelPath); if(resolved.is_relative()) resolved=std::filesystem::path(ANVIL_ROOT_DIR)/resolved; auto loaded=Mesh::loadObj(resolved,&err); e->modelMesh=std::shared_ptr<Mesh>(loaded.release()); if(!e->modelMesh && *modelPath) ANVIL_LOG("OBJ: %s",err.c_str()); }
            }
            ImGui::ColorEdit3("Albedo", &e->material.albedo.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Roughness", &e->material.roughness, 0.04f, 1.0f);
            ImGui::SliderFloat("Metallic", &e->material.metallic, 0.0f, 1.0f);
            char texturePath[512]; std::snprintf(texturePath, sizeof texturePath, "%s", e->material.albedoTexturePath.c_str());
            if (ImGui::InputText("Albedo Texture (PPM)", texturePath, sizeof texturePath)) {
                e->material.albedoTexturePath = texturePath; e->material.albedoTexture.reset();
                if (*texturePath) { std::filesystem::path p(texturePath); if (p.is_relative()) p = std::filesystem::path(ANVIL_ROOT_DIR) / p;
                    e->material.albedoTexture = Texture2D::load(p, true); }
            }
        }
        if (e->isLight) {
            ImGui::SeparatorText("Point Light");
            ImGui::ColorEdit3("Color", &e->lightColor.x, ImGuiColorEditFlags_Float);
            ImGui::DragFloat("Intensity", &e->lightIntensity, 1.0f, 0.0f, 5000.0f, "%.0f");
        }
        if (e->orbit) {
            ImGui::SeparatorText("Orbit (demo script)");
            vec3Row("Center", e->orbitCenter, 0.05f, 0.0f);
            ImGui::DragFloat("Radius", &e->orbitRadius, 0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Speed", &e->orbitSpeed, 0.01f, -5.0f, 5.0f);
            ImGui::TextDisabled("Runs only in Play mode.");
        }

        if (e->physics.enabled) {
            auto& c = e->physics;
            ImGui::SeparatorText("Rigidbody");
            static const char* kMotion[] = {"Static", "Kinematic", "Dynamic"};
            static const char* kShape[] = {"Box", "Sphere", "Capsule"};
            int motion = (int)c.motion;
            if (ImGui::Combo("Motion", &motion, kMotion, 3)) c.motion = (physics::MotionType)motion;
            ImGui::Checkbox("Auto-fit collider to mesh", &c.autoFit);
            const bool fitted = c.autoFit && (e->primitive == Primitive::Sphere || e->primitive == Primitive::Cube || e->primitive == Primitive::Plane);
            if (!fitted) {
                int shape = (int)c.shape;
                if (ImGui::Combo("Collider", &shape, kShape, 3)) c.shape = (physics::ShapeType)shape;
                if (c.shape == physics::ShapeType::Box) vec3Row("Half Extents", c.halfExtents, 0.02f, 1.0f);
                else ImGui::DragFloat("Radius", &c.radius, 0.01f, 0.01f, 100.0f);
                if (c.shape == physics::ShapeType::Capsule) ImGui::DragFloat("Half Height", &c.halfHeight, 0.01f, 0.01f, 100.0f);
            }
            if (c.motion == physics::MotionType::Dynamic) {
                ImGui::DragFloat("Mass", &c.mass, 0.1f, 0.01f, 100000.0f);
                ImGui::DragFloat("Gravity Scale", &c.gravityScale, 0.02f, -5.0f, 5.0f);
                ImGui::DragFloat("Linear Damping", &c.linearDamping, 0.005f, 0.0f, 10.0f);
                ImGui::DragFloat("Angular Damping", &c.angularDamping, 0.005f, 0.0f, 10.0f);
                ImGui::Checkbox("Continuous collision (fast bodies)", &c.continuous);
            }
            ImGui::SliderFloat("Friction", &c.friction, 0.0f, 1.0f);
            ImGui::SliderFloat("Bounciness", &c.restitution, 0.0f, 1.0f);
            ImGui::Checkbox("Is Trigger", &c.isTrigger);
            ImGui::TextDisabled(playing_ ? "Collider/body changes apply on the next Play." : "Simulated while Play mode runs.");
            if (ImGui::Button("Remove Rigidbody")) c.enabled = false;
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Add Component", ImVec2(-1, 0))) ImGui::OpenPopup("addcomp");
        if (ImGui::BeginPopup("addcomp")) {
            if (ImGui::MenuItem("Mesh Renderer", nullptr, false, e->primitive == Primitive::None)) e->primitive = Primitive::Sphere;
            if (ImGui::MenuItem("OBJ Model", nullptr, false, e->primitive != Primitive::Model)) e->primitive = Primitive::Model;
            if (ImGui::MenuItem("Rigidbody", nullptr, false, !e->physics.enabled)) e->physics.enabled = true;
            if (ImGui::MenuItem("Point Light", nullptr, false, !e->isLight)) e->isLight = true;
            if (ImGui::MenuItem("Orbit (demo script)", nullptr, false, !e->orbit)) { e->orbit = true; e->orbitCenter = e->position; }
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    // ------------------------------------------------------------------ render settings
    void drawRenderSettings() {
        ImGui::Begin("Render Settings");
        RenderSettings& r = renderer_->settings;
        ImGui::SeparatorText("Sun");
        bool sunChanged = false;
        sunChanged |= ImGui::SliderFloat("Azimuth", &sunAz_, -180.0f, 180.0f, "%.0f deg");
        sunChanged |= ImGui::SliderFloat("Elevation", &sunEl_, 2.0f, 90.0f, "%.0f deg");
        if (sunChanged) anglesToSun();
        ImGui::ColorEdit3("Sun Color", &scene_.sun.color.x, ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Sun Intensity", &scene_.sun.intensity, 0.0f, 10.0f);
        ImGui::SeparatorText("Shadows");
        ImGui::SliderFloat("Shadow Distance", &r.shadowDistance, 40.0f, 500.0f, "%.0f m");
        ImGui::SliderFloat("Cascade Split Blend", &r.shadowSplitLambda, 0.0f, 1.0f, "%.2f");
        ImGui::Checkbox("Sun shadows", &r.shadows);
        ImGui::SliderFloat("RHI Shadow Radius", &r.shadowRadius, 5.0f, 120.0f, "%.0f m");
        ImGui::SliderFloat("Bias", &r.shadowBias, 0.0f, 0.01f, "%.4f");
        ImGui::SeparatorText("Post Processing");
        ImGui::Checkbox("Bloom", &r.bloom);
        ImGui::SliderFloat("Threshold", &r.bloomThreshold, 0.2f, 4.0f);
        ImGui::Checkbox("Temporal AA", &r.taa);
        ImGui::SliderFloat("TAA History", &r.taaHistoryWeight, 0.0f, 0.98f, "%.2f");
        ImGui::Checkbox("Camera Motion Blur", &r.motionBlur);
        ImGui::SliderFloat("Motion Blur Strength", &r.motionBlurStrength, 0.0f, 1.0f, "%.2f");
        ImGui::TextDisabled("TAA reprojects static surfaces; moving-object velocity is not yet available.");
        ImGui::SeparatorText("Ambient Occlusion");
        ImGui::Checkbox("SSAO", &r.ssao);
        ImGui::SliderFloat("AO Radius", &r.aoRadius, 0.1f, 3.0f, "%.2f m");
        ImGui::SliderFloat("AO Strength", &r.aoStrength, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Strength", &r.bloomStrength, 0.0f, 0.5f);
        ImGui::SliderFloat("Exposure", &r.exposure, 0.1f, 4.0f);
        ImGui::SeparatorText("Stats");
        ImGui::Text("Frame: %.2f ms (%.0f FPS)", frameMs_, frameMs_ > 0 ? 1000.0f / frameMs_ : 0.0f);
        ImGui::Text("Objects: %d   Point lights: %d / %d", (int)items_.size(), (int)lights_.size(), Renderer::kMaxPointLights);
        ImGui::Text("Frustum: %u visible / %u culled", renderer_->stats.cameraVisibleObjects, renderer_->stats.cameraCulledObjects);
        ImGui::Text("Shadow casters: %u", renderer_->stats.shadowVisibleObjects);
        ImGui::TextDisabled("%s", (const char*)glGetString(GL_RENDERER));
        ImGui::End();
    }

    // ------------------------------------------------------------------ project + console
    void drawProject() {
        ImGui::Begin("Project");
        if (projDir_.empty()) projDir_ = ANVIL_ROOT_DIR;
        const fs::path root = ANVIL_ROOT_DIR;
        if (ImGui::SmallButton("..") && projDir_ != root) projDir_ = fs::path(projDir_).parent_path().string();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", fs::relative(projDir_, root.parent_path()).string().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh Index")) { std::string err; if (!assetDb_.scan(root, &err)) ANVIL_LOG("Asset scan: %s", err.c_str()); }
        ImGui::TextDisabled("Asset index: %zu files", assetDb_.assets().size());
        ImGui::Separator();
        std::vector<fs::directory_entry> entries;
        std::error_code ec;
        for (const auto& de : fs::directory_iterator(projDir_, ec)) {
            std::string n = de.path().filename().string();
            if (n.empty() || n[0] == '.' || n == "build") continue;
            entries.push_back(de);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            if (a.is_directory() != b.is_directory()) return a.is_directory();
            return a.path().filename() < b.path().filename();
        });
        for (const auto& de : entries) {
            const bool dir = de.is_directory();
            icon(dir ? Icon::Folder : Icon::File, 14.0f, dir ? hex(0xE8C56A) : hex(0xA8B0BA));
            ImGui::SameLine(0, 6);
            if (!dir) { const auto type=AssetDatabase::classify(de.path()); const char* tag=type==AssetType::Texture?" [Texture]":type==AssetType::Model?" [Model]":type==AssetType::Scene?" [Scene]":type==AssetType::Shader?" [Shader]":type==AssetType::Material?" [Material]":""; ImGui::TextDisabled("%s", tag); ImGui::SameLine(0, 4); }
            if (ImGui::Selectable(de.path().filename().string().c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && dir && ImGui::IsMouseDoubleClicked(0))
                projDir_ = de.path().string();
        }
        ImGui::End();
    }

    bool resolveProjectCodePath(fs::path& out) const {
        const fs::path root = fs::weakly_canonical(fs::path(ANVIL_ROOT_DIR));
        const fs::path candidate = fs::weakly_canonical(root / fs::path(codePath_));
        const fs::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") return false;
        out = candidate;
        return true;
    }

    void loadCodeFile() {
        fs::path path;
        if (!resolveProjectCodePath(path)) {
            ANVIL_LOG("Code Editor: path must stay inside the project.");
            return;
        }
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            ANVIL_LOG("Code Editor: cannot open %s", codePath_);
            return;
        }
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (contents.size() >= codeBuffer_.size()) {
            ANVIL_LOG("Code Editor: file exceeds the 512 KiB editor buffer.");
            return;
        }
        std::fill(codeBuffer_.begin(), codeBuffer_.end(), '\0');
        std::copy(contents.begin(), contents.end(), codeBuffer_.begin());
        codeDirty_ = false;
        ANVIL_LOG("Code Editor: loaded %s", codePath_);
    }

    void saveCodeFile() {
        fs::path path;
        if (!resolveProjectCodePath(path)) {
            ANVIL_LOG("Code Editor: path must stay inside the project.");
            return;
        }
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            ANVIL_LOG("Code Editor: cannot create parent directory: %s", ec.message().c_str());
            return;
        }
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            ANVIL_LOG("Code Editor: cannot write %s", codePath_);
            return;
        }
        file.write(codeBuffer_.data(), (std::streamsize)std::strlen(codeBuffer_.data()));
        if (!file) {
            ANVIL_LOG("Code Editor: write failed for %s", codePath_);
            return;
        }
        codeDirty_ = false;
        ANVIL_LOG("Code Editor: saved %s", codePath_);
    }

    void drawCodeEditor() {
        ImGui::Begin("Code Editor");
        ImGui::SetNextItemWidth(-150.0f);
        ImGui::InputText("Project file", codePath_, sizeof(codePath_));
        ImGui::SameLine();
        if (ImGui::Button("Open")) loadCodeFile();
        ImGui::SameLine();
        if (ImGui::Button("Save")) saveCodeFile();
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            std::fill(codeBuffer_.begin(), codeBuffer_.end(), '\0');
            codeDirty_ = true;
        }
        ImGui::TextDisabled("%s", codeDirty_ ? "Modified (not saved)" : "Saved / clean");
        ImGui::Separator();
        if (ImGui::InputTextMultiline("##anvil_source", codeBuffer_.data(), codeBuffer_.size(),
                                      ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_AllowTabInput)) {
            codeDirty_ = true;
        }
        ImGui::End();
    }

    // ------------------------------------------------------------------ visual UI designer (M40)
    void buildDefaultUIDocument() {
        uiRoot_.name = "Canvas";
        uiRoot_.layout.mode = ui::LayoutMode::Absolute;
        uiRoot_.style.background = {0.075f, 0.085f, 0.105f, 1.0f};
        auto title = std::make_unique<ui::Widget>(ui::WidgetType::Label, "Title");
        title->text = "ANVIL GAME";
        title->layout.preferredSize = {360, 58};
        title->layout.offsets = {220, 72, 0, 0};
        title->style.foreground = {0.95f, 0.68f, 0.25f, 1.0f};
        uiRoot_.add(std::move(title));
        auto play = std::make_unique<ui::Widget>(ui::WidgetType::Button, "PlayButton");
        play->text = "PLAY";
        play->layout.preferredSize = {220, 56};
        play->layout.offsets = {290, 190, 0, 0};
        play->style.background = {0.16f, 0.36f, 0.62f, 1.0f};
        play->style.foreground = {1.0f, 1.0f, 1.0f, 1.0f};
        play->style.cornerRadius = 8.0f;
        uiRoot_.add(std::move(play));
        uiSelected_ = uiRoot_.children().back()->id();
        ui::LayoutEngine::arrange(uiRoot_, {{0, 0}, {800, 450}});
    }

    void drawUIDesignerNode(ui::Widget& widget) {
        const std::string label = widget.name.empty() ? "Widget" : widget.name;
        const bool leaf = widget.children().empty();
        ImGui::PushID(static_cast<int>(widget.id() & 0x7fffffff));
        if (leaf) {
            if (ImGui::Selectable(label.c_str(), uiSelected_ == widget.id())) uiSelected_ = widget.id();
        } else {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (uiSelected_ == widget.id()) flags |= ImGuiTreeNodeFlags_Selected;
            const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
            if (ImGui::IsItemClicked()) uiSelected_ = widget.id();
            if (open) { for (const auto& child : widget.children()) drawUIDesignerNode(*child); ImGui::TreePop(); }
        }
        ImGui::PopID();
    }

    void drawUIPreviewNode(ui::Widget& widget, ImDrawList* draw, ImVec2 origin, float scale, int depth) {
        if (!widget.visible) return;
        const ImVec2 p(origin.x + widget.bounds.position.x * scale, origin.y + widget.bounds.position.y * scale);
        const ImVec2 q(p.x + widget.bounds.size.x * scale, p.y + widget.bounds.size.y * scale);
        if (depth > 0) {
            const auto& c = widget.style.background;
            ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a * widget.style.opacity));
            const float radius = std::max(0.0f, widget.style.cornerRadius * scale);
            if (widget.type == ui::WidgetType::Button || c.a > 0.001f)
                draw->AddRectFilled(p, q, bg, radius);
            draw->AddRect(p, q, widget.id() == uiSelected_ ? IM_COL32(255, 190, 70, 255) : IM_COL32(90, 100, 115, 180), radius, 0, widget.id() == uiSelected_ ? 2.0f : 1.0f);
            if (!widget.text.empty()) {
                const auto& f = widget.style.foreground;
                draw->AddText(ImVec2(p.x + 8.0f * scale, p.y + 7.0f * scale), ImGui::ColorConvertFloat4ToU32(ImVec4(f.r, f.g, f.b, f.a)), widget.text.c_str());
            } else if (widget.type == ui::WidgetType::Label) {
                draw->AddText(ImVec2(p.x + 6, p.y + 6), IM_COL32(220, 220, 220, 255), widget.name.c_str());
            }
        }
        for (const auto& child : widget.children()) drawUIPreviewNode(*child, draw, origin, scale, depth + 1);
    }

    void drawUIDesigner() {
        ImGui::Begin("UI Designer");
        if (ImGui::Button("+ Panel")) addUIWidget(ui::WidgetType::Panel);
        ImGui::SameLine(); if (ImGui::Button("+ Button")) addUIWidget(ui::WidgetType::Button);
        ImGui::SameLine(); if (ImGui::Button("+ Label")) addUIWidget(ui::WidgetType::Label);
        ImGui::SameLine(); if (ImGui::Button("Delete") && uiSelected_ != uiRoot_.id()) {
            if (removeUIWidget(uiRoot_, uiSelected_)) uiSelected_ = uiRoot_.id();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save UI")) {
            std::error_code ec; fs::create_directories(fs::path(ANVIL_ROOT_DIR) / "assets/ui", ec);
            std::string err;
            if (!ui::saveDocument(uiRoot_, (fs::path(ANVIL_ROOT_DIR) / "assets/ui/main.anvilui").string(), &err)) ANVIL_LOG("UI save failed: %s", err.c_str());
            else ANVIL_LOG("UI document saved: assets/ui/main.anvilui");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load UI")) {
            std::string err; auto loaded = ui::loadDocument((fs::path(ANVIL_ROOT_DIR) / "assets/ui/main.anvilui").string(), &err);
            if (loaded) { uiRoot_ = std::move(*loaded); uiSelected_ = uiRoot_.id(); }
            else ANVIL_LOG("UI load failed: %s", err.c_str());
        }
        ImGui::Separator();
        const float leftWidth = std::max(150.0f, ImGui::GetContentRegionAvail().x * 0.22f);
        ImGui::BeginChild("ui_hierarchy", ImVec2(leftWidth, 0), true);
        ImGui::TextDisabled("WIDGET TREE");
        drawUIDesignerNode(uiRoot_);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("ui_canvas_area", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextDisabled("PREVIEW · 800 × 450 · Drag widgets to position");
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const float scale = std::max(0.05f, std::min(avail.x / 800.0f, std::max(1.0f, avail.y) / 450.0f));
        const ImVec2 canvasSize(800.0f * scale, 450.0f * scale);
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("ui_preview_surface", canvasSize, ImGuiButtonFlags_MouseButtonLeft);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const auto& rootBg = uiRoot_.style.background;
        draw->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), ImGui::ColorConvertFloat4ToU32(ImVec4(rootBg.r, rootBg.g, rootBg.b, rootBg.a)));
        ui::LayoutEngine::arrange(uiRoot_, {{0, 0}, {800, 450}});
        drawUIPreviewNode(uiRoot_, draw, canvasPos, scale, 0);
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool inCanvas = mouse.x >= canvasPos.x && mouse.y >= canvasPos.y && mouse.x < canvasPos.x + canvasSize.x && mouse.y < canvasPos.y + canvasSize.y;
        if (inCanvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ui::Widget* hit = ui::hitTest(uiRoot_, {(mouse.x - canvasPos.x) / scale, (mouse.y - canvasPos.y) / scale});
            if (hit) uiSelected_ = hit->id();
            ui::Widget* selected = uiRoot_.find(uiSelected_);
            if (selected && selected != &uiRoot_) { uiDragActive_ = true; uiDragMouseStart_ = mouse; uiDragOffsetStart_ = {selected->layout.offsets.left, selected->layout.offsets.top}; }
        }
        if (uiDragActive_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (ui::Widget* selected = uiRoot_.find(uiSelected_)) {
                selected->layout.offsets.left = uiDragOffsetStart_.x + (mouse.x - uiDragMouseStart_.x) / scale;
                selected->layout.offsets.top = uiDragOffsetStart_.y + (mouse.y - uiDragMouseStart_.y) / scale;
            }
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) uiDragActive_ = false;
        ImGui::EndChild();
        ImGui::End();
        drawUIInspector();
    }

    void addUIWidget(ui::WidgetType type) {
        ui::Widget* parent = uiRoot_.find(uiSelected_);
        if (!parent || (parent->type != ui::WidgetType::Panel && parent != &uiRoot_)) parent = &uiRoot_;
        const char* kind = type == ui::WidgetType::Button ? "Button" : type == ui::WidgetType::Label ? "Label" : "Panel";
        auto item = std::make_unique<ui::Widget>(type, std::string(kind) + std::to_string(parent->children().size() + 1));
        item->text = type == ui::WidgetType::Button ? "Button" : type == ui::WidgetType::Label ? "Label" : "";
        item->layout.preferredSize = type == ui::WidgetType::Label ? ui::Vec2{180, 36} : ui::Vec2{180, 52};
        item->layout.offsets = {40.0f + 18.0f * static_cast<float>(parent->children().size()), 40.0f + 18.0f * static_cast<float>(parent->children().size()), 0, 0};
        if (type == ui::WidgetType::Button) item->style.background = {0.16f, 0.36f, 0.62f, 1.0f};
        uiSelected_ = parent->add(std::move(item)).id();
    }

    bool removeUIWidget(ui::Widget& parent, ui::WidgetId id) {
        if (parent.remove(id)) return true;
        for (const auto& child : parent.children()) if (removeUIWidget(*child, id)) return true;
        return false;
    }

    void drawUIInspector() {
        ImGui::Begin("UI Inspector");
        ui::Widget* w = uiRoot_.find(uiSelected_);
        if (!w) { ImGui::TextDisabled("Select a widget in the UI Designer."); ImGui::End(); return; }
        if (uiInspectorId_ != w->id()) {
            uiInspectorId_ = w->id();
            std::snprintf(uiNameBuffer_, sizeof(uiNameBuffer_), "%s", w->name.c_str());
            std::snprintf(uiTextBuffer_, sizeof(uiTextBuffer_), "%s", w->text.c_str());
        }
        if (ImGui::InputText("Name", uiNameBuffer_, sizeof(uiNameBuffer_))) w->name = uiNameBuffer_;
        if (ImGui::InputText("Text", uiTextBuffer_, sizeof(uiTextBuffer_))) w->text = uiTextBuffer_;
        ImGui::SeparatorText("Rect Transform");
        ImGui::DragFloat2("Position", &w->layout.offsets.left, 1.0f);
        ImGui::DragFloat2("Size", &w->layout.preferredSize.x, 1.0f, 1.0f, 4096.0f);
        ImGui::DragFloat4("Margins", &w->layout.margin.left, 1.0f);
        ImGui::SeparatorText("Appearance");
        float bg[4] = {w->style.background.r, w->style.background.g, w->style.background.b, w->style.background.a};
        if (ImGui::ColorEdit4("Background", bg)) w->style.background = {bg[0], bg[1], bg[2], bg[3]};
        float fg[4] = {w->style.foreground.r, w->style.foreground.g, w->style.foreground.b, w->style.foreground.a};
        if (ImGui::ColorEdit4("Foreground", fg)) w->style.foreground = {fg[0], fg[1], fg[2], fg[3]};
        ImGui::DragFloat("Corner Radius", &w->style.cornerRadius, 0.5f, 0.0f, 64.0f);
        ImGui::Checkbox("Visible", &w->visible);
        ImGui::Checkbox("Enabled", &w->enabled);
        ImGui::End();
    }

    void drawConsole() {
        ImGui::Begin("Console");
        if (ImGui::SmallButton("Clear")) gConsole.clear();
        ImGui::Separator();
        ImGui::BeginChild("log", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const std::string& l : gConsole) ImGui::TextUnformatted(l.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::End();
    }

    // ------------------------------------------------------------------ viewport: camera, picking, gizmo
    bool project(const glm::mat4& vp, const glm::vec3& p, ImVec2& out) const {
        glm::vec4 c = vp * glm::vec4(p, 1.0f);
        if (c.w <= 0.001f) return false;
        glm::vec3 n = glm::vec3(c) / c.w;
        out = ImVec2(vpMin_.x + (n.x * 0.5f + 0.5f) * vpSize_.x, vpMin_.y + (1.0f - (n.y * 0.5f + 0.5f)) * vpSize_.y);
        return true;
    }

    static float distPointSegment(ImVec2 p, ImVec2 a, ImVec2 b) {
        float vx = b.x - a.x, vy = b.y - a.y, wx = p.x - a.x, wy = p.y - a.y;
        float len2 = vx * vx + vy * vy;
        float t = len2 > 0 ? glm::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
        float dx = a.x + vx * t - p.x, dy = a.y + vy * t - p.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    void mouseRay(const glm::mat4& inv, ImVec2 mouse, glm::vec3& o, glm::vec3& d) const {
        float nx = (mouse.x - vpMin_.x) / vpSize_.x * 2.0f - 1.0f;
        float ny = 1.0f - (mouse.y - vpMin_.y) / vpSize_.y * 2.0f;
        glm::vec4 n = inv * glm::vec4(nx, ny, -1.0f, 1.0f), f = inv * glm::vec4(nx, ny, 1.0f, 1.0f);
        n /= n.w; f /= f.w;
        o = glm::vec3(n);
        d = glm::normalize(glm::vec3(f) - glm::vec3(n));
    }

    // returns hit distance along the (world) ray, or -1
    float rayHit(const Entity& e, const glm::vec3& o, const glm::vec3& d) const {
        glm::mat4 inv = glm::inverse(scene_.worldModel(e.id));
        glm::vec3 lo = glm::vec3(inv * glm::vec4(o, 1.0f)), ld = glm::vec3(inv * glm::vec4(d, 0.0f));
        if (e.primitive == Primitive::Sphere) {
            float a = glm::dot(ld, ld), b = 2.0f * glm::dot(lo, ld), c = glm::dot(lo, lo) - 1.0f;
            float disc = b * b - 4 * a * c;
            if (disc < 0) return -1.0f;
            float t = (-b - std::sqrt(disc)) / (2 * a);
            return t >= 0 ? t : -1.0f;
        }
        glm::vec3 h = Scene::localHalfExtents(e);
        float tmin = 0.0f, tmax = 1e9f;
        for (int i = 0; i < 3; ++i) {
            if (std::abs(ld[i]) < 1e-8f) { if (lo[i] < -h[i] || lo[i] > h[i]) return -1.0f; continue; }
            float t1 = (-h[i] - lo[i]) / ld[i], t2 = (h[i] - lo[i]) / ld[i];
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1); tmax = std::min(tmax, t2);
            if (tmin > tmax) return -1.0f;
        }
        return tmin;
    }

    void drawViewport() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        vpSize_ = ImVec2(std::max(avail.x, 16.0f), std::max(avail.y, 16.0f));
        vpMin_ = ImGui::GetCursorScreenPos();
        ImGui::Image(texId(renderer_->outputTexture()), vpSize_, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SetCursorScreenPos(vpMin_);
        ImGui::InvisibleButton("##vp", vpSize_, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // fly camera: hold RMB, WASD/QE, Shift = fast, wheel = dolly
        if (active && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            cam_.yaw += io.MouseDelta.x * 0.15f;
            cam_.pitch = glm::clamp(cam_.pitch - io.MouseDelta.y * 0.15f, -89.0f, 89.0f);
            float sp = (io.KeyShift ? 20.0f : 7.0f) * io.DeltaTime;
            if (ImGui::IsKeyDown(ImGuiKey_W)) cam_.position += cam_.forward() * sp;
            if (ImGui::IsKeyDown(ImGuiKey_S)) cam_.position -= cam_.forward() * sp;
            if (ImGui::IsKeyDown(ImGuiKey_D)) cam_.position += cam_.right() * sp;
            if (ImGui::IsKeyDown(ImGuiKey_A)) cam_.position -= cam_.right() * sp;
            if (ImGui::IsKeyDown(ImGuiKey_E)) cam_.position.y += sp;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) cam_.position.y -= sp;
        }
        if (hovered && io.MouseWheel != 0.0f) cam_.position += cam_.forward() * io.MouseWheel * 2.0f;

        const float aspect = vpSize_.x / vpSize_.y;
        const glm::mat4 view = cam_.view(), proj = cam_.proj(aspect), vp = proj * view;
        const glm::mat4 invVP = glm::inverse(vp);

        // light icons
        for (const Entity& e : scene_.entities) {
            if (!e.isLight || !e.active) continue;
            ImVec2 s;
            if (!project(vp, glm::vec3(scene_.worldModel(e.id)[3]), s)) continue;
            dl->AddImage(texId(icons::texture(Icon::Light)), ImVec2(s.x - 11, s.y - 11), ImVec2(s.x + 11, s.y + 11), ImVec2(0, 0), ImVec2(1, 1),
                         ImGui::ColorConvertFloat4ToU32(ImVec4(e.lightColor.r * 0.4f + 0.6f, e.lightColor.g * 0.4f + 0.6f, e.lightColor.b * 0.4f + 0.6f, 0.95f)));
        }

        Entity* sel = scene_.find(selected_);
        int hoverAxis = -1;
        glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        const ImU32 axisCol[3] = {IM_COL32(230, 70, 70, 255), IM_COL32(90, 200, 90, 255), IM_COL32(80, 130, 240, 255)};

        if (sel) {
            // selection outline (oriented bounding box)
            glm::vec3 h = Scene::localHalfExtents(*sel);
            glm::mat4 m = scene_.worldModel(sel->id);
            ImVec2 c[8]; bool ok[8];
            for (int i = 0; i < 8; ++i) ok[i] = project(vp, glm::vec3(m * glm::vec4((i & 1 ? h.x : -h.x), (i & 2 ? h.y : -h.y), (i & 4 ? h.z : -h.z), 1.0f)), c[i]);
            const int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
            for (auto& ed : edges) if (ok[ed[0]] && ok[ed[1]]) dl->AddLine(c[ed[0]], c[ed[1]], IM_COL32(240, 170, 60, 200), 1.5f);

            // move gizmo, constant screen size
            ImVec2 o;
            const glm::vec3 selectedWorld(scene_.worldModel(sel->id)[3]);
            if (project(vp, selectedWorld, o)) {
                float len = glm::length(selectedWorld - cam_.position) * 0.16f;
                ImVec2 tip[3]; bool tipOk[3];
                float best = 9.0f;
                for (int a = 0; a < 3; ++a) {
                    tipOk[a] = project(vp, selectedWorld + axes[a] * len, tip[a]);
                    if (tipOk[a] && hovered && dragAxis_ < 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        float d = distPointSegment(io.MousePos, o, tip[a]);
                        if (d < best) { best = d; hoverAxis = a; }
                    }
                }
                int shown = dragAxis_ >= 0 ? dragAxis_ : hoverAxis;
                for (int a = 0; a < 3; ++a) {
                    if (!tipOk[a]) continue;
                    ImU32 col = (a == shown) ? IM_COL32(255, 230, 90, 255) : axisCol[a];
                    dl->AddLine(o, tip[a], col, a == shown ? 4.0f : 3.0f);
                    if (gizmoMode_ == 1) {
                        dl->AddCircle(tip[a], 7.0f, col, 16, 2.5f);
                    } else if (gizmoMode_ == 2) {
                        dl->AddRectFilled(ImVec2(tip[a].x - 5, tip[a].y - 5),
                                          ImVec2(tip[a].x + 5, tip[a].y + 5), col);
                    } else {
                        ImVec2 dir(tip[a].x - o.x, tip[a].y - o.y);
                        float l = std::sqrt(dir.x * dir.x + dir.y * dir.y) + 1e-5f;
                        dir = ImVec2(dir.x / l, dir.y / l);
                        ImVec2 n(-dir.y, dir.x);
                        dl->AddTriangleFilled(ImVec2(tip[a].x + dir.x * 12, tip[a].y + dir.y * 12),
                                              ImVec2(tip[a].x + n.x * 5, tip[a].y + n.y * 5), ImVec2(tip[a].x - n.x * 5, tip[a].y - n.y * 5), col);
                    }
                }
                dl->AddCircleFilled(o, 4.0f, IM_COL32(240, 240, 240, 255));
            }
        }

        bool roadHandled = false;
        if (roadTool_ && ImGui::IsItemClicked(ImGuiMouseButton_Left)) { glm::vec3 ro, rd; mouseRay(invVP, io.MousePos, ro, rd); roadHandled = roadClick(ro, rd); }
        drawRoadsOverlay(dl, vp);
        // left click: start axis drag or pick
        if (!roadHandled && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            glm::vec3 ro, rd;
            mouseRay(invVP, io.MousePos, ro, rd);
            if (sel && hoverAxis >= 0) {
                dragAxis_ = hoverAxis;
                dragStartPos_ = sel->position;
                dragStartWorld_ = glm::vec3(scene_.worldModel(sel->id)[3]);
                dragStartRotation_ = sel->rotation;
                dragStartScale_ = sel->scale;
                dragStartS_ = axisParam(dragStartWorld_, axes[hoverAxis], ro, rd);
            } else {
                float bestT = 1e9f; uint32_t pick = 0;
                for (const Entity& e : scene_.entities) {
                    if (!e.active) continue;
                    float t = rayHit(e, ro, rd);
                    if (t >= 0 && t < bestT) { bestT = t; pick = e.id; }
                }
                selected_ = pick;
            }
        }
        if (dragAxis_ >= 0) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && sel) {
                glm::vec3 ro, rd;
                mouseRay(invVP, io.MousePos, ro, rd);
                float s = axisParam(dragStartWorld_, axes[dragAxis_], ro, rd);
                const float delta = s - dragStartS_;
                if (gizmoMode_ == 0) {
                    glm::vec3 worldPosition = dragStartWorld_ + axes[dragAxis_] * delta;
                    glm::mat4 parentWorld = sel->parentId ? scene_.worldModel(sel->parentId) : glm::mat4(1.0f);
                    sel->position = glm::vec3(glm::inverse(parentWorld) * glm::vec4(worldPosition, 1.0f));
                } else if (gizmoMode_ == 1) {
                    sel->rotation = dragStartRotation_;
                    sel->rotation[dragAxis_] += delta * 90.0f;
                } else {
                    sel->scale = dragStartScale_;
                    sel->scale[dragAxis_] = glm::max(0.01f, dragStartScale_[dragAxis_] + delta * 0.25f);
                }
            } else dragAxis_ = -1;
        }

        // viewport overlay label
        dl->AddText(ImVec2(vpMin_.x + 10, vpMin_.y + 8), IM_COL32(255, 255, 255, 140), "Perspective");
        drawCutsceneOverlay(dl);
        ImGui::End();
    }

    // parameter along the (infinite) axis line closest to the mouse ray
    static float axisParam(const glm::vec3& p, const glm::vec3& a, const glm::vec3& ro, const glm::vec3& rd) {
        glm::vec3 w0 = p - ro;
        float b = glm::dot(a, rd), d = glm::dot(a, w0), e = glm::dot(rd, w0);
        float den = 1.0f - b * b;
        return std::abs(den) < 1e-4f ? 0.0f : (b * e - d) / den;
    }

    // ------------------------------------------------------------------ state
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ControllerSystem> controllers_;
    std::unordered_map<int,bool> rumbleActive_;
    std::unordered_map<int,int> captureTargetLogical_;
    int captureDeviceId_ = -1;
    std::string controllerCaptureStatus_;
    bool showControllerDebugger_ = false;
    Scene scene_;
    AssetDatabase assetDb_;
    Camera cam_;
    std::vector<DrawItem> items_;
    std::vector<PointLight> lights_;

    uint32_t selected_ = 0;
    ImVec2 vpSize_{1280, 720}, vpMin_{0, 0};
    int dragAxis_ = -1;
    int gizmoMode_ = 0; // 0 Move, 1 Rotate, 2 Scale
    glm::vec3 dragStartPos_{0.0f}, dragStartWorld_{0.0f}, dragStartRotation_{0.0f}, dragStartScale_{1.0f};
    float dragStartS_ = 0.0f;

    bool playing_ = false, paused_ = false;
    float playTime_ = 0.0f;
    std::vector<Entity> snapshot_;

    ui::Widget uiRoot_{ui::WidgetType::Panel, "Canvas"};
    ui::WidgetId uiSelected_ = 0, uiInspectorId_ = 0;
    bool uiDragActive_ = false;
    ImVec2 uiDragMouseStart_{};
    ui::Vec2 uiDragOffsetStart_{};
    char uiNameBuffer_[128]{};
    char uiTextBuffer_[256]{};

    float sunAz_ = 0.0f, sunEl_ = 60.0f;
    float frameMs_ = 16.0f;
    int frameNo_ = 0;
    bool layoutBuilt_ = false, forceLayout_ = false;
    std::string projDir_, projectRoot_;
    bool showHub_ = false;
    bool showStart_ = true;
    OpenWorldSettings world_; bool showWorld_ = false; 
    std::map<long long, WCell> wCells_;
    RoadNetwork roads_; TrafficSim traffic_; bool showRoads_ = false, roadTool_ = false, trafficOn_ = false; uint32_t roadChain_ = 0;
    int roadFwd_ = 1, roadBack_ = 1; float roadLane_ = 3.5f, roadSpeedKmh_ = 50.0f;
    Cutscene cut_; bool showCut_ = false, cutPlay_ = false, cutPreview_ = false, cutLoop_ = false;
    float cutTime_ = 0, cutPrev_ = 0; Camera cutSavedCam_; std::vector<std::pair<std::string, CutXformKey>> cutSavedXf_;
    int cutSelCam_ = -1, cutEvType_ = 0; float cutEvDur_ = 2.5f, cutEvValue_ = 1; char cutEvText_[256] = "", cutEvSpeaker_[64] = ""; std::string cutStatus_;
    int startTab_ = 0, startTemplate_ = 1;
    char startName_[128] = "MyGame";
    std::string startStatus_;
    char hubProjectName_[128] = "NewProject";
    char hubProjectPath_[512] = "Projects/NewProject";
    char hubOpenPath_[512] = "Projects";
    char hubImportSource_[512] = "";
    char hubImportDest_[512] = "ImportedProject";
    bool hubTargets_[6] = {true,false,false,false,false,false};
    int hubTemplate_ = 2;
    std::string hubStatus_, hubImportSummary_;
    ImportReport hubImportReport_;
    char codePath_[512] = "scripts/main.cpp";
    std::vector<char> codeBuffer_ = std::vector<char>(512 * 1024, '\0');
    bool codeDirty_ = false;
};

int main() {
    Editor app;
    app.run();
    return 0;
}

#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>
struct GLFWwindow;
#if defined(__ANDROID__)
struct AInputEvent;
#endif
namespace anvil {
enum class ControllerConnection { Unknown, USB, Bluetooth };
enum class ControllerFeature { Rumble, TriggerRumble, Touchpad, Gyroscope, Accelerometer, AdaptiveTriggers, Speaker, Microphone, Lightbar, PlayerLED, Battery };
enum class InputDeviceType { None, KeyboardMouse, Gamepad };
enum class ControllerGlyphStyle { Generic, Xbox, PlayStation, Nintendo };
struct ControllerInfo {
 int id=-1; std::string name, guid, mapping, manufacturer; int vendorId=0, productId=0;
 ControllerConnection connection=ControllerConnection::Unknown;
 int buttonCount=0, axisCount=0; float battery=-1.0f;
 std::vector<ControllerFeature> features;
};
struct ControllerState {
 std::array<bool,64> buttons{};
 std::array<float,16> axes{};
 std::array<bool,64> physicalButtons{};
 std::array<float,16> physicalAxes{};
 std::array<unsigned char,16> hats{};
 int physicalButtonCount=0,physicalAxisCount=0,physicalHatCount=0;
 bool connected=false;
};
struct ControllerMotionState {
 std::array<float,4> orientation{0.0f,0.0f,0.0f,1.0f};
 std::array<float,3> acceleration{};
 std::array<float,3> angularVelocity{};
 bool available=false;
};
struct AxisSettings { float deadzone=0.12f, sensitivity=1.0f; bool inverted=false; };
struct AxisBinding { int axis=-1; float scale=1.0f; };
enum class ControllerCaptureKind { None, Button, Axis };
struct ControllerCaptureResult {
 ControllerCaptureKind kind=ControllerCaptureKind::None;
 int index=-1;
 float value=0.0f;
};
class ControllerSystem {
public:
 explicit ControllerSystem(GLFWwindow* window);
 ~ControllerSystem();
 ControllerSystem(const ControllerSystem&) = delete;
 ControllerSystem& operator=(const ControllerSystem&) = delete;
 std::vector<ControllerInfo> connected() const;
 ControllerState state(int joystickId) const;
 ControllerMotionState motionState(int joystickId) const;
 bool loadGamepadMappings(const std::filesystem::path& file,std::size_t* loaded=nullptr,std::string* error=nullptr);
 void notifyKeyboardMouseActivity();
 InputDeviceType activeInputDevice() const;
 void setAxisSettings(int joystickId,int axis,AxisSettings settings);
 AxisSettings axisSettings(int joystickId,int axis) const;
 bool setRumble(int joystickId,float lowFrequency,float highFrequency);
 // Steam Input can route these values to Xbox impulse-trigger motors when the
 // controller/driver supports them. Other backends return false.
 bool setTriggerRumble(int joystickId,float leftTrigger,float rightTrigger);
 // Steamworks must already be initialized by the host application. The manifest
 // defines the action set and action names; this class never initializes SteamAPI.
 bool initializeSteamInput(const std::filesystem::path& actionManifest,const std::string& actionSet,std::string* error=nullptr);
 void shutdownSteamInput();
 bool steamInputAvailable() const;
 void bindSteamDigitalAction(std::string steamAction,int button);
 void bindSteamAnalogAction(std::string steamAction,int xAxis,int yAxis=-1);
#if defined(__ANDROID__)
 bool handleAndroidInputEvent(const AInputEvent* event);
#endif
 void setButtonMapping(int joystickId,int physicalButton,int logicalButton);
 int buttonMapping(int joystickId,int physicalButton) const;
 void resetButtonMappings(int joystickId);
 // Starts a one-shot capture. pollInputCapture returns the first newly pressed
 // button or axis moved beyond the activation threshold. Call repeatedly from
 // the editor update loop; cancelInputCapture abandons the operation.
 bool beginInputCapture(int joystickId,float axisThreshold=0.65f);
 ControllerCaptureResult pollInputCapture();
 void cancelInputCapture();
 bool inputCapturePending() const;
 static const char* buttonGlyph(int button,ControllerGlyphStyle style);
 static ControllerGlyphStyle inferGlyphStyle(const std::string& controllerName);
 std::string buttonPrompt(int joystickId,int button) const;
 void bindAction(std::string action,std::vector<int> buttons);
 bool actionPressed(int joystickId,const std::string& action) const;
 void bindAxis(std::string action,AxisBinding binding);
 float axisValue(int joystickId,const std::string& action) const;
 bool saveProfile(const std::filesystem::path& file,const std::string& guid,std::string* error=nullptr) const;
 bool loadProfile(const std::filesystem::path& file,const std::string& guid,std::string* error=nullptr);
 static const char* buttonName(int button);
private:
 struct NativeBackend;
 GLFWwindow* window_;
 std::unique_ptr<NativeBackend> native_;
 std::unordered_map<std::uint64_t,AxisSettings> axisSettings_;
 std::unordered_map<int,std::array<int,64>> buttonMappings_;
 std::unordered_map<std::string,std::vector<int>> actionButtons_;
 std::unordered_map<std::string,AxisBinding> actionAxes_;
 std::unordered_map<std::string,int> steamDigitalBindings_;
 std::unordered_map<std::string,std::pair<int,int>> steamAnalogBindings_;
 mutable InputDeviceType activeInputDevice_=InputDeviceType::KeyboardMouse;
 struct CaptureState {
  int deviceId=-1;
  float axisThreshold=0.65f;
  std::array<bool,64> previousButtons{};
  std::array<float,16> initialAxes{};
  bool pending=false;
 } capture_;
};
}

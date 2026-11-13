#include <GLFW/glfw3.h>
#include "anvil/Controller.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
static bool present=true,gamepad=false;static int mappingCount=0;static unsigned char physicalButtons[4]={0,1,0,0};static float physicalAxes[2]={0.5f,-1.0f};static unsigned char physicalHats[1]={GLFW_HAT_UP|GLFW_HAT_RIGHT};static float rumbleLow=0,rumbleHigh=0;
extern "C" {
int glfwJoystickPresent(int id){return (present&&id==0)?1:0;}const char*glfwGetJoystickName(int){return "Test HID";}const char*glfwGetJoystickGUID(int){return "030000005e0400008e02000000000000";}int glfwJoystickIsGamepad(int){return gamepad?1:0;}
const unsigned char*glfwGetJoystickButtons(int,int*n){*n=4;return physicalButtons;}const float*glfwGetJoystickAxes(int,int*n){*n=2;return physicalAxes;}const unsigned char*glfwGetJoystickHats(int,int*n){*n=1;return physicalHats;}
int glfwGetGamepadState(int,GLFWgamepadstate*){return 0;}int glfwUpdateGamepadMappings(const char*line){if(line&&std::string(line).find(",")!=std::string::npos){++mappingCount;return 1;}return 0;}int glfwSetJoystickRumble(int,float l,float h){rumbleLow=l;rumbleHigh=h;return 1;}
}
int main(){
 auto mappingFile=std::filesystem::temp_directory_path()/"anvil_gamecontrollerdb.txt";{std::ofstream map(mappingFile);map<<"# SDL controller DB\n030000005e0400008e02000000000000,Test Controller,a:b0,b:b1\n";}anvil::ControllerSystem c(nullptr);std::size_t loaded=0;std::string mappingError;assert(c.loadGamepadMappings(mappingFile,&loaded,&mappingError)&&loaded==1&&mappingCount==1);std::error_code mapEc;std::filesystem::remove(mappingFile,mapEc);auto devices=c.connected();assert(devices.size()==1&&devices[0].name=="Test HID"&&devices[0].buttonCount==4);assert(devices[0].vendorId==0x045e&&devices[0].productId==0x028e&&devices[0].manufacturer=="Microsoft");assert(c.activeInputDevice()==anvil::InputDeviceType::KeyboardMouse);
 auto s=c.state(0);assert(c.activeInputDevice()==anvil::InputDeviceType::Gamepad);assert(s.connected&&s.buttons[1]&&s.buttons[11]&&s.buttons[12]&&s.axes[0]>0.3f&&s.physicalButtonCount==4&&s.physicalAxisCount==2&&s.physicalHatCount==1&&s.physicalButtons[1]);
 c.setAxisSettings(0,0,{0.2f,2.0f,true});s=c.state(0);assert(s.axes[0]<-0.7f);
 assert(!c.motionState(0).available);assert(!c.setTriggerRumble(0,0.5f,0.5f));
 c.setButtonMapping(0,1,0);s=c.state(0);assert(s.buttons[0]&&!s.buttons[1]&&s.physicalButtons[1]);assert(c.buttonMapping(0,1)==0);assert(c.buttonPrompt(0,0)=="South 1");assert(anvil::ControllerSystem::inferGlyphStyle("Xbox Wireless Controller")==anvil::ControllerGlyphStyle::Xbox);assert(std::string(anvil::ControllerSystem::buttonGlyph(0,anvil::ControllerGlyphStyle::Nintendo))=="B");c.bindAction("Jump Action",{0,2});assert(c.actionPressed(0,"Jump Action"));c.bindAxis("Move",{1,-1});assert(c.axisValue(0,"Move")>0.9f);
 assert(c.setRumble(0,0.4f,0.8f));assert(rumbleLow==0.4f&&rumbleHigh==0.8f);
 // Capture detects only new button presses, then supports axis capture.
 physicalButtons[1]=0;physicalButtons[2]=0;physicalAxes[0]=0.0f;physicalAxes[1]=0.0f;
 assert(c.beginInputCapture(0));assert(c.inputCapturePending());
 assert(c.pollInputCapture().kind==anvil::ControllerCaptureKind::None);
 physicalButtons[2]=1;auto captured=c.pollInputCapture();
 assert(captured.kind==anvil::ControllerCaptureKind::Button&&captured.index==2&&!c.inputCapturePending());
 physicalButtons[2]=0;physicalAxes[0]=0.0f;physicalAxes[1]=-1.0f;
 assert(c.beginInputCapture(0,0.7f));assert(c.pollInputCapture().kind==anvil::ControllerCaptureKind::None);
 physicalAxes[1]=-0.1f;captured=c.pollInputCapture();
 assert(captured.kind==anvil::ControllerCaptureKind::Axis&&captured.index==1&&captured.value<0.0f&&captured.value> -0.2f);
 assert(!c.beginInputCapture(999));assert(!c.inputCapturePending());
 physicalButtons[1]=1;physicalAxes[0]=0.5f;physicalAxes[1]=-1.0f;
 auto file=std::filesystem::temp_directory_path()/"anvil_controller_test.profile";std::string error;assert(c.saveProfile(file,"030000005e0400008e02000000000000",&error));
 anvil::ControllerSystem c2(nullptr);assert(c2.loadProfile(file,"030000005e0400008e02000000000000",&error));assert(c2.actionPressed(0,"Jump Action"));assert(c2.buttonMapping(0,1)==0);assert(c2.state(0).buttons[0]);assert(c2.axisValue(0,"Move")>0.9f);
 assert(!c2.loadProfile(file,"other-guid",&error));c2.notifyKeyboardMouseActivity();assert(c2.activeInputDevice()==anvil::InputDeviceType::KeyboardMouse);std::error_code ec;std::filesystem::remove(file,ec);
 present=false;assert(!c2.state(0).connected);std::cout<<"Controller tests passed\n";
}

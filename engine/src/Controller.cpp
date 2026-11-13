#include "anvil/Controller.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <map>
#if defined(ANVIL_WITH_STEAM_INPUT)
#  include <steam/steam_api.h>
#endif

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <xinput.h>
#elif defined(__ANDROID__)
#  include <android/input.h>
#  include <cstdlib>
#elif defined(__linux__)
#  include <dirent.h>
#  include <fcntl.h>
#  include <linux/input.h>
#  include <linux/input-event-codes.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#  include <cerrno>
#  include <cstring>
#endif

namespace anvil {
namespace {
constexpr int GP_A=0,GP_B=1,GP_X=2,GP_Y=3,GP_LB=4,GP_RB=5,GP_BACK=6,GP_START=7,GP_GUIDE=8,GP_LTHUMB=9,GP_RTHUMB=10,GP_UP=11,GP_RIGHT=12,GP_DOWN=13,GP_LEFT=14;
constexpr int GA_LX=0,GA_LY=1,GA_RX=2,GA_RY=3,GA_LT=4,GA_RT=5;
}
namespace {
std::uint64_t settingKey(int id,int axis){return (std::uint64_t(std::uint32_t(id))<<32)|std::uint32_t(axis);}
unsigned short guidWord(const std::string& guid,std::size_t at){
 if(guid.size()<at+4)return 0;
 try{return static_cast<unsigned short>(std::stoul(guid.substr(at+2,2)+guid.substr(at,2),nullptr,16));}catch(...){return 0;}
}
std::string manufacturerFor(unsigned short vendor){
 switch(vendor){case 0x045e:return "Microsoft";case 0x054c:return "Sony";case 0x057e:return "Nintendo";case 0x046d:return "Logitech";case 0x28de:return "Valve";case 0x0e6f:return "PDP";case 0x1532:return "Razer";default:return {};}
}
#if defined(__linux__)
bool guidMatchesVidPid(const std::string& guid,unsigned short vendor,unsigned short product){
 if(guid.size()<16)return false;
 auto parseLittle=[](const std::string& value,std::size_t at)->unsigned short{try{return static_cast<unsigned short>(std::stoul(value.substr(at,4).substr(2,2)+value.substr(at,2),nullptr,16));}catch(...){return 0;}};
 return parseLittle(guid,8)==vendor&&parseLittle(guid,16)==product;
}
#endif
float applyAxis(float v,const AxisSettings& cfg){
 const float dz=std::clamp(cfg.deadzone,0.0f,0.95f);
 if(std::abs(v)<=dz)v=0.0f;else v=std::copysign((std::abs(v)-dz)/(1.0f-dz),v);
 v=std::clamp(v*std::clamp(cfg.sensitivity,0.0f,4.0f),-1.0f,1.0f);
 return cfg.inverted?-v:v;
}
}

struct ControllerSystem::NativeBackend {
#if defined(ANVIL_WITH_STEAM_INPUT)
 std::string steamIdentifier(const std::string& name){
  // Steamworks resolves the action's manifest identifier (e.g. "Jump"),
  // not the JSON/VDF structural path used by some tooling.
  const auto marker=name.rfind("/in/");
  if(marker!=std::string::npos)return name.substr(marker+4);
  const auto root=name.find("/actions/");
  return root==std::string::npos?name:name.substr(root+9);
 }
 bool steamInitialized=false;
 InputActionSetHandle_t steamActionSet=0;
 int nextSteamId=4000;
 std::map<int,InputHandle_t> steamHandles;
 std::unordered_map<std::string,int> steamDigitalBindings;
 std::unordered_map<std::string,std::pair<int,int>> steamAnalogBindings;
 bool initializeSteam(const std::filesystem::path& manifest,const std::string& actionSet,std::string* error){
  if(!SteamAPI_IsSteamRunning()||!SteamInput()){if(error)*error="Steamworks is not initialized. Call SteamAPI_Init before initializeSteamInput.";return false;}
  auto* input=SteamInput();
  if(!input->Init(true)){if(error)*error="Steam Input initialization failed.";return false;}
  const auto manifestPath=std::filesystem::absolute(manifest).string();
  if(!input->SetInputActionManifestFilePath(manifestPath.c_str())){input->Shutdown();if(error)*error="Steam Input rejected the action manifest path.";return false;}
  const auto setName=steamIdentifier(actionSet);
  const auto set=input->GetActionSetHandle(setName.c_str());
  if(set==0){input->Shutdown();if(error)*error="Steam Input action set was not found in the manifest: "+actionSet;return false;}
  steamActionSet=set;steamInitialized=true;refreshSteam();return true;
 }
 void shutdownSteam(){if(steamInitialized&&SteamInput())SteamInput()->Shutdown();steamInitialized=false;steamActionSet=0;steamHandles.clear();}
 void refreshSteam(){
  if(!steamInitialized||!SteamInput())return;
  auto* input=SteamInput();input->RunFrame();
  InputHandle_t handles[STEAM_INPUT_MAX_COUNT]{};
  const int count=input->GetConnectedControllers(handles);
  std::map<InputHandle_t,bool> present;
  for(int i=0;i<count;++i)if(handles[i])present[handles[i]]=true;
  for(auto it=steamHandles.begin();it!=steamHandles.end();){if(!present.contains(it->second))it=steamHandles.erase(it);else ++it;}
  for(const auto&[handle,unused]:present){(void)unused;const bool known=std::any_of(steamHandles.begin(),steamHandles.end(),[&](const auto& item){return item.second==handle;});if(!known){while(std::any_of(steamHandles.begin(),steamHandles.end(),[&](const auto& item){return item.first==nextSteamId;}))++nextSteamId;steamHandles[nextSteamId++]=handle;}}
 }
 void appendSteamDevices(std::vector<ControllerInfo>& out){
  refreshSteam();for(const auto&[id,handle]:steamHandles){ControllerInfo d;d.id=id;d.name="Steam Input controller";d.guid="steaminput:"+std::to_string(handle);d.mapping="Steam Input action manifest";d.buttonCount=static_cast<int>(steamDigitalBindings.size());d.axisCount=static_cast<int>(steamAnalogBindings.size()*2);d.features.push_back(ControllerFeature::Rumble);out.push_back(std::move(d));}
 }
 bool steamState(int id,ControllerState& out){
  refreshSteam();auto it=steamHandles.find(id);if(it==steamHandles.end()||!steamInitialized||!SteamInput())return false;
  auto* input=SteamInput();input->ActivateActionSet(it->second,steamActionSet);out={};out.connected=true;
  for(const auto&[name,button]:steamDigitalBindings){const auto id=steamIdentifier(name);auto action=input->GetDigitalActionHandle(id.c_str());if(!action||button<0||button>=64)continue;const auto data=input->GetDigitalActionData(it->second,action);out.buttons[static_cast<std::size_t>(button)]=data.bActive&&data.bState;out.physicalButtons[static_cast<std::size_t>(button)]=out.buttons[static_cast<std::size_t>(button)];out.physicalButtonCount=std::max(out.physicalButtonCount,button+1);}
  for(const auto&[name,axes]:steamAnalogBindings){const auto id=steamIdentifier(name);auto action=input->GetAnalogActionHandle(id.c_str());if(!action)continue;const auto data=input->GetAnalogActionData(it->second,action);if(!data.bActive)continue;if(axes.first>=0&&axes.first<16){out.axes[static_cast<std::size_t>(axes.first)]=std::clamp(data.x,-1.0f,1.0f);out.physicalAxes[static_cast<std::size_t>(axes.first)]=out.axes[static_cast<std::size_t>(axes.first)];out.physicalAxisCount=std::max(out.physicalAxisCount,axes.first+1);}if(axes.second>=0&&axes.second<16){out.axes[static_cast<std::size_t>(axes.second)]=std::clamp(data.y,-1.0f,1.0f);out.physicalAxes[static_cast<std::size_t>(axes.second)]=out.axes[static_cast<std::size_t>(axes.second)];out.physicalAxisCount=std::max(out.physicalAxisCount,axes.second+1);}}
  return true;
 }
 bool steamMotionState(int id,ControllerMotionState& out){
  refreshSteam();auto it=steamHandles.find(id);if(it==steamHandles.end()||!steamInitialized||!SteamInput())return false;
  const auto motion=SteamInput()->GetMotionData(it->second);
  out.orientation={motion.rotQuatX,motion.rotQuatY,motion.rotQuatZ,motion.rotQuatW};
  out.acceleration={motion.posAccelX,motion.posAccelY,motion.posAccelZ};
  out.angularVelocity={motion.rotVelX,motion.rotVelY,motion.rotVelZ};
  out.available=true;return true;
 }
 bool steamRumble(int id,float low,float high){refreshSteam();auto it=steamHandles.find(id);if(it==steamHandles.end()||!SteamInput())return false;SteamInput()->TriggerVibration(it->second,static_cast<unsigned short>(std::clamp(low,0.0f,1.0f)*65535.0f),static_cast<unsigned short>(std::clamp(high,0.0f,1.0f)*65535.0f));return true;}
 bool steamTriggerRumble(int id,float left,float right){refreshSteam();auto it=steamHandles.find(id);if(it==steamHandles.end()||!SteamInput())return false;SteamInput()->TriggerVibrationExtended(it->second,0,0,static_cast<unsigned short>(std::clamp(left,0.0f,1.0f)*65535.0f),static_cast<unsigned short>(std::clamp(right,0.0f,1.0f)*65535.0f));return true;}
 bool steamAvailable()const{return steamInitialized&&SteamInput()!=nullptr;}
#else
 bool initializeSteam(const std::filesystem::path&,const std::string&,std::string* error){if(error)*error="ANVIL was built without ANVIL_WITH_STEAM_INPUT and the Steamworks SDK.";return false;}
 void shutdownSteam(){}
 bool steamAvailable()const{return false;}
 void appendSteamDevices(std::vector<ControllerInfo>&){}
 bool steamState(int,ControllerState&){return false;}
 bool steamMotionState(int,ControllerMotionState&){return false;}
 bool steamRumble(int,float,float){return false;}
 bool steamTriggerRumble(int,float,float){return false;}
#endif
#if defined(_WIN32)
 using GetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
 using SetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_VIBRATION*);
 HMODULE module=nullptr; GetStateFn getState=nullptr; SetStateFn setState=nullptr;
 NativeBackend(){
  const wchar_t* names[]={L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"};
  for(const auto* name:names){module=LoadLibraryW(name);if(module)break;}
  if(module){getState=reinterpret_cast<GetStateFn>(GetProcAddress(module,"XInputGetState"));setState=reinterpret_cast<SetStateFn>(GetProcAddress(module,"XInputSetState"));}
 }
 bool state(int slot, ControllerState& out) const {
  if(!getState||slot<0||slot>=XUSER_MAX_COUNT)return false;
  XINPUT_STATE x{};if(getState(static_cast<DWORD>(slot),&x)!=ERROR_SUCCESS)return false;
  out={};out.connected=true;
  const WORD b=x.Gamepad.wButtons;
  auto set=[&](int i,bool v){out.buttons[static_cast<std::size_t>(i)]=v;out.physicalButtons[static_cast<std::size_t>(i)]=v;};
  set(GP_A,(b&XINPUT_GAMEPAD_A)!=0);set(GP_B,(b&XINPUT_GAMEPAD_B)!=0);
  set(GP_X,(b&XINPUT_GAMEPAD_X)!=0);set(GP_Y,(b&XINPUT_GAMEPAD_Y)!=0);
  set(GP_LB,(b&XINPUT_GAMEPAD_LEFT_SHOULDER)!=0);set(GP_RB,(b&XINPUT_GAMEPAD_RIGHT_SHOULDER)!=0);
  set(GP_BACK,(b&XINPUT_GAMEPAD_BACK)!=0);set(GP_START,(b&XINPUT_GAMEPAD_START)!=0);
  set(GP_GUIDE,(b&0x0400u)!=0);set(GP_LTHUMB,(b&XINPUT_GAMEPAD_LEFT_THUMB)!=0);set(GP_RTHUMB,(b&XINPUT_GAMEPAD_RIGHT_THUMB)!=0);
  set(GP_UP,(b&XINPUT_GAMEPAD_DPAD_UP)!=0);set(GP_RIGHT,(b&XINPUT_GAMEPAD_DPAD_RIGHT)!=0);
  set(GP_DOWN,(b&XINPUT_GAMEPAD_DPAD_DOWN)!=0);set(GP_LEFT,(b&XINPUT_GAMEPAD_DPAD_LEFT)!=0);
  auto stick=[](SHORT v){return v<0?static_cast<float>(v)/32768.0f:static_cast<float>(v)/32767.0f;};
  out.axes[GA_LX]=stick(x.Gamepad.sThumbLX);out.axes[GA_LY]=-stick(x.Gamepad.sThumbLY);
  out.axes[GA_RX]=stick(x.Gamepad.sThumbRX);out.axes[GA_RY]=-stick(x.Gamepad.sThumbRY);
  out.axes[GA_LT]=static_cast<float>(x.Gamepad.bLeftTrigger)/127.5f-1.0f;
  out.axes[GA_RT]=static_cast<float>(x.Gamepad.bRightTrigger)/127.5f-1.0f;
  out.physicalAxes=out.axes;out.physicalButtonCount=15;out.physicalAxisCount=6;return true;
 }
 bool rumble(int slot,float low,float high) const {
  if(!setState||slot<0||slot>=XUSER_MAX_COUNT)return false;
  XINPUT_VIBRATION v{};v.wLeftMotorSpeed=static_cast<WORD>(std::clamp(low,0.0f,1.0f)*65535.0f);v.wRightMotorSpeed=static_cast<WORD>(std::clamp(high,0.0f,1.0f)*65535.0f);
  return setState(static_cast<DWORD>(slot),&v)==ERROR_SUCCESS;
 }
#elif defined(__ANDROID__)
 std::map<int,ControllerState> androidStates;
 bool state(int deviceId,ControllerState& out) const {auto it=androidStates.find(deviceId);if(it==androidStates.end())return false;out=it->second;return out.connected;}
#elif defined(__linux__)
 struct Device { int fd=-1,rumbleEffect=-1;bool rumbleSupported=false;ControllerConnection connection=ControllerConnection::Unknown;std::string path,name,guid;unsigned short vendor=0,product=0;ControllerState state{};std::array<input_absinfo,ABS_CNT> abs{};std::array<bool,KEY_MAX+1> keys{}; };
 std::map<int,Device> devices;
 static bool bitSet(const unsigned long* bits,unsigned bit){return (bits[bit/(8*sizeof(unsigned long))]>>(bit%(8*sizeof(unsigned long))))&1UL;}
 static float normalizeAbs(const input_absinfo& a,int value){if(a.maximum<=a.minimum)return 0.0f;const float v=(static_cast<float>(value)-a.minimum)/static_cast<float>(a.maximum-a.minimum);return std::clamp(v*2.0f-1.0f,-1.0f,1.0f);}
 static int mapKey(unsigned code){switch(code){case BTN_SOUTH:return GP_A;case BTN_EAST:return GP_B;case BTN_WEST:return GP_X;case BTN_NORTH:return GP_Y;case BTN_TL:return GP_LB;case BTN_TR:return GP_RB;case BTN_SELECT:return GP_BACK;case BTN_START:return GP_START;case BTN_MODE:return GP_GUIDE;case BTN_THUMBL:return GP_LTHUMB;case BTN_THUMBR:return GP_RTHUMB;case BTN_DPAD_UP:return GP_UP;case BTN_DPAD_RIGHT:return GP_RIGHT;case BTN_DPAD_DOWN:return GP_DOWN;case BTN_DPAD_LEFT:return GP_LEFT;default:return -1;}}
 static void update(Device& d,const input_event& e){if(e.type==EV_KEY&&e.code<=KEY_MAX){d.keys[e.code]=e.value!=0;const int b=mapKey(e.code);if(b>=0)d.state.buttons[static_cast<std::size_t>(b)]=e.value!=0;}else if(e.type==EV_ABS&&e.code<ABS_CNT){d.abs[e.code].value=e.value;auto axis=[&](int target,unsigned code,bool invert=false){if(d.abs[code].maximum>d.abs[code].minimum){float v=normalizeAbs(d.abs[code],e.value);d.state.axes[static_cast<std::size_t>(target)]=invert?-v:v;}};switch(e.code){case ABS_X:axis(GA_LX,ABS_X);break;case ABS_Y:axis(GA_LY,ABS_Y);break;case ABS_RX:axis(GA_RX,ABS_RX);break;case ABS_RY:axis(GA_RY,ABS_RY);break;case ABS_Z:case ABS_BRAKE:d.state.axes[GA_LT]=normalizeAbs(d.abs[e.code],e.value);break;case ABS_RZ:case ABS_GAS:d.state.axes[GA_RT]=normalizeAbs(d.abs[e.code],e.value);break;case ABS_HAT0X:d.state.buttons[GP_LEFT]=e.value<0;d.state.buttons[GP_RIGHT]=e.value>0;break;case ABS_HAT0Y:d.state.buttons[GP_UP]=e.value<0;d.state.buttons[GP_DOWN]=e.value>0;break;default:break;}}}
 static bool isGamepad(int fd){unsigned long ev[(EV_MAX/(8*sizeof(unsigned long)))+1]{};if(ioctl(fd,EVIOCGBIT(0,sizeof(ev)),ev)<0||!bitSet(ev,EV_KEY)||!bitSet(ev,EV_ABS))return false;unsigned long keys[(KEY_MAX/(8*sizeof(unsigned long)))+1]{};if(ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keys)),keys)<0)return false;return bitSet(keys,BTN_SOUTH)||bitSet(keys,BTN_GAMEPAD)||bitSet(keys,BTN_JOYSTICK);}
 void refresh(){
  DIR* dir=opendir("/dev/input");if(!dir)return;std::vector<std::string> paths;while(auto* ent=readdir(dir)){if(std::strncmp(ent->d_name,"event",5)==0)paths.emplace_back(std::string("/dev/input/")+ent->d_name);}closedir(dir);
  for(const auto& path:paths){bool known=false;for(const auto&[id,d]:devices){(void)id;if(d.path==path){known=true;break;}}if(known)continue;
   int fd=open(path.c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0||!isGamepad(fd)){if(fd>=0)::close(fd);continue;}
   Device d;d.fd=fd;d.path=path;char name[256]{};if(ioctl(fd,EVIOCGNAME(sizeof(name)),name)>=0)d.name=name;else d.name="Linux evdev gamepad";input_id iid{};if(ioctl(fd,EVIOCGID,&iid)>=0){d.vendor=iid.vendor;d.product=iid.product;}
   std::ostringstream guid;guid<<"evdev:"<<std::hex<<d.vendor<<":"<<d.product<<":"<<path;d.guid=guid.str();
   const auto eventName=path.substr(path.find_last_of('/')+1);std::error_code pathError;auto sysPath=std::filesystem::canonical(std::filesystem::path("/sys/class/input")/eventName/"device",pathError).string();
   if(!pathError){std::string lower=sysPath;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(lower.find("bluetooth")!=std::string::npos)d.connection=ControllerConnection::Bluetooth;else if(lower.find("usb")!=std::string::npos)d.connection=ControllerConnection::USB;}
   unsigned long ffBits[(FF_MAX/(8*sizeof(unsigned long)))+1]{};if(ioctl(fd,EVIOCGBIT(EV_FF,sizeof(ffBits)),ffBits)>=0)d.rumbleSupported=bitSet(ffBits,FF_RUMBLE);
   for(unsigned code=0;code<ABS_CNT;++code){input_absinfo a{};if(ioctl(fd,EVIOCGABS(code),&a)>=0)d.abs[code]=a;}
   unsigned long keyBits[(KEY_MAX/(8*sizeof(unsigned long)))+1]{};if(ioctl(fd,EVIOCGKEY(sizeof(keyBits)),keyBits)>=0)for(unsigned code=0;code<=KEY_MAX;++code)d.keys[code]=bitSet(keyBits,code);
   d.state.connected=true;for(unsigned code=0;code<=KEY_MAX;++code)if(d.keys[code]){int b=mapKey(code);if(b>=0)d.state.buttons[static_cast<std::size_t>(b)]=true;}
   auto initAxis=[&](unsigned code,int target,bool invert=false){if(d.abs[code].maximum>d.abs[code].minimum){float v=normalizeAbs(d.abs[code],d.abs[code].value);d.state.axes[static_cast<std::size_t>(target)]=invert?-v:v;}};
   initAxis(ABS_X,GA_LX);initAxis(ABS_Y,GA_LY);initAxis(ABS_RX,GA_RX);initAxis(ABS_RY,GA_RY);
   auto trigger=[&](unsigned code,int target){if(d.abs[code].maximum>d.abs[code].minimum)d.state.axes[static_cast<std::size_t>(target)]=normalizeAbs(d.abs[code],d.abs[code].value);};
   if(d.abs[ABS_Z].maximum>d.abs[ABS_Z].minimum)trigger(ABS_Z,GA_LT);else trigger(ABS_BRAKE,GA_LT);
   if(d.abs[ABS_RZ].maximum>d.abs[ABS_RZ].minimum)trigger(ABS_RZ,GA_RT);else trigger(ABS_GAS,GA_RT);
   d.state.physicalButtons=d.state.buttons;d.state.physicalAxes=d.state.axes;d.state.physicalButtonCount=15;d.state.physicalAxisCount=6;
   int id=2000;while(devices.contains(id))++id;devices.emplace(id,std::move(d));
  }
  for(auto it=devices.begin();it!=devices.end();){bool alive=true;char name[2]{};if(it->second.fd<0||ioctl(it->second.fd,EVIOCGNAME(sizeof(name)),name)<0)alive=false;if(!alive){if(it->second.fd>=0){if(it->second.rumbleEffect>=0)ioctl(it->second.fd,EVIOCRMFF,it->second.rumbleEffect);::close(it->second.fd);}it=devices.erase(it);}else ++it;}
  for(auto&[id,d]:devices){(void)id;input_event events[32];for(;;){ssize_t n=read(d.fd,events,sizeof(events));if(n<static_cast<ssize_t>(sizeof(input_event)))break;const auto count=static_cast<std::size_t>(n)/sizeof(input_event);for(std::size_t i=0;i<count;++i)update(d,events[i]);}d.state.physicalButtons=d.state.buttons;d.state.physicalAxes=d.state.axes;}
 }
 bool state(int id,ControllerState& out){refresh();auto it=devices.find(id);if(it==devices.end())return false;out=it->second.state;return true;}
 bool rumble(int id,float low,float high){refresh();auto it=devices.find(id);if(it==devices.end()||!it->second.rumbleSupported)return false;auto& d=it->second;ff_effect effect{};effect.type=FF_RUMBLE;effect.id=d.rumbleEffect;effect.u.rumble.strong_magnitude=static_cast<std::uint16_t>(std::clamp(high,0.0f,1.0f)*65535.0f);effect.u.rumble.weak_magnitude=static_cast<std::uint16_t>(std::clamp(low,0.0f,1.0f)*65535.0f);effect.replay.length=5000;effect.replay.delay=0;if(ioctl(d.fd,EVIOCSFF,&effect)<0)return false;d.rumbleEffect=effect.id;input_event event{};event.type=EV_FF;event.code=static_cast<__u16>(effect.id);event.value=(low>0.0f||high>0.0f)?1:0;return write(d.fd,&event,sizeof(event))==static_cast<ssize_t>(sizeof(event));}
#else
 bool state(int,ControllerState&)const{return false;}
#endif
 ~NativeBackend(){
#if defined(_WIN32)
  if(module)FreeLibrary(module);
#elif defined(__linux__)
  for(auto&[id,d]:devices){(void)id;if(d.fd>=0){if(d.rumbleEffect>=0)ioctl(d.fd,EVIOCRMFF,d.rumbleEffect);::close(d.fd);}}
#endif
  shutdownSteam();
 }
};

ControllerSystem::ControllerSystem(GLFWwindow* w):window_(w),native_(std::make_unique<NativeBackend>()){}
ControllerSystem::~ControllerSystem()=default;
std::vector<ControllerInfo> ControllerSystem::connected() const {
 std::vector<ControllerInfo> out;
 for(int id=GLFW_JOYSTICK_1;id<=GLFW_JOYSTICK_LAST;++id){
  if(!glfwJoystickPresent(id))continue;
  ControllerInfo d;d.id=id;const char*n=glfwGetJoystickName(id);const char*g=glfwGetJoystickGUID(id);
  d.name=n?n:"Unknown controller";d.guid=g?g:"";d.vendorId=guidWord(d.guid,8);d.productId=guidWord(d.guid,16);d.manufacturer=manufacturerFor(static_cast<unsigned short>(d.vendorId));
  int nb=0,na=0;glfwGetJoystickButtons(id,&nb);glfwGetJoystickAxes(id,&na);d.buttonCount=nb;d.axisCount=na;
  if(glfwJoystickIsGamepad(id))d.mapping="GLFW standard gamepad mapping";
  out.push_back(std::move(d));
 }
#if defined(_WIN32)
 if(native_){
  const bool glfwHasStandardGamepad=std::any_of(out.begin(),out.end(),[](const ControllerInfo& d){return d.mapping=="GLFW standard gamepad mapping";});
  if(!glfwHasStandardGamepad)for(int slot=0;slot<XUSER_MAX_COUNT;++slot){ControllerState s;if(native_->state(slot,s)){ControllerInfo d;d.id=1000+slot;d.name="XInput-compatible controller";d.guid="xinput:slot:"+std::to_string(slot);d.mapping="XInput standard gamepad";d.buttonCount=15;d.axisCount=6;d.features.push_back(ControllerFeature::Rumble);out.push_back(std::move(d));}}
 }
#elif defined(__ANDROID__)
 if(native_){size_t count=0;int32_t* ids=AInputDevice_getDeviceIds(&count);if(ids){for(size_t i=0;i<count;++i){const int deviceId=ids[i];const int sources=AInputDevice_getDeviceSources(deviceId);if((sources&AINPUT_SOURCE_GAMEPAD)!=AINPUT_SOURCE_GAMEPAD&&(sources&AINPUT_SOURCE_JOYSTICK)!=AINPUT_SOURCE_JOYSTICK)continue;native_->androidStates[deviceId].connected=true;ControllerInfo d;d.id=3000+deviceId;const char* name=AInputDevice_getName(deviceId);d.name=name?name:"Android game controller";d.guid="android:"+std::to_string(AInputDevice_getVendorId(deviceId))+":"+std::to_string(AInputDevice_getProductId(deviceId))+":"+std::to_string(deviceId);d.vendorId=AInputDevice_getVendorId(deviceId);d.productId=AInputDevice_getProductId(deviceId);d.manufacturer=manufacturerFor(static_cast<unsigned short>(d.vendorId));d.buttonCount=15;d.axisCount=6;d.mapping="Android NDK gamepad/joystick events";out.push_back(std::move(d));}std::free(ids);}}
#elif defined(__linux__)
 if(native_){native_->refresh();for(const auto&[id,n]:native_->devices){
  const bool alreadyMapped=std::any_of(out.begin(),out.end(),[&](const ControllerInfo& glfwDevice){return guidMatchesVidPid(glfwDevice.guid,n.vendor,n.product);});
  if(alreadyMapped)continue;
  ControllerInfo d;d.id=id;d.name=n.name;d.guid=n.guid;d.vendorId=n.vendor;d.productId=n.product;d.manufacturer=manufacturerFor(n.vendor);d.buttonCount=15;d.axisCount=6;d.mapping="Linux evdev standard gamepad";d.connection=n.connection;if(n.rumbleSupported)d.features.push_back(ControllerFeature::Rumble);out.push_back(std::move(d));}}
#endif
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(native_)native_->appendSteamDevices(out);
#endif
 return out;
}
ControllerState ControllerSystem::state(int id) const {
 ControllerState s;
 auto finish=[&]() { auto it=buttonMappings_.find(id); if(it!=buttonMappings_.end()){auto physical=s.buttons; s.buttons.fill(false); for(std::size_t b=0;b<physical.size();++b){const int target=it->second[b];if(physical[b]&&target>=0&&target<static_cast<int>(s.buttons.size()))s.buttons[static_cast<std::size_t>(target)]=true;}} if(s.connected&&(std::any_of(s.buttons.begin(),s.buttons.end(),[](bool v){return v;})||std::any_of(s.axes.begin(),s.axes.end(),[](float v){return std::abs(v)>0.2f;})))activeInputDevice_=InputDeviceType::Gamepad; return s; };
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(id>=4000&&native_&&native_->steamState(id,s)){for(int i=0;i<16;++i)s.axes[static_cast<std::size_t>(i)]=applyAxis(s.axes[static_cast<std::size_t>(i)],axisSettings(id,i));return finish();}
#endif
 if(id>=1000){
#if defined(_WIN32)
  if(native_&&native_->state(id-1000,s)){for(int i=0;i<GLFW_GAMEPAD_AXIS_LAST+1;++i)s.axes[static_cast<std::size_t>(i)]=applyAxis(s.axes[static_cast<std::size_t>(i)],axisSettings(id,i));return finish();}
#elif defined(__ANDROID__)
  if(native_&&native_->state(id-3000,s)){for(int i=0;i<GLFW_GAMEPAD_AXIS_LAST+1;++i)s.axes[static_cast<std::size_t>(i)]=applyAxis(s.axes[static_cast<std::size_t>(i)],axisSettings(id,i));return finish();}
#elif defined(__linux__)
  if(native_&&native_->state(id,s)){for(int i=0;i<GLFW_GAMEPAD_AXIS_LAST+1;++i)s.axes[static_cast<std::size_t>(i)]=applyAxis(s.axes[static_cast<std::size_t>(i)],axisSettings(id,i));return finish();}
#endif
  return finish();
 }
 if(!glfwJoystickPresent(id))return finish();
 s.connected=true;
 int count=0;const unsigned char* physicalButtons=glfwGetJoystickButtons(id,&count);s.physicalButtonCount=std::max(0,count);
 if(physicalButtons&&count>0)for(int i=0;i<std::min(count,static_cast<int>(s.physicalButtons.size()));++i)s.physicalButtons[static_cast<std::size_t>(i)]=physicalButtons[i]==GLFW_PRESS;
 const float* physicalAxes=glfwGetJoystickAxes(id,&count);s.physicalAxisCount=std::max(0,count);
 if(physicalAxes&&count>0)for(int i=0;i<std::min(count,static_cast<int>(s.physicalAxes.size()));++i)s.physicalAxes[static_cast<std::size_t>(i)]=physicalAxes[i];
 const unsigned char* hats=glfwGetJoystickHats(id,&count);s.physicalHatCount=std::max(0,count);
 if(hats&&count>0)for(int i=0;i<std::min(count,static_cast<int>(s.hats.size()));++i){s.hats[static_cast<std::size_t>(i)]=hats[i];if(i==0){s.buttons[GP_UP]=(hats[i]&GLFW_HAT_UP)!=0;s.buttons[GP_RIGHT]=(hats[i]&GLFW_HAT_RIGHT)!=0;s.buttons[GP_DOWN]=(hats[i]&GLFW_HAT_DOWN)!=0;s.buttons[GP_LEFT]=(hats[i]&GLFW_HAT_LEFT)!=0;}}
 if(glfwJoystickIsGamepad(id)){GLFWgamepadstate mapped{};if(glfwGetGamepadState(id,&mapped)){for(int i=0;i<=GLFW_GAMEPAD_BUTTON_LAST;++i)s.buttons[static_cast<std::size_t>(i)]=mapped.buttons[i]==GLFW_PRESS;for(int i=0;i<=GLFW_GAMEPAD_AXIS_LAST;++i)s.axes[static_cast<std::size_t>(i)]=applyAxis(mapped.axes[i],axisSettings(id,i));if(std::any_of(s.buttons.begin(),s.buttons.end(),[](bool v){return v;})||std::any_of(s.axes.begin(),s.axes.end(),[](float v){return std::abs(v)>0.2f;}))activeInputDevice_=InputDeviceType::Gamepad;return finish();}}
 for(int i=0;i<std::min(s.physicalButtonCount,static_cast<int>(s.buttons.size()));++i)s.buttons[static_cast<std::size_t>(i)]=s.physicalButtons[static_cast<std::size_t>(i)];
 for(int i=0;i<std::min(s.physicalAxisCount,static_cast<int>(s.axes.size()));++i)s.axes[static_cast<std::size_t>(i)]=applyAxis(s.physicalAxes[static_cast<std::size_t>(i)],axisSettings(id,i));
 if(std::any_of(s.buttons.begin(),s.buttons.end(),[](bool v){return v;})||std::any_of(s.axes.begin(),s.axes.end(),[](float v){return std::abs(v)>0.2f;}))activeInputDevice_=InputDeviceType::Gamepad;
 return finish();
}
ControllerMotionState ControllerSystem::motionState(int id)const{
 ControllerMotionState motion;
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(id>=4000&&native_)native_->steamMotionState(id,motion);
#else
 (void)id;
#endif
 return motion;
}
bool ControllerSystem::loadGamepadMappings(const std::filesystem::path& file,std::size_t* loaded,std::string* error){
 if(loaded)*loaded=0;
 std::ifstream in(file);if(!in){if(error)*error="Cannot open gamepad mapping database";return false;}
 std::string line;std::size_t count=0,lineNumber=0;
 while(std::getline(in,line)){++lineNumber;if(line.empty()||line[0]=='#')continue;if(line.find(',')==std::string::npos){if(error)*error="Invalid gamepad mapping at line "+std::to_string(lineNumber);return false;}if(glfwUpdateGamepadMappings(line.c_str())==GLFW_TRUE)++count;}
 if(in.bad()){if(error)*error="Failed while reading gamepad mapping database";return false;}if(loaded)*loaded=count;return true;
}
void ControllerSystem::notifyKeyboardMouseActivity(){activeInputDevice_=InputDeviceType::KeyboardMouse;}
InputDeviceType ControllerSystem::activeInputDevice()const{return activeInputDevice_;}
void ControllerSystem::setAxisSettings(int id,int axis,AxisSettings c){if(axis<0||axis>=16)return;c.deadzone=std::clamp(c.deadzone,0.0f,0.95f);c.sensitivity=std::clamp(c.sensitivity,0.0f,4.0f);axisSettings_[settingKey(id,axis)]=c;}
AxisSettings ControllerSystem::axisSettings(int id,int axis)const{auto i=axisSettings_.find(settingKey(id,axis));return i==axisSettings_.end()?AxisSettings{}:i->second;}
bool ControllerSystem::setRumble(int id,float low,float high){
 low=std::clamp(low,0.0f,1.0f);high=std::clamp(high,0.0f,1.0f);
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(id>=4000)return native_&&native_->steamRumble(id,low,high);
#endif
 if(id>=1000){
#if defined(_WIN32)
  return native_&&native_->rumble(id-1000,low,high);
#elif defined(__linux__)
  return native_&&native_->rumble(id,low,high);
#elif defined(__ANDROID__)
  return false;
#else
  return false;
#endif
 }
 // GLFW 3.4 has no rumble API; plain GLFW joysticks cannot rumble (native backends above handle id>=1000).
 (void)low;(void)high;
 if(!glfwJoystickPresent(id))return false;
 return false;
}
bool ControllerSystem::setTriggerRumble(int id,float left,float right){
 left=std::clamp(left,0.0f,1.0f);right=std::clamp(right,0.0f,1.0f);
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(id>=4000)return native_&&native_->steamTriggerRumble(id,left,right);
#endif
 (void)id;
 return false;
}
bool ControllerSystem::initializeSteamInput(const std::filesystem::path& manifest,const std::string& actionSet,std::string* error){
 if(!native_){if(error)*error="Controller backend is unavailable";return false;}
 const bool initialized=native_->initializeSteam(manifest,actionSet,error);
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(initialized){native_->steamDigitalBindings=steamDigitalBindings_;native_->steamAnalogBindings=steamAnalogBindings_;}
#endif
 return initialized;
}
void ControllerSystem::shutdownSteamInput(){if(native_)native_->shutdownSteam();}
bool ControllerSystem::steamInputAvailable()const{
#if defined(ANVIL_WITH_STEAM_INPUT)
 return native_&&native_->steamInitialized;
#else
 return false;
#endif
}
void ControllerSystem::bindSteamDigitalAction(std::string action,int button){if(action.empty())return;if(button<0||button>=64){steamDigitalBindings_.erase(action);return;}steamDigitalBindings_[std::move(action)]=button;
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(native_)native_->steamDigitalBindings=steamDigitalBindings_;
#endif
}
void ControllerSystem::bindSteamAnalogAction(std::string action,int xAxis,int yAxis){if(action.empty())return;if(xAxis<0||xAxis>=16||yAxis>=16||yAxis< -1){steamAnalogBindings_.erase(action);return;}steamAnalogBindings_[std::move(action)]={xAxis,yAxis};
#if defined(ANVIL_WITH_STEAM_INPUT)
 if(native_)native_->steamAnalogBindings=steamAnalogBindings_;
#endif
}
void ControllerSystem::setButtonMapping(int id,int physical,int logical){
 if(physical<0||physical>=64||logical< -1||logical>=64)return;
 auto [it,inserted]=buttonMappings_.try_emplace(id);if(inserted)for(int b=0;b<64;++b)it->second[static_cast<std::size_t>(b)]=b;
 it->second[static_cast<std::size_t>(physical)]=logical;
}
int ControllerSystem::buttonMapping(int id,int physical)const{if(physical<0||physical>=64)return -1;auto it=buttonMappings_.find(id);return it==buttonMappings_.end()?physical:it->second[static_cast<std::size_t>(physical)];}
void ControllerSystem::resetButtonMappings(int id){buttonMappings_.erase(id);}
bool ControllerSystem::beginInputCapture(int id,float threshold){
 const auto devices=connected();
 if(std::none_of(devices.begin(),devices.end(),[&](const ControllerInfo& d){return d.id==id;}))return false;
 const auto current=state(id);
 capture_.deviceId=id;capture_.axisThreshold=std::clamp(threshold,0.1f,0.99f);
 capture_.previousButtons=current.physicalButtons;
 if(current.physicalButtonCount==0)capture_.previousButtons=current.buttons;
 capture_.initialAxes=current.physicalAxisCount>0?current.physicalAxes:current.axes;
 capture_.pending=true;return true;
}
ControllerCaptureResult ControllerSystem::pollInputCapture(){
 ControllerCaptureResult result;
 if(!capture_.pending)return result;
 const auto current=state(capture_.deviceId);
 if(!current.connected){capture_.pending=false;return result;}
 const auto& buttons=(current.physicalButtonCount>0)?current.physicalButtons:current.buttons;
 for(std::size_t i=0;i<buttons.size();++i){
  if(buttons[i]&&!capture_.previousButtons[i]){result.kind=ControllerCaptureKind::Button;result.index=static_cast<int>(i);result.value=1.0f;capture_.pending=false;return result;}
 }
 capture_.previousButtons=buttons;
 const auto& axes=(current.physicalAxisCount>0)?current.physicalAxes:current.axes;
 for(std::size_t i=0;i<axes.size();++i){
  const float value=axes[i];
  if(std::isfinite(value)&&std::abs(value-capture_.initialAxes[i])>=capture_.axisThreshold){result.kind=ControllerCaptureKind::Axis;result.index=static_cast<int>(i);result.value=value;capture_.pending=false;return result;}
 }
 return result;
}
void ControllerSystem::cancelInputCapture(){capture_.pending=false;capture_.deviceId=-1;}
bool ControllerSystem::inputCapturePending()const{return capture_.pending;}
const char* ControllerSystem::buttonGlyph(int b,ControllerGlyphStyle style){
 static const char* xbox[]={"A","B","X","Y","LB","RB","View","Menu","Xbox","LS","RS","D-Up","D-Right","D-Down","D-Left"};
 static const char* ps[]={"Cross","Circle","Square","Triangle","L1","R1","Share","Options","PS","L3","R3","D-Up","D-Right","D-Down","D-Left"};
 static const char* nintendo[]={"B","A","Y","X","L","R","−","+","Home","L-Stick","R-Stick","D-Up","D-Right","D-Down","D-Left"};
 static const char* generic[]={"South 1","East 2","West 3","North 4","LB","RB","Back","Start","Guide","L-Stick","R-Stick","D-Up","D-Right","D-Down","D-Left"};
 if(b<0||b>=15)return "Unknown";
 const char** table=style==ControllerGlyphStyle::Xbox?xbox:style==ControllerGlyphStyle::PlayStation?ps:style==ControllerGlyphStyle::Nintendo?nintendo:generic;
 return table[b];
}
ControllerGlyphStyle ControllerSystem::inferGlyphStyle(const std::string& name){std::string n=name;std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(n.find("playstation")!=std::string::npos||n.find("dualsense")!=std::string::npos||n.find("dualshock")!=std::string::npos||n.find("sony")!=std::string::npos)return ControllerGlyphStyle::PlayStation;if(n.find("nintendo")!=std::string::npos||n.find("switch")!=std::string::npos||n.find("joy-con")!=std::string::npos||n.find("pro controller")!=std::string::npos)return ControllerGlyphStyle::Nintendo;if(n.find("xbox")!=std::string::npos||n.find("xinput")!=std::string::npos||n.find("microsoft")!=std::string::npos)return ControllerGlyphStyle::Xbox;return ControllerGlyphStyle::Generic;}
std::string ControllerSystem::buttonPrompt(int id,int button)const{ControllerGlyphStyle style=ControllerGlyphStyle::Generic;for(const auto& d:connected())if(d.id==id){style=inferGlyphStyle(d.name);break;}const int mapped=buttonMapping(id,button);if(mapped<0)return "Unbound";return buttonGlyph(mapped,style);}
void ControllerSystem::bindAction(std::string action,std::vector<int> buttons){buttons.erase(std::remove_if(buttons.begin(),buttons.end(),[](int b){return b<0||b>=64;}),buttons.end());actionButtons_[std::move(action)]=std::move(buttons);}
bool ControllerSystem::actionPressed(int id,const std::string& action)const{auto it=actionButtons_.find(action);if(it==actionButtons_.end())return false;auto s=state(id);return s.connected&&std::any_of(it->second.begin(),it->second.end(),[&](int b){return s.buttons[static_cast<std::size_t>(b)];});}
void ControllerSystem::bindAxis(std::string action,AxisBinding b){if(b.axis<0||b.axis>=16){actionAxes_.erase(action);return;}b.scale=std::clamp(b.scale,-4.0f,4.0f);actionAxes_[std::move(action)]=b;}
float ControllerSystem::axisValue(int id,const std::string& action)const{auto it=actionAxes_.find(action);if(it==actionAxes_.end())return 0.0f;auto s=state(id);return s.connected?std::clamp(s.axes[static_cast<std::size_t>(it->second.axis)]*it->second.scale,-1.0f,1.0f):0.0f;}
bool ControllerSystem::saveProfile(const std::filesystem::path& file,const std::string& guid,std::string* error)const{
 int deviceId=-1;for(const auto& d:connected())if(d.guid==guid){deviceId=d.id;break;}
 if(deviceId<0){if(error)*error="Cannot save profile: matching controller GUID is not connected";return false;}
 std::ofstream out(file,std::ios::trunc);if(!out){if(error)*error="Cannot open controller profile for writing";return false;}
 out<<"ANVIL_CONTROLLER_PROFILE 3\nGUID "<<guid<<'\n';
 auto mapIt=buttonMappings_.find(deviceId); if(mapIt!=buttonMappings_.end()) for(int b=0;b<64;++b) if(mapIt->second[static_cast<std::size_t>(b)]!=b) out<<"BUTTON_MAP "<<b<<' '<<mapIt->second[static_cast<std::size_t>(b)]<<'\n';
 for(const auto&[key,c]:axisSettings_)if(static_cast<int>(key>>32)==deviceId)out<<"AXIS "<<deviceId<<' '<<std::uint32_t(key)<<' '<<c.deadzone<<' '<<c.sensitivity<<' '<<c.inverted<<'\n';
 for(const auto&[name,buttons]:actionButtons_){out<<"ACTION "<<std::quoted(name);for(int b:buttons)out<<' '<<b;out<<'\n';}
 for(const auto&[name,b]:actionAxes_)out<<"AXIS_ACTION "<<std::quoted(name)<<' '<<b.axis<<' '<<b.scale<<'\n';
 if(!out){if(error)*error="Failed while writing controller profile";return false;}return true;
}
bool ControllerSystem::loadProfile(const std::filesystem::path& file,const std::string& guid,std::string* error){
 std::ifstream in(file);if(!in){if(error)*error="Cannot open controller profile";return false;}std::string line;if(!std::getline(in,line)||(line!="ANVIL_CONTROLLER_PROFILE 1"&&line!="ANVIL_CONTROLLER_PROFILE 2"&&line!="ANVIL_CONTROLLER_PROFILE 3")){if(error)*error="Unsupported controller profile format";return false;}
 if(!std::getline(in,line)||line!="GUID "+guid){if(error)*error="Controller profile GUID does not match this device";return false;}
 int deviceId=-1;for(const auto& d:connected())if(d.guid==guid){deviceId=d.id;break;}if(deviceId<0){if(error)*error="Matching controller is not connected";return false;}
 auto axes=axisSettings_;auto actions=actionButtons_;auto axisActions=actionAxes_;auto mappings=buttonMappings_;std::array<int,64> buttonMap{};for(int b=0;b<64;++b)buttonMap[static_cast<std::size_t>(b)]=b;
 while(std::getline(in,line)){std::istringstream row(line);std::string type,name;row>>type;if(type=="AXIS"){std::uint32_t id=0,axis=0;AxisSettings c;if(!(row>>id>>axis>>c.deadzone>>c.sensitivity>>c.inverted)||axis>=16||c.deadzone<0||c.deadzone>0.95f||c.sensitivity<0||c.sensitivity>4){if(error)*error="Invalid AXIS profile record";return false;}(void)id;axes[settingKey(deviceId,static_cast<int>(axis))]=c;}else if(type=="ACTION"){row>>std::quoted(name);if(name.empty()){if(error)*error="Invalid ACTION profile record";return false;}std::vector<int>b;int v;while(row>>v){if(v<0||v>=64){if(error)*error="Invalid action button index";return false;}b.push_back(v);}actions[name]=std::move(b);}else if(type=="BUTTON_MAP"){int physical=-1,logical=-1;if(!(row>>physical>>logical)||physical<0||physical>=64||logical< -1||logical>=64){if(error)*error="Invalid BUTTON_MAP profile record";return false;}buttonMap[static_cast<std::size_t>(physical)]=logical;}else if(type=="AXIS_ACTION"){AxisBinding b;if(!(row>>std::quoted(name)>>b.axis>>b.scale)||name.empty()||b.axis<0||b.axis>=16||!std::isfinite(b.scale)){if(error)*error="Invalid AXIS_ACTION profile record";return false;}axisActions[name]=b;}else if(!type.empty()){if(error)*error="Unknown controller profile record: "+type;return false;}}
 axisSettings_=std::move(axes);actionButtons_=std::move(actions);actionAxes_=std::move(axisActions);mappings[deviceId]=buttonMap;buttonMappings_=std::move(mappings);return true;
}
#if defined(__ANDROID__)
bool ControllerSystem::handleAndroidInputEvent(const AInputEvent* event){
 if(!event||!native_)return false;
 const int sources=AInputEvent_getSource(event);
 if((sources&AINPUT_SOURCE_GAMEPAD)!=AINPUT_SOURCE_GAMEPAD&&(sources&AINPUT_SOURCE_JOYSTICK)!=AINPUT_SOURCE_JOYSTICK)return false;
 const int deviceId=AInputEvent_getDeviceId(event);if(deviceId<0)return false;
 auto& state=native_->androidStates[deviceId];state.connected=true;
 if(AInputEvent_getType(event)==AINPUT_EVENT_TYPE_KEY){
  const int key=AKeyEvent_getKeyCode(event);const bool down=AKeyEvent_getAction(event)==AKEY_EVENT_ACTION_DOWN;
  int button=-1;switch(key){case AKEYCODE_BUTTON_A:button=GP_A;break;case AKEYCODE_BUTTON_B:button=GP_B;break;case AKEYCODE_BUTTON_X:button=GP_X;break;case AKEYCODE_BUTTON_Y:button=GP_Y;break;case AKEYCODE_BUTTON_L1:button=GP_LB;break;case AKEYCODE_BUTTON_R1:button=GP_RB;break;case AKEYCODE_BUTTON_SELECT:button=GP_BACK;break;case AKEYCODE_BUTTON_START:button=GP_START;break;case AKEYCODE_BUTTON_MODE:button=GP_GUIDE;break;case AKEYCODE_BUTTON_THUMBL:button=GP_LTHUMB;break;case AKEYCODE_BUTTON_THUMBR:button=GP_RTHUMB;break;case AKEYCODE_DPAD_UP:button=GP_UP;break;case AKEYCODE_DPAD_RIGHT:button=GP_RIGHT;break;case AKEYCODE_DPAD_DOWN:button=GP_DOWN;break;case AKEYCODE_DPAD_LEFT:button=GP_LEFT;break;default:break;}
  if(button<0)return false;
  state.buttons[static_cast<std::size_t>(button)]=down;state.physicalButtons=state.buttons;state.physicalButtonCount=15;return true;
 }
 if(AInputEvent_getType(event)==AINPUT_EVENT_TYPE_MOTION){
  const auto axis=[&](int code,int target){state.axes[static_cast<std::size_t>(target)]=std::clamp(AMotionEvent_getAxisValue(event,code,0),-1.0f,1.0f);};
  axis(AMOTION_EVENT_AXIS_X,GA_LX);axis(AMOTION_EVENT_AXIS_Y,GA_LY);axis(AMOTION_EVENT_AXIS_Z,GA_RX);axis(AMOTION_EVENT_AXIS_RZ,GA_RY);
  state.axes[GA_LT]=std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_LTRIGGER,0)*2.0f-1.0f,-1.0f,1.0f);state.axes[GA_RT]=std::clamp(AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_RTRIGGER,0)*2.0f-1.0f,-1.0f,1.0f);
  const float hx=AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_HAT_X,0),hy=AMotionEvent_getAxisValue(event,AMOTION_EVENT_AXIS_HAT_Y,0);state.buttons[GP_LEFT]=hx<-.5f;state.buttons[GP_RIGHT]=hx>.5f;state.buttons[GP_UP]=hy<-.5f;state.buttons[GP_DOWN]=hy>.5f;
  state.physicalAxes=state.axes;state.physicalButtons=state.buttons;state.physicalAxisCount=6;state.physicalButtonCount=15;return true;
 }
 return false;
}
#endif
const char* ControllerSystem::buttonName(int b){static const char* names[]={"A/Cross","B/Circle","X/Square","Y/Triangle","Left bumper","Right bumper","Back/Share","Start/Options","Guide","Left stick","Right stick","D-pad up","D-pad right","D-pad down","D-pad left"};return b>=0&&b<15?names[b]:"Unknown";}
}

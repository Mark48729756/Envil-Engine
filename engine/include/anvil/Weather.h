#pragma once
#include <algorithm>
#include <cmath>
namespace anvil {
enum class WeatherType{Clear,Cloudy,Rain,Snow,Storm};
struct WeatherState{WeatherType type=WeatherType::Clear;float cloudiness=0,precipitation=0,windSpeed=0,windDirection=0,temperatureC=18,wetness=0,snowCover=0;};
class WeatherController {
public:
    explicit WeatherController(WeatherState initial={}){state_=sanitize(initial);target_=state_;}
    void setTarget(WeatherState target,float transitionSeconds=10){target_=sanitize(target);if(!std::isfinite(transitionSeconds))transitionSeconds=10;transitionRate_=1.0f/std::max(0.001f,transitionSeconds);}
    void update(float deltaSeconds){if(!std::isfinite(deltaSeconds)||deltaSeconds<=0)return;const float dt=std::clamp(deltaSeconds,0.0f,0.25f),blend=std::clamp(dt*transitionRate_,0.0f,1.0f);state_.cloudiness=approach(state_.cloudiness,target_.cloudiness,blend);state_.precipitation=approach(state_.precipitation,target_.precipitation,blend);state_.windSpeed=approach(state_.windSpeed,target_.windSpeed,blend);state_.windDirection=approachAngle(state_.windDirection,target_.windDirection,blend);state_.temperatureC=approach(state_.temperatureC,target_.temperatureC,blend);state_.type=target_.type;const float precip=std::clamp(state_.precipitation,0.0f,1.0f);if(state_.temperatureC<=0){state_.snowCover=std::clamp(state_.snowCover+precip*dt*0.025f,0.0f,1.0f);state_.wetness=std::clamp(state_.wetness-dt*0.01f,0.0f,1.0f);}else{state_.wetness=std::clamp(state_.wetness+precip*dt*0.08f-dt*0.012f,0.0f,1.0f);state_.snowCover=std::clamp(state_.snowCover-dt*0.015f*std::max(0.0f,state_.temperatureC),0.0f,1.0f);}}
    const WeatherState&state()const{return state_;}
private:
    static WeatherState sanitize(WeatherState s){if(!std::isfinite(s.cloudiness))s.cloudiness=0;if(!std::isfinite(s.precipitation))s.precipitation=0;if(!std::isfinite(s.windSpeed))s.windSpeed=0;if(!std::isfinite(s.windDirection))s.windDirection=0;if(!std::isfinite(s.temperatureC))s.temperatureC=18;if(!std::isfinite(s.wetness))s.wetness=0;if(!std::isfinite(s.snowCover))s.snowCover=0;s.cloudiness=std::clamp(s.cloudiness,0.0f,1.0f);s.precipitation=std::clamp(s.precipitation,0.0f,1.0f);s.windSpeed=std::max(0.0f,s.windSpeed);s.wetness=std::clamp(s.wetness,0.0f,1.0f);s.snowCover=std::clamp(s.snowCover,0.0f,1.0f);return s;}
    static float approach(float a,float b,float t){return a+(b-a)*t;}
    static float approachAngle(float a,float b,float t){constexpr float pi=3.14159265358979323846f;const float d=std::fmod(b-a+3*pi,2*pi)-pi;return a+d*t;}
    WeatherState state_{},target_{};float transitionRate_=0.1f;
};
} // namespace anvil

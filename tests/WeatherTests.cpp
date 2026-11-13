#include "anvil/Weather.h"
#include <cassert>
#include <cmath>
int main() {
    anvil::WeatherController weather;
    anvil::WeatherState rain; rain.type=anvil::WeatherType::Rain; rain.precipitation=1.0f; rain.cloudiness=0.9f; rain.windSpeed=8.0f; rain.temperatureC=12.0f;
    weather.setTarget(rain,1.0f);
    for(int i=0;i<40;++i) weather.update(0.1f);
    assert(weather.state().precipitation>0.9f);
    assert(weather.state().wetness>0.0f);
    anvil::WeatherState snow; snow.type=anvil::WeatherType::Snow; snow.precipitation=0.8f; snow.temperatureC=-5.0f;
    weather.setTarget(snow,0.1f);
    for(int i=0;i<40;++i) weather.update(0.1f);
    assert(weather.state().snowCover>0.0f);
    assert(std::isfinite(weather.state().windDirection));
}

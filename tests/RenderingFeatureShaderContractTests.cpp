#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
static std::string read(const char* p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 if(argc!=4){std::cerr<<"expected water, parallax and weather shader paths\n";return 2;}
 const auto water=read(argv[1]), parallax=read(argv[2]), weather=read(argv[3]);
 const bool ok=water.find("samplerCube uEnvironment")!=std::string::npos && water.find("fresnel")!=std::string::npos && water.find("uSceneDepth")!=std::string::npos &&
 parallax.find("anvilParallaxOcclusion")!=std::string::npos && parallax.find("anvilTriplanarWeights")!=std::string::npos &&
 weather.find("layout(local_size_x=64)")!=std::string::npos && weather.find("particles[id]")!=std::string::npos;
 if(!ok){std::cerr<<"water/material/weather shader contract failed\n";return 1;} return 0;
}

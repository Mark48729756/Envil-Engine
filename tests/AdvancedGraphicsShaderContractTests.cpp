#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
static std::string read(const char* p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 if(argc!=4){std::cerr<<"expected cloud, GI and skinning shader paths\\n";return 2;}
 const auto cloud=read(argv[1]), gi=read(argv[2]), skin=read(argv[3]);
 const bool ok=cloud.find("sampler3D uCloudNoise")!=std::string::npos && cloud.find("transmittance")!=std::string::npos && cloud.find("for(int i=0;i<48")!=std::string::npos &&
 gi.find("SHCoefficients")!=std::string::npos && gi.find("coeffs[9]")!=std::string::npos &&
 skin.find("BonePalette")!=std::string::npos && skin.find("inJoints")!=std::string::npos && skin.find("inWeights")!=std::string::npos;
 if(!ok){std::cerr<<"advanced graphics shader contract failed\\n";return 1;} return 0;
}

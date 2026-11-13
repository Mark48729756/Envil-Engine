#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
static std::string read(const char* p){std::ifstream f(p);std::ostringstream s;s<<f.rdbuf();return f? s.str():std::string{};}
static bool has(const std::string&s,const char*t){return s.find(t)!=std::string::npos;}
int main(int argc,char**argv){if(argc!=6){std::cerr<<"usage: photorealism_contract GL_LIGHT RHI_LIGHT GL_GBUFFER RHI_GBUFFER IBL_DIR\n";return 2;}
 auto gl=read(argv[1]),rhi=read(argv[2]),gb=read(argv[3]),rg=read(argv[4]);
 if(gl.empty()||rhi.empty()||gb.empty()||rg.empty())return 2;
 const char* atmosphere[]={"integrateAtmosphere","rayleighPhase","miePhase","renderVolumetricClouds","cloudDensity","CLOUD_STEPS"};
 for(auto t:atmosphere)if(!has(gl,t)||!has(rhi,t)){std::cerr<<"missing atmosphere/cloud contract: "<<t<<"\n";return 1;}
 const char* M50[]={"poisson[16]"};
 for(auto t:M50)if(!has(gl,t)||!has(rhi,t)){std::cerr<<"missing improved shadow filtering: "<<t<<"\n";return 1;}
 if(!has(gl,"return visibility / 16.0") || !has(rhi,"return sum / 16.0")){std::cerr<<"missing 16-tap PCF resolve\n";return 1;}
 if(!has(rhi,"rotation * poisson[i]") || (!has(gl,"rot*poisson[i]") && !has(gl,"rotation * poisson[i]"))){std::cerr<<"missing rotated Poisson shadow samples\n";return 1;}
 const char* M51[]={"uIrradianceMap","uPrefilterMap","uBrdfLut","evaluateIBL"};
 for(auto t:M51)if(!has(gl,t)||!has(rhi,t)){std::cerr<<"missing IBL integration contract: "<<t<<"\n";return 1;}
 if(!has(gl,"uIBLReady")||!has(rhi,"IBL_ENABLED")){std::cerr<<"missing safe IBL enable gate\n";return 1;}
 const char* M52[]={"heightFogTransmittance","heightFalloff","opticalDepth"};
 for(auto t:M52)if(!has(gl,t)||!has(rhi,t)){std::cerr<<"missing height fog contract: "<<t<<"\n";return 1;}
 const char* material[]={"uHasNormalMap","uHasRoughnessMap","uHasMetallicMap","uHasAOMap","uNormalMap","uRoughnessMap","uMetallicMap","uAOMap"};
 for(auto t:material)if(!has(gb,t)){std::cerr<<"missing GL material map: "<<t<<"\n";return 1;}
 const char* ibl[]={"equirect_to_cubemap.comp","irradiance_convolution.comp","prefilter_ggx.comp","brdf_lut.comp"};
 for(auto t:ibl){std::string path=std::string(argv[5])+"/"+t;if(read(path.c_str()).empty()){std::cerr<<"missing IBL stage: "<<t<<"\n";return 1;}}
 if(!has(rg,"uAlbedoMap")){std::cerr<<"RHI G-buffer legacy binding contract missing\n";return 1;}
 std::cout<<"Photorealism pipeline contracts passed (shader source checks; not GPU compilation).\n";return 0;}

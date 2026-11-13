#include "anvil/Texture2D.h"
#include <glad/gl.h>
#include <atomic>
#include <fstream>
#include <limits>
#include <vector>
#include <cctype>
#include <algorithm>
namespace anvil {
namespace {
bool token(std::istream& in,std::string& out){out.clear();char c;while(in.get(c)){if(std::isspace(static_cast<unsigned char>(c)))continue;if(c=='#'){std::string line;std::getline(in,line);continue;}out+=c;break;}while(in.get(c)){if(std::isspace(static_cast<unsigned char>(c))){if(c=='\r' && in.peek()=='\n')in.get();break;}out+=c;}return !out.empty();}
}
Texture2D::~Texture2D(){if(id_)glDeleteTextures(1,&id_);}
std::shared_ptr<Texture2D> Texture2D::load(const std::filesystem::path& path,bool srgb,std::string* error){
    auto fail=[&](const char*m)->std::shared_ptr<Texture2D>{if(error)*error=m;return {};};
    std::ifstream f(path,std::ios::binary);if(!f)return fail("Cannot open texture file");std::string t;
    if(!token(f,t)||(t!="P6"&&t!="P3"))return fail("Texture loader supports PPM P6/P3 only");const bool binary=t=="P6";
    if(!token(f,t))return fail("Missing PPM width");int w=0;try{w=std::stoi(t);}catch(...){return fail("Invalid PPM width");}
    if(!token(f,t))return fail("Missing PPM height");int h=0;try{h=std::stoi(t);}catch(...){return fail("Invalid PPM height");}
    if(!token(f,t))return fail("Missing PPM max value");int maxv=0;try{maxv=std::stoi(t);}catch(...){return fail("Invalid PPM max value");}
    if(w<=0||h<=0||w>16384||h>16384||maxv!=255)return fail("Unsupported PPM dimensions or channel range");
    const std::size_t bytes=static_cast<std::size_t>(w)*static_cast<std::size_t>(h)*3;if(bytes>std::numeric_limits<std::size_t>::max()/2)return fail("Texture is too large");std::vector<unsigned char> pixels(bytes);
    if(binary){f.read(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(bytes));if(f.gcount()!=static_cast<std::streamsize>(bytes))return fail("Truncated PPM pixel data");}
    else for(auto&v:pixels){if(!token(f,t))return fail("Truncated PPM pixel data");try{int n=std::stoi(t);if(n<0||n>255)return fail("Invalid PPM pixel value");v=static_cast<unsigned char>(n);}catch(...){return fail("Invalid PPM pixel value");}}
    // PPM rows are top-to-bottom; flip them for the engine's bottom-left UV convention.
    const std::size_t rowBytes=static_cast<std::size_t>(w)*3; std::vector<unsigned char> row(rowBytes);
    for(int y=0;y<h/2;++y){auto* a=pixels.data()+static_cast<std::size_t>(y)*rowBytes;auto* b=pixels.data()+static_cast<std::size_t>(h-1-y)*rowBytes;std::copy(a,a+rowBytes,row.data());std::copy(b,b+rowBytes,a);std::copy(row.data(),row.data()+rowBytes,b);}
    static std::atomic<std::uint64_t> next{1};
    auto tex=std::shared_ptr<Texture2D>(new Texture2D);tex->width_=w;tex->height_=h;tex->srgb_=srgb;tex->uid_=next.fetch_add(1);tex->pixels_=std::move(pixels);
    return tex;
}
void Texture2D::createGl() const{
    glCreateTextures(GL_TEXTURE_2D,1,&id_);glTextureStorage2D(id_,1,srgb_?GL_SRGB8:GL_RGB8,width_,height_);GLint oldAlignment=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&oldAlignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTextureSubImage2D(id_,0,0,0,width_,height_,GL_RGB,GL_UNSIGNED_BYTE,pixels_.data());glPixelStorei(GL_UNPACK_ALIGNMENT,oldAlignment);glGenerateTextureMipmap(id_);glTextureParameteri(id_,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTextureParameteri(id_,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTextureParameteri(id_,GL_TEXTURE_WRAP_S,GL_REPEAT);glTextureParameteri(id_,GL_TEXTURE_WRAP_T,GL_REPEAT);
}
}

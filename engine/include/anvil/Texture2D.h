#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace anvil {
class Texture2D {
public:
    ~Texture2D(); Texture2D(const Texture2D&)=delete; Texture2D& operator=(const Texture2D&)=delete;
    // Decodes the file on the CPU only; no graphics API (and no GL context) is needed to load.
    static std::shared_ptr<Texture2D> load(const std::filesystem::path& path,bool srgb=true,std::string* error=nullptr);
    // OpenGL texture name, created lazily on first use (needs a current GL context).
    unsigned id() const{ if(!id_) createGl(); return id_; }
    int width()const{return width_;} int height()const{return height_;}
    // CPU pixels for RHI backends: tightly packed RGB8, row 0 = bottom row (same convention the GL path uses).
    const std::vector<unsigned char>& rgb() const{return pixels_;}
    bool srgb() const{return srgb_;}
    std::uint64_t uid() const{return uid_;}
private:
    Texture2D()=default; void createGl() const;
    mutable unsigned id_=0; int width_=0,height_=0; bool srgb_=true; std::uint64_t uid_=0; std::vector<unsigned char> pixels_;
};
}

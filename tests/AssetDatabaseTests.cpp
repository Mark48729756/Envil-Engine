#include "anvil/AssetDatabase.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(){
    using namespace anvil; namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/"anvil_asset_db_test";std::error_code ec;fs::remove_all(root,ec);fs::create_directories(root/"Textures");fs::create_directories(root/"Models");fs::create_directories(root/"build");
    {std::ofstream(root/"Textures"/"Albedo.PPM")<<"P3\n1 1\n255\n255 0 0\n";std::ofstream(root/"Models"/"cube.obj")<<"v 0 0 0\n";std::ofstream(root/"notes.txt")<<"ignored";std::ofstream(root/"build"/"generated.obj")<<"ignored";}
    AssetDatabase db;std::string error;assert(db.scan(root,&error));assert(db.assets().size()==3);
    auto* texture=db.find(fs::path("Textures")/"Albedo.PPM");assert(texture&&texture->type==AssetType::Texture&&texture->id!=0);assert(db.find(texture->id)==texture);
    auto* model=db.find(fs::path("Models")/"cube.obj");assert(model&&model->type==AssetType::Model);
    assert(AssetDatabase::makeId("Textures/Albedo.PPM")==AssetDatabase::makeId("Textures/Albedo.PPM"));
    assert(AssetDatabase::classify("thing.unknown")==AssetType::Unknown);
    const auto previousCount=db.assets().size();assert(!db.scan(root/"missing",&error));assert(db.assets().size()==previousCount);
    fs::remove_all(root,ec);std::cout<<"AssetDatabase tests passed\n";
}

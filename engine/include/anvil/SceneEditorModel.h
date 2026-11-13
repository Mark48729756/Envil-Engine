#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace anvil {
struct EditorEntity {std::uint64_t id=0;std::string name;BoneTransform transform{};bool visible=true,locked=false;};
class SceneEditorModel {
public:
 bool add(EditorEntity e){if(!e.id||find(e.id))return false;entities_.push_back(std::move(e));return true;}
 EditorEntity* find(std::uint64_t id){auto i=std::find_if(entities_.begin(),entities_.end(),[&](auto&e){return e.id==id;});return i==entities_.end()?nullptr:&*i;}
 bool select(std::uint64_t id,bool additive=false){if(!find(id))return false;if(!additive)selection_.clear();if(std::find(selection_.begin(),selection_.end(),id)==selection_.end())selection_.push_back(id);return true;}
 void clearSelection(){selection_.clear();}
 const std::vector<std::uint64_t>& selection()const{return selection_;}
 bool translateSelection(Vec3 delta){if(selection_.empty())return false;for(auto id:selection_){auto*e=find(id);if(!e||e->locked)return false;}for(auto id:selection_){auto*e=find(id);e->transform.translation.x+=delta.x;e->transform.translation.y+=delta.y;e->transform.translation.z+=delta.z;}return true;}
 bool setVisible(std::uint64_t id,bool v){auto*e=find(id);if(!e)return false;e->visible=v;return true;}
 bool setLocked(std::uint64_t id,bool v){auto*e=find(id);if(!e)return false;e->locked=v;return true;}
 const std::vector<EditorEntity>& entities()const{return entities_;}
private:std::vector<EditorEntity>entities_;std::vector<std::uint64_t>selection_;
};
}

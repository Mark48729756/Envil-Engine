#pragma once
#include <cstdint>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace anvil {
struct DialogueChoice { std::string text; std::uint32_t nextNode=0; std::string requiredFlag; std::string setFlag; };
struct DialogueNode { std::uint32_t id=0; std::string speaker,text,voiceEvent; std::vector<DialogueChoice> choices; std::uint32_t nextNode=0; };
class DialogueSystem {
public:
 bool addNode(DialogueNode n){if(nodes_.count(n.id)||n.text.empty())return false;nodes_.emplace(n.id,std::move(n));return true;}
 bool start(std::uint32_t id){if(!nodes_.count(id))return false;current_=id;active_=true;return true;}
 void stop(){active_=false;}
 bool active()const{return active_;}
 const DialogueNode* current()const{auto i=nodes_.find(current_);return active_&&i!=nodes_.end()?&i->second:nullptr;}
 void setFlag(std::string flag,bool value=true){if(value)flags_[flag]=true;else flags_.erase(flag);}
 bool hasFlag(const std::string&f)const{return flags_.count(f)!=0;}
 std::vector<std::size_t> availableChoices()const{std::vector<std::size_t> out;auto*n=current();if(!n)return out;for(std::size_t i=0;i<n->choices.size();++i)if(n->choices[i].requiredFlag.empty()||hasFlag(n->choices[i].requiredFlag))out.push_back(i);return out;}
 bool choose(std::size_t index){auto*n=current();if(!n)return false;auto available=availableChoices();if(std::find(available.begin(),available.end(),index)==available.end())return false;const auto c=n->choices[index];if(!c.setFlag.empty())setFlag(c.setFlag);if(nodes_.count(c.nextNode)){current_=c.nextNode;return true;}active_=false;return true;}
 bool advance(){auto*n=current();if(!n||!n->choices.empty())return false;if(nodes_.count(n->nextNode)){current_=n->nextNode;return true;}active_=false;return true;}
private: std::unordered_map<std::uint32_t,DialogueNode>nodes_;std::unordered_map<std::string,bool>flags_;std::uint32_t current_=0;bool active_=false;
};
}

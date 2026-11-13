#pragma once
#include "anvil/Animation.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace anvil {
struct AnimationState { std::string name; const AnimationClip* clip=nullptr; bool loop=true; };
struct AnimationTransition { std::size_t from=0,to=0; float blendSeconds=0.15f; };
struct BlendTree1DNode { float threshold=0.0f; const AnimationClip* clip=nullptr; };
struct BlendTree1DResult { const AnimationClip* lower=nullptr; const AnimationClip* upper=nullptr; float alpha=0.0f; };
class AnimationGraph {
public:
    explicit AnimationGraph(const Skeleton& skeleton): animator_(skeleton) {}
    std::size_t addState(AnimationState state){states_.push_back(std::move(state));return states_.size()-1;}
    bool addTransition(AnimationTransition t){if(t.from>=states_.size()||t.to>=states_.size()||t.from==t.to||t.blendSeconds<0)return false;transitions_.push_back(t);return true;}
    bool start(std::size_t state){if(state>=states_.size()||!states_[state].clip)return false;current_=state;started_=animator_.play(*states_[state].clip,0,states_[state].loop);return started_;}
    bool transitionTo(std::size_t target){if(!started_||target>=states_.size()||!states_[target].clip)return false;const auto it=std::find_if(transitions_.begin(),transitions_.end(),[&](const auto&t){return t.from==current_&&t.to==target;});if(it==transitions_.end())return false;current_=target;return animator_.crossFade(*states_[target].clip,it->blendSeconds,states_[target].loop);}
    bool setBlendTree1D(std::vector<BlendTree1DNode> nodes){if(nodes.empty())return false;for(const auto&n:nodes)if(!n.clip)return false;std::sort(nodes.begin(),nodes.end(),[](const auto&a,const auto&b){return a.threshold<b.threshold;});blendNodes_=std::move(nodes);return true;}
    BlendTree1DResult evaluateBlendTree1D(float value)const{if(blendNodes_.empty())return {};auto hi=std::lower_bound(blendNodes_.begin(),blendNodes_.end(),value,[](const auto&n,float v){return n.threshold<v;});if(hi==blendNodes_.begin())return {hi->clip,hi->clip,0.0f};if(hi==blendNodes_.end())return {blendNodes_.back().clip,blendNodes_.back().clip,0.0f};const auto&lo=*(hi-1);return {lo.clip,hi->clip,std::clamp((value-lo.threshold)/std::max(1e-6f,hi->threshold-lo.threshold),0.0f,1.0f)};}
    void update(float dt){animator_.update(dt);} const Animator& animator()const{return animator_;} std::size_t currentState()const{return current_;}
private: Animator animator_; std::vector<AnimationState>states_;std::vector<AnimationTransition>transitions_;std::vector<BlendTree1DNode>blendNodes_;std::size_t current_=0;bool started_=false;
};
}

#include "anvil/Animation.h"
#include <cassert>
#include <cmath>
#include <stdexcept>
int main(){
    anvil::Bone root; root.name="root"; anvil::Bone child; child.name="child"; child.parent=0;
    anvil::Skeleton skeleton({root,child}); anvil::Animator animator(skeleton);
    anvil::AnimationClip clip; clip.name="move"; clip.duration=1; clip.tracks.resize(2);
    anvil::BoneTransform start{},end{}; end.translation.x=10;
    clip.tracks[0].keys={{0,start},{1,end}};
    assert(animator.play(clip,0.5f,false)); assert(std::abs(animator.pose()[0].translation.x-5.0f)<1e-4f);
    animator.update(0.25f); assert(std::abs(animator.pose()[0].translation.x-7.5f)<1e-4f);
    anvil::AnimationClip idle; idle.duration=1; idle.tracks.resize(2); idle.tracks[0].keys={{0,start},{1,start}};
    assert(animator.crossFade(idle,0.5f)); animator.update(0.25f);
    assert(std::abs(animator.pose()[0].translation.x-3.75f)<1e-4f);
    animator.update(0.25f); assert(std::abs(animator.pose()[0].translation.x)<1e-4f);
    auto elbow=anvil::solveTwoBoneIK({0,0,0},{1,1,0},1,1);
    assert(std::isfinite(elbow.x) && std::isfinite(elbow.y));
    bool rejected=false; try { anvil::Skeleton bad({child,root}); } catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}

#include "anvil/CharacterController.h"
#include <cassert>
#include <cmath>
int main(){anvil::CharacterController c; c.setGroundContact(true); c.update({1,0,false},1.0f/60); assert(c.state().position.x>0); c.setGroundContact(true); c.update({0,0,true},1.0f/60); assert(c.state().velocity.y>0 && c.state().jumping); c.setGroundContact(true,{0,0.1f,0.99f}); assert(!c.state().grounded);}

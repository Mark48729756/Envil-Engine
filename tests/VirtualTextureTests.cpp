#include "anvil/VirtualTexture.h"
#include <cassert>
int main() {
    anvil::VirtualTextureCache cache(2);
    auto admitted = cache.processFeedback({{{0,0,0},1.0f},{{0,0,0},2.0f},{{0,1,0},0.5f}});
    assert(admitted.size()==2 && cache.residentCount()==2);
    assert(cache.contains({0,0,0}));
    assert(cache.touch({0,0,0}));
    cache.processFeedback({{{0,2,0},3.0f}});
    assert(cache.contains({0,0,0}));
    assert(!cache.contains({0,1,0}));
    auto parent=cache.resolve({0,0,0});
    assert((parent.exact && parent.resident==anvil::VirtualPageId{0,0,0}));
    anvil::VirtualTextureCache fallbackCache(2);
    fallbackCache.processFeedback({{{1,2,2},1.0f}});
    auto fallback=fallbackCache.resolve({0,4,4});
    assert((!fallback.exact && fallback.resident==anvil::VirtualPageId{1,2,2}));
}

# M6 APIs

```cpp
#include <anvil/SceneSerializer.h>
std::string error;
anvil::SceneSerializer::save(scene, "assets/scenes/demo.anvil", &error);
anvil::SceneSerializer::load(scene, "assets/scenes/demo.anvil", &error);

#include <anvil/UndoRedo.h>
anvil::UndoRedoStack history;
// Apply an edit first, then register its inverse and replay operation:
history.pushApplied({"Move entity", undoMove, redoMove});
history.undo(); history.redo();

#include <anvil/ECS.h>
struct Transform { float x=0, y=0, z=0; };
anvil::ecs::Registry world;
auto entity = world.create();
world.emplace<Transform>(entity, Transform{1,2,3});
for(auto e : world.view<Transform>()) { (void)world.get<Transform>(e); }
```

The ECS is additive in this milestone; the legacy Scene API is preserved. UndoRedoStack is deliberately command-agnostic, allowing editor commands to capture IDs and values rather than unstable pointers into `Scene::entities`.

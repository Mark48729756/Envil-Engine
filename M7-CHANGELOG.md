# ANVIL ENGINE M7 — Hierarchy & Editor Workflow

## Included

### Scene hierarchy
- `Entity::parentId` defines the parent relationship (`0` means scene root).
- `Scene::setParent` validates handles and rejects self-parenting and cycles.
- `Scene::childrenOf` returns direct children in stable scene order.
- `Scene::worldModel` composes local transforms through the ancestor chain with a traversal guard.
- Meshes and point lights are submitted to the renderer using world-space transforms.
- Removing a parent safely promotes its direct children to the root.
- Duplicated entities retain their parent relationship.

### Editor
- Hierarchy is rendered recursively as a tree.
- Drag an entity onto another entity to parent it.
- Context menu offers Unparent, Duplicate, Focus, and Delete.
- Inspector exposes the parent selector and identifies transform fields as local-space.

### Serialization
- Scene JSON is now version 2 and stores `parentId`.
- Version 1 files are accepted and loaded as flat/root entities.
- Loading validates parent references and rejects hierarchy cycles before replacing the live scene.

## Compatibility and limitations
- Existing flat scenes continue to load.
- Existing renderer remains in place; no rendering backend migration is required for this milestone.
- Reparenting preserves local transforms, not world transforms. A keep-world option is planned.
- Runtime compilation and graphical interaction tests must still be run on a machine with the project's CMake FetchContent dependencies and an OpenGL 4.5 driver.

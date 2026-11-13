# M83 — Scene Editor Model
- Added editor entity model with stable IDs, names, transforms, visibility, and lock state.
- Added additive selection and multi-selection translation.
- Added atomic transform rejection when any selected entity is locked.
- This is the editor's data/model layer; viewport gizmos, hierarchy UI, serialization, and undo/redo integration remain future work.

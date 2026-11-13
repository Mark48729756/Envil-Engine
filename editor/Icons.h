#pragma once
// Editor icons are plain SVG strings, rasterized once at startup with nanosvg into GL textures.
// They are rendered white and tinted at draw time, so one icon works for every theme color.
enum class Icon {
    Move, Rotate, Scale, Play, Pause, Step, Stop, Cube, Sphere, Plane, Light,
    Folder, File, Eye, Trash, Copy, Plus, Hierarchy, Inspector, Console, Gear, Anvil, Count
};

namespace icons {
void init();
void shutdown();
unsigned texture(Icon icon);
}

# ANVIL package import API quick start

Include `anvil/Import.h` and link the ANVIL engine target.

## Analyze an APK

```cpp
#include "anvil/Import.h"

const auto report = anvil::ImportAnalyzer::analyze("D:/incoming/game.apk");
if (!report.errors.empty()) {
    // Present report.errors and stop.
}
// report.packageName, permissions, minSdkVersion, targetSdkVersion, abis,
// libraries and entries are inventory/metadata, not a recovered source project.
```

## Extract an APK package

```cpp
anvil::PackageImportOptions options;
options.maxTotalBytes = 2ull * 1024 * 1024 * 1024;
options.maxArchiveBytes = 2ull * 1024 * 1024 * 1024;
options.maxEntryBytes = 256ull * 1024 * 1024;
options.includeAssets = true;
options.includeNativeLibraries = true;
const auto result = anvil::PackageImporter::importApk(
    "D:/incoming/game.apk", "D:/ANVIL/Imports/game_package", options);
if (!result.success) {
    // Show result.errors; staging output is rolled back on failure.
}
```

## Analyze and snapshot a Windows executable

```cpp
const auto report = anvil::ImportAnalyzer::analyze("D:/incoming/Game.exe");
// report.architecture, report.libraries, report.graphicsApis and
// report.embeddedResources describe what the PE file actually contains.

const auto result = anvil::PackageImporter::importWindowsExecutable(
    "D:/incoming/Game.exe", "D:/ANVIL/Imports/Game_runtime");
```

The EXE importer is a runtime-file snapshot. It copies available adjacent files and embedded PE resource bytes; it cannot infer missing transitive/system dependencies, installer state, registry configuration or original source code.

## Controller setup

```cpp
#include "anvil/Controller.h"

anvil::ControllerSystem controllers(window);
std::size_t mappingsLoaded = 0;
std::string error;
controllers.loadGamepadMappings("gamecontrollerdb.txt", &mappingsLoaded, &error);

// Call from the host keyboard/mouse event callbacks:
controllers.notifyKeyboardMouseActivity();

for (const auto& device : controllers.connected()) {
    const auto state = controllers.state(device.id);
    if (controllers.actionPressed(device.id, "Jump")) {
        // Trigger the game action.
    }
}
```

The mapping file uses GLFW's SDL-compatible controller mapping line format. Standard gamepad state and raw buttons/axes/hats are available through GLFW. `setRumble()` uses GLFW's two-motor rumble function; it returns false when GLFW/device support is unavailable. Advanced vendor features are not implied by a controller being detected.

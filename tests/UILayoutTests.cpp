#include "anvil/UI.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <cstdio>
#include <string>
#include <fstream>

using namespace anvil::ui;
static bool near(float a, float b) { return std::abs(a - b) < 0.001f; }

int main() {
    // Anchored absolute widget stretches with its parent while respecting insets.
    Widget root(WidgetType::Panel, "Root");
    root.layout.padding = {10, 10, 10, 10};
    auto canvas = std::make_unique<Widget>(WidgetType::Panel, "Canvas");
    canvas->layout.anchorMin = {0.0f, 0.0f};
    canvas->layout.anchorMax = {1.0f, 1.0f};
    canvas->layout.offsets = {5, 6, 7, 8};
    Widget& canvasRef = root.add(std::move(canvas));
    LayoutEngine::arrange(root, {{0, 0}, {800, 600}});
    assert(near(canvasRef.bounds.position.x, 15));
    assert(near(canvasRef.bounds.position.y, 16));
    assert(near(canvasRef.bounds.size.x, 768));
    assert(near(canvasRef.bounds.size.y, 566));

    // Vertical stack distributes remaining main-axis space among Fill children.
    Widget stack(WidgetType::Panel, "Stack");
    stack.layout.mode = LayoutMode::Vertical;
    stack.layout.spacing = 10;
    auto fixed = std::make_unique<Widget>(WidgetType::Label, "Fixed");
    fixed->layout.preferredSize = {100, 40};
    auto fillA = std::make_unique<Widget>(WidgetType::Panel, "FillA");
    fillA->layout.heightMode = SizeMode::Fill;
    auto fillB = std::make_unique<Widget>(WidgetType::Panel, "FillB");
    fillB->layout.heightMode = SizeMode::Fill;
    Widget& a = stack.add(std::move(fixed));
    Widget& b = stack.add(std::move(fillA));
    Widget& c = stack.add(std::move(fillB));
    LayoutEngine::arrange(stack, {{0, 0}, {300, 300}});
    assert(near(a.bounds.size.y, 40));
    assert(near(b.bounds.size.y, 120));
    assert(near(c.bounds.size.y, 120));
    assert(near(b.bounds.position.y, 50));
    assert(near(c.bounds.position.y, 180));

    // Hit testing returns the topmost/deepest eligible child and respects clipping.
    Widget hitRoot(WidgetType::Panel, "HitRoot");
    auto lower = std::make_unique<Widget>(WidgetType::Button, "Lower");
    lower->layout.preferredSize = {100, 100};
    Widget& lowerRef = hitRoot.add(std::move(lower));
    auto upper = std::make_unique<Widget>(WidgetType::Button, "Upper");
    upper->layout.preferredSize = {100, 100};
    Widget& upperRef = hitRoot.add(std::move(upper));
    LayoutEngine::arrange(hitRoot, {{0, 0}, {200, 200}});
    assert(hitTest(hitRoot, {50, 50}) == &upperRef);
    upperRef.visible = false;
    assert(hitTest(hitRoot, {50, 50}) == &lowerRef);
    hitRoot.clipChildren = true;
    assert(hitTest(hitRoot, {250, 50}) == nullptr);

    // Stable lookup/removal by generated ID.
    const WidgetId id = lowerRef.id();
    assert(hitRoot.find(id) == &lowerRef);
    auto removed = hitRoot.remove(id);
    assert(removed && removed->id() == id);
    assert(hitRoot.find(id) == nullptr);

    // Versioned document round-trip preserves hierarchy, names, text and layout values.
    Widget document(WidgetType::Panel, "Document Root");
    document.layout.padding = {4, 5, 6, 7};
    auto docButton = std::make_unique<Widget>(WidgetType::Button, "Play Button");
    docButton->text = "Start Game";
    docButton->layout.preferredSize = {180, 44};
    docButton->layout.offsets = {12, 24, 0, 0};
    docButton->style.background = {0.1f, 0.3f, 0.8f, 1.0f};
    const WidgetId originalButtonId = docButton->id();
    document.add(std::move(docButton));
    auto docLabel = std::make_unique<Widget>(WidgetType::Label, "Title");
    docLabel->text = "ANVIL UI";
    document.add(std::move(docLabel));
    const std::string documentPath = "anvil_ui_roundtrip_test.anvilui";
    std::string documentError;
    assert(saveDocument(document, documentPath, &documentError));
    auto loaded = loadDocument(documentPath, &documentError);
    std::remove(documentPath.c_str());
    assert(loaded && loaded->name == "Document Root");
    assert(loaded->children().size() == 2);
    assert(loaded->children()[0]->name == "Play Button");
    assert(loaded->children()[0]->text == "Start Game");
    assert(near(loaded->children()[0]->layout.preferredSize.x, 180));
    assert(near(loaded->children()[0]->layout.offsets.top, 24));
    assert(near(loaded->children()[0]->style.background.b, 0.8f));
    assert(loaded->children()[0]->id() != originalButtonId);

    // Reject malformed headers instead of silently loading a partial document.
    const std::string invalidPath = "anvil_ui_invalid_test.anvilui";
    { std::ofstream invalid(invalidPath); invalid << "NOT_ANVIL_UI\\n"; }
    assert(!loadDocument(invalidPath, &documentError));
    std::remove(invalidPath.c_str());

    std::cout << "ANVIL UI layout tests passed\n";
    return 0;
}

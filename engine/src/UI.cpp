#include "anvil/UI.h"

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace anvil::ui {
namespace {
std::atomic<WidgetId> gNextWidgetId{1};

float clampSize(float value, float minimum, float maximum) {
    const float lo = std::max(0.0f, minimum);
    const float hi = std::max(lo, maximum);
    return std::clamp(value, lo, hi);
}

Rect contentRect(const Widget& widget) {
    const Edges& p = widget.layout.padding;
    const Vec2 origin = widget.bounds.position + Vec2(p.left, p.top);
    const Vec2 size{
        std::max(0.0f, widget.bounds.size.x - p.left - p.right),
        std::max(0.0f, widget.bounds.size.y - p.top - p.bottom)
    };
    return {origin, size};
}

Rect applyMargin(Rect r, const Edges& m) {
    r.position += Vec2(m.left, m.top);
    r.size.x = std::max(0.0f, r.size.x - m.left - m.right);
    r.size.y = std::max(0.0f, r.size.y - m.top - m.bottom);
    return r;
}

float mainSize(const Vec2& v, LayoutMode mode) {
    return mode == LayoutMode::Horizontal ? v.x : v.y;
}
float crossSize(const Vec2& v, LayoutMode mode) {
    return mode == LayoutMode::Horizontal ? v.y : v.x;
}
}

bool Rect::contains(Vec2 p) const {
    return p.x >= position.x && p.y >= position.y &&
           p.x < position.x + size.x && p.y < position.y + size.y;
}

Widget::Widget(WidgetType widgetType, std::string widgetName)
    : type(widgetType), name(std::move(widgetName)), id_(gNextWidgetId.fetch_add(1, std::memory_order_relaxed)) {}

Widget& Widget::add(std::unique_ptr<Widget> child) {
    if (!child) throw std::invalid_argument("UI widget child cannot be null");
    children_.push_back(std::move(child));
    return *children_.back();
}

std::unique_ptr<Widget> Widget::remove(WidgetId target) {
    const auto it = std::find_if(children_.begin(), children_.end(),
        [target](const std::unique_ptr<Widget>& child) { return child->id() == target; });
    if (it == children_.end()) return {};
    auto result = std::move(*it);
    children_.erase(it);
    return result;
}

Widget* Widget::find(WidgetId target) {
    if (id_ == target) return this;
    for (auto& child : children_) if (Widget* found = child->find(target)) return found;
    return nullptr;
}

const Widget* Widget::find(WidgetId target) const {
    if (id_ == target) return this;
    for (const auto& child : children_) if (const Widget* found = child->find(target)) return found;
    return nullptr;
}

void LayoutEngine::arrange(Widget& root, Rect viewport) {
    root.bounds = viewport;
    arrangeChildren(root, contentRect(root));
}

void LayoutEngine::arrangeChildren(Widget& parent, Rect content) {
    auto& children = parent.children_;
    if (parent.layout.mode == LayoutMode::Absolute) {
        for (auto& ptr : children) {
            Widget& child = *ptr;
            if (!child.visible) continue;
            const auto& l = child.layout;
            const Vec2 anchorStart = content.position + content.size * l.anchorMin;
            const Vec2 anchorEnd = content.position + content.size * l.anchorMax;
            Rect r;
            r.position = anchorStart + Vec2(l.offsets.left, l.offsets.top);
            r.size = anchorEnd - anchorStart - Vec2(
                l.offsets.left + l.offsets.right, l.offsets.top + l.offsets.bottom);
            if (l.anchorMin.x == l.anchorMax.x && l.widthMode == SizeMode::Fixed) r.size.x = l.preferredSize.x;
            if (l.anchorMin.y == l.anchorMax.y && l.heightMode == SizeMode::Fixed) r.size.y = l.preferredSize.y;
            r.size.x = clampSize(r.size.x, l.minimumSize.x, l.maximumSize.x);
            r.size.y = clampSize(r.size.y, l.minimumSize.y, l.maximumSize.y);
            child.bounds = applyMargin(r, l.margin);
            arrangeChildren(child, contentRect(child));
        }
        return;
    }

    const LayoutMode mode = parent.layout.mode;
    float totalSpacing = 0.0f;
    float fixedMain = 0.0f;
    float fillWeight = 0.0f;
    std::size_t visibleCount = 0;
    for (const auto& ptr : children) {
        const Widget& child = *ptr;
        if (!child.visible) continue;
        ++visibleCount;
        const auto& l = child.layout;
        const bool fill = mode == LayoutMode::Horizontal
            ? l.widthMode == SizeMode::Fill : l.heightMode == SizeMode::Fill;
        if (fill) fillWeight += 1.0f;
        else fixedMain += mainSize(l.preferredSize, mode);
        fixedMain += mode == LayoutMode::Horizontal
            ? l.margin.left + l.margin.right : l.margin.top + l.margin.bottom;
    }
    if (visibleCount > 1) totalSpacing = parent.layout.spacing * static_cast<float>(visibleCount - 1);
    const float availableMain = std::max(0.0f, mainSize(content.size, mode) - totalSpacing - fixedMain);
    const float fillMain = fillWeight > 0.0f ? availableMain / fillWeight : 0.0f;
    float cursor = mainSize(content.position, mode);

    for (auto& ptr : children) {
        Widget& child = *ptr;
        if (!child.visible) continue;
        const auto& l = child.layout;
        const bool fill = mode == LayoutMode::Horizontal
            ? l.widthMode == SizeMode::Fill : l.heightMode == SizeMode::Fill;
        float desiredMain = fill ? fillMain : mainSize(l.preferredSize, mode);
        const float minMain = mainSize(l.minimumSize, mode);
        const float maxMain = mainSize(l.maximumSize, mode);
        desiredMain = clampSize(desiredMain, minMain, maxMain);

        const float marginBefore = mode == LayoutMode::Horizontal ? l.margin.left : l.margin.top;
        const float marginAfter = mode == LayoutMode::Horizontal ? l.margin.right : l.margin.bottom;
        cursor += marginBefore;
        float crossAvailable = crossSize(content.size, mode) -
            (mode == LayoutMode::Horizontal ? l.margin.top + l.margin.bottom : l.margin.left + l.margin.right);
        const bool crossFill = mode == LayoutMode::Horizontal
            ? l.heightMode == SizeMode::Fill : l.widthMode == SizeMode::Fill;
        float desiredCross = crossFill ? crossAvailable : crossSize(l.preferredSize, mode);
        desiredCross = clampSize(desiredCross, crossSize(l.minimumSize, mode), crossSize(l.maximumSize, mode));

        Vec2 pos = content.position;
        Vec2 size{0.0f};
        if (mode == LayoutMode::Horizontal) {
            pos.x = cursor;
            pos.y += l.margin.top;
            size = {desiredMain, desiredCross};
        } else {
            pos.y = cursor;
            pos.x += l.margin.left;
            size = {desiredCross, desiredMain};
        }
        child.bounds = {pos, size};
        arrangeChildren(child, contentRect(child));
        cursor += desiredMain + marginAfter + parent.layout.spacing;
    }
}

Widget* hitTest(Widget& root, Vec2 point) {
    if (!root.visible || !root.enabled) return nullptr;
    if (root.clipChildren && !root.bounds.contains(point)) return nullptr;
    const auto& children = root.children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (Widget* hit = hitTest(**it, point)) return hit;
    }
    return root.hitTestVisible && root.bounds.contains(point) ? &root : nullptr;
}


namespace {
void writeWidget(std::ostream& out, const Widget& w, int depth) {
    const auto& l = w.layout; const auto& st = w.style;
    out << depth << ' ' << static_cast<int>(w.type) << ' ' << std::quoted(w.name) << ' ' << std::quoted(w.text)
        << ' ' << w.visible << ' ' << w.enabled << ' ' << w.hitTestVisible << ' ' << w.clipChildren
        << ' ' << static_cast<int>(l.mode) << ' ' << static_cast<int>(l.widthMode) << ' ' << static_cast<int>(l.heightMode)
        << ' ' << l.preferredSize.x << ' ' << l.preferredSize.y
        << ' ' << l.minimumSize.x << ' ' << l.minimumSize.y << ' ' << l.maximumSize.x << ' ' << l.maximumSize.y
        << ' ' << l.anchorMin.x << ' ' << l.anchorMin.y << ' ' << l.anchorMax.x << ' ' << l.anchorMax.y
        << ' ' << l.offsets.left << ' ' << l.offsets.top << ' ' << l.offsets.right << ' ' << l.offsets.bottom
        << ' ' << l.margin.left << ' ' << l.margin.top << ' ' << l.margin.right << ' ' << l.margin.bottom
        << ' ' << l.padding.left << ' ' << l.padding.top << ' ' << l.padding.right << ' ' << l.padding.bottom
        << ' ' << l.spacing
        << ' ' << st.background.r << ' ' << st.background.g << ' ' << st.background.b << ' ' << st.background.a
        << ' ' << st.foreground.r << ' ' << st.foreground.g << ' ' << st.foreground.b << ' ' << st.foreground.a
        << ' ' << st.cornerRadius << ' ' << st.opacity << '\n';
    for (const auto& child : w.children()) writeWidget(out, *child, depth + 1);
}

bool readWidgetLine(const std::string& line, int& depth, std::unique_ptr<Widget>& out) {
    std::istringstream in(line);
    int type = 0, layoutMode = 0, widthMode = 0, heightMode = 0;
    std::string name, text;
    bool visible = true, enabled = true, hit = true, clip = false;
    auto w = std::make_unique<Widget>();
    if (!(in >> depth >> type >> std::quoted(name) >> std::quoted(text)
        >> visible >> enabled >> hit >> clip >> layoutMode >> widthMode >> heightMode)) return false;
    if (depth < 0 || depth > 128 || type < 0 || type > static_cast<int>(WidgetType::Custom)
        || layoutMode < 0 || layoutMode > static_cast<int>(LayoutMode::Vertical)
        || widthMode < 0 || widthMode > static_cast<int>(SizeMode::Fill)
        || heightMode < 0 || heightMode > static_cast<int>(SizeMode::Fill)) return false;
    w->type = static_cast<WidgetType>(type); w->name = std::move(name); w->text = std::move(text);
    w->visible = visible; w->enabled = enabled; w->hitTestVisible = hit; w->clipChildren = clip;
    auto& l = w->layout; auto& st = w->style;
    l.mode = static_cast<LayoutMode>(layoutMode); l.widthMode = static_cast<SizeMode>(widthMode); l.heightMode = static_cast<SizeMode>(heightMode);
    if (!(in >> l.preferredSize.x >> l.preferredSize.y
        >> l.minimumSize.x >> l.minimumSize.y >> l.maximumSize.x >> l.maximumSize.y
        >> l.anchorMin.x >> l.anchorMin.y >> l.anchorMax.x >> l.anchorMax.y
        >> l.offsets.left >> l.offsets.top >> l.offsets.right >> l.offsets.bottom
        >> l.margin.left >> l.margin.top >> l.margin.right >> l.margin.bottom
        >> l.padding.left >> l.padding.top >> l.padding.right >> l.padding.bottom >> l.spacing
        >> st.background.r >> st.background.g >> st.background.b >> st.background.a
        >> st.foreground.r >> st.foreground.g >> st.foreground.b >> st.foreground.a
        >> st.cornerRadius >> st.opacity)) return false;
    std::string extra;
    if (in >> extra) return false;
    out = std::move(w);
    return true;
}
}

bool saveDocument(const Widget& root, const std::string& path, std::string* error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) { if (error) *error = "cannot open UI document for writing"; return false; }
    out << "ANVIL_UI 1\n" << std::setprecision(9);
    writeWidget(out, root, 0);
    if (!out) { if (error) *error = "failed while writing UI document"; return false; }
    return true;
}

std::unique_ptr<Widget> loadDocument(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { if (error) *error = "cannot open UI document"; return {}; }
    std::string header;
    if (!std::getline(in, header) || header != "ANVIL_UI 1") {
        if (error) *error = "unsupported or invalid UI document header";
        return {};
    }
    std::unique_ptr<Widget> root;
    std::vector<Widget*> parents;
    std::string line; int previousDepth = -1;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        int depth = 0; std::unique_ptr<Widget> node;
        if (!readWidgetLine(line, depth, node) || depth > previousDepth + 1) {
            if (error) *error = "malformed UI node or invalid hierarchy depth";
            return {};
        }
        if (depth == 0) {
            if (root) { if (error) *error = "UI document contains multiple roots"; return {}; }
            root = std::move(node); parents.clear(); parents.push_back(root.get());
        } else {
            if (!root || static_cast<std::size_t>(depth) > parents.size()) {
                if (error) *error = "UI node has no valid parent";
                return {};
            }
            Widget* parent = parents[static_cast<std::size_t>(depth - 1)];
            Widget& added = parent->add(std::move(node));
            parents.resize(static_cast<std::size_t>(depth));
            parents.push_back(&added);
        }
        previousDepth = depth;
    }
    if (!root) { if (error) *error = "UI document is empty"; return {}; }
    return root;
}

} // namespace anvil::ui

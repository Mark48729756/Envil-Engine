#pragma once

#include <cstdint>
#include <memory>
#include <iosfwd>
#include <string>
#include <vector>

namespace anvil::ui {

using WidgetId = std::uint64_t;

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    constexpr Vec2() = default;
    constexpr explicit Vec2(float value) : x(value), y(value) {}
    constexpr Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}
    constexpr Vec2 operator+(Vec2 rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr Vec2 operator-(Vec2 rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr Vec2 operator*(Vec2 rhs) const { return {x * rhs.x, y * rhs.y}; }
    constexpr Vec2 operator*(float rhs) const { return {x * rhs, y * rhs}; }
    constexpr Vec2& operator+=(Vec2 rhs) { x += rhs.x; y += rhs.y; return *this; }
};

struct Color { float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f; };

struct Rect {
    Vec2 position{};
    Vec2 size{};

    bool contains(Vec2 point) const;
};

enum class WidgetType : std::uint8_t { Panel, Label, Button, Image, Custom };
enum class LayoutMode : std::uint8_t { Absolute, Horizontal, Vertical };
enum class SizeMode : std::uint8_t { Fixed, Fill };

struct Edges {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct Layout {
    LayoutMode mode = LayoutMode::Absolute;
    SizeMode widthMode = SizeMode::Fixed;
    SizeMode heightMode = SizeMode::Fixed;
    Vec2 preferredSize{100.0f, 30.0f};
    Vec2 minimumSize{0.0f};
    Vec2 maximumSize{100000.0f};
    // Used by Absolute layout. Anchors are normalized in the parent content rect.
    Vec2 anchorMin{0.0f};
    Vec2 anchorMax{0.0f};
    // Insets from the anchored rectangle. Positive right/bottom values move inward.
    Edges offsets{};
    Edges margin{};
    Edges padding{};
    float spacing = 0.0f;
};

struct Style {
    Color background{0.0f};
    Color foreground{1.0f};
    float cornerRadius = 0.0f;
    float opacity = 1.0f;
};

class Widget {
public:
    explicit Widget(WidgetType type = WidgetType::Panel, std::string name = {});
    ~Widget() = default;
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) noexcept = default;
    Widget& operator=(Widget&&) noexcept = default;

    Widget& add(std::unique_ptr<Widget> child);
    std::unique_ptr<Widget> remove(WidgetId id);
    Widget* find(WidgetId id);
    const Widget* find(WidgetId id) const;

    WidgetId id() const { return id_; }
    WidgetType type = WidgetType::Panel;
    std::string name;
    std::string text;
    Layout layout;
    Style style;
    Rect bounds;
    bool visible = true;
    bool enabled = true;
    bool hitTestVisible = true;
    bool clipChildren = false;

    const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }

private:
    friend class LayoutEngine;
    WidgetId id_ = 0;
    std::vector<std::unique_ptr<Widget>> children_;
};

// Computes widget bounds from the root viewport. No graphics API is required.
class LayoutEngine {
public:
    static void arrange(Widget& root, Rect viewport);
private:
    static void arrangeChildren(Widget& parent, Rect content);
};

// Returns the deepest visible, enabled, hit-testable widget under a point.
// Later siblings are considered visually above earlier siblings.
Widget* hitTest(Widget& root, Vec2 point);

// Versioned, deterministic text serialization for editable UI documents.
bool saveDocument(const Widget& root, const std::string& path, std::string* error = nullptr);
std::unique_ptr<Widget> loadDocument(const std::string& path, std::string* error = nullptr);

} // namespace anvil::ui

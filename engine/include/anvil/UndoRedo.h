#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace anvil {
// Small command history with bounded memory and redo invalidation after new edits.
class UndoRedoStack {
public:
    struct Command { std::string label; std::function<void()> undo; std::function<void()> redo; };
    explicit UndoRedoStack(std::size_t capacity = 256) : capacity_(capacity ? capacity : 1) {}
    void pushApplied(Command command) { if(!command.undo || !command.redo) return; if(cursor_ < commands_.size()) commands_.erase(commands_.begin()+static_cast<std::ptrdiff_t>(cursor_),commands_.end()); commands_.push_back(std::move(command)); if(commands_.size()>capacity_) commands_.erase(commands_.begin()); cursor_=commands_.size(); }
    bool undo() { if(!canUndo()) return false; commands_[--cursor_].undo(); return true; }
    bool redo() { if(!canRedo()) return false; commands_[cursor_++].redo(); return true; }
    bool canUndo() const { return cursor_>0; }
    bool canRedo() const { return cursor_<commands_.size(); }
    const std::string& undoLabel() const { static const std::string empty; return canUndo()?commands_[cursor_-1].label:empty; }
    const std::string& redoLabel() const { static const std::string empty; return canRedo()?commands_[cursor_].label:empty; }
    void clear() { commands_.clear(); cursor_=0; }
    std::size_t size() const { return commands_.size(); }
private: std::vector<Command> commands_; std::size_t cursor_=0, capacity_;
};
}

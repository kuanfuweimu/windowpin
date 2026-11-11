#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace wp {
using Window = std::uintptr_t;
struct Snapshot { std::uint32_t pid; bool topmost; };
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool Inspect(Window, Snapshot&) = 0;
    virtual bool Mark(Window) = 0;
    virtual bool Marked(Window) = 0;
    virtual void Unmark(Window) = 0;
    virtual bool SetTopmost(Window, bool) = 0;
};
struct Change { std::uint32_t pid; bool original; };
enum class Result { Pinned, Unpinned, Invalid, Failed };

// Only touch windows bearing this instance's marker. Windows removes properties
// on destruction, so recycling an HWND cannot accidentally restore a new window.
class PinManager {
    Backend& api_;
    std::map<Window, Change> changes_;
public:
    explicit PinManager(Backend& api) : api_(api) {}
    void Prune() {
        for (auto it = changes_.begin(); it != changes_.end();) {
            Snapshot s{};
            if (!api_.Inspect(it->first, s) || s.pid != it->second.pid || !api_.Marked(it->first))
                it = changes_.erase(it);
            else ++it;
        }
    }
    Result Toggle(Window w) {
        Prune();
        Snapshot s{};
        if (!api_.Inspect(w, s)) return Result::Invalid;
        const bool added = changes_.find(w) == changes_.end();
        if (added) {
            if (!api_.Mark(w)) return Result::Failed;
            changes_.emplace(w, Change{s.pid, s.topmost});
        }
        if (!api_.SetTopmost(w, !s.topmost)) {
            if (added) { api_.Unmark(w); changes_.erase(w); }
            return Result::Failed;
        }
        if (changes_.at(w).original == !s.topmost) {
            api_.Unmark(w);
            changes_.erase(w);
        }
        return s.topmost ? Result::Unpinned : Result::Pinned;
    }
    std::size_t RestoreAll() {
        Prune();
        for (auto it = changes_.begin(); it != changes_.end();) {
            if (api_.SetTopmost(it->first, it->second.original)) {
                api_.Unmark(it->first);
                it = changes_.erase(it);
            } else ++it;
        }
        return changes_.size();
    }
    // On forced session shutdown leave no instance markers behind.
    void Forget() { Prune(); for (auto& [w, change] : changes_) { (void)change; api_.Unmark(w); } changes_.clear(); }
    bool Changed(Window w) const { return changes_.find(w) != changes_.end(); }
    std::size_t Count() { Prune(); return changes_.size(); }
};
}

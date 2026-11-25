// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include <algorithm>

struct MouseLockRect
{
    long left, top, right, bottom;

    bool valid() const { return right > left && bottom > top; }

    MouseLockRect intersected(const MouseLockRect &other) const
    {
        return {(std::max)(left, other.left), (std::max)(top, other.top),
                (std::min)(right, other.right), (std::min)(bottom, other.bottom)};
    }
};

class MouseLockState
{
public:
    void beginSession(bool automatic)
    {
        requested = automatic;
        heldShortcut = 0;
    }

    void focusLost() { heldShortcut = 0; }

    bool wantsClip(bool gameplay, bool foreground, bool relativeInput) const
    {
        return requested && gameplay && foreground && relativeInput;
    }

    bool shortcut(int key, int binding, bool pressed, bool repeated, bool eligible)
    {
        if (!pressed && heldShortcut && key == heldShortcut) {
            if (!repeated)
                heldShortcut = 0;
            return true;
        }
        if (!pressed || !eligible || !binding || key != binding)
            return false;
        if (!repeated && !heldShortcut) {
            requested = !requested;
            heldShortcut = key;
        }
        return true;
    }

private:
    bool requested = false;
    int heldShortcut = 0;
};

inline bool acceptRelativeMouseMotion(bool relative, bool gameplay, bool captured, bool cursorInside)
{
    return relative && gameplay && (captured || cursorInside);
}

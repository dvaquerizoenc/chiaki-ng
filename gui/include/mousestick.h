// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include <chiaki/controller.h>
#include <algorithm>
#include <array>
#include <cmath>

enum class MouseStickMode { Off, Left, Right };

enum : int { MouseSensitivityMax = 500, MouseFullTiltDefault = 24, MouseFullTiltMin = 4, MouseFullTiltMax = 256 };

inline int calibratedMouseSensitivity(int current, int measuredDistance, int desiredDistance)
{
    if (current <= 0 || measuredDistance <= 0 || desiredDistance <= 0)
        return 0;
    const double estimate = double(current) * measuredDistance / desiredDistance;
    return int(std::lround(std::max(1.0, std::min(double(MouseSensitivityMax), estimate))));
}

struct MouseStickOptions
{
    MouseStickMode mode = MouseStickMode::Off;
    bool immediate = true;
    bool rawInput = true;
    bool lockDuringGameplay = true;
    int lockShortcut = 8;
    bool wasd = true;
    int horizontal = 100;
    int vertical = 100;
    int aimHorizontal = 55;
    int aimVertical = 55;
    int fullTilt = MouseFullTiltDefault;
    int deadzone = 0;
    int smoothingMs = 0;
    static constexpr int DefaultButton = -1;
    static constexpr int LegacyTouchpad = -2;
    std::array<int, 5> buttons{{-1, -1, -1, -1, -1}};

    int action(size_t button) const
    {
        if (buttons[button] != DefaultButton)
            return buttons[button];
        if (mode == MouseStickMode::Right) {
            if (button == 0)
                return CHIAKI_CONTROLLER_ANALOG_BUTTON_R2;
            if (button == 1)
                return CHIAKI_CONTROLLER_ANALOG_BUTTON_L2;
            return 0;
        }
        return button == 0 ? 0 : LegacyTouchpad;
    }
};

struct MouseStickAxes
{
    double x;
    double y;
};

struct MouseStickSample
{
    MouseStickAxes axes;
    bool saturated;
};

struct MouseStickSaturation
{
    uint64_t samples = 0;
    uint64_t saturated = 0;

    void record(bool reachedLimit)
    {
        ++samples;
        saturated += reachedLimit;
    }

    double percent() const { return samples ? 100.0 * saturated / samples : 0.0; }
};

inline uint32_t mouseButtonActions(const MouseStickOptions &options, unsigned pressed, bool touchpad)
{
    uint32_t actions = 0;
    for (size_t i = 0; i < options.buttons.size(); ++i) {
        if (!(pressed & (1u << i)))
            continue;
        const int action = options.action(i);
        if (action == MouseStickOptions::LegacyTouchpad)
            actions |= touchpad ? CHIAKI_CONTROLLER_BUTTON_TOUCHPAD : 0;
        else
            actions |= action;
    }
    return actions;
}

inline int keyboardStickAxis(unsigned held, unsigned positive, unsigned negative)
{
    return (int(bool(held & positive)) - int(bool(held & negative))) * 32767;
}

inline void applyMouseStickAxes(ChiakiControllerState &state, MouseStickMode mode, int16_t x, int16_t y)
{
    if (mode == MouseStickMode::Left && !state.left_x && !state.left_y) {
        state.left_x = x;
        state.left_y = y;
    }
    if (mode == MouseStickMode::Right && !state.right_x && !state.right_y) {
        state.right_x = x;
        state.right_y = y;
    }
}

class MouseStickMotion
{
public:
    enum : uint64_t { SampleUs = 16000, ReleaseUs = 12000 };

    MouseStickSample move(const MouseStickOptions &options, double dx, double dy, uint64_t now, bool aiming)
    {
        const bool continuing = lastMovement && now >= lastMovement && now - lastMovement < holdUs + ReleaseUs;
        const uint64_t elapsed = continuing ? std::max<uint64_t>(1000, now - lastMovement) : SampleUs;
        const auto interval = std::min<uint64_t>(elapsed, SampleUs);
        // Scale relative movement to the legacy displacement per sampling interval.
        // Time normalization leaves the first motion and the batched mode unscaled.
        const double timeScale = options.immediate ? double(SampleUs) / interval : 1.0;
        const double scale = timeScale / (100.0 * std::max(1, options.fullTilt));
        MouseStickAxes target{dx * scale * (aiming ? options.aimHorizontal : options.horizontal),
                              dy * scale * (aiming ? options.aimVertical : options.vertical)};
        const double radius = std::hypot(target.x, target.y);
        if (radius > 0) {
            const double deadzone = options.deadzone / 100.0;
            const double compensated = deadzone + (1 - deadzone) * std::min(1.0, radius);
            target.x *= compensated / radius;
            target.y *= compensated / radius;
        }
        if (options.smoothingMs > 0 && continuing && aiming == wasAiming) {
            const double alpha = 1 - std::exp(-double(elapsed) / (1000 * options.smoothingMs));
            const auto previous = at(now);
            target.x = previous.x + alpha * (target.x - previous.x);
            target.y = previous.y + alpha * (target.y - previous.y);
        }
        wasAiming = aiming;
        lastMovement = now;
        holdUs = options.immediate ? std::max<uint64_t>(4000, std::min<uint64_t>(12000, interval * 2)) : SampleUs;
        last = target;
        return {target, radius >= 1.0};
    }

    MouseStickAxes at(uint64_t now) const
    {
        if (!lastMovement || now < lastMovement)
            return {0, 0};
        const uint64_t elapsed = now - lastMovement;
        if (elapsed <= holdUs)
            return last;
        if (elapsed >= holdUs + ReleaseUs)
            return {0, 0};
        const double scale = double(holdUs + ReleaseUs - elapsed) / ReleaseUs;
        return {last.x * scale, last.y * scale};
    }

    uint64_t nextReleaseTick(uint64_t now) const
    {
        const uint64_t first = lastMovement + holdUs;
        const uint64_t end = first + ReleaseUs;
        if (!lastMovement || now >= end)
            return 0;
        return now < first ? first : std::min<uint64_t>(end, now + 3000);
    }

    void reset() { *this = MouseStickMotion{}; }

private:
    MouseStickAxes last{0, 0};
    uint64_t lastMovement = 0;
    uint64_t holdUs = 0;
    bool wasAiming = false;
};

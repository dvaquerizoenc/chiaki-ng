// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "../gui/include/mousestick.h"
#include "../gui/include/mouselock.h"
#include <munit.h>

static MunitResult test_independent_sticks(const MunitParameter[], void *)
{
    ChiakiControllerState state = {};
    state.left_x = keyboardStickAxis(1, 1, 2);
    state.left_y = keyboardStickAxis(2, 1, 2);
    applyMouseStickAxes(state, MouseStickMode::Right, 600, -200);
    munit_assert_int(state.left_x, ==, 32767);
    munit_assert_int(state.left_y, ==, -32767);
    munit_assert_int(state.right_x, ==, 600);
    munit_assert_int(state.right_y, ==, -200);
    applyMouseStickAxes(state, MouseStickMode::Left, -100, 500);
    munit_assert_int(state.left_x, ==, 32767);
    munit_assert_int(keyboardStickAxis(3, 1, 2), ==, 0);
    munit_assert_int(keyboardStickAxis(2, 1, 2), ==, -32767);
    applyMouseStickAxes(state, MouseStickMode::Off, 0, 0);
    munit_assert_int(state.right_x, ==, 600);
    return MUNIT_OK;
}

static MunitResult test_mouse_buttons(const MunitParameter[], void *)
{
    MouseStickOptions options;
    munit_assert_uint32(mouseButtonActions(options, 2, true), ==, CHIAKI_CONTROLLER_BUTTON_TOUCHPAD);
    munit_assert_uint32(mouseButtonActions(options, 2, false), ==, 0);
    options.mode = MouseStickMode::Right;
    munit_assert_uint32(mouseButtonActions(options, 1, true), ==, CHIAKI_CONTROLLER_ANALOG_BUTTON_R2);
    munit_assert_uint32(mouseButtonActions(options, 2, true), ==, CHIAKI_CONTROLLER_ANALOG_BUTTON_L2);
    options.buttons[0] = CHIAKI_CONTROLLER_BUTTON_CROSS;
    options.buttons[1] = CHIAKI_CONTROLLER_BUTTON_CROSS;
    options.buttons[3] = CHIAKI_CONTROLLER_BUTTON_R3;
    munit_assert_uint32(mouseButtonActions(options, 11, true), ==, CHIAKI_CONTROLLER_BUTTON_CROSS | CHIAKI_CONTROLLER_BUTTON_R3);
    munit_assert_uint32(mouseButtonActions(options, 2, true), ==, CHIAKI_CONTROLLER_BUTTON_CROSS);
    munit_assert_uint32(mouseButtonActions(options, 0, true), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_sensitivity_and_smoothing(const MunitParameter[], void *)
{
    MouseStickOptions options;
    MouseStickMotion motion;
    options.horizontal = 200;
    options.vertical = 50;
    options.aimHorizontal = 25;
    options.aimVertical = 75;
    auto axes = motion.move(options, 2.4, 2.4, 100000, false).axes;
    munit_assert_double_equal(axes.x, 0.2, 9);
    munit_assert_double_equal(axes.y, 0.05, 9);
    motion.reset();
    axes = motion.move(options, 2.4, 2.4, 100000, true).axes;
    munit_assert_double_equal(axes.x, 0.025, 9);
    munit_assert_double_equal(axes.y, 0.075, 9);

    options.deadzone = 20;
    motion.reset();
    axes = motion.move(options, 2.4, 0, 100000, false).axes;
    munit_assert_double_equal(axes.x, 0.36, 9);
    axes = motion.move(options, 100, 100, 110000, false).axes;
    munit_assert_double_equal(std::hypot(axes.x, axes.y), 1.0, 9);

    options.deadzone = 0;
    options.smoothingMs = 20;
    motion.reset();
    motion.move(options, 2.4, 0, 100000, false);
    axes = motion.move(options, -1.2, 0, 104000, false).axes;
    munit_assert_true(axes.x > 0 && axes.x < 0.2);
    options.smoothingMs = 0;
    axes = motion.move(options, -1.2, 0, 108000, false).axes;
    munit_assert_true(axes.x < 0);
    return MUNIT_OK;
}

static MunitResult test_neutral_and_precise_restart(const MunitParameter[], void *)
{
    MouseStickOptions options;
    MouseStickMotion motion;
    const auto first = motion.move(options, 2.4, -1.2, 100000, false).axes;
    munit_assert_uint64(motion.nextReleaseTick(100000), ==, 112000);
    const auto half = motion.at(118000);
    munit_assert_double_equal(half.x, first.x / 2, 9);
    munit_assert_double_equal(half.y, first.y / 2, 9);
    munit_assert_double_equal(motion.at(124000).x, 0.0, 9);
    munit_assert_uint64(motion.nextReleaseTick(150000), ==, 0);
    munit_assert_double_equal(motion.move(options, 2.4, -1.2, 150000, false).axes.x, first.x, 9);
    motion.reset();
    munit_assert_double_equal(motion.at(151000).y, 0.0, 9);
    munit_assert_uint64(motion.nextReleaseTick(151000), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_saturation_and_flick(const MunitParameter[], void *)
{
    MouseStickOptions options;
    MouseStickMotion motion;
    MouseStickSaturation telemetry;

    auto sample = motion.move(options, 0.25, -0.125, 100000, false);
    telemetry.record(sample.saturated);
    munit_assert_false(sample.saturated);
    munit_assert_double_equal(sample.axes.x, 0.25 / 24, 9);
    munit_assert_double_equal(sample.axes.y, -0.125 / 24, 9);
    sample = motion.move(options, 1.5, 0, 101000, false);
    telemetry.record(sample.saturated);
    munit_assert_true(sample.saturated);
    munit_assert_double_equal(sample.axes.x, 1.0, 9);
    munit_assert_double_equal(telemetry.percent(), 50.0, 9);

    // The saturated event uses the same release envelope and stores no overflow.
    sample = motion.move(options, 123, 456, 102000, false);
    munit_assert_true(sample.saturated);
    munit_assert_double_equal(std::hypot(sample.axes.x, sample.axes.y), 1.0, 9);
    munit_assert_double_equal(sample.axes.y / sample.axes.x, 456.0 / 123, 9);
    munit_assert_uint64(motion.nextReleaseTick(102000), ==, 106000);
    munit_assert_double_equal(motion.at(118000).x, 0.0, 9);
    sample = motion.move(options, -0.25, 0, 120000, false);
    munit_assert_false(sample.saturated);
    munit_assert_double_equal(sample.axes.x, -0.25 / 24, 9);

    options.horizontal = MouseSensitivityMax;
    options.fullTilt = 48;
    motion.reset();
    sample = motion.move(options, 4.8, 0, 200000, false);
    munit_assert_false(sample.saturated);
    munit_assert_double_equal(sample.axes.x, 0.5, 9);
    options.fullTilt = 12;
    motion.reset();
    sample = motion.move(options, 4.8, 0, 200000, false);
    munit_assert_true(sample.saturated);
    munit_assert_double_equal(sample.axes.x, 1.0, 9);
    return MUNIT_OK;
}

static MunitResult test_calibration(const MunitParameter[], void *)
{
    munit_assert_int(calibratedMouseSensitivity(100, 450, 300), ==, 150);
    munit_assert_int(calibratedMouseSensitivity(55, 150, 300), ==, 28);
    munit_assert_int(calibratedMouseSensitivity(250, 900, 300), ==, MouseSensitivityMax);
    munit_assert_int(calibratedMouseSensitivity(55, 0, 300), ==, 0);
    munit_assert_int(calibratedMouseSensitivity(55, 100, 0), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_mouse_lock_lifecycle(const MunitParameter[], void *)
{
    MouseLockState lock;
    lock.beginSession(true);
    munit_assert_true(lock.wantsClip(true, true, true));
    munit_assert_false(lock.wantsClip(true, false, true));
    munit_assert_false(lock.wantsClip(false, true, true));
    munit_assert_false(lock.wantsClip(true, true, false));

    munit_assert_false(lock.shortcut(7, 8, true, false, true));
    munit_assert_false(lock.shortcut(8, 8, true, false, false));
    munit_assert_true(lock.shortcut(8, 8, true, false, true));
    munit_assert_false(lock.wantsClip(true, true, true));
    munit_assert_true(lock.shortcut(8, 8, true, true, true));
    munit_assert_false(lock.wantsClip(true, true, true));
    // The release belongs to the hotkey even if a menu opened after the press.
    munit_assert_true(lock.shortcut(8, 8, false, false, false));
    lock.focusLost();
    munit_assert_false(lock.wantsClip(true, true, true));
    munit_assert_true(lock.shortcut(8, 8, true, false, true));
    lock.focusLost();
    munit_assert_true(lock.wantsClip(true, true, true));

    lock.beginSession(false);
    munit_assert_false(lock.wantsClip(true, true, true));
    munit_assert_true(lock.shortcut(9, 9, true, false, true));
    munit_assert_true(lock.wantsClip(true, true, true));
    return MUNIT_OK;
}

static MunitResult test_mouse_lock_geometry_and_motion(const MunitParameter[], void *)
{
    const MouseLockRect client{-1000, -240, 100, 620};
    const MouseLockRect leftMonitor{-1920, -1080, 0, 0};
    const auto clip = client.intersected(leftMonitor);
    munit_assert_true(clip.valid());
    munit_assert_long(clip.left, ==, -1000);
    munit_assert_long(clip.top, ==, -240);
    munit_assert_long(clip.right, ==, 0);
    munit_assert_long(clip.bottom, ==, 0);
    munit_assert_false(client.intersected({100, 0, 1200, 1080}).valid());

    munit_assert_true(acceptRelativeMouseMotion(true, true, true, false));
    munit_assert_false(acceptRelativeMouseMotion(true, true, false, false));
    munit_assert_true(acceptRelativeMouseMotion(true, true, false, true));
    munit_assert_false(acceptRelativeMouseMotion(false, true, true, true));
    munit_assert_false(acceptRelativeMouseMotion(true, false, true, true));
    return MUNIT_OK;
}

struct MouseTrace { double average; int samples; int saturated; int counts; };

static MouseTrace replay_inch(int dpi, int hz, uint64_t duration, uint64_t window, bool grouped)
{
    const uint64_t origin = 100000;
    const uint64_t period = 1000000 / hz;
    const uint64_t firstMeasured = origin + 24000;
    const uint64_t lastMeasured = origin + duration - 8000;
    MouseStickOptions options;
    options.fullTilt = 128;
    options.horizontal = 80000 / dpi;
    MouseStickMotion motion;
    MouseTrace trace{};
    uint64_t next = 0, lastInput = 0, previous = origin;
    double area = 0, last = 0;
    int dx = 0;
    const auto flush = [&](uint64_t now) {
        const auto start = std::max(previous, firstMeasured);
        const auto end = std::min(now, lastMeasured);
        if (end > start)
            area += last * (end - start);
        const auto sample = motion.moveWindow(options, dx, 0, now, lastInput, false);
        if (now >= firstMeasured && now <= lastMeasured) {
            ++trace.samples;
            trace.saturated += sample.saturated;
        }
        last = sample.axes.x;
        trace.counts += dx;
        dx = 0;
        next = 0;
        previous = now;
    };
    int lastCount = 0;
    for (uint64_t report = period; report <= duration; report += period) {
        const auto arrival = origin + (grouped ? ((report + 3999) / 4000) * 4000 : report);
        if (next && next < arrival)
            flush(next);
        if (!next)
            next = arrival + window;
        const int position = int(std::lround(double(dpi) * report / duration));
        dx += position - lastCount;
        lastCount = position;
        lastInput = arrival;
    }
    if (next)
        flush(next);
    trace.average = area / (lastMeasured - firstMeasured);
    return trace;
}

static MunitResult test_velocity_polling_and_dpi(const MunitParameter[], void *)
{
    for (int dpi : {400, 800, 1600, 3200}) {
        for (int hz : {125, 250, 500, 1000}) {
            for (uint64_t window : {1000, 2000, 3000, 4000}) {
                double previous = 0;
                for (uint64_t duration : {512000, 256000, 128000, 64000}) {
                    const auto trace = replay_inch(dpi, hz, duration, window, false);
                    const double expected = std::min(1.0, 800.0 * 16000 / duration / 128);
                    munit_assert_int(trace.counts, ==, dpi);
                    munit_assert_int(trace.samples, >, 0);
                    munit_assert_true(std::abs(trace.average - expected) < 0.035);
                    munit_assert_true(trace.average > previous);
                    munit_assert_int(trace.saturated, ==, expected == 1.0 ? trace.samples : 0);
                    previous = trace.average;
                }
            }
        }
    }
    for (uint64_t window : {1000, 2000, 3000, 4000}) {
        const auto burst = replay_inch(800, 1000, 256000, window, true);
        const auto regular = replay_inch(800, 1000, 256000, window, false);
        munit_assert_int(burst.counts, ==, regular.counts);
        munit_assert_true(std::abs(burst.average - regular.average) < 0.015);
        munit_assert_int(burst.saturated, ==, 0);
    }
    return MUNIT_OK;
}

static MunitResult test_window_reversal_and_deadline(const MunitParameter[], void *)
{
    MouseStickOptions options;
    MouseStickMotion motion;
    munit_assert_true(motion.moveWindow(options, 8, 0, 102000, 100000, false).axes.x > 0);
    munit_assert_true(motion.moveWindow(options, -1, 0, 104000, 103000, false).axes.x < 0);
    munit_assert_double_equal(motion.moveWindow(options, 0, 0, 106000, 105000, false).axes.x, 0.0, 9);
    motion.reset();
    motion.moveWindow(options, 48, 0, 202000, 200000, false);
    munit_assert_uint64(motion.nextReleaseTick(202000), ==, 212000);
    munit_assert_double_equal(motion.at(224000).x, 0.0, 9);
    return MUNIT_OK;
}

static MunitResult test_grouped_reports_vs_per_event(const MunitParameter[], void *)
{
    MouseStickOptions options;
    options.fullTilt = 128;
    MouseStickMotion legacy, windowed;
    legacy.move(options, 2, 0, 100000, false);
    windowed.moveWindow(options, 4, 0, 102000, 100000, false);

    for (uint64_t arrival = 104000; arrival <= 116000; arrival += 4000) {
        const auto first = legacy.move(options, 2, 0, arrival, false).axes.x;
        const auto second = legacy.move(options, 2, 0, arrival, false).axes.x;
        const auto grouped = windowed.moveWindow(options, 4, 0, arrival + 2000, arrival, false).axes.x;
        munit_assert_double_equal(first, 0.0625, 9);
        munit_assert_double_equal(second, 0.25, 9);
        munit_assert_double_equal(grouped, 0.125, 9);
    }
    return MUNIT_OK;
}

extern "C" {
MunitTest tests_mouse_stick[] = {
    { const_cast<char *>("/independent_sticks"), test_independent_sticks, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/buttons"), test_mouse_buttons, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/sensitivity"), test_sensitivity_and_smoothing, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/neutral"), test_neutral_and_precise_restart, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/saturation"), test_saturation_and_flick, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/calibration"), test_calibration, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/lock_lifecycle"), test_mouse_lock_lifecycle, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/lock_geometry_and_motion"), test_mouse_lock_geometry_and_motion, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/velocity_polling_and_dpi"), test_velocity_polling_and_dpi, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/window_reversal_and_deadline"), test_window_reversal_and_deadline, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/grouped_vs_per_event"), test_grouped_reports_vs_per_event, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr }
};
}

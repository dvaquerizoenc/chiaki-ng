// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "../gui/include/mousestick.h"
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
    auto axes = motion.move(options, 2.4, 2.4, 100000, false);
    munit_assert_double_equal(axes.x, 0.2, 9);
    munit_assert_double_equal(axes.y, 0.05, 9);
    motion.reset();
    axes = motion.move(options, 2.4, 2.4, 100000, true);
    munit_assert_double_equal(axes.x, 0.025, 9);
    munit_assert_double_equal(axes.y, 0.075, 9);

    options.deadzone = 20;
    motion.reset();
    axes = motion.move(options, 2.4, 0, 100000, false);
    munit_assert_double_equal(axes.x, 0.36, 9);
    axes = motion.move(options, 100, 100, 110000, false);
    munit_assert_double_equal(std::hypot(axes.x, axes.y), 1.0, 9);

    options.deadzone = 0;
    options.smoothingMs = 20;
    motion.reset();
    motion.move(options, 2.4, 0, 100000, false);
    axes = motion.move(options, -1.2, 0, 104000, false);
    munit_assert_true(axes.x > 0 && axes.x < 0.2);
    options.smoothingMs = 0;
    axes = motion.move(options, -1.2, 0, 108000, false);
    munit_assert_true(axes.x < 0);
    return MUNIT_OK;
}

static MunitResult test_neutral_and_precise_restart(const MunitParameter[], void *)
{
    MouseStickOptions options;
    MouseStickMotion motion;
    const auto first = motion.move(options, 2.4, -1.2, 100000, false);
    munit_assert_uint64(motion.nextReleaseTick(100000), ==, 112000);
    const auto half = motion.at(118000);
    munit_assert_double_equal(half.x, first.x / 2, 9);
    munit_assert_double_equal(half.y, first.y / 2, 9);
    munit_assert_double_equal(motion.at(124000).x, 0.0, 9);
    munit_assert_uint64(motion.nextReleaseTick(150000), ==, 0);
    munit_assert_double_equal(motion.move(options, 2.4, -1.2, 150000, false).x, first.x, 9);
    motion.reset();
    munit_assert_double_equal(motion.at(151000).y, 0.0, 9);
    munit_assert_uint64(motion.nextReleaseTick(151000), ==, 0);
    return MUNIT_OK;
}

extern "C" {
MunitTest tests_mouse_stick[] = {
    { const_cast<char *>("/independent_sticks"), test_independent_sticks, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/buttons"), test_mouse_buttons, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/sensitivity"), test_sensitivity_and_smoothing, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/neutral"), test_neutral_and_precise_restart, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr }
};
}

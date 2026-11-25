// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "../gui/include/controllerkeys.h"
#include <chiaki/controller.h>
#include <munit.h>

static MunitResult test_reserved_keys_round_trip(const MunitParameter[], void *)
{
    const Qt::Key keys[] = {Qt::Key_Tab, Qt::Key_Shift, Qt::Key_Control, Qt::Key_Alt,
                            Qt::Key_Escape, Qt::Key_Return, Qt::Key_Enter, Qt::Key_Backspace};
    for (const auto key : keys) {
        ControllerKeyCapture capture;
        capture.setActive(true);
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
        munit_assert_int(capture.consume(press), ==, Qt::Key_unknown);
        const auto assigned = capture.consume(release);
        munit_assert_int(assigned, ==, key);
        const auto saved = readControllerKey(writeControllerKey(assigned));
        munit_assert_int(saved, ==, key);
        munit_assert_false(controllerKeyName(saved).isEmpty());
        const QMap<Qt::Key, int> mapping{{saved, CHIAKI_CONTROLLER_BUTTON_L1}};
        munit_assert_int(controllerButtonForKeyEvent(press, mapping), ==, CHIAKI_CONTROLLER_BUTTON_L1);
        munit_assert_int(controllerButtonForKeyEvent(release, mapping), ==, CHIAKI_CONTROLLER_BUTTON_L1);
    }
    munit_assert_int(readControllerKey(QStringLiteral("2")), ==, Qt::Key_2);
    munit_assert_int(readControllerKey(QStringLiteral("Tab")), ==, Qt::Key_Tab);
    munit_assert_int(readControllerKey(QStringLiteral("qt-key:%1").arg(int(Qt::Key_Alt))), ==, Qt::Key_Alt);
    munit_assert_int(readControllerKey(QStringLiteral("qt-key:invalid")), ==, Qt::Key_unknown);
    return MUNIT_OK;
}

static MunitResult test_tab_capture_and_opening_key_release(const MunitParameter[], void *)
{
    ControllerKeyCapture capture;
    capture.setActive(true);
    QKeyEvent openingRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
    QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_Backtab, Qt::ShiftModifier);
    QKeyEvent backtab(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
    QKeyEvent repeated(QEvent::KeyRelease, Qt::Key_Backtab, Qt::ShiftModifier, {}, true);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Tab, Qt::NoModifier);
    munit_assert_int(capture.consume(openingRelease), ==, Qt::Key_unknown);
    munit_assert_int(capture.consume(override), ==, Qt::Key_unknown);
    munit_assert_int(capture.consume(backtab), ==, Qt::Key_unknown);
    munit_assert_int(capture.consume(openingRelease), ==, Qt::Key_unknown);
    munit_assert_int(capture.consume(repeated), ==, Qt::Key_unknown);
    munit_assert_int(capture.consume(release), ==, Qt::Key_Tab);
    munit_assert_int(capture.consume(release), ==, Qt::Key_unknown);
    capture.setActive(false);
    capture.setActive(true);
    munit_assert_int(capture.consume(release), ==, Qt::Key_unknown);

    const QMap<Qt::Key, int> mapping{{Qt::Key_Tab, CHIAKI_CONTROLLER_BUTTON_L1}};
    munit_assert_int(controllerButtonForKeyEvent(backtab, mapping), ==, CHIAKI_CONTROLLER_BUTTON_L1);
    munit_assert_int(controllerButtonForKeyEvent(release, mapping), ==, CHIAKI_CONTROLLER_BUTTON_L1);
    munit_assert_int(controllerButtonForKeyEvent(repeated, mapping), ==, 0);
    munit_assert_int(controllerButtonForKeyEvent(override, mapping), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_input_target(const MunitParameter[], void *)
{
    const auto ui = ControllerKeyTarget::Interface;
    const auto gameplay = ControllerKeyTarget::Gameplay;
    const auto capture = ControllerKeyTarget::Capture;
    munit_assert_int(int(controllerKeyTarget(QEvent::ShortcutOverride, true, true, false)), ==, int(capture));
    munit_assert_int(int(controllerKeyTarget(QEvent::KeyPress, true, false, true)), ==, int(capture));
    munit_assert_int(int(controllerKeyTarget(QEvent::KeyRelease, true, false, false)), ==, int(capture));
    munit_assert_int(int(controllerKeyTarget(QEvent::ShortcutOverride, false, true, false)), ==, int(gameplay));
    munit_assert_int(int(controllerKeyTarget(QEvent::KeyPress, false, true, true)), ==, int(gameplay));
    munit_assert_int(int(controllerKeyTarget(QEvent::KeyRelease, false, true, true)), ==, int(gameplay));
    munit_assert_int(int(controllerKeyTarget(QEvent::KeyPress, false, true, false)), ==, int(ui));
    munit_assert_int(int(controllerKeyTarget(QEvent::KeyPress, false, false, true)), ==, int(ui));
    munit_assert_int(int(controllerKeyTarget(QEvent::ShortcutOverride, false, false, true)), ==, int(ui));
    return MUNIT_OK;
}

static MunitResult test_reassign_default_navigation_keys(const MunitParameter[], void *)
{
    const QMap<int, Qt::Key> defaults{{CHIAKI_CONTROLLER_BUTTON_L1, Qt::Key_2},
        {CHIAKI_CONTROLLER_BUTTON_CROSS, Qt::Key_Return},
        {CHIAKI_CONTROLLER_BUTTON_MOON, Qt::Key_Backspace}, {CHIAKI_CONTROLLER_BUTTON_PS, Qt::Key_Escape}};
    for (const auto oldButton : {CHIAKI_CONTROLLER_BUTTON_CROSS, CHIAKI_CONTROLLER_BUTTON_MOON, CHIAKI_CONTROLLER_BUTTON_PS}) {
        const auto key = defaults.value(oldButton);
        const auto changes = controllerKeyChanges(defaults, CHIAKI_CONTROLLER_BUTTON_L1, key);
        munit_assert_int(changes.size(), ==, 2);
        munit_assert_int(changes.value(oldButton), ==, Qt::Key_unknown);
        munit_assert_int(changes.value(CHIAKI_CONTROLLER_BUTTON_L1), ==, key);
        munit_assert_int(readControllerKey(writeControllerKey(changes.value(oldButton))), ==, Qt::Key_unknown);
    }
    return MUNIT_OK;
}

extern "C" {
MunitTest tests_controller_keys[] = {
    { const_cast<char *>("/reserved_keys"), test_reserved_keys_round_trip, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/tab_capture"), test_tab_capture_and_opening_key_release, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/input_target"), test_input_target, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { const_cast<char *>("/reassign_default_keys"), test_reassign_default_navigation_keys, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr },
    { nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr }
};
}

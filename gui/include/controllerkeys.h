// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#pragma once

#include <QKeyEvent>
#include <QCoreApplication>
#include <QKeySequence>
#include <QMap>

inline Qt::Key controllerKey(int key)
{
    if (key <= 0 || key >= int(Qt::Key_unknown))
        return Qt::Key_unknown;
    return key == Qt::Key_Backtab ? Qt::Key_Tab : Qt::Key(key);
}

inline Qt::Key readControllerKey(const QString &value)
{
    if (value.startsWith(QStringLiteral("qt-key:"))) {
        bool ok = false;
        const int key = value.mid(7).toInt(&ok);
        return ok ? controllerKey(key) : Qt::Key_unknown;
    }
    const auto sequence = QKeySequence::fromString(value, QKeySequence::NativeText);
    return sequence.isEmpty() ? Qt::Key_unknown : controllerKey(sequence[0].key());
}

inline QString writeControllerKey(Qt::Key key)
{
    key = controllerKey(key);
    const auto native = QKeySequence(key).toString(QKeySequence::NativeText);
    if (key != Qt::Key_unknown && readControllerKey(native) == key)
        return native;
    return QStringLiteral("qt-key:%1").arg(int(key));
}

inline QString controllerKeyName(Qt::Key key)
{
    key = controllerKey(key);
    if (key == Qt::Key_unknown)
        return {};
    const auto native = QKeySequence(key).toString(QKeySequence::NativeText);
    if (!native.isEmpty())
        return native;
    switch (key) {
    case Qt::Key_Shift: return QCoreApplication::translate("QShortcut", "Shift");
    case Qt::Key_Control: return QCoreApplication::translate("QShortcut", "Ctrl");
    case Qt::Key_Alt: return QCoreApplication::translate("QShortcut", "Alt");
    default: return native;
    }
}

inline QMap<int, Qt::Key> controllerKeyChanges(const QMap<int, Qt::Key> &mapping, int button, Qt::Key key)
{
    QMap<int, Qt::Key> changes{{button, key}};
    for (auto it = mapping.cbegin(); it != mapping.cend(); ++it) {
        if (it.key() != button && controllerKey(it.value()) == key)
            changes.insert(it.key(), Qt::Key_unknown);
    }
    return changes;
}

enum class ControllerKeyTarget { Interface, Capture, Gameplay };

inline ControllerKeyTarget controllerKeyTarget(QEvent::Type type, bool capturing, bool playing, bool spontaneous)
{
    if (capturing)
        return ControllerKeyTarget::Capture;
    if (playing && (spontaneous || type == QEvent::ShortcutOverride))
        return ControllerKeyTarget::Gameplay;
    return ControllerKeyTarget::Interface;
}

class ControllerKeyCapture
{
public:
    bool isActive() const { return active; }

    void setActive(bool value)
    {
        if (active == value)
            return;
        active = value;
        pending = Qt::Key_unknown;
        finished = false;
    }

    Qt::Key consume(const QKeyEvent &event)
    {
        if (!active || finished || event.isAutoRepeat())
            return Qt::Key_unknown;
        const Qt::Key key = controllerKey(event.key());
        if (key == Qt::Key_unknown)
            return Qt::Key_unknown;
        if (event.type() == QEvent::KeyPress && pending == Qt::Key_unknown)
            pending = key;
        if (event.type() == QEvent::KeyRelease && pending == key) {
            finished = true;
            return key;
        }
        return Qt::Key_unknown;
    }

private:
    bool active = false;
    bool finished = false;
    Qt::Key pending = Qt::Key_unknown;
};

inline int controllerButtonForKeyEvent(const QKeyEvent &event, const QMap<Qt::Key, int> &mapping)
{
    if (event.isAutoRepeat() || (event.type() != QEvent::KeyPress && event.type() != QEvent::KeyRelease))
        return 0;
    return mapping.value(controllerKey(event.key()));
}

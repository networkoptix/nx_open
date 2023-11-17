// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtQml/QtQml>

namespace nx::vms::client::desktop {
namespace ui {
namespace scene {

class BaseEvent: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accepted MEMBER accepted)

public:
    BaseEvent();
    bool accepted = false;
};

class SinglePointEvent: public BaseEvent
{
    Q_OBJECT
    Q_PROPERTY(QPointF localPosition MEMBER localPosition CONSTANT)
    Q_PROPERTY(QPointF windowPosition MEMBER windowPosition CONSTANT)
    Q_PROPERTY(QPointF screenPosition MEMBER screenPosition CONSTANT)
    Q_PROPERTY(QPoint position READ position CONSTANT)
    Q_PROPERTY(QPoint globalPosition READ globalPosition CONSTANT)

public:
    SinglePointEvent(const QSinglePointEvent* event);

    QPoint position() const { return localPosition.toPoint(); }
    QPoint globalPosition() const { return screenPosition.toPoint(); }

    QPointF localPosition;
    QPointF windowPosition;
    QPointF screenPosition;

    Qt::MouseButton button;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
};

class MouseEvent: public SinglePointEvent
{
    Q_OBJECT
    Q_PROPERTY(int button MEMBER button CONSTANT)
    Q_PROPERTY(int buttons MEMBER buttons CONSTANT)
    Q_PROPERTY(int modifiers MEMBER modifiers CONSTANT)

public:
    MouseEvent(const QMouseEvent* event);
};

class WheelEvent: public SinglePointEvent
{
    Q_OBJECT
    Q_PROPERTY(QPoint angleDelta MEMBER angleDelta CONSTANT)
    Q_PROPERTY(QPoint pixelDelta MEMBER pixelDelta CONSTANT)
    Q_PROPERTY(bool inverted MEMBER inverted CONSTANT)

public:
    WheelEvent(const QWheelEvent* event);

    QPoint angleDelta;
    QPoint pixelDelta;
    bool inverted = false;
};

class HoverEvent: public SinglePointEvent
{
    Q_OBJECT

public:
    HoverEvent(const QHoverEvent* event);
};

class KeyEvent: public BaseEvent
{
    Q_OBJECT
    Q_PROPERTY(int key MEMBER key CONSTANT)
    Q_PROPERTY(int modifiers MEMBER modifiers CONSTANT)

public:
    KeyEvent(const QKeyEvent* event);

    int key{};
    Qt::KeyboardModifiers modifiers;
};

} // namespace scene
} // namespace ui
} // namespace nx::vms::client::desktop

QML_DECLARE_TYPE(nx::vms::client::desktop::ui::scene::MouseEvent)
QML_DECLARE_TYPE(nx::vms::client::desktop::ui::scene::WheelEvent)
QML_DECLARE_TYPE(nx::vms::client::desktop::ui::scene::HoverEvent)

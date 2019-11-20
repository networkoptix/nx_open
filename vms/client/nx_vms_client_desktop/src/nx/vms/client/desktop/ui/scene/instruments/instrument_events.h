#pragma once

#include <QtCore/QObject>
#include <QtGui/QMouseEvent>
#include <QtQml/QtQml>

namespace nx::vms::client::desktop {
namespace ui {
namespace scene {

class MouseEvent: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPointF localPosition MEMBER localPosition CONSTANT)
    Q_PROPERTY(QPointF windowPosition MEMBER windowPosition CONSTANT)
    Q_PROPERTY(QPointF screenPosition MEMBER screenPosition CONSTANT)
    Q_PROPERTY(QPoint position READ position CONSTANT)
    Q_PROPERTY(QPoint globalPosition READ globalPosition CONSTANT)

    Q_PROPERTY(int button MEMBER button CONSTANT)
    Q_PROPERTY(int buttons MEMBER buttons CONSTANT)
    Q_PROPERTY(int modifiers MEMBER modifiers CONSTANT)

    Q_PROPERTY(bool accepted MEMBER accepted)

public:
    MouseEvent(const QMouseEvent* event);

    QPoint position() const { return localPosition.toPoint(); }
    QPoint globalPosition() const { return screenPosition.toPoint(); }

    QPointF localPosition;
    QPointF windowPosition;
    QPointF screenPosition;

    Qt::MouseButton button;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;

    bool accepted = false;
};

class HoverEvent: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPoint position MEMBER position CONSTANT)

    Q_PROPERTY(bool accepted MEMBER accepted)

public:
    HoverEvent(const QHoverEvent* event);

    QPoint position;

    bool accepted = false;
};

} // namespace scene
} // namespace ui
} // namespace nx::vms::client::desktop

QML_DECLARE_TYPE(nx::vms::client::desktop::ui::scene::MouseEvent)
QML_DECLARE_TYPE(nx::vms::client::desktop::ui::scene::HoverEvent)

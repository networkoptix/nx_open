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

    Q_PROPERTY(QPoint position MEMBER position CONSTANT)
    Q_PROPERTY(QPoint globalPosition MEMBER globalPosition CONSTANT)
    Q_PROPERTY(int button MEMBER button CONSTANT)
    Q_PROPERTY(int buttons MEMBER buttons CONSTANT)
    Q_PROPERTY(int modifiers MEMBER modifiers CONSTANT)

    Q_PROPERTY(bool accepted MEMBER accepted)

public:
    MouseEvent(const QMouseEvent* event);

    QPoint position;
    QPoint globalPosition;
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

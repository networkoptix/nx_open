// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

Q_MOC_INCLUDE("QtQuick/QQuickItem")

class QQuickItem;

namespace nx::vms::client::desktop {
namespace ui {
namespace scene {

class MouseEvent;
class HoverEvent;
class KeyEvent;
class WheelEvent;

class Instrument: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool hoverEnabled READ hoverEnabled WRITE setHoverEnabled NOTIFY hoverEnabledChanged)
    Q_PROPERTY(Qt::MouseButtons acceptedButtons READ acceptedButtons WRITE setAcceptedButtons
        NOTIFY acceptedButtonsChanged)

    using base_type = QObject;

public:
    Instrument(QObject* parent = nullptr);
    virtual ~Instrument() override;

    QObject* item() const;
    void setItem(QObject* value);

    bool enabled() const;
    void setEnabled(bool value);

    bool hoverEnabled() const;
    void setHoverEnabled(bool value);

    Qt::MouseButtons acceptedButtons() const;
    void setAcceptedButtons(Qt::MouseButtons value);

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    static void registerQmlType();

signals:
    void itemChanged();
    void enabledChanged();
    void hoverEnabledChanged();
    void acceptedButtonsChanged();

    void mousePress(nx::vms::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseRelease(nx::vms::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseMove(nx::vms::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseDblClick(nx::vms::client::desktop::ui::scene::MouseEvent* mouse);
    void mouseWheel(nx::vms::client::desktop::ui::scene::WheelEvent* wheel);
    void hoverEnter(nx::vms::client::desktop::ui::scene::HoverEvent* hover);
    void hoverLeave(nx::vms::client::desktop::ui::scene::HoverEvent* hover);
    void hoverMove(nx::vms::client::desktop::ui::scene::HoverEvent* hover);
    void keyPress(nx::vms::client::desktop::ui::scene::KeyEvent* key);
    void keyRelease(nx::vms::client::desktop::ui::scene::KeyEvent* key);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace scene
} // namespace ui
} // namespace nx::vms::client::desktop

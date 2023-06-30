// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>

#include <ui/dialogs/common/dialog.h>

class QQuickWidget;

namespace Ui { class VirtualJoystickDialog; }

namespace nx::vms::client::desktop {

namespace joystick {

class Manager;
class OsHidDeviceVirtual;

} // namespace joystick

// TODO: Rework to use QmlDialogWrapper.
class VirtualJoystickDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    VirtualJoystickDialog(QWidget* parent = nullptr);
    virtual ~VirtualJoystickDialog() override;

    static void registerAction();

public slots:
    void onHorizontalStickMoved(qreal value);
    void onVerticalStickMoved(qreal value);
    void onButtonPressed(int buttonIndex);
    void onButtonReleased(int buttonIndex);

private:
    int buttonShiftFromName(const QString& name) const;

    void attachVirtualJoystick();
    void detachVirtualJoystick();
    void setVirtualJoystickState(const QBitArray& newState);

    QScopedPointer<Ui::VirtualJoystickDialog> ui;
    QQuickWidget* m_virtualJoystickWidget = nullptr;
    QBitArray m_joystickControlsState;

    static joystick::OsHidDeviceVirtual* m_virtualJoystickDevice;
};

} // namespace nx::vms::client::desktop

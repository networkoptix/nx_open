// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::joystick {

class Manager;

class JoystickSettingsDialog: public QmlDialogWrapper, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QmlDialogWrapper;

public:
    explicit JoystickSettingsDialog(Manager* manager, QWidget* parent = nullptr);
    virtual ~JoystickSettingsDialog() override;

signals:
    void resetToDefault(); //< For connecting labmda to QML item signal.

private:
    bool initModel(Manager* manager, bool initWithDefaults = false);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick

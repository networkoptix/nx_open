// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

namespace joystick { class Manager; }

class JoystickSettingsDialog: public QmlDialogWrapper, public WindowContextAware
{
    Q_OBJECT

    using base_type = QmlDialogWrapper;

public:
    explicit JoystickSettingsDialog(
        joystick::Manager* manager,
        WindowContext* windowContext,
        QWidget* parent = nullptr);
    virtual ~JoystickSettingsDialog() override;

    void initWithCurrentActiveJoystick();

signals:
    void resetToDefault(); //< For connecting labmda to QML item signal.

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

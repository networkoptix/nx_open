// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class JoystickSettingsDialog;

class JoystickSettingsActionHandler: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit JoystickSettingsActionHandler(WindowContext* context, QObject* parent = nullptr);
    virtual ~JoystickSettingsActionHandler() override;

private:
    std::unique_ptr<JoystickSettingsDialog> m_dialog;
};

} // namespace nx::vms::client::desktop

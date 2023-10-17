// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_settings_action_handler.h"

#include <QtGui/QAction>

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <ui/workbench/workbench_context.h>

#include "dialog/joystick_settings_dialog.h"
#include "settings/manager.h"

namespace nx::vms::client::desktop {

JoystickSettingsActionHandler::JoystickSettingsActionHandler(
    WindowContext* context,
    QObject* parent)
    :
    base_type(parent),
    WindowContextAware(context)
{
    connect(action(menu::JoystickSettingsAction), &QAction::triggered, this,
        [this]
        {
            if (!m_dialog)
            {
                m_dialog.reset(new JoystickSettingsDialog(
                    appContext()->joystickManager(),
                    windowContext(),
                    mainWindowWidget()));

                connect(m_dialog.get(), &JoystickSettingsDialog::done,
                    [this]
                    {
                        appContext()->joystickManager()->setDeviceActionsEnabled(true);
                    });
            }
            else
            {
                m_dialog->initWithCurrentActiveJoystick();
            }

            appContext()->joystickManager()->setDeviceActionsEnabled(false);

            m_dialog->open();
            m_dialog->raise();
        },
        Qt::QueuedConnection);
}

JoystickSettingsActionHandler::~JoystickSettingsActionHandler()
{
}

} // namespace nx::vms::client::desktop

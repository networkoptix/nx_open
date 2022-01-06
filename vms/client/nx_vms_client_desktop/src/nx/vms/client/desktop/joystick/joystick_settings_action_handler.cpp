// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_settings_action_handler.h"

#include <QtGui/QAction>

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <ui/workbench/workbench_context.h>

#include "dialog/joystick_settings_dialog.h"
#include "settings/manager.h"

namespace nx::vms::client::desktop::joystick {

JoystickSettingsActionHandler::JoystickSettingsActionHandler(
    QnWorkbenchContext* context,
    QObject* parent)
    :
    base_type(parent)
{
    connect(context->action(ui::action::JoystickSettingsAction), &QAction::triggered, this,
        [this, context]
        {
            if (!m_dialog)
            {
                m_dialog.reset(new JoystickSettingsDialog(
                    context->joystickManager(), context->mainWindowWidget()));

                connect(m_dialog.get(), &JoystickSettingsDialog::done,
                    [context]
                    {
                        context->joystickManager()->setDeviceActionsEnabled(true);
                    });
            }
            else
            {
                m_dialog->initWithCurrentActiveJoystick();
            }

            context->joystickManager()->setDeviceActionsEnabled(false);

            m_dialog->open();
            m_dialog->raise();
        },
        Qt::QueuedConnection);
}

JoystickSettingsActionHandler::~JoystickSettingsActionHandler()
{
}

} // namespace nx::vms::client::desktop::joystick

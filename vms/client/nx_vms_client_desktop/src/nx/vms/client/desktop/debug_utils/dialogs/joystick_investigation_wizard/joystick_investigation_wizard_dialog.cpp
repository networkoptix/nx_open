// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_investigation_wizard_dialog.h"

#include <QtCore/QFile>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QPushButton>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <ui/workbench/workbench_context.h>

#include "../../utils/debug_custom_actions.h"
#include "joystick_manager.h"

namespace nx::vms::client::desktop {

JoystickInvestigationWizardDialog::JoystickInvestigationWizardDialog(QWidget* parent):
    QmlDialogWrapper(
        appContext()->qmlEngine(),
        QUrl("Nx/JoystickInvestigationWizard/JoystickInvestigationWizard.qml"),
        {},
        parent),
    QnWorkbenchContextAware(parent),
    m_joystickManager(new joystick::JoystickManager())
{
    QmlProperty<joystick::JoystickManager*>(
        rootObjectHolder(),
        "joystickManager"
    ) = m_joystickManager.get();

    connect(rootObjectHolder()->object(), SIGNAL(resultReady(QString, QString)),
        this, SLOT(onResultReady(QString, QString)));
}

JoystickInvestigationWizardDialog::~JoystickInvestigationWizardDialog()
{
}

void JoystickInvestigationWizardDialog::onResultReady(
    const QString& filePath,
    const QString& jsonData)
{
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        NX_DEBUG(this, "Failed to open file for writing: %s", filePath);

        return;
    }

    file.write(jsonData.toUtf8());
    file.close();

    accept();
}

void JoystickInvestigationWizardDialog::registerAction()
{
    registerDebugAction(
        "Unknown joystick investigation dialog",
        [](QnWorkbenchContext* context)
        {
            auto dialog = new JoystickInvestigationWizardDialog(context->mainWindowWidget());

            connect(dialog, &QmlDialogWrapper::done, dialog, &QObject::deleteLater);

            dialog->open();
        });
}

} // namespace nx::vms::client::desktop

#include "workbench_resources_settings_handler.h"

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/dialogs/camera_settings_dialog.h>
#include <ui/dialogs/server_settings_dialog.h>

#include <ui/dialogs/non_modal_dialog_constructor.h>


QnWorkbenchResourcesSettingsHandler::QnWorkbenchResourcesSettingsHandler(QObject *parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent)
    , m_cameraSettingsDialog()
    , m_serverSettingsDialog()
{
    connect(action(QnActions::CameraSettingsAction), &QAction::triggered,    this,   &QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered);
    connect(action(QnActions::ServerSettingsAction), &QAction::triggered,    this,   &QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered);
}

QnWorkbenchResourcesSettingsHandler::~QnWorkbenchResourcesSettingsHandler() {
     delete m_cameraSettingsDialog.data();
     delete m_serverSettingsDialog.data();
}

void QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered() {
    QnVirtualCameraResourceList cameras = menu()->currentParameters(sender()).resources().filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return;

    QnNonModalDialogConstructor<QnCameraSettingsDialog> dialogConstructor(m_cameraSettingsDialog, mainWindow());
    dialogConstructor.setDontFocus(true);

    m_cameraSettingsDialog->setCameras(cameras);
}

void QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered() {
    QnMediaServerResourceList servers = menu()->currentParameters(sender()).resources().filtered<QnMediaServerResource>([](const QnMediaServerResourcePtr &server) {
        return !QnMediaServerResource::isFakeServer(server);
    });

    Q_ASSERT_X(servers.size() == 1, Q_FUNC_INFO, "Invalid action condition");
    if(servers.isEmpty())
        return;

    QnNonModalDialogConstructor<QnServerSettingsDialog> dialogConstructor(m_serverSettingsDialog, mainWindow());
    dialogConstructor.setDontFocus(true);

    m_serverSettingsDialog->setServer(servers.first());
}



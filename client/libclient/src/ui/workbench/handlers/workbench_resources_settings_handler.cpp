#include "workbench_resources_settings_handler.h"

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <ui/dialogs/resource_properties/camera_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_groups_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

#include <ui/workbench/workbench_access_controller.h>
#include <core/resource/fake_media_server.h>

QnWorkbenchResourcesSettingsHandler::QnWorkbenchResourcesSettingsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_cameraSettingsDialog(),
    m_serverSettingsDialog(),
    m_userSettingsDialog()
{
    connect(action(QnActions::CameraSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered);
    connect(action(QnActions::ServerSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered);
    connect(action(QnActions::NewUserAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered);
    connect(action(QnActions::UserSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered);
    connect(action(QnActions::UserRolesAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userGroupsAction_triggered);
}

QnWorkbenchResourcesSettingsHandler::~QnWorkbenchResourcesSettingsHandler()
{
     delete m_cameraSettingsDialog.data();
     delete m_serverSettingsDialog.data();
     delete m_userSettingsDialog.data();
}

void QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered()
{
    const auto parameters =  menu()->currentParameters(sender());
    auto cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return;

    QnNonModalDialogConstructor<QnCameraSettingsDialog> dialogConstructor(m_cameraSettingsDialog, mainWindow());
    dialogConstructor.setDontFocus(true);

    m_cameraSettingsDialog->setCameras(cameras);

    if (parameters.hasArgument(Qn::FocusTabRole))
    {
        auto tab = parameters.argument(Qn::FocusTabRole).toInt();
        m_cameraSettingsDialog->setCurrentTab(static_cast<Qn::CameraSettingsTab>(tab));
    }
}

void QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered()
{
    QnActionParameters params = menu()->currentParameters(sender());
    QnMediaServerResourceList servers = params.resources().filtered<QnMediaServerResource>(
        [](const QnMediaServerResourcePtr &server)
        {
            return !server.dynamicCast<QnFakeMediaServerResource>();
        });

    NX_ASSERT(servers.size() == 1, Q_FUNC_INFO, "Invalid action condition");
    if(servers.isEmpty())
        return;

    QnMediaServerResourcePtr server = servers.first();

    bool hasAccess = accessController()->hasPermissions(server, Qn::ReadPermission);
    if (!hasAccess)
        return;

    QnNonModalDialogConstructor<QnServerSettingsDialog> dialogConstructor(m_serverSettingsDialog, mainWindow());
    dialogConstructor.setDontFocus(true);

    m_serverSettingsDialog->setServer(server);
    if (params.hasArgument(Qn::FocusTabRole))
        m_serverSettingsDialog->setCurrentPage(params.argument<int>(Qn::FocusTabRole), true);
}

void QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered()
{
    QnUserResourcePtr user(new QnUserResource(QnUserType::Local));
    user->setRawPermissions(Qn::GlobalLiveViewerPermissionSet);
    user->addFlags(Qn::local);

    QnNonModalDialogConstructor<QnUserSettingsDialog> dialogConstructor(m_userSettingsDialog, mainWindow());
   // dialogConstructor.setDontFocus(true);
    m_userSettingsDialog->setUser(user);
    m_userSettingsDialog->setCurrentPage(QnUserSettingsDialog::SettingsPage);
}

void QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered()
{
    QnActionParameters params = menu()->currentParameters(sender());
    QnUserResourcePtr user = params.resource().dynamicCast<QnUserResource>();
    if (!user)
        return;

    bool hasAccess = accessController()->hasPermissions(user, Qn::ReadPermission);
    if (!hasAccess)
        return;

    QnNonModalDialogConstructor<QnUserSettingsDialog> dialogConstructor(m_userSettingsDialog,
        mainWindow());
    dialogConstructor.setDontFocus(true);

    m_userSettingsDialog->setUser(user);
    if (params.hasArgument(Qn::FocusTabRole))
        m_userSettingsDialog->setCurrentPage(params.argument<int>(Qn::FocusTabRole), true);

    //dialog->setFocusedElement(params.argument<QString>(Qn::FocusElementRole));
}

void QnWorkbenchResourcesSettingsHandler::at_userGroupsAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnUuid groupId = parameters.argument(Qn::UuidRole).value<QnUuid>();

    QScopedPointer<QnUserGroupsDialog> groupsDialog(new QnUserGroupsDialog(mainWindow()));
    if (!groupId.isNull())
        groupsDialog->selectGroup(groupId);

    groupsDialog->exec();
}


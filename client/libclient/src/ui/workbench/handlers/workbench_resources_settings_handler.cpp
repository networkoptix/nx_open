#include "workbench_resources_settings_handler.h"

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/fake_media_server.h>

#include <ui/dialogs/resource_properties/camera_settings_dialog.h>
#include <ui/dialogs/resource_properties/layout_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_roles_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout.h>

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
    connect(action(QnActions::LayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered);
    connect(action(QnActions::CurrentLayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_currentLayoutSettingsAction_triggered);
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

    if (!m_cameraSettingsDialog->setCameras(cameras))
        return;

    m_cameraSettingsDialog->updateFromResources();

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
    m_userSettingsDialog->forcedUpdate();
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
    m_userSettingsDialog->forcedUpdate();

    //dialog->setFocusedElement(params.argument<QString>(Qn::FocusElementRole));
}

void QnWorkbenchResourcesSettingsHandler::at_userGroupsAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnUuid groupId = parameters.argument(Qn::UuidRole).value<QnUuid>();

    QScopedPointer<QnUserRolesDialog> dialog(new QnUserRolesDialog(mainWindow()));
    if (!groupId.isNull())
        dialog->selectGroup(groupId);

    dialog->exec();
}

void QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered()
{
    QnActionParameters params = menu()->currentParameters(sender());
    openLayoutSettingsDialog(params.resource().dynamicCast<QnLayoutResource>());
}

void QnWorkbenchResourcesSettingsHandler::at_currentLayoutSettingsAction_triggered()
{
    openLayoutSettingsDialog(workbench()->currentLayout()->resource());
}

void QnWorkbenchResourcesSettingsHandler::openLayoutSettingsDialog(
    const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return;

    if (!accessController()->hasPermissions(layout, Qn::EditLayoutSettingsPermission))
        return;

    QScopedPointer<QnLayoutSettingsDialog> dialog(new QnLayoutSettingsDialog(mainWindow()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->readFromResource(layout);

    bool backgroundWasEmpty = layout->backgroundImageFilename().isEmpty();
    if (!dialog->exec() || !dialog->submitToResource(layout))
        return;

    /* Move layout items to grid center to best fit the background */
    if (backgroundWasEmpty && !layout->backgroundImageFilename().isEmpty())
    {
        if (auto wlayout = QnWorkbenchLayout::instance(layout))
            wlayout->centralizeItems();
    }
    menu()->triggerIfPossible(QnActions::SaveLayoutAction, layout);
}


#include "workbench_resources_settings_handler.h"

#include <QtWidgets/QAction>

#include <ini.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/fake_media_server.h>

#include <nx/client/desktop/resource_properties/camera/legacy_camera_settings_dialog.h>
#include <nx/client/desktop/resource_properties/camera/camera_settings_dialog.h>

#include <ui/dialogs/resource_properties/layout_settings_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_roles_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/utils/raii_guard.h>
#include <nx/client/desktop/utils/parameter_helper.h>

using namespace nx::client::desktop;
using namespace ui;

QnWorkbenchResourcesSettingsHandler::QnWorkbenchResourcesSettingsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(action::CameraSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered);
    connect(action(action::ServerSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered);
    connect(action(action::NewUserAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered);
    connect(action(action::UserSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered);
    connect(action(action::UserRolesAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_userRolesAction_triggered);
    connect(action(action::LayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered);
    connect(action(action::CurrentLayoutSettingsAction), &QAction::triggered, this,
        &QnWorkbenchResourcesSettingsHandler::at_currentLayoutSettingsAction_triggered);
}

QnWorkbenchResourcesSettingsHandler::~QnWorkbenchResourcesSettingsHandler()
{
}

void QnWorkbenchResourcesSettingsHandler::at_cameraSettingsAction_triggered()
{
    const auto parameters =  menu()->currentParameters(sender());
    auto cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return;

    const auto parent = nx::utils::extractParentWidget(parameters, mainWindowWidget());

    if (ini().redesignedCameraSettingsDialog)
    {
        QnNonModalDialogConstructor<CameraSettingsDialog> dialogConstructor(
            m_cameraSettingsDialog, parent);
        dialogConstructor.disableAutoFocus();

        if (!m_cameraSettingsDialog->setCameras(cameras))
            return;

        if (parameters.hasArgument(Qn::FocusTabRole))
        {
            const auto tab = parameters.argument(Qn::FocusTabRole).toInt();
            m_cameraSettingsDialog->setCurrentPage(tab);
        }
    }
    else
    {
        QnNonModalDialogConstructor<LegacyCameraSettingsDialog> dialogConstructor(
            m_legacyCameraSettingsDialog, parent);
        dialogConstructor.disableAutoFocus();

        if (!m_legacyCameraSettingsDialog->setCameras(cameras))
            return;

        if (parameters.hasArgument(Qn::FocusTabRole))
        {
            const auto tab = parameters.argument(Qn::FocusTabRole).toInt();
            m_legacyCameraSettingsDialog->setCurrentTab(static_cast<CameraSettingsTab>(tab));
        }
    }
}

void QnWorkbenchResourcesSettingsHandler::at_serverSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    QnMediaServerResourceList servers = params.resources().filtered<QnMediaServerResource>(
        [](const QnMediaServerResourcePtr &server)
        {
            return !server.dynamicCast<QnFakeMediaServerResource>();
        });

    NX_ASSERT(servers.size() == 1, Q_FUNC_INFO, "Invalid action condition");
    if(servers.isEmpty())
        return;

    QnMediaServerResourcePtr server = servers.first();

    bool hasAccess = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);
    NX_ASSERT(hasAccess, Q_FUNC_INFO, "Invalid action condition"); /*< It must be checked on action level. */
    if (!hasAccess)
        return;

    const auto parent = nx::utils::extractParentWidget(params, mainWindowWidget());
    QnNonModalDialogConstructor<QnServerSettingsDialog> dialogConstructor(
        m_serverSettingsDialog, parent);

    dialogConstructor.disableAutoFocus();

    m_serverSettingsDialog->setServer(server);
    if (params.hasArgument(Qn::FocusTabRole))
        m_serverSettingsDialog->setCurrentPage(params.argument<int>(Qn::FocusTabRole), true);
}

void QnWorkbenchResourcesSettingsHandler::at_newUserAction_triggered()
{
    QnUserResourcePtr user(new QnUserResource(QnUserType::Local));
    user->setRawPermissions(Qn::GlobalLiveViewerPermissionSet);

    // Shows New User dialog as modal because we can't pick anothr user from resources tree anyway.
    const auto params = menu()->currentParameters(sender());
    const auto parent = nx::utils::extractParentWidget(params, mainWindowWidget());

    if (!m_userSettingsDialog)
        m_userSettingsDialog = new QnUserSettingsDialog(parent);
    else
        DialogUtils::setDialogParent(m_userSettingsDialog, parent);

    m_userSettingsDialog->setUser(user);
    m_userSettingsDialog->setCurrentPage(QnUserSettingsDialog::SettingsPage);
    m_userSettingsDialog->forcedUpdate();

    m_userSettingsDialog->exec();
}

void QnWorkbenchResourcesSettingsHandler::at_userSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    QnUserResourcePtr user = params.resource().dynamicCast<QnUserResource>();
    if (!user)
        return;

    bool hasAccess = accessController()->hasPermissions(user, Qn::ReadPermission);
    NX_ASSERT(hasAccess, Q_FUNC_INFO, "Invalid action condition");
    if (!hasAccess)
        return;

    const auto parent = nx::utils::extractParentWidget(params, mainWindowWidget());
    QnNonModalDialogConstructor<QnUserSettingsDialog> dialogConstructor(
        m_userSettingsDialog, parent);

    // Navigating resource tree, we should not take focus. From System Administration - we must.
    bool force = params.argument(Qn::ForceRole, false);
    if (!force)
        dialogConstructor.disableAutoFocus();

    m_userSettingsDialog->setUser(user);
    if (params.hasArgument(Qn::FocusTabRole))
        m_userSettingsDialog->setCurrentPage(params.argument<int>(Qn::FocusTabRole), true);
    m_userSettingsDialog->forcedUpdate();
}

void QnWorkbenchResourcesSettingsHandler::at_userRolesAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto parent = nx::utils::extractParentWidget(parameters, mainWindowWidget());

    QnUuid userRoleId = parameters.argument(Qn::UuidRole).value<QnUuid>();

    QScopedPointer<QnUserRolesDialog> dialog(new QnUserRolesDialog(parent));
    if (!userRoleId.isNull())
        dialog->selectUserRole(userRoleId);

    dialog->exec();
}

void QnWorkbenchResourcesSettingsHandler::at_layoutSettingsAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
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

    QScopedPointer<QnLayoutSettingsDialog> dialog(new QnLayoutSettingsDialog(mainWindowWidget()));
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
    menu()->triggerIfPossible(action::SaveLayoutAction, layout);
}


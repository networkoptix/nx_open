// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incompatible_servers_action_handler.h"

#include <QtGui/QAction>

#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/vms/client/core/system_logon/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/desktop/common/dialogs/progress_dialog.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connection_delegate_helper.h>
#include <nx/vms/client/desktop/system_merge/connect_to_current_system_tool.h>
#include <nx/vms/client/desktop/ui/dialogs/merge_systems_dialog.h>
#include <nx/vms/license/remote_licenses.h>
#include <ui/dialogs/common/input_dialog.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/workbench/workbench_context.h>
#include <utils/merge_systems_common.h>

#include "merge_systems_tool.h"

namespace nx::vms::client::desktop {

IncompatibleServersActionHandler::IncompatibleServersActionHandler(
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(menu::ConnectToCurrentSystem), &QAction::triggered, this,
        &IncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered);
    connect(action(menu::MergeSystems), &QAction::triggered, this,
        &IncompatibleServersActionHandler::at_mergeSystemsAction_triggered);
}

IncompatibleServersActionHandler::~IncompatibleServersActionHandler()
{
}

void IncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered()
{
    if (m_connectTool)
    {
        QnMessageBox::information(mainWindowWidget(),
            tr("Systems will be merged shortly"),
            tr("Servers from the other System will appear in the resource tree."));
        return;
    }

    const auto serverId =
        menu()->currentParameters(sender()).argument(core::UuidRole).value<QnUuid>();

    connectToCurrentSystem(serverId);
}

void IncompatibleServersActionHandler::connectToCurrentSystem(const QnUuid& serverId)
{
    if (!NX_ASSERT(!serverId.isNull()) || !NX_ASSERT(!m_connectTool))
        return;

    const auto otherServersManager = context()->systemContext()->otherServersManager();
    const auto moduleInformation =
        otherServersManager->getModuleInformationWithAddresses(serverId);

    auto delegate = createConnectionUserInteractionDelegate(
        [this]() { return mainWindowWidget(); });
    m_connectTool = new ConnectToCurrentSystemTool(this, std::move(delegate));

    auto progressDialog = new ProgressDialog(mainWindowWidget());
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    progressDialog->setWindowTitle(tr("Connecting to the current System..."));
    progressDialog->show();

    connect(m_connectTool, &ConnectToCurrentSystemTool::finished,
        this, &IncompatibleServersActionHandler::at_connectTool_finished);
    connect(
        m_connectTool,
        &ConnectToCurrentSystemTool::canceled,
        this,
        &IncompatibleServersActionHandler::at_connectTool_canceled);
    connect(
        m_connectTool,
        &ConnectToCurrentSystemTool::canceled,
        progressDialog,
        &ProgressDialog::close);
    connect(m_connectTool, &ConnectToCurrentSystemTool::finished,
        progressDialog, &ProgressDialog::close);
    connect(m_connectTool, &ConnectToCurrentSystemTool::progressChanged,
        progressDialog, &ProgressDialog::setValue);
    connect(m_connectTool, &ConnectToCurrentSystemTool::stateChanged,
        progressDialog, &ProgressDialog::setText);
    connect(progressDialog, &ProgressDialog::canceled,
        m_connectTool, &ConnectToCurrentSystemTool::cancel);

    std::shared_ptr<QnStatisticsScenarioGuard> mergeScenario =
        statisticsModule()->certificates()->beginScenario(
            QnCertificateStatisticsModule::Scenario::mergeFromTree);

    // The captured scenario guard will be deleted after tool is destroyed.
    connect(m_connectTool, &ConnectToCurrentSystemTool::destroyed, this, [mergeScenario] {});

    m_connectTool->start(serverId);
}

void IncompatibleServersActionHandler::at_mergeSystemsAction_triggered()
{
    if (m_mergeDialog)
    {
        m_mergeDialog->raise();
        return;
    }

    std::shared_ptr<QnStatisticsScenarioGuard> mergeScenario =
        statisticsModule()->certificates()->beginScenario(
            QnCertificateStatisticsModule::Scenario::mergeFromDialog);

    auto delegate = createConnectionUserInteractionDelegate(
        [this]() { return mainWindowWidget(); });
    m_mergeDialog = new MergeSystemsDialog(mainWindowWidget(), std::move(delegate));
    connect(m_mergeDialog.data(), &QDialog::finished, this,
        [this, mergeScenario]()
        {
            m_mergeDialog->deleteLater();
            m_mergeDialog.clear();
        });

    m_mergeDialog->open();
}

void IncompatibleServersActionHandler::at_connectTool_canceled()
{
    m_connectTool->disconnect(this);
    m_connectTool->deleteLater();
}

void IncompatibleServersActionHandler::at_connectTool_finished(
    MergeSystemsStatus mergeStatus,
    const QString& errorText,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    m_connectTool->deleteLater();
    m_connectTool->disconnect(this);

    if (mergeStatus == MergeSystemsStatus::ok)
    {
        QnMessageBox::success(mainWindowWidget(),
            tr("Server will be connected to System shortly"),
            tr("It will appear in the resource tree when the database synchronization"
               " is finished."));
    }
    else
    {
        QString message = errorText.isEmpty()
            ? MergeSystemsTool::getErrorMessage(mergeStatus, moduleInformation)
            : errorText;

        auto serverName = m_connectTool->getServerName();
        QnMessageBox::critical(
            mainWindowWidget(),
            tr("Failed to merge %1 to our system.").arg(serverName),
            message);
    }
}

} // namespace nx::vms::client::desktop

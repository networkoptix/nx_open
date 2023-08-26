// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connect_to_current_system_tool.h"

#include <chrono>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/common/system_settings.h>
#include <ui/dialogs/common/input_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <utils/common/delayed.h>

#include "merge_systems_tool.h"
#include "merge_systems_validator.h"

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

static const int kStartProgress = 25;
static const int kHalfProgress = 50;
static const int kAlmostDoneProgress = 75;
static const milliseconds kWaitTimeout = 2min;

QnFakeMediaServerResourcePtr incompatibleServerForOriginalId(
    const QnUuid& id, const QnResourcePool* resourcePool)
{
    for (const auto& server: resourcePool->getIncompatibleServers())
    {
        if (const auto& fakeServer = server.dynamicCast<QnFakeMediaServerResource>())
        {
            if (fakeServer->getOriginalGuid() == id)
                return fakeServer;
        }
    }

    return {};
}

} // namespace

namespace nx::vms::client::desktop {

ConnectToCurrentSystemTool::ConnectToCurrentSystemTool(
    QObject* parent,
    std::unique_ptr<Delegate> delegate)
    :
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_delegate(std::move(delegate))

{
    NX_ASSERT(m_delegate);
}

ConnectToCurrentSystemTool::~ConnectToCurrentSystemTool() {}

bool ConnectToCurrentSystemTool::tryClose(bool /*force*/)
{
    cancel();
    return true;
}

void ConnectToCurrentSystemTool::forcedUpdate()
{
}

void ConnectToCurrentSystemTool::start(const QnUuid& targetId)
{
    NX_DEBUG(this, "Try to connect server with id: %1", targetId);

    if (!NX_ASSERT(!targetId.isNull()))
    {
        finish(MergeSystemsStatus::notFound, {}, {});
        return;
    }

    auto server = resourcePool()->getIncompatibleServerById(targetId, true);
    if (!NX_ASSERT(server))
    {
        finish(MergeSystemsStatus::notFound, {}, {});
        return;
    }

    auto serverInformation = server->getModuleInformation();

    // Following checks can be run before requesting password, so do it now.
    MergeSystemsStatus mergeStatus =
        MergeSystemsValidator::checkCloudCompatibility(currentServer(), serverInformation);

    if (mergeStatus == MergeSystemsStatus::ok)
        mergeStatus = MergeSystemsValidator::checkVersionCompatibility(serverInformation);

    if (mergeStatus != MergeSystemsStatus::ok)
    {
        finish(mergeStatus, {}, serverInformation);
        return;
    }

    const QString password = helpers::isNewSystem(serverInformation)
        ? helpers::kFactorySystemPassword
        : requestPassword();

    if (password.isEmpty())
    {
        cancel();
        return;
    }

    m_targetId = targetId;
    m_originalTargetId = server->getOriginalGuid();
    m_serverName = server->getName();

    NX_DEBUG(this,
        "Start connecting server %1 (id=%2, url=%3",
        m_serverName,
        m_originalTargetId,
        server->getApiUrl());

    emit progressChanged(kStartProgress);
    mergeServer(password);
}

QString ConnectToCurrentSystemTool::getServerName() const
{
    return m_serverName;
}

void ConnectToCurrentSystemTool::cancel()
{
    if (m_mergeTool)
        m_mergeTool->deleteLater();

    emit canceled();
}

void ConnectToCurrentSystemTool::finish(
    MergeSystemsStatus mergeStatus,
    const QString& errorText,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    using nx::utils::log::Level;

    NX_UTILS_LOG(
        mergeStatus == MergeSystemsStatus::ok ? Level::debug : Level::error,
        this,
        "Finished, mergeStatus: %1, errorText: %2",
        mergeStatus,
        errorText);

    if (m_mergeTool)
        m_mergeTool->deleteLater();

    emit finished(mergeStatus, errorText, moduleInformation);
}

void ConnectToCurrentSystemTool::mergeServer(const QString& adminPassword)
{
    NX_ASSERT(!m_mergeTool);

    const auto server = incompatibleServerForOriginalId(m_originalTargetId, resourcePool());
    if (!server)
    {
        const auto compatibleServer =
            resourcePool()->getResourceById<QnMediaServerResource>(m_originalTargetId);
        if (compatibleServer && compatibleServer->getStatus() == nx::vms::api::ResourceStatus::online)
        {
            finish(MergeSystemsStatus::ok, {}, {});
            return;
        }

        finish(MergeSystemsStatus::unknownError, {}, {});
        return;
    }

    auto proxy = currentServer();

    if (!proxy)
    {
        finish(MergeSystemsStatus::unknownError, {}, {});
        return;
    }

    NX_DEBUG(this, "Start server configuration.");

    emit progressChanged(kHalfProgress);
    emit stateChanged(tr("Configuring Server"));

    m_mergeTool = new MergeSystemsTool(
        this,
        qnClientCoreModule->networkModule()->certificateVerifier(),
        m_delegate.get(),
        appContext()->localSettings()->locale().toStdString());

    connect(
        m_mergeTool,
        &MergeSystemsTool::mergeFinished,
        this,
        &ConnectToCurrentSystemTool::mergeFinished);

    connect(
        m_mergeTool,
        &MergeSystemsTool::systemFound,
        this,
        &ConnectToCurrentSystemTool::systemFound);

    const auto auth = nx::network::http::PasswordCredentials(
        helpers::kFactorySystemUser.toStdString(),
        adminPassword.toStdString());

    m_mergeContextId = m_mergeTool->pingSystem(
        proxy,
        server->getApiUrl(),
        auth);
}

bool ConnectToCurrentSystemTool::showLicenseWarning(
    MergeSystemsStatus mergeStatus,
    const nx::vms::api::ModuleInformation& moduleInformation) const
{
    // Warn that some of the licenses will be deactivated.
    const auto message = MergeSystemsTool::getErrorMessage(mergeStatus, moduleInformation);

    const auto result = QnMessageBox::warning(
        mainWindowWidget(),
        tr("Total amount of licenses will decrease"),
        message,
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QDialogButtonBox::Ok);

    return (result != QDialogButtonBox::Cancel);
}

void ConnectToCurrentSystemTool::systemFound(
    MergeSystemsStatus mergeStatus,
    const QString& errorText,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    if (mergeStatus == MergeSystemsStatus::certificateRejected)
    {
        cancel();
        return;
    }

    if (!allowsToMerge(mergeStatus))
    {
        NX_ERROR(this, "Server ping failed, status: %1, error: %2", mergeStatus, errorText);

        finish(mergeStatus, errorText, moduleInformation);
        return;
    }

    if ((mergeStatus != MergeSystemsStatus::ok)
        && !showLicenseWarning(mergeStatus, moduleInformation))
    {
        cancel();
        return;
    }

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        mainWindowWidget(),
        tr("Merge Server to the System", "Dialog title"),
        tr("Enter your account password to merge Server to the System"),
        tr("Merge", "Merge Server to the current System (dialog button text)"),
        FreshSessionTokenHelper::ActionType::merge);

    auto ownerSessionToken = sessionTokenHelper->refreshToken();
    if (!ownerSessionToken)
    {
        cancel();
        return;
    }

    m_mergeTool->mergeSystem(
        m_mergeContextId,
        ownerSessionToken->value,
        /*ownSettings*/ true,
        /*oneServer*/ true,
        /*ignoreIncompatible*/ true);
}

void ConnectToCurrentSystemTool::mergeFinished(
    MergeSystemsStatus mergeStatus,
    const QString& errorText,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    if (mergeStatus != MergeSystemsStatus::ok)
    {
        NX_ERROR(
            this,
            "Server configuration failed, status: %1, error: %2.",
            mergeStatus,
            errorText);

        finish(mergeStatus, errorText, moduleInformation);
        return;
    }

    NX_DEBUG(this, "Server configuration finished.");
    waitServer();
}

void ConnectToCurrentSystemTool::waitServer()
{
    NX_ASSERT(m_mergeTool);

    auto finishMerge =
        [this](MergeSystemsStatus mergeStatus)
        {
            resourcePool()->disconnect(this);
            finish(mergeStatus, {}, {});
        };

    if (resourcePool()->getIncompatibleServerById(m_targetId, true))
    {
        finishMerge(MergeSystemsStatus::ok);
        return;
    }

    auto handleResourceChanged =
        [this, finishMerge](const QnResourcePtr& resource)
        {
            if ((resource->getId() == m_originalTargetId)
                && (resource->getStatus() != nx::vms::api::ResourceStatus::offline))
            {
                finishMerge(MergeSystemsStatus::ok);
            }
        };

    NX_DEBUG(this, "Start waiting server.");

    // Receiver object is m_mergeTool.
    // This helps us to break the connection when m_mergeTool is deleted in the cancel() method.
    connect(resourcePool(), &QnResourcePool::resourceAdded, m_mergeTool, handleResourceChanged);
    connect(resourcePool(), &QnResourcePool::statusChanged, m_mergeTool, handleResourceChanged);

    executeDelayedParented(
        [finishMerge]()
        {
            finishMerge(MergeSystemsStatus::unknownError);
        },
        kWaitTimeout.count(),
        m_mergeTool);

    emit progressChanged(kAlmostDoneProgress);
}

QString ConnectToCurrentSystemTool::requestPassword() const
{
    QnInputDialog dialog(mainWindowWidget());
    dialog.setWindowTitle(tr("Enter password..."));
    dialog.setCaption(tr("Administrator password"));
    dialog.useForPassword();
    setHelpTopic(&dialog, HelpTopic::Id::Systems_ConnectToCurrentSystem);

    return dialog.exec() == QDialog::Accepted ? dialog.value() : QString();
}

} // namespace nx::vms::client::desktop

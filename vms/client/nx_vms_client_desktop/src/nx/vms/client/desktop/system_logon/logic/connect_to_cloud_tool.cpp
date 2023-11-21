// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connect_to_cloud_tool.h"

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <cloud/cloud_result_info.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/ui/oauth_login_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/dialogs/cloud/cloud_result_messages.h>
#include <ui/dialogs/common/session_aware_dialog.h>

using namespace nx::cloud::db::api;
using namespace std::chrono;

namespace {

constexpr auto kSuppressCloudTimeout = 60s;

} // namespace

namespace nx::vms::client::desktop {

ConnectToCloudTool::ConnectToCloudTool(QWidget* parent, nx::vms::common::SystemSettings* settings)
    :
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_parent(parent),
    m_cloudConnectionFactory(std::make_unique<core::CloudConnectionFactory>()),
    m_systemSettings(settings)
{
    NX_ASSERT(m_parent);
}

ConnectToCloudTool::~ConnectToCloudTool()
{
}

bool ConnectToCloudTool::tryClose(bool /*force*/)
{
    cancel();
    return true;
}

bool ConnectToCloudTool::start()
{
    if (!NX_ASSERT(system()->resourcePool()->getAdministrator()))
    {
        showFailure(tr("Local System owner is absent or disabled."));
        return false;
    }

    if (!system()->resourcePool()->masterCloudSyncServer())
    {
        showFailure(tr("None of your Servers has connection to %1.",
            "%1 is the short cloud name (like Cloud)")
                        .arg(nx::branding::shortCloudName()));
        return false;
    }

    requestCloudAuthData();
    return true;
}

void ConnectToCloudTool::cancel()
{
    if (m_localLoginDialog)
        m_localLoginDialog->deleteLater();
    if (m_oauthLoginDialog)
        m_oauthLoginDialog->deleteLater();

    emit finished();
}

void ConnectToCloudTool::showSuccess(const QString& /*cloudLogin*/)
{
    showOnceSettings()->cloudPromo = true;

    QnSessionAwareMessageBox::success(getTopWidget(),
        tr("System connected to %1", "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::branding::cloudName()));

    m_localLoginDialog->accept();
    m_localLoginDialog->deleteLater();

    emit finished();
}

void ConnectToCloudTool::showFailure(const QString& message)
{
    QnSessionAwareMessageBox::critical(getTopWidget(),
        tr("Failed to connect System to %1", "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::branding::cloudName()),
        message);

    cancel();
}

QWidget* ConnectToCloudTool::getTopWidget() const
{
    if (m_localLoginDialog)
        return m_localLoginDialog;

    if (m_oauthLoginDialog)
        return m_oauthLoginDialog;

    return m_parent;
}

void ConnectToCloudTool::requestCloudAuthData()
{
    const QString title = tr("Connect System to %1", "%1 is the cloud name (like Nx Cloud)")
                              .arg(nx::branding::cloudName());

    m_oauthLoginDialog = new OauthLoginDialog(m_parent, core::OauthClientType::connect);
    m_oauthLoginDialog->setWindowTitle(title);
    connect(m_oauthLoginDialog,
        &OauthLoginDialog::authDataReady,
        this,
        &ConnectToCloudTool::onCloudAuthDataReady);
    connect(
        m_oauthLoginDialog,
        &OauthLoginDialog::bindToCloudDataReady,
        this,
        &ConnectToCloudTool::onBindToCloudDataReady);
    connect(
        m_oauthLoginDialog,
        &OauthLoginDialog::rejected,
        this,
        &ConnectToCloudTool::cancel);
    m_oauthLoginDialog->setWindowModality(Qt::ApplicationModal);

    // Prefill cloud username, if client is logged in.
    m_oauthLoginDialog->setCredentials(
        nx::network::http::Credentials(qnCloudStatusWatcher->credentials().username, {}));
    m_oauthLoginDialog->setSystemName(systemSettings()->systemName());

    m_oauthLoginDialog->show();
}

void ConnectToCloudTool::onBindToCloudDataReady()
{
    const auto bindResult = m_oauthLoginDialog->cloudBindData();

    nx::cloud::db::api::SystemData systemData;
    systemData.id = bindResult.systemId.toStdString();
    systemData.authKey = bindResult.authKey.toStdString();
    if (!bindResult.owner.isEmpty())
        systemData.setOwnerAccountEmail(bindResult.owner.toStdString());
    if (!bindResult.organizationId.isEmpty())
        systemData.setOrganizationId(bindResult.organizationId.toStdString());

    onBindFinished(std::move(systemData));
}

void ConnectToCloudTool::onCloudAuthDataReady()
{
    m_cloudAuthData = m_oauthLoginDialog->authData();
    NX_ASSERT(!m_cloudAuthData.empty());

    SystemRegistrationData request;
    request.name = system()->globalSettings()->systemName().toStdString();
    request.customization = nx::branding::customization().toStdString();

    auto handler = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](ResultCode result, nx::cloud::db::api::SystemData data)
        {
            onBindFinished(result, std::move(data));
        });

    m_cloudConnection = m_cloudConnectionFactory->createConnection();
    m_cloudConnection->setCredentials(m_cloudAuthData.credentials);
    m_cloudConnection->systemManager()->bindSystem(request, std::move(handler));
}

bool ConnectToCloudTool::processBindResult(ResultCode result)
{
    if (result == ResultCode::ok)
        return true;

    switch (result)
    {
        case ResultCode::badUsername:
        case ResultCode::notAuthorized:
            showFailure(QnCloudResultMessages::invalidPassword());
            break;

        case ResultCode::accountNotActivated:
            showFailure(QnCloudResultMessages::accountNotActivated());
            break;

        case ResultCode::accountBlocked:
            showFailure(QnCloudResultMessages::userLockedOut());
            break;

        default:
            showFailure(QnCloudResultInfo(result));
            break;
    }

    const QString currentCloudUser = qnCloudStatusWatcher->cloudLogin();
    const QString user = QString::fromStdString(m_cloudAuthData.credentials.username);

    if (qnCloudStatusWatcher->isCloudEnabled() && currentCloudUser == user)
    {
        if (result == ResultCode::accountBlocked)
            qnCloudStatusWatcher->resetAuthData();
        else
            qnCloudStatusWatcher->suppressCloudInteraction(kSuppressCloudTimeout);
    }

    return false;
}

void ConnectToCloudTool::onBindFinished(ResultCode result, nx::cloud::db::api::SystemData systemData)
{
    NX_DEBUG(this, "Cloud DB bind finished, result: %1", result);

    if (!processBindResult(result))
        return;
    onBindFinished(std::move(systemData));
}

void ConnectToCloudTool::onBindFinished(nx::cloud::db::api::SystemData systemData)
{
    NX_DEBUG(this, "Cloud DB bind finished");
    qnCloudStatusWatcher->resumeCloudInteraction();

    m_cloudAuthData.credentials.username = systemData.ownerAccountEmail();
    m_systemData = std::move(systemData);

    m_oauthLoginDialog->accept();
    m_oauthLoginDialog->deleteLater();

    requestLocalSessionToken();
}

void ConnectToCloudTool::requestLocalSessionToken()
{
    const QString title = tr("Connect System to %1?", "%1 is the cloud name (like Nx Cloud)")
                              .arg(nx::branding::cloudName());
    const QString mainText = tr("Enter your account password to connect System to %1",
        "%1 is the cloud name (like Nx Cloud)")
                                 .arg(nx::branding::cloudName());

    m_localLoginDialog = new SessionRefreshDialog(m_parent,
        title,
        mainText,
        /*infoText*/ QString(),
        tr("Connect", "Connect current System to cloud"),
        /*warningStyledAction*/ false);

    connect(m_localLoginDialog,
        &SessionRefreshDialog::sessionTokenReady,
        this,
        &ConnectToCloudTool::onLocalSessionTokenReady);
    connect(
        m_localLoginDialog, &SessionRefreshDialog::rejected, this, &ConnectToCloudTool::cancel);
    m_localLoginDialog->setWindowModality(Qt::ApplicationModal);
    m_localLoginDialog->show();
}

void ConnectToCloudTool::onLocalSessionTokenReady()
{
    const auto localToken = m_localLoginDialog->refreshResult().token;
    if (!NX_ASSERT(!localToken.empty()))
        return;

    const auto api = system()->connectedServerApi();
    if (!api)
    {
        cancel();
        return;
    }

    auto handleReply = nx::utils::guarded(this,
        [this](bool, rest::Handle, rest::ErrorOrEmpty reply)
        {
            if (std::holds_alternative<rest::Empty>(reply))
            {
                NX_DEBUG(this, "Server bind succeded");
                showSuccess(QString::fromStdString(m_cloudAuthData.credentials.username));
                return;
            }

            const auto& error = std::get<nx::network::rest::Result>(reply);
            NX_DEBUG(
                this, "Server bind failed, error: %1, string: %2", error.error, error.errorString);

            QString errorMessage = tr("Internal error. Please try again later.");
            if (ini().developerMode)
                errorMessage += '\n' + error.errorString;
            showFailure(errorMessage);
        });

    using namespace nx::cloud::db::api;
    api->bindSystemToCloud(
        QString::fromStdString(m_systemData.id),
        QString::fromStdString(m_systemData.authKey),
        QString::fromStdString(m_cloudAuthData.credentials.username),
        QString::fromStdString(getAttrValueOr(m_systemData.attributes, "organizationId", "")),
        localToken.value,
        std::move(handleReply),
        m_parent->thread());
}

} // namespace nx::vms::client::desktop

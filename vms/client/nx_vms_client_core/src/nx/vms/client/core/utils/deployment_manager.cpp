// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deployment_manager.h"

#include <QtGui/QDesktopServices>

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/watchers/feature_access_watcher.h>

namespace nx::vms::client::core {

namespace {

void openDeployServerWebPage(const std::string& authCode, const QString& cloudSystemId)
{
    const auto url = network::url::Builder()
        .setScheme(network::http::kSecureUrlSchemeName)
        .setHost(network::SocketGlobals::cloud().cloudHost())
        .setPath("deploy")
        .addQueryItem("siteId", cloudSystemId)
        .addQueryItem("skipSiteSelection", "true")
        .addQueryItem("code", authCode).toUrl();

    QDesktopServices::openUrl(url.toQUrl());
}

} // namespace

struct DeploymentManager::Private
{
    std::unique_ptr<cloud::db::api::Connection> connection;
};

DeploymentManager::DeploymentManager(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context),
    d{new Private{}}
{
    auto factory = std::make_unique<core::CloudConnectionFactory>();
    d->connection = factory->createConnection();
}

DeploymentManager::~DeploymentManager()
{
}

void DeploymentManager::deployNewServer()
{
    if (!NX_ASSERT(systemContext()->featureAccess()->canUseDeployByQrFeature()))
        return;

    using namespace nx::cloud::db::api;

    IssueTokenRequest request;
    request.client_id = CloudConnectionFactory::clientId();
    request.grant_type = GrantType::refresh_token;
    request.response_type = ResponseType::code;
    request.refresh_token =
        appContext()->cloudStatusWatcher()->remoteConnectionCredentials().authToken.value;

    auto handler = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](ResultCode result, IssueCodeResponse response)
        {
            if (result == ResultCode::ok)
                openDeployServerWebPage(response.code, systemContext()->cloudSystemId());
            else
                NX_DEBUG(this, "Can't issue authorization code, error %1", result);
        });

    d->connection->oauthManager()->issueAuthorizationCode(request, std::move(handler));
}

} // namespace nx::vms::client::core

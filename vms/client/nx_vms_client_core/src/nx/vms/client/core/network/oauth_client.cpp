// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_client.h"

#include <QtQml/QtQml>

#include <client_core/client_core_module.h>
#include <nx/cloud/db/api/connection.h>
#include <nx/cloud/db/api/oauth_data.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/to_string.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>

namespace nx::vms::client::core {

using namespace nx::cloud::db::api;

struct OauthClient::Private: public QObject
{
    OauthClient* const q;

    const OauthClientType clientType = OauthClientType::undefined;
    const OauthViewType viewType = OauthViewType::desktop;
    const QString cloudSystem;
    const QString clientId;
    QString locale;
    CloudAuthData authData;
    std::unique_ptr<Connection> m_connection;
    std::unique_ptr<CloudConnectionFactory> m_cloudConnectionFactory;

    Private(
        OauthClient* q,
        OauthClientType clientType,
        OauthViewType viewType,
        const QString& cloudSystem,
        const QString& clientId);

    void issueAccessToken();
    std::string email() const;
};

OauthClient::Private::Private(
    OauthClient* q,
    OauthClientType clientType,
    OauthViewType viewType,
    const QString& cloudSystem,
    const QString& clientId)
    :
    q(q),
    clientType(clientType),
    viewType(viewType),
    cloudSystem(cloudSystem),
    clientId(clientId),
    m_cloudConnectionFactory(std::make_unique<core::CloudConnectionFactory>())
{
}

void OauthClient::Private::issueAccessToken()
{
    m_connection = m_cloudConnectionFactory->createConnection();
    if (!NX_ASSERT(m_connection))
    {
        emit q->cancelled();
        return;
    }

    auto handler = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](ResultCode result, IssueTokenResponse response)
        {
            NX_DEBUG(
                this,
                "Issue token result: %1, error: %2",
                result,
                response.error.value_or(std::string()));

            if (result != ResultCode::ok)
            {
                q->cancelled();
                return;
            }

            authData.authorizationCode.clear();
            authData.credentials = nx::network::http::BearerAuthToken(response.access_token);
            authData.refreshToken = std::move(response.refresh_token);
            authData.expiresAt = response.expires_at;
            emit q->authDataReady();
        });

    IssueTokenRequest request;
    request.grant_type = GrantType::authorizationCode;
    request.code = authData.authorizationCode;
    if (!cloudSystem.isEmpty())
        request.scope = nx::format("cloudSystemId=%1", cloudSystem).toStdString();

    m_connection->oauthManager()->issueToken(request, std::move(handler));
}

std::string OauthClient::Private::email() const
{
    auto email = authData.credentials.username;

    if (email.empty())
    {
        RemoteConnectionAware connectionHelper;
        auto remoteConnection = connectionHelper.connection();

        if (remoteConnection && remoteConnection->connectionInfo().isCloud())
            email = remoteConnection->credentials().username;
    }

    return email;
}

//-------------------------------------------------------------------------------------------------
// OauthClient

void OauthClient::registerQmlType()
{
    qmlRegisterType<OauthClient>("nx.vms.client.core", 1, 0, "OauthClient");
}

OauthClient::OauthClient(QObject* parent):
    OauthClient(OauthClientType::undefined, OauthViewType::desktop,
        /*cloudSystem*/ {}, /*clienId*/ {}, parent)
{
}

OauthClient::OauthClient(
    OauthClientType clientType,
    OauthViewType viewType,
    const QString& cloudSystem,
    const QString& clientId,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, clientType, viewType, cloudSystem, clientId))
{
}

OauthClient::~OauthClient()
{
}

QUrl OauthClient::url() const
{
    if (d->clientType == OauthClientType::undefined)
        return QUrl();

    const auto cloudHost = nx::network::SocketGlobals::cloud().cloudHost();

    auto builder = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setHost(cloudHost)
        .setPath("authorize")
        .addQueryItem("client_type", nx::reflect::toString(d->clientType))
        .addQueryItem("view_type", nx::reflect::toString(d->viewType))
        .addQueryItem("redirect_url", "redirect-oauth");

    if (!d->clientId.isEmpty())
        builder.addQueryItem("client_id", d->clientId);

    if (d->authData.empty()) //< Request auth code.
        builder.addQueryItem("response_type", "code");
    else if (!d->authData.credentials.authToken.empty()) //< Request 2FA validation.
        builder.addQueryItem("access_token", d->authData.credentials.authToken.value);

    if (const auto email = d->email(); !email.empty())
        builder.addQueryItem("email", email);

    if (!d->locale.isEmpty())
        builder.addQueryItem("lang", d->locale);

    return builder.toUrl().toQUrl();
}

void OauthClient::setCredentials(const nx::network::http::Credentials& credentials)
{
    CloudAuthData authData;
    authData.credentials = credentials;
    d->authData = std::move(authData);
}

void OauthClient::setLocale(const QString& locale)
{
    d->locale = locale;
}

const CloudAuthData& OauthClient::authData() const
{
    return d->authData;
}

void OauthClient::setCode(const QString& code)
{
    NX_DEBUG(this, "Auth code set, length: %1", code.size());

    if (code.isEmpty())
    {
        emit cancelled();
        return;
    }

    d->authData.authorizationCode = code.toStdString();
    d->issueAccessToken();
}

void OauthClient::twoFaVerified(const QString& code)
{
    NX_DEBUG(this, "2FA code verified, length: %1", code.size());

    if (!NX_ASSERT(!code.isEmpty() && (code == d->authData.authorizationCode
        || code == d->authData.credentials.authToken.value)))
    {
        emit cancelled();
        return;
    }

    emit authDataReady();
}

} // namespace nx::vms::client::core

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_uri.h"

#include <QtCore/QUrlQuery>

#include <nx/branding.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/login.h>

using namespace nx::vms::utils;

namespace {

const QString kAuthKey = "auth";
const QString kAuthCodeKey = "code";
const QString kReferralSourceKey = "from";
const QString kReferralContextKey = "context";
const QString kResourceIdsKey = "resources";
const QString kTimestampKey = "timestamp";

const int kDefaultPort = 80;

const int kUuidLength = 36;

const QMap<SystemUri::Scope, QString> scopeToString
{
    {SystemUri::Scope::generic, "generic"},
    {SystemUri::Scope::direct, "direct"}
};

const QMap<SystemUri::Protocol, QString> protocolToString
{
    {SystemUri::Protocol::Http, "http"},
    {SystemUri::Protocol::Https, "https"},
    {SystemUri::Protocol::Native, nx::branding::nativeUriProtocol()}
};

const QMap<SystemUri::ClientCommand, QString> clientCommandToString
{
    {SystemUri::ClientCommand::None,            "_invalid_"},
    {SystemUri::ClientCommand::Client,          "client"},
    {SystemUri::ClientCommand::LoginToCloud,    "cloud"},
    {SystemUri::ClientCommand::OpenOnPortal,    "systems"}
};

const QMap<SystemUri::SystemAction, QString> systemActionToString
{
    {SystemUri::SystemAction::View, "view"}
};

const QMap<SystemUri::ReferralSource, QString> referralSourceToString
{
    {SystemUri::ReferralSource::None,           ""},
    {SystemUri::ReferralSource::DesktopClient,  "client"},
    {SystemUri::ReferralSource::MobileClient,   "mobile"},
    {SystemUri::ReferralSource::CloudPortal,    "portal"},
    {SystemUri::ReferralSource::WebAdmin,       "webadmin"}
};

const QMap<SystemUri::ReferralContext, QString> referralContextToString
{
    {SystemUri::ReferralContext::None,          ""},
    {SystemUri::ReferralContext::SetupWizard,   "setup"},
    {SystemUri::ReferralContext::SettingsDialog,"settings"},
    {SystemUri::ReferralContext::AboutDialog,   "about"},
    {SystemUri::ReferralContext::WelcomePage,   "startpage"},
    {SystemUri::ReferralContext::CloudMenu,     "menu"}
};

QString resourceIdsToString(const QList<QnUuid>& ids)
{
    QStringList result;
    for (const auto& id: ids)
        result.push_back(id.toSimpleString());

    return result.join(':');
}

QList<QnUuid> resourceIdsFromString(const QString& string)
{
    QList<QnUuid> result;
    const auto values = string.split(':');
    for (const auto& value: values)
    {
        const auto uuid = QnUuid::fromStringSafe(value);
        if (!uuid.isNull())
            result.push_back(uuid);
    }
    return result;
}

void splitString(const QString& source, QChar separator, QString& left, QString& right)
{
    int idx = source.indexOf(separator);
    if (idx < 0)
    {
        left = source;
        right = QString();
    }
    else
    {
        left = source.left(idx);
        right = source.mid(idx + 1);
    }
}

bool isCloudHostname(const QString& hostname)
{
    if (hostname.length() != kUuidLength)
        return false;

    QnUuid uuid = QnUuid::fromStringSafe(hostname);
    return !uuid.isNull();
}

void parseParameters(const nx::utils::Url& url, SystemUri* uri)
{
    if (!NX_ASSERT(uri))
        return;

    QHash<QString, QString> parameters;

    if (!url.hasQuery())
        return;

    QUrlQuery query(url.query());
    for (const auto [key, value]: query.queryItems(QUrl::FullyDecoded))
        parameters.insert(key.toLower(), value);

    const QString authKey = parameters.take(kAuthKey);
    if (!authKey.isEmpty())
    {
        if (authKey.toStdString().starts_with(nx::vms::api::LoginSession::kTokenPrefix))
        {
            uri->credentials = nx::network::http::BearerAuthToken(authKey.toStdString());
        }
        else
        {
            static const QChar kDelimiter(':');
            const QString auth = QString::fromUtf8(QByteArray::fromBase64(authKey.toUtf8()));
            if (auth.contains(kDelimiter))
            {
                QString user;
                QString password;
                splitString(auth, kDelimiter, user, password);
                uri->credentials = nx::network::http::PasswordCredentials(
                    user.toStdString(),
                    password.toStdString());
            }
        }
    }

    uri->authCode = parameters.take(kAuthCodeKey);

    const auto resourceIdParameters = parameters.take(kResourceIdsKey);
    uri->resourceIds = resourceIdsFromString(resourceIdParameters);

    const auto timestampParameter = parameters.take(kTimestampKey);
    uri->timestamp = timestampParameter.isEmpty() ? -1 : timestampParameter.toLongLong();

    QString referralFrom = parameters.take(kReferralSourceKey).toLower();
    uri->referral.source = referralSourceToString.key(referralFrom);

    QString referralContext = parameters.take(kReferralContextKey).toLower();
    uri->referral.context = referralContextToString.key(referralContext);
}

void parse(const nx::utils::Url& url, SystemUri* uri)
{
    if (!NX_ASSERT(uri))
        return;

    // For now we support parsing of generic links only
    uri->scope = SystemUri::Scope::generic;
    uri->protocol = protocolToString.key(url.scheme().toLower(), SystemUri::Protocol::Native);
    uri->cloudHost = url.host();
    int port = url.port(kDefaultPort);
    if (port != kDefaultPort)
        uri->cloudHost += ':' + QString::number(port);

    QStringList path = url.path().split('/', Qt::SkipEmptyParts);
    if (path.isEmpty())
    {
        uri->clientCommand = SystemUri::ClientCommand::Client;
    }
    else
    {
        QString clientCommandStr = path.takeFirst().toLower();
        uri->clientCommand = clientCommandToString.key(clientCommandStr,
            SystemUri::ClientCommand::None);
    }

    parseParameters(url, uri);

    switch (uri->clientCommand)
    {
        // No more parameters are required for these commands
        case SystemUri::ClientCommand::None:
        case SystemUri::ClientCommand::LoginToCloud:
            return;

        // Client opens such links as usual client links
        case SystemUri::ClientCommand::OpenOnPortal:
            NX_ASSERT(uri->protocol == SystemUri::Protocol::Native);
            uri->clientCommand = SystemUri::ClientCommand::Client;
            break;

        default:
            break;
    }

    if (!path.isEmpty())
    {
        QString systemAddressOrAction = path.takeFirst();
        if (systemActionToString.values().contains(systemAddressOrAction))
        {
            uri->systemAddress = uri->cloudHost;
        }
        else
        {
            uri->systemAddress = systemAddressOrAction;
        }
    }
}

bool isValidSystemAddress(const SystemUri& uri)
{
    if (uri.systemAddress.isEmpty())
        return false;

    const bool cloudAllowed = uri.scope == SystemUri::Scope::generic
        || uri.protocol == SystemUri::Protocol::Native;

    const auto isValidLocalAddress =
        [&uri]()
        {
            const auto url = QUrl::fromUserInput(uri.systemAddress);
            return url.isValid() && url.port() > 0;
        };

    return (cloudAllowed && isCloudHostname(uri.systemAddress))
        || isValidLocalAddress();
}

bool hasValidAuth(const SystemUri& uri)
{
    const auto& credentials = uri.credentials;
    if (credentials.authToken.isPassword())
        return !credentials.username.empty() && !credentials.authToken.empty();

    if (credentials.authToken.isBearerToken())
        return !credentials.authToken.empty();

    return false;
}

QString encodePasswordCredentials(const nx::network::http::Credentials& credentials)
{
    return QString(
        QString::fromStdString(credentials.username)
        + ':'
        + QString::fromStdString(credentials.authToken.value))
        .toUtf8().toBase64();
}

} // namespace

SystemUri::SystemUri(const nx::utils::Url& url) :
    SystemUri()
{
    parse(url, this);
}

SystemUri::SystemUri(const QString& uri)
{
    nx::utils::Url url(uri);
    if (url.isValid())
        parse(url, this);
}

bool SystemUri::hasCloudSystemAddress() const
{
    return isValid() && isCloudHostname(systemAddress);
}

bool SystemUri::isValid() const
{
    if (scope == Scope::generic)
    {
        if (cloudHost.isEmpty())
            return false;
    }
    else
    {
        // Universal links cannot be handled for the direct uri, only native protocol is supported.
        if (protocol != SystemUri::Protocol::Native)
            return false;

        // Direct links are starting with system address.
        if (systemAddress.isEmpty())
            return false;
    }

    const bool hasSystemAddress = !systemAddress.isEmpty();
    const bool hasValidAuth = ::hasValidAuth(*this);
    const bool isValidSystemAddress = ::isValidSystemAddress(*this);

    switch (clientCommand)
    {
        case SystemUri::ClientCommand::Client:
        {
            if (!hasSystemAddress || !isValidSystemAddress)
                return !hasValidAuth;

            // Login to a Cloud System is allowed only by using auth code.
            return isCloudHostname(systemAddress) ? !authCode.isEmpty() : hasValidAuth;
        }

        // Login to Cloud with credentials is not supported anymore. Auth code should be used.
        case SystemUri::ClientCommand::LoginToCloud:
        {
            return !hasValidAuth && !authCode.isEmpty();
        }

        case SystemUri::ClientCommand::OpenOnPortal:
        {
            return isValidSystemAddress;
        }

        default:
            break;
    }

    return false;
}

QString SystemUri::authKey() const
{
    if (credentials.authToken.isBearerToken())
        return QString::fromStdString(credentials.authToken.value);

    if (credentials.authToken.isPassword() && !credentials.username.empty())
        return encodePasswordCredentials(credentials);

    return QString();
}

QString SystemUri::toString() const
{
    return toUrl().toString();
}

nx::utils::Url SystemUri::toUrl() const
{
    nx::utils::Url result;
    result.setScheme(protocolToString[protocol]);

    if (scope == Scope::direct)
    {
        QUrl address = QUrl::fromUserInput(systemAddress);
        result.setHost(address.host());
        result.setPort(address.port());
    }
    else // Generic url.
    {
        result.setHost(cloudHost);
    }

    QStringList pathSegments;
    if (scope == Scope::generic)
        pathSegments.push_back(clientCommandToString[clientCommand]);

    if (clientCommand == SystemUri::ClientCommand::Client && isValidSystemAddress(*this))
    {
        if (scope == SystemUri::Scope::generic)
            pathSegments.push_back(systemAddress);

        pathSegments.push_back(systemActionToString[systemAction]);
    }

    const QString path = pathSegments.empty() ? QString() : "/" + pathSegments.join('/');
    result.setPath(path);

    QUrlQuery query;

    const QString authKey = this->authKey();
    if (!authKey.isEmpty())
        query.addQueryItem(kAuthKey, authKey);

    if (!authCode.isEmpty())
        query.addQueryItem(kAuthCodeKey, authCode);

    if (systemAction == SystemUri::SystemAction::View)
    {
        if (!resourceIds.isEmpty())
            query.addQueryItem(kResourceIdsKey, resourceIdsToString(resourceIds));

        if (timestamp != -1)
            query.addQueryItem(kTimestampKey, QString::number(timestamp));
    }

    if (referral.source != SystemUri::ReferralSource::None)
        query.addQueryItem(kReferralSourceKey, SystemUri::toString(referral.source));

    if (referral.context != SystemUri::ReferralContext::None)
        query.addQueryItem(kReferralContextKey, SystemUri::toString(referral.context));

    result.setQuery(query);
    return result;
}



QString SystemUri::toString(SystemUri::Scope value)
{
    return scopeToString[value];
}

QString SystemUri::toString(SystemUri::Protocol value)
{
    return protocolToString[value];
}

QString SystemUri::toString(SystemUri::ClientCommand value)
{
    return clientCommandToString[value];
}

QString SystemUri::toString(SystemUri::SystemAction value)
{
    return systemActionToString[value];
}

QString SystemUri::toString(SystemUri::ReferralSource value)
{
    return referralSourceToString[value];
}

QString SystemUri::toString(SystemUri::ReferralContext value)
{
    return referralContextToString[value];
}



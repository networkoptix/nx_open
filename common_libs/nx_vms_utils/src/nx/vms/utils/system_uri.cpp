#include "system_uri.h"

#include <QtCore/QUrlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/utils/app_info.h>

using namespace nx::vms::utils;

namespace {

const QString kAuthKey = "auth";
const QString kReferralSourceKey = "from";
const QString kReferralContextKey = "context";

const int kDefaultPort = 80;
const int kMaxPort = 65535;

const int kUuidLength = 36;

const QMap<SystemUri::Scope, QString> scopeToString
{
    {SystemUri::Scope::Generic, "generic"},
    {SystemUri::Scope::Direct, "direct"}
};

const QMap<SystemUri::Protocol, QString> protocolToString
{
    {SystemUri::Protocol::Http, "http"},
    {SystemUri::Protocol::Https, "https"},
    {SystemUri::Protocol::Native, AppInfo::nativeUriProtocol()}
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
    {SystemUri::ReferralContext::WelcomePage,   "startpage"},
    {SystemUri::ReferralContext::CloudMenu,     "menu"}
};

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

} // namespace

class nx::vms::utils::SystemUriPrivate
{
public:
    SystemUri::Scope scope = SystemUri::Scope::Generic;
    SystemUri::Protocol protocol = SystemUri::Protocol::Http;
    QString domain;
    SystemUri::ClientCommand clientCommand = SystemUri::ClientCommand::None;
    QString systemId;
    SystemUri::SystemAction systemAction = SystemUri::SystemAction::View;
    SystemUri::Auth authenticator;
    SystemUri::Referral referral;
    SystemUri::Parameters parameters;

    SystemUriPrivate()
    {}

    SystemUriPrivate(const SystemUriPrivate &other) :
        scope(other.scope),
        protocol(other.protocol),
        domain(other.domain),
        clientCommand(other.clientCommand),
        systemId(other.systemId),
        systemAction(other.systemAction),
        authenticator(other.authenticator),
        referral(other.referral),
        parameters(other.parameters)
    {}

    void parse(const nx::utils::Url& url)
    {
        // For now we support parsing of generic links only
        scope = SystemUri::Scope::Generic;
        protocol = protocolToString.key(url.scheme().toLower(), SystemUri::Protocol::Native);
        domain = url.host();
        int port = url.port(kDefaultPort);
        if (port != kDefaultPort)
            domain += L':' + QString::number(port);

        QStringList path = url.path().split('/', QString::SkipEmptyParts);
        if (path.isEmpty())
        {
            clientCommand = SystemUri::ClientCommand::Client;
        }
        else
        {
            QString clientCommandStr = path.takeFirst().toLower();
            clientCommand = clientCommandToString.key(clientCommandStr, SystemUri::ClientCommand::None);
        }

        parseParameters(url);

        switch (clientCommand)
        {
            // No more parameters are required for these commands
            case SystemUri::ClientCommand::None:
            case SystemUri::ClientCommand::LoginToCloud:
                return;

            // Client opens such links as usual client links
            case SystemUri::ClientCommand::OpenOnPortal:
                NX_ASSERT(protocol == SystemUri::Protocol::Native);
                clientCommand = SystemUri::ClientCommand::Client;
                break;

            default:
                break;
        }

        if (!path.isEmpty())
        {
            QString systemIdOrAction = path.takeFirst();
            if (systemActionToString.values().contains(systemIdOrAction))
            {
                systemId = domain;
            }
            else
            {
                systemId = systemIdOrAction;
            }
        }
    }

    nx::utils::Url toUrl() const
    {
        nx::utils::Url result;
        result.setScheme(protocolToString[protocol]);

        LocalHostName hostname = parseLocalHostname(domain);
        if (isLocalHostname(hostname))
        {
            result.setHost(hostname.first);
            result.setPort(hostname.second);
        }
        else
        {
            result.setHost(domain);
        }

        QString path = '/' + clientCommandToString[clientCommand];
        if (clientCommand == SystemUri::ClientCommand::Client && isValidSystemId())
        {
            if (scope == SystemUri::Scope::Generic)
                path += '/' + systemId;

            path += '/' + systemActionToString[systemAction];
        }

        result.setPath(path);

        QUrlQuery query;

        if (!authenticator.user.isEmpty() && !authenticator.password.isEmpty())
        {
            query.addQueryItem(kAuthKey, authenticator.encode());
        }

        if (referral.source != SystemUri::ReferralSource::None)
            query.addQueryItem(kReferralSourceKey, SystemUri::toString(referral.source));

        if (referral.context != SystemUri::ReferralContext::None)
            query.addQueryItem(kReferralContextKey, SystemUri::toString(referral.context));

        for (auto iter = parameters.cbegin(); iter != parameters.cend(); ++iter)
            query.addQueryItem(iter.key(), iter.value());

        result.setQuery(query);
        return result;
    }

    nx::utils::Url connectionUrl() const
    {
        if (systemId.isEmpty())
            return nx::utils::Url();

        const auto hostName = parseLocalHostname(systemId);

        nx::utils::Url result;
        result.setScheme(
            protocol == SystemUri::Protocol::Native ? lit("http") : protocolToString[protocol]);
        result.setHost(hostName.first);
        result.setPort(hostName.second);
        result.setUserName(authenticator.user);
        result.setPassword(authenticator.password);

        return result;
    }

    QString toString() const
    {
        return toUrl().toDisplayString();
    }

    bool isNull() const
    {
        return protocol == SystemUri::Protocol::Http
            && clientCommand == SystemUri::ClientCommand::None
            && domain.isEmpty()
            && systemId.isEmpty()
            && authenticator.user.isEmpty()
            && authenticator.password.isEmpty()
            && referral.source == SystemUri::ReferralSource::None
            && referral.context == SystemUri::ReferralContext::None
            && parameters.isEmpty();
    }

    bool isValid() const
    {
        switch (scope)
        {
            case SystemUri::Scope::Generic:
                return isValidGenericUri();
            case SystemUri::Scope::Direct:
                return isValidDirectUri();
            default:
                break;
        }
        return false;
    }

    bool operator==(const SystemUriPrivate& other) const
    {
        return protocol == other.protocol
            && clientCommand == other.clientCommand
            && domain == other.domain
            && systemId == other.systemId
            && authenticator.user == other.authenticator.user
            && authenticator.password == other.authenticator.password
            && parameters == other.parameters;
    }

private:
    bool isValidGenericUri() const
    {
        bool hasDomain = !domain.isEmpty();
        bool hasAuth = !authenticator.user.isEmpty() && !authenticator.password.isEmpty();
        bool hasSystemId = !systemId.isEmpty();

        switch (clientCommand)
        {
            case SystemUri::ClientCommand::Client:
                return hasDomain && (hasSystemId ? hasAuth && isValidSystemId() : !hasAuth );
            case SystemUri::ClientCommand::LoginToCloud:
                return hasDomain && hasAuth;
            case SystemUri::ClientCommand::OpenOnPortal:
                return hasDomain
                    && hasAuth
                    && isValidSystemId();
            default:
                break;
        }

        return false;
    }

    bool isValidDirectUri() const
    {
        /* Direct links are starting with systemId. */
        if (systemId.isEmpty())
            return false;

        bool hasAuth = !authenticator.user.isEmpty() && !authenticator.password.isEmpty();

        /* Checking http/https protocol - only direct link to connect to system is allowed. */
        switch (clientCommand)
        {
            case SystemUri::ClientCommand::Client:
                return protocol == SystemUri::Protocol::Native && (!hasAuth || isValidSystemId());
            case SystemUri::ClientCommand::LoginToCloud:
                return protocol == SystemUri::Protocol::Native
                    && hasAuth;
            case SystemUri::ClientCommand::OpenOnPortal:
                return hasAuth
                    && isValidSystemId();
            default:
                break;
        }
        return false;
    }

    typedef QPair<QString, int> LocalHostName;
    static LocalHostName parseLocalHostname(const QString& hostname)
    {
        QString host, portStr;
        splitString(hostname, ':', host, portStr);
        bool ok;
        int port = portStr.toInt(&ok);
        if (!ok)
            port = -1;
        return LocalHostName(host, port);
    }

    static bool isLocalHostname(const LocalHostName& hostname)
    {
        return !hostname.first.isEmpty()
            && hostname.second > 0
            && hostname.second <= kMaxPort;
    }

    /** Check that system id is in form localhost:7001. Port is mandatory. */
    static bool isLocalHostname(const QString& hostname)
    {
        return isLocalHostname(parseLocalHostname(hostname));
    }

    static bool isCloudHostname(const QString& hostname)
    {
        if (hostname.length() != kUuidLength)
            return false;

        QnUuid uuid = QnUuid::fromStringSafe(hostname);
        return !uuid.isNull();
    }

    bool isValidSystemId() const
    {
        if (systemId.isEmpty())
            return false;

        bool cloudAllowed = scope == SystemUri::Scope::Generic
            || protocol == SystemUri::Protocol::Native;

        return (cloudAllowed && isCloudHostname(systemId))
            || isLocalHostname(systemId);
    }

    void parseParameters(const nx::utils::Url& url)
    {
        if (!url.hasQuery())
            return;

        QStringList query = url.query(QUrl::FullyDecoded).split('&', QString::SkipEmptyParts);
        for (const QString& parameter : query)
        {
            QString key, value;
            splitString(parameter, '=', key, value);
            parameters.insert(key.toLower(), value);
        }

        QString encodedAuth = parameters.take(kAuthKey);
        if (!encodedAuth.isEmpty())
        {
            QString auth = QString::fromUtf8(QByteArray::fromBase64(encodedAuth.toUtf8()));
            splitString(auth, ':', authenticator.user, authenticator.password);
        }

        QString referralFrom = parameters.take(kReferralSourceKey).toLower();
        referral.source = referralSourceToString.key(referralFrom);

        QString referralContext = parameters.take(kReferralContextKey).toLower();
        referral.context = referralContextToString.key(referralContext);
    }

};

SystemUri::SystemUri() :
    d_ptr(new SystemUriPrivate())
{

}

SystemUri::SystemUri(const nx::utils::Url& url) :
    SystemUri()
{
    Q_D(SystemUri);
    d->parse(url);
}

SystemUri::SystemUri(const QString& uri) :
    SystemUri()
{
    Q_D(SystemUri);

    nx::utils::Url url(uri);
    if (url.isValid())
        d->parse(url);
}

nx::vms::utils::SystemUri::SystemUri(const SystemUri& other) :
    d_ptr(other.d_ptr
          ? new SystemUriPrivate(*other.d_ptr.data())
          : new SystemUriPrivate()
    )
{
    NX_ASSERT(other.d_ptr);
}

SystemUri& SystemUri::operator=(const SystemUri& other)
{
    if (&other == this)
        return *this;

    NX_ASSERT(other.d_ptr);
    if (!other.d_ptr)
        return *this;

    (*d_ptr.data()) = (*other.d_ptr.data());
    return *this;
}

SystemUri::~SystemUri()
{

}

SystemUri::Scope SystemUri::scope() const
{
    Q_D(const SystemUri);
    return d->scope;
}

void SystemUri::setScope(Scope value)
{
    Q_D(SystemUri);
    d->scope = value;
}

SystemUri::Protocol SystemUri::protocol() const
{
    Q_D(const SystemUri);
    return d->protocol;
}

void SystemUri::setProtocol(Protocol value)
{
    Q_D(SystemUri);
    d->protocol = value;
}

QString SystemUri::domain() const
{
    Q_D(const SystemUri);
    return d->domain;
}

void SystemUri::setDomain(const QString& value)
{
    Q_D(SystemUri);
    d->domain = value;
}

SystemUri::ClientCommand SystemUri::clientCommand() const
{
    Q_D(const SystemUri);
    return d->clientCommand;
}

void SystemUri::setClientCommand(ClientCommand value)
{
    Q_D(SystemUri);
    d->clientCommand = value;
}

QString SystemUri::systemId() const
{
    Q_D(const SystemUri);
    return d->systemId;
}

void SystemUri::setSystemId(const QString& value)
{
    Q_D(SystemUri);
    d->systemId = value;
}

SystemUri::SystemAction SystemUri::systemAction() const
{
    Q_D(const SystemUri);
    return d->systemAction;
}

void SystemUri::setSystemAction(SystemAction value)
{
    Q_D(SystemUri);
    d->systemAction = value;
}

SystemUri::Auth SystemUri::authenticator() const
{
    Q_D(const SystemUri);
    return d->authenticator;
}

void SystemUri::setAuthenticator(const Auth& value)
{
    Q_D(SystemUri);
    d->authenticator = value;
}

void SystemUri::setAuthenticator(const QString& user, const QString& password)
{
    Q_D(SystemUri);
    d->authenticator.user = user;
    d->authenticator.password = password;
}

SystemUri::Referral SystemUri::referral() const
{
    Q_D(const SystemUri);
    return d->referral;
}

void SystemUri::setReferral(const Referral& value)
{
    Q_D(SystemUri);
    d->referral = value;
}

void SystemUri::setReferral(ReferralSource source, ReferralContext context)
{
    Q_D(SystemUri);
    d->referral.source = source;
    d->referral.context = context;
}


SystemUri::Parameters SystemUri::rawParameters() const
{
    Q_D(const SystemUri);
    return d->parameters;
}

void SystemUri::setRawParameters(const Parameters& value)
{
    Q_D(SystemUri);
    d->parameters = value;
}

void SystemUri::addParameter(const QString& key, const QString& value)
{
    Q_D(SystemUri);
    d->parameters.insert(key, value);
}

bool SystemUri::isNull() const
{
    Q_D(const SystemUri);
    return d->isNull();
}

bool SystemUri::isValid() const
{
    Q_D(const SystemUri);
    return d->isValid();
}

QString SystemUri::toString() const
{
    Q_D(const SystemUri);
    return d->toString();
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

nx::utils::Url SystemUri::toUrl() const
{
    Q_D(const SystemUri);
    return d->toUrl();
}

nx::utils::Url SystemUri::connectionUrl() const
{
    Q_D(const SystemUri);
    return d->connectionUrl();
}

bool SystemUri::operator==(const SystemUri& other) const
{
    return *d_ptr.data() == *other.d_ptr.data();
}

QString nx::vms::utils::SystemUri::Auth::encode() const
{
    return QString(user + ':' + password).toUtf8().toBase64();
}

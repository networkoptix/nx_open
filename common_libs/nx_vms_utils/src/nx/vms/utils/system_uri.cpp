#include "system_uri.h"

#include <nx/utils/log/log.h>

using namespace nx::vms::utils;

namespace
{
    const QString kAuthKey = "auth";

    const SystemUri::Scope kDefaultScope = SystemUri::Scope::Generic;
    const SystemUri::Protocol kDefaultProtocol = SystemUri::Protocol::Http;
    const SystemUri::SystemAction kDefaultSystemAction = SystemUri::SystemAction::View;

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
        {SystemUri::Protocol::Native, "nx-vms"} //TODO: #GDM make customizable
    };

    const QMap<SystemUri::ClientCommand, QString> clientCommandToString
    {
        {SystemUri::ClientCommand::None,            "_invalid_"},
        {SystemUri::ClientCommand::Client,          "client"},
        {SystemUri::ClientCommand::LoginToCloud,    "cloud"},
        {SystemUri::ClientCommand::ConnectToSystem, "system"}
    };

    const QMap<SystemUri::SystemAction, QString> systemActionToString
    {
        {SystemUri::SystemAction::View, "view"}
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
}

class nx::vms::utils::SystemUriPrivate
{
public:
    SystemUri::Scope scope;
    SystemUri::Protocol protocol;
    QString domain;
    SystemUri::ClientCommand clientCommand;
    QString systemId;
    SystemUri::SystemAction systemAction;
    SystemUri::Auth authenticator;
    SystemUri::Parameters parameters;

    SystemUriPrivate() :
        scope(kDefaultScope),
        protocol(kDefaultProtocol),
        domain(),
        clientCommand(SystemUri::ClientCommand::None),
        systemId(),
        systemAction(kDefaultSystemAction),
        authenticator(),
        parameters()
    {}

    void parse(const QString& uri)
    {
        QUrl url(uri, QUrl::TolerantMode);
        if (!url.isValid())
            return;

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
            case SystemUri::ClientCommand::Client:
            case SystemUri::ClientCommand::LoginToCloud:
                /* No more parameters are required for these commands */
                return;
            default:
                break;
        }

        if (path.isEmpty())
        {
            scope = SystemUri::Scope::Direct;
            systemId = domain;
        }
        else
        {
            QString systemIdOrAction = path.takeFirst();
            if (systemActionToString.values().contains(systemIdOrAction))
            {
                scope = SystemUri::Scope::Direct;
                systemId = domain;
            }
            else
            {
                systemId = systemIdOrAction;
            }
        }
    }

    QString toString() const
    {
        return QString("%1://%2/%3")
            .arg(protocolToString[protocol])
            .arg(domain)
            .arg(clientCommandToString[clientCommand]);
    }


    bool isNull() const
    {
        return protocol == kDefaultProtocol
            && clientCommand == SystemUri::ClientCommand::None
            && domain.isEmpty()
            && systemId.isEmpty()
            && authenticator.user.isEmpty()
            && authenticator.password.isEmpty()
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

        switch (clientCommand)
        {
            case SystemUri::ClientCommand::Client:
                return hasDomain;
            case SystemUri::ClientCommand::LoginToCloud:
                return hasDomain && hasAuth;
            case SystemUri::ClientCommand::ConnectToSystem:
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
                return protocol == SystemUri::Protocol::Native;
            case SystemUri::ClientCommand::LoginToCloud:
                return protocol == SystemUri::Protocol::Native
                    && hasAuth;
            case SystemUri::ClientCommand::ConnectToSystem:
                return hasAuth
                    && isValidSystemId();
            default:
                break;
        }
        return false;
    }

    /** Check that system id is in form localhost:7001. Port is mandatory. */
    bool isLocalSystemId() const
    {
        QString host, portStr;
        splitString(systemId, ':', host, portStr);
        bool portOk = false;
        int port = portStr.toInt(&portOk);

        return !host.isEmpty()
            && portOk
            && port > 0
            && port <= kMaxPort;
    }

    bool isCloudSystemId() const
    {
        if (systemId.length() != kUuidLength)
            return false;

        QnUuid uuid = QnUuid::fromStringSafe(systemId);
        return !uuid.isNull();
    }

    bool isValidSystemId() const
    {
        if (systemId.isEmpty())
            return false;

        bool cloudAllowed = scope == SystemUri::Scope::Generic
            || protocol == SystemUri::Protocol::Native;

        return (cloudAllowed && isCloudSystemId())
            || isLocalSystemId();
    }

    void parseParameters(const QUrl& url)
    {
        if (!url.hasQuery())
            return;

        QStringList query = url.query(QUrl::FullyDecoded).split('&', QString::SkipEmptyParts);
        for (const QString& parameter : query)
        {
            QString key, value;
            splitString(parameter, '=', key, value);
            parameters.insert(key, value);
        }

        QString encodedAuth = parameters.take(kAuthKey);
        if (!encodedAuth.isEmpty())
        {
            QString auth = QString::fromUtf8(QByteArray::fromBase64(encodedAuth.toUtf8()));
            splitString(auth, ':', authenticator.user, authenticator.password);
        }
    }

};

SystemUri::SystemUri() :
    d_ptr(new SystemUriPrivate())
{

}

SystemUri::SystemUri(const QString& uri):
    SystemUri()
{
    Q_D(SystemUri);
    d->parse(uri);
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

QUrl SystemUri::toUrl() const
{
    return QUrl(toString()); //TODO: #GDM vise-versa?
}

bool SystemUri::operator==(const SystemUri& other) const
{
    return *d_ptr.data() == *other.d_ptr.data();
}

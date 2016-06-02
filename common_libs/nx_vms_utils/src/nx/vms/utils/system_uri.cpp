#include "system_uri.h"

#include <nx/utils/log/log.h>

using namespace nx::vms::utils;

namespace
{
    const QString kAuthKey = lit("auth=");

    const SystemUri::Scope kDefaultScope = SystemUri::Scope::Generic;
    const SystemUri::Protocol kDefaultProtocol = SystemUri::Protocol::Http;
    const SystemUri::SystemAction kDefaultSystemAction = SystemUri::SystemAction::View;

    const int kDefaultPort = 80;

    const QMap<SystemUri::Scope, QString> scopeToString
    {
        {SystemUri::Scope::Generic, "generic"},
        {SystemUri::Scope::Direct, "direct"}
    };

    const QMap<SystemUri::Protocol, QString> protocolToString
    {
        {SystemUri::Protocol::Http, "http"},
        {SystemUri::Protocol::Https, "http"},
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
}

SystemUriResolver::SystemUriResolver():
    m_valid(false)
{

}

SystemUriResolver::SystemUriResolver(const QUrl &uri):
    m_valid(false),
    m_uri(uri)
{
    parseUri();
}

bool SystemUriResolver::isValid() const
{
    return m_valid;
}

SystemUriResolver::Result SystemUriResolver::result() const
{
    return m_result;
}

bool SystemUriResolver::parseUri()
{
    NX_ASSERT(!m_uri.isEmpty(), Q_FUNC_INFO, "Empty uri, invalid class usage");
    if (m_uri.isEmpty())
        return false;

    if (!m_uri.isValid())
        return false;

    QString encodedAuth = m_uri.query(QUrl::FullyDecoded);
    if (!encodedAuth.startsWith(kAuthKey))
        return false;

    m_result.actions.clear();

    encodedAuth = encodedAuth.mid(kAuthKey.length());

    QString system = m_uri.host();
    int port = m_uri.port();

    QString auth = QString::fromUtf8(QByteArray::fromBase64(encodedAuth.toUtf8()));

    QStringList credentials = auth.split(L':');
    m_result.login      = credentials.size() > 0 ? credentials[0] : QString();
    m_result.password   = credentials.size() > 1 ? credentials[1] : QString();

    /* For now all valid actions must have valid credentials. */
    m_valid = credentials.size() == 2;

    /* Check connect to cloud system */
    if (!QnUuid::fromStringSafe(system).isNull())
    {
        m_result.serverUrl.setScheme(lit("http"));
        m_result.serverUrl.setHost(system);
        m_result.serverUrl.setUserName(m_result.login);
        m_result.serverUrl.setPassword(m_result.password);
        m_result.actions << Action::LoginToCloud;
        m_result.actions << Action::ConnectToServer;
    }
    /* Check connect to local system */
    else if (port > 0)
    {
        m_result.serverUrl.setScheme(lit("http"));
        m_result.serverUrl.setHost(system);
        m_result.serverUrl.setPort(port);
        m_result.serverUrl.setUserName(m_result.login);
        m_result.serverUrl.setPassword(m_result.password);
        m_result.actions << Action::ConnectToServer;
    }
    else
    {
        m_result.actions << Action::LoginToCloud;
    }

    return m_valid;
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

        if (url.scheme() == "http")
            protocol = SystemUri::Protocol::Http;
        else if (url.scheme() == "https")
            protocol = SystemUri::Protocol::Https;
        else
            protocol = SystemUri::Protocol::Native;

        domain = url.host();
        int port = url.port(kDefaultPort);
        if (port != kDefaultPort)
            domain += L':' + QString::number(port);

        QStringList path = url.path().split('/', QString::SkipEmptyParts);
        if (path.isEmpty())
        {
            clientCommand = SystemUri::ClientCommand::Client;
            return;
        }

        QString clientCommandStr = path.takeFirst().toLower();
        clientCommand = clientCommandToString.key(clientCommandStr, SystemUri::ClientCommand::None);
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
                    && !systemId.isEmpty();
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

        /* Checking http/https protocol - only direct link to connect to system is allowed. */
        if (protocol != SystemUri::Protocol::Native)
            return clientCommand == SystemUri::ClientCommand::ConnectToSystem;

        bool hasAuth = !authenticator.user.isEmpty() && !authenticator.password.isEmpty();
        switch (clientCommand)
        {
            case SystemUri::ClientCommand::Client:
                return true;
            case SystemUri::ClientCommand::LoginToCloud:
            case SystemUri::ClientCommand::ConnectToSystem:
                return hasAuth;
            default:
                break;
        }
        return false;
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

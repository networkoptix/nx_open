#include "system_uri.h"

#include <nx/utils/log/log.h>

using namespace nx::vms;

namespace
{
    const QString kAuthKey = lit("auth=");

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

class nx::vms::SystemUriPrivate
{
public:
    SystemUri::Protocol protocol;
    QString domain;
    SystemUri::ClientCommand clientCommand;
    QString systemId;
    SystemUri::SystemAction systemAction;
    SystemUri::Parameters parameters;

    void parse(const QString& uri)
    {

    }

};

SystemUri::SystemUri()
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

SystemUri::Parameters SystemUri::parameters() const
{
    Q_D(const SystemUri);
    return d->parameters;
}

void SystemUri::setParameters(const Parameters& value)
{
    Q_D(SystemUri);
    d->parameters = value;
}

bool SystemUri::isNull() const
{
    return true;
}

QString SystemUri::toString() const
{
    return QString();
}

QUrl SystemUri::toUrl() const
{
    return QUrl();
}

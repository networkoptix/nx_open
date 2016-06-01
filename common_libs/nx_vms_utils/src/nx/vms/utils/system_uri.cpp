#include "system_uri.h"

#include <nx/utils/log/log.h>

namespace
{
    const QString kAuthKey = lit("auth=");

}

QnSystemUriResolver::QnSystemUriResolver():
    m_valid(false)
{

}

QnSystemUriResolver::QnSystemUriResolver(const QUrl &uri):
    m_valid(false),
    m_uri(uri)
{
    parseUri();
}

bool QnSystemUriResolver::isValid() const
{
    return m_valid;
}

QnSystemUriResolver::Result QnSystemUriResolver::result() const
{
    return m_result;
}

bool QnSystemUriResolver::parseUri()
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

class QnSystemUriPrivate
{
public:

    QnSystemUri::Protocol protocol;
    QString domain;
    QnSystemUri::ClientCommand clientCommand;
    QString systemId;
    QnSystemUri::SystemAction systemAction;
    QnSystemUri::Parameters parameters;

    void parse(const QString& uri)
    {

    }

};

QnSystemUri::QnSystemUri()
{

}

QnSystemUri::QnSystemUri(const QString& uri):
    QnSystemUri()
{
    Q_D(QnSystemUri);
    d->parse(uri);
}

QnSystemUri::~QnSystemUri()
{

}

QnSystemUri::Protocol QnSystemUri::protocol() const
{
    Q_D(const QnSystemUri);
    return d->protocol;
}

void QnSystemUri::setProtocol(Protocol value)
{
    Q_D(QnSystemUri);
    d->protocol = value;
}

QString QnSystemUri::domain() const
{
    Q_D(const QnSystemUri);
    return d->domain;
}

void QnSystemUri::setDomain(const QString& value)
{
    Q_D(QnSystemUri);
    d->domain = value;
}

QnSystemUri::ClientCommand QnSystemUri::clientCommand() const
{
    Q_D(const QnSystemUri);
    return d->clientCommand;
}

void QnSystemUri::setClientCommand(ClientCommand value)
{
    Q_D(QnSystemUri);
    d->clientCommand = value;
}

QString QnSystemUri::systemId() const
{
    Q_D(const QnSystemUri);
    return d->systemId;
}

void QnSystemUri::setSystemId(const QString& value)
{
    Q_D(QnSystemUri);
    d->systemId = value;
}

QnSystemUri::SystemAction QnSystemUri::systemAction() const
{
    Q_D(const QnSystemUri);
    return d->systemAction;
}

void QnSystemUri::setSystemAction(SystemAction value)
{
    Q_D(QnSystemUri);
    d->systemAction = value;
}

QnSystemUri::Parameters QnSystemUri::parameters() const
{
    Q_D(const QnSystemUri);
    return d->parameters;
}

void QnSystemUri::setParameters(const Parameters& value)
{
    Q_D(QnSystemUri);
    d->parameters = value;
}

bool QnSystemUri::isNull() const
{
    return true;
}

QString QnSystemUri::toString() const
{
    return QString();
}

QUrl QnSystemUri::toUrl() const
{
    return QUrl();
}

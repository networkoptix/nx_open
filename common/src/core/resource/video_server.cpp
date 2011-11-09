#include "video_server.h"
#include <QUrl>

QnVideoServer::QnVideoServer():
    QnResource()
    //,m_rtspListener(0)
{
    setTypeId(qnResTypePool->getResourceTypeId("", "Server"));
}

QnVideoServer::~QnVideoServer()
{
    //delete m_rtspListener;
}

QString QnVideoServer::getUniqueId() const
{
    QnVideoServer* nonConstThis = const_cast<QnVideoServer*> (this);
    if (!getId().isValid())
        nonConstThis->setId(QnId::generateSpecialId());
    return QString("Server ") + getId().toString();
}

QnAbstractStreamDataProvider* QnVideoServer::createDataProviderInternal(ConnectionRole /*role*/)
{
    return 0;
}

void QnVideoServer::setApiUrl(const QString& restUrl)
{
    m_apiUrl = restUrl;
    QUrl u(restUrl);
    QAuthenticator auth;
    auth.setPassword(u.password());
    m_restConnection = QnVideoServerConnectionPtr(new QnVideoServerConnection(QHostAddress(u.host()), u.port(), auth));
}

QnVideoServerConnectionPtr QnVideoServer::apiConnection()
{
    return m_restConnection;
}


#if 0
void QnVideoServer::startRTSPListener(const QHostAddress& address, int port)
{
    if (m_rtspListener)
        return;
    m_rtspListener = new QnRtspListener(address, port);
}

void QnVideoServer::stopRTSPListener()
{
    delete m_rtspListener;
    m_rtspListener = 0;
}
#endif

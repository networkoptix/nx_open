#include "video_server.h"

#include <QtCore/QUrl>

QnLocalVideoServer::QnLocalVideoServer()
    : QnResource()
{
    //setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("LocalServer")));
    addFlag(server | local);
    setName(QLatin1String("Local"));
    setId(QnId());
}

QnLocalVideoServer::~QnLocalVideoServer()
{
}

QString QnLocalVideoServer::getUniqueId() const
{
    return QLatin1String("LocalServer");
}

QnAbstractStreamDataProvider *QnLocalVideoServer::createDataProviderInternal(ConnectionRole role)
{
    Q_UNUSED(role)
    return 0;
}


QnVideoServer::QnVideoServer():
    QnResource()
    //,m_rtspListener(0)
{
    setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("Server")));
    addFlag(server | remote);
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
    return QLatin1String("Server ") + getId().toString();
}

QnAbstractStreamDataProvider* QnVideoServer::createDataProviderInternal(ConnectionRole /*role*/)
{
    return 0;
}

void QnVideoServer::setApiUrl(const QString& restUrl)
{
    m_apiUrl = restUrl;
    m_restConnection = QnVideoServerConnectionPtr(new QnVideoServerConnection(restUrl));
}

QString QnVideoServer::getApiUrl() const
{
    return m_apiUrl;
}

QnVideoServerConnectionPtr QnVideoServer::apiConnection()
{
    return m_restConnection;
}

void QnVideoServer::setGuid(const QString& guid)
{
    m_guid = guid;
}

QString QnVideoServer::getGuid() const
{
    return m_guid;
}

QnResourcePtr QnVideoServerFactory::createResource(const QnId &/*resourceTypeId*/, const QnResourceParameters &parameters)
{
    QnResourcePtr result(new QnVideoServer());
    result->deserialize(parameters);

    return result;
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

#include "video_server.h"

#include <QtCore/QUrl>
#include "utils/common/delete_later.h"

QnLocalVideoServer::QnLocalVideoServer()
    : QnResource()
{
    //setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("LocalServer"))); // ###
    addFlag(server | local);
    setName(QLatin1String("Local"));
    setStatus(Online);
}

QString QnLocalVideoServer::getUniqueId() const
{
    return QLatin1String("LocalServer");
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
    QMutexLocker mutexLocker(&m_mutex); // needed here !!!
    QnVideoServer* nonConstThis = const_cast<QnVideoServer*> (this);
    if (!getId().isValid())
        nonConstThis->setId(QnId::generateSpecialId());
    return QLatin1String("Server ") + getId().toString();
}

void QnVideoServer::setApiUrl(const QString& restUrl)
{
    m_apiUrl = restUrl;

    /* We want the video server connection to be deleted in its associated thread, 
     * no matter where the reference count reached zero. Hence the custom deleter. */
    m_restConnection = QnVideoServerConnectionPtr(new QnVideoServerConnection(restUrl), &qnDeleteLater);
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

QnResourcePtr QnVideoServerFactory::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    Q_UNUSED(resourceTypeId)

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

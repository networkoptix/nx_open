#include "video_server.h"

#include <QtCore/QUrl>
#include "utils/common/delete_later.h"

QnLocalVideoServerResource::QnLocalVideoServerResource()
    : QnResource()
{
    //setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("LocalServer"))); // ###
    addFlag(server | local);
    setName(QLatin1String("Local"));
    setStatus(Online);
}

QString QnLocalVideoServerResource::getUniqueId() const
{
    return QLatin1String("LocalServer");
}


QnVideoServerResource::QnVideoServerResource():
    QnResource()
    //,m_rtspListener(0)
{
    setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("Server")));
    addFlag(server | remote);
}

QnVideoServerResource::~QnVideoServerResource()
{
    //delete m_rtspListener;
}

QString QnVideoServerResource::getUniqueId() const
{
    QMutexLocker mutexLocker(&m_mutex); // needed here !!!
    QnVideoServerResource* nonConstThis = const_cast<QnVideoServerResource*> (this);
    if (!getId().isValid())
        nonConstThis->setId(QnId::generateSpecialId());
    return QLatin1String("Server ") + getId().toString();
}

void QnVideoServerResource::setApiUrl(const QString& restUrl)
{
    m_apiUrl = restUrl;

    /* We want the video server connection to be deleted in its associated thread, 
     * no matter where the reference count reached zero. Hence the custom deleter. */
    m_restConnection = QnVideoServerConnectionPtr(new QnVideoServerConnection(restUrl), &qnDeleteLater);
}

QString QnVideoServerResource::getApiUrl() const
{
    return m_apiUrl;
}

QnVideoServerConnectionPtr QnVideoServerResource::apiConnection()
{
    return m_restConnection;
}

void QnVideoServerResource::setGuid(const QString& guid)
{
    m_guid = guid;
}

QString QnVideoServerResource::getGuid() const
{
    return m_guid;
}

QnResourcePtr QnVideoServerResourceFactory::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    Q_UNUSED(resourceTypeId)

    QnResourcePtr result(new QnVideoServerResource());
    result->deserialize(parameters);

    return result;
}

QnStorageList QnVideoServerResource::getStorages() const
{
    return m_storages;
}

void QnVideoServerResource::setStorages(const QnStorageList &storages)
{
    m_storages = storages;
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

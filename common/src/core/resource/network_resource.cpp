
#include "network_resource.h"

#include <memory>

#include <QtCore/QElapsedTimer>
#include <QCryptographicHash>

#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "utils/network/ping.h"
#include "utils/network/socket.h"
#include "utils/network/http/httptypes.h"
#include "utils/network/rtsp/rtsp_types.h"
#include "resource_consumer.h"
#include "utils/common/long_runnable.h"
#include "utils/network/http/httptypes.h"

#include <recording/time_period_list.h>


QnNetworkResource::QnNetworkResource(): 
    QnResource(),
    m_authenticated(true),
    m_networkStatus(0),
    m_networkTimeout(1000 * 10),
    m_httpPort(nx_http::DEFAULT_HTTP_PORT),
    m_mediaPort(nx_rtsp::DEFAULT_RTSP_PORT),
    m_probablyNeedToUpdateStatus(false)
{
    //TODO: #GDM #Common motion flag should be set in QnVirtualCameraResource depending on motion support
    addFlags(Qn::network);
}

QnNetworkResource::~QnNetworkResource()
{
}

QString QnNetworkResource::getUniqueId() const
{
    return getPhysicalId();
}


QString QnNetworkResource::getHostAddress() const
{
    //QMutexLocker mutex(&m_mutex);
    //return m_hostAddr;
    QString url = getUrl();
    if (url.indexOf(QLatin1String("://")) == -1)
        return url;
    else
        return QUrl(url).host();
}

void QnNetworkResource::setHostAddress(const QString &ip)
{
    //QMutexLocker mutex(&m_mutex);
    //m_hostAddr = ip;
    QUrl currentValue(getUrl());
    if (currentValue.scheme().isEmpty()) {
        setUrl(ip);
    }
    else {
        currentValue.setHost(ip);
        setUrl(currentValue.toString());
    }
}

QnMacAddress QnNetworkResource::getMAC() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_macAddress;
}

void QnNetworkResource::setMAC(const QnMacAddress &mac)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_macAddress = mac;

    if (m_physicalId.isEmpty())
        m_physicalId = mac.toString();
}

QString QnNetworkResource::getPhysicalId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_physicalId;
}

void QnNetworkResource::setPhysicalId(const QString &physicalId)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_physicalId = physicalId;
}

void QnNetworkResource::setAuth(const QAuthenticator &auth)
{
    setProperty( Qn::CAMERA_CREDENTIALS_PARAM_NAME, lit("%1:%2").arg(auth.user()).arg(auth.password()), true);
}

void QnNetworkResource::setDefaultAuth(const QAuthenticator &auth)
{
    setProperty( Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME, lit("%1:%2").arg(auth.user()).arg(auth.password()), true);
}

QAuthenticator QnNetworkResource::getAuth() const
{
    QString value = getProperty(Qn::CAMERA_CREDENTIALS_PARAM_NAME);
    if (value.isNull())
        value = getProperty(Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME);
    const QStringList& credentialsList = value.split(lit(":"));
    QAuthenticator auth;
    if( credentialsList.size() >= 1 )
        auth.setUser( credentialsList[0] );
    if( credentialsList.size() >= 2 )
        auth.setPassword( credentialsList[1] );
    return auth;
}

bool QnNetworkResource::isAuthenticated() const
{
    return m_authenticated;
}

void QnNetworkResource::setAuthenticated(bool auth)
{
    m_authenticated = auth;
}

int QnNetworkResource::httpPort() const
{
    return m_httpPort;
}

void QnNetworkResource::setHttpPort( int newPort )
{
    m_httpPort = newPort;
}

int QnNetworkResource::mediaPort() const
{
    return m_mediaPort;
}

void QnNetworkResource::setMediaPort( int newPort )
{
    m_mediaPort = newPort;
}

QString QnNetworkResource::toString() const
{
    QString result;
    QTextStream(&result) << getName() << "(" << getHostAddress() << ") live";
    return result;
}

QString QnNetworkResource::toSearchString() const
{
    QString result;
    QTextStream(&result) << QnResource::toSearchString() << " " << getPhysicalId(); //TODO: #Elric evil!
    return result;
}

void QnNetworkResource::addNetworkStatus(NetworkStatus status)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkStatus |= status;
}

void QnNetworkResource::removeNetworkStatus(NetworkStatus status)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkStatus &= (~status);
}

bool QnNetworkResource::checkNetworkStatus(NetworkStatus status) const
{
    QMutexLocker mutexLocker(&m_mutex);
    return (m_networkStatus & status) == status;
}

void QnNetworkResource::setNetworkStatus(NetworkStatus status)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkStatus = status;
}

void QnNetworkResource::setNetworkTimeout(unsigned int timeout)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkTimeout = timeout;
}

unsigned int QnNetworkResource::getNetworkTimeout() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_networkTimeout;
}

void QnNetworkResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields)
{
    QMutexLocker mutexLocker(&m_mutex);
    QnResource::updateInner(other, modifiedFields);
    QnNetworkResourcePtr other_casted = qSharedPointerDynamicCast<QnNetworkResource>(other);
    if (other_casted)
    {
        //m_auth = other_casted->m_auth;    //auth moved to resource properties
        m_macAddress = other_casted->m_macAddress;
    }
}

bool QnNetworkResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source )
{
    if (source->getUrl() != getUrl())
    {
        setUrl(source->getUrl());
        return true;
    }

    return false;
}


int QnNetworkResource::getChannel() const
{
    return 0;
}

bool QnNetworkResource::ping()
{
    std::auto_ptr<AbstractStreamSocket> sock( SocketFactory::createStreamSocket() );
    return sock->connect( getHostAddress(), QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT) );
}

QnTimePeriodList QnNetworkResource::getDtsTimePeriods(qint64 startTimeMs, qint64 endTimeMs, int detailLevel) {
    Q_UNUSED(startTimeMs)
    Q_UNUSED(endTimeMs)
    Q_UNUSED(detailLevel)
    return QnTimePeriodList();
}

/*
void QnNetworkResource::getDevicesBasicInfo(QnResourceMap& lst, int threads)
{
    // cannot make concurrent work with pointer CLDevice* ; => so extra steps needed

    NX_LOG(QLatin1String("Geting device info..."), cl_logDEBUG1);
    QTime time;
    time.start();


    QList<QnResourcePtr> local_list;
    for (const QnResourcePtr& res: lst.values())
    {
        QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
        if (netRes && !(netRes->checkNetworkStatus(QnNetworkResource::HasConflicts)))
            local_list << res.data();
    }


    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(local_list, std::mem_fun(&QnResource::getBaseInfo));
    for (int i = 0; i < threads; ++i )global->reserveThread();

    CL_LOG(cl_logDEBUG1)
    {
        NX_LOG(QLatin1String("Done. Time elapsed: "), time.elapsed(), cl_logDEBUG1);

        for(const QnResourcePtr& res: lst)
            NX_LOG(res->toString(), cl_logDEBUG1);

    }

}
*/

QnUuid QnNetworkResource::uniqueIdToId(const QString& uniqId)
{
    Q_ASSERT(!uniqId.isEmpty());
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(uniqId.toUtf8());
    QnUuid id = QnUuid::fromRfc4122(md5.result());
    return id;
}

void QnNetworkResource::initializationDone()
{
    QnResource::initializationDone();

    if (hasFlags(Qn::desktop_camera))
        return;
    
    if (getStatus() == Qn::Offline || getStatus() == Qn::Unauthorized || getStatus() == Qn::NotDefined)
        setStatus(Qn::Online);
}

void QnNetworkResource::setAuth(const QString &user, const QString &password)
{ 
    QAuthenticator auth; 
    auth.setUser(user); 
    auth.setPassword(password); 
    setAuth(auth); 
}

void QnNetworkResource::setDefaultAuth(const QString &user, const QString &password)
{ 
    QAuthenticator auth; 
    auth.setUser(user); 
    auth.setPassword(password); 
    setDefaultAuth(auth); 
}

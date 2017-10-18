
#include "network_resource.h"

#include <memory>

#include <QCryptographicHash>

#include <nx/network/nettools.h>
#include "utils/common/sleep.h"
#include <nx/network/ping.h>
#include <nx/network/socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rtsp/rtsp_types.h>
#include "resource_consumer.h"
#include "nx/utils/thread/long_runnable.h"
#include <utils/crypt/symmetrical.h>

#include <recording/time_period_list.h>
#include <nx_ec/data/api_camera_data.h>


QnNetworkResource::QnNetworkResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_authenticated(true),
    m_networkStatus(0),
    m_networkTimeout(1000 * 10),
    m_httpPort(nx_http::DEFAULT_HTTP_PORT),
    m_mediaPort(nx_rtsp::DEFAULT_RTSP_PORT),
    m_probablyNeedToUpdateStatus(false)
{
    // TODO: #GDM #Common motion flag should be set in QnVirtualCameraResource depending on motion support
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
    //QnMutexLocker mutex( &m_mutex );
    //return m_hostAddr;
    QString url = getUrl();
    if (url.indexOf(QLatin1String("://")) == -1)
        return url;
    else
        return QUrl(url).host();
}

void QnNetworkResource::setHostAddress(const QString &ip)
{
    //QnMutexLocker mutex( &m_mutex );
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
    QnMutexLocker mutexLocker( &m_mutex );
    return m_macAddress;
}

void QnNetworkResource::setMAC(const QnMacAddress &mac)
{
    QnMutexLocker mutexLocker( &m_mutex );
    m_macAddress = mac;

    if (m_physicalId.isEmpty() && !mac.isNull())
        m_physicalId = mac.toString();
}

QString QnNetworkResource::getPhysicalId() const
{
    QnMutexLocker mutexLocker( &m_mutex );
    return m_physicalId;
}

void QnNetworkResource::setPhysicalId(const QString &physicalId)
{
    QnMutexLocker mutexLocker( &m_mutex );
    m_physicalId = physicalId;
}

void QnNetworkResource::setAuth(const QAuthenticator &auth)
{
    setProperty(
        Qn::CAMERA_CREDENTIALS_PARAM_NAME,
        nx::utils::encodeHexStringFromStringAES128CBC(
            lit("%1:%2").arg(auth.user())
                        .arg(auth.password())));
}

void QnNetworkResource::setDefaultAuth(const QAuthenticator &auth)
{
    setProperty(
        Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME,
        nx::utils::encodeHexStringFromStringAES128CBC(
            lit("%1:%2").arg(auth.user())
                        .arg(auth.password())));
}

QAuthenticator QnNetworkResource::getResourceAuth(
    QnCommonModule* commonModule,
    const QnUuid &resourceId,
    const QnUuid &resourceTypeId)
{
    // TODO: #GDM think about code duplication
    NX_ASSERT(!resourceId.isNull() && !resourceTypeId.isNull(), Q_FUNC_INFO, "Invalid input, reading from local data is requred");
    QString value = getResourceProperty(
        commonModule,
        Qn::CAMERA_CREDENTIALS_PARAM_NAME,
        resourceId,
        resourceTypeId);
    if (value.isNull())
        value = getResourceProperty(
            commonModule,
            Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME,
            resourceId,
            resourceTypeId);

    value = nx::utils::decodeStringFromHexStringAES128CBC(value);

    const QStringList& credentialsList = value.split(lit(":"));
    QAuthenticator auth;
    if (credentialsList.size() >= 1)
        auth.setUser(credentialsList[0]);
    if (credentialsList.size() >= 2)
        auth.setPassword(credentialsList[1]);
    return auth;
}

bool QnNetworkResource::isDefaultAuth() const
{
    auto currentAuth = getAuth();
    auto defaultAuth = getAuthInternal(getProperty(Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME));
    return currentAuth == defaultAuth;
}

QAuthenticator QnNetworkResource::getAuth() const
{
    QString value = getProperty(Qn::CAMERA_CREDENTIALS_PARAM_NAME);
    if (value.isNull())
        value = getProperty(Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME);
    return getAuthInternal(value);
}

QAuthenticator QnNetworkResource::getAuthInternal(const QString& encodedAuth) const
{
    QString value = nx::utils::decodeStringFromHexStringAES128CBC(encodedAuth);

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

QString QnNetworkResource::toSearchString() const
{
    return base_type::toSearchString()
        + L' ' + getMAC().toString()
        + L' ' + getHostAddress()
        + L' ' + lit("live")
        ; // TODO: #Elric evil!
}

void QnNetworkResource::addNetworkStatus(NetworkStatus status)
{
    QnMutexLocker mutexLocker( &m_mutex );
    m_networkStatus |= status;
}

void QnNetworkResource::removeNetworkStatus(NetworkStatus status)
{
    QnMutexLocker mutexLocker( &m_mutex );
    m_networkStatus &= (~status);
}

bool QnNetworkResource::checkNetworkStatus(NetworkStatus status) const
{
    QnMutexLocker mutexLocker( &m_mutex );
    return (m_networkStatus & status) == status;
}

void QnNetworkResource::setNetworkStatus(NetworkStatus status)
{
    QnMutexLocker mutexLocker( &m_mutex );
    m_networkStatus = status;
}

void QnNetworkResource::setNetworkTimeout(unsigned int timeout)
{
    QnMutexLocker mutexLocker( &m_mutex );
    m_networkTimeout = timeout;
}

unsigned int QnNetworkResource::getNetworkTimeout() const
{
    QnMutexLocker mutexLocker( &m_mutex );
    return m_networkTimeout;
}

void QnNetworkResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);
    QnNetworkResourcePtr other_casted = qSharedPointerDynamicCast<QnNetworkResource>(other);
    if (other_casted)
    {
        m_macAddress = other_casted->m_macAddress;
    }
}

bool QnNetworkResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source )
{
    bool mergedSomething = false;
    if (source->getUrl() != getUrl())
    {
        setUrl(source->getUrl());
        mergedSomething = true;
    }
    if (!source->getMAC().isNull() && source->getMAC() != getMAC())
    {
        setMAC(source->getMAC());
        mergedSomething = true;
    }

    return mergedSomething;
}


int QnNetworkResource::getChannel() const
{
    return 0;
}

bool QnNetworkResource::ping()
{
    std::unique_ptr<AbstractStreamSocket> sock( SocketFactory::createStreamSocket() );
    return sock->connect( getHostAddress(), QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT) );
}

void QnNetworkResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    //calling completionHandler(false) in aio_thread
    nx::network::SocketGlobals::aioService().post(std::bind(completionHandler, false));
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

QnUuid QnNetworkResource::physicalIdToId(const QString& physicalId)
{
    return ec2::ApiCameraData::physicalIdToId(physicalId);
}

void QnNetworkResource::initializationDone()
{
    QnResource::initializationDone();

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

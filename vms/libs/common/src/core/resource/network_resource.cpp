
#include "network_resource.h"

#include <memory>

#include <QCryptographicHash>

#include <nx/network/nettools.h>
#include "utils/common/sleep.h"
#include <nx/network/aio/aio_service.h>
#include <nx/network/ping.h>
#include <nx/network/socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rtsp/rtsp_types.h>
#include "resource_consumer.h"
#include "nx/utils/thread/long_runnable.h"

#include <recording/time_period_list.h>
#include <nx/vms/api/data/camera_data.h>

namespace {

static const QString kMediaPortParamName = lit("mediaPort");

} // namespace

QnNetworkResource::QnNetworkResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_authenticated(true),
    m_networkStatus(0),
    m_networkTimeout(1000 * 10),
    m_httpPort(nx::network::http::DEFAULT_HTTP_PORT),
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

nx::utils::MacAddress QnNetworkResource::getMAC() const
{
    QnMutexLocker mutexLocker( &m_mutex );
    return m_macAddress;
}

void QnNetworkResource::setMAC(const nx::utils::MacAddress &mac)
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
    setProperty(ResourcePropertyKey::kCredentials,
        lit("%1:%2").arg(auth.user()) .arg(auth.password()));
}

void QnNetworkResource::setDefaultAuth(const QAuthenticator &auth)
{
    setProperty(ResourcePropertyKey::kDefaultCredentials,
        lit("%1:%2").arg(auth.user()).arg(auth.password()));
}

QAuthenticator QnNetworkResource::getResourceAuth(
    QnCommonModule* commonModule,
    const QnUuid &resourceId,
    const QnUuid &resourceTypeId)
{
    NX_ASSERT(!resourceId.isNull() && !resourceTypeId.isNull(), Q_FUNC_INFO, "Invalid input, reading from local data is requred");
    QString value = getResourceProperty(
        commonModule,
        ResourcePropertyKey::kCredentials,
        resourceId,
        resourceTypeId);
    if (value.isNull())
        value = getResourceProperty(
            commonModule,
            ResourcePropertyKey::kDefaultCredentials,
            resourceId,
            resourceTypeId);

    return getAuthInternal(value);
}

QAuthenticator QnNetworkResource::getAuth() const
{
    QString value = getProperty(ResourcePropertyKey::kCredentials);
    if (value.isNull())
        value = getProperty(ResourcePropertyKey::kDefaultCredentials);

    return getAuthInternal(value);
}

QAuthenticator QnNetworkResource::getDefaultAuth() const
{
    QString value = getProperty(ResourcePropertyKey::kDefaultCredentials);
    return getAuthInternal(value);
}

QAuthenticator QnNetworkResource::getAuthInternal(const QString& value)
{
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

QString QnNetworkResource::mediaPortKey()
{
    return kMediaPortParamName;
}

int QnNetworkResource::mediaPort() const
{
    return getProperty(mediaPortKey()).toInt();
}

void QnNetworkResource::setMediaPort(int value)
{
    setProperty(mediaPortKey(), value > 0 ? QString::number(value) : QString());
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

QDateTime QnNetworkResource::getLastDiscoveredTime() const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_lastDiscoveredTime;
}

void QnNetworkResource::setLastDiscoveredTime(const QDateTime& time)
{
    QnMutexLocker mutexLocker(&m_mutex);
    m_lastDiscoveredTime = time;
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
        m_lastDiscoveredTime = other_casted->m_lastDiscoveredTime;
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
    std::unique_ptr<nx::network::AbstractStreamSocket> sock( nx::network::SocketFactory::createStreamSocket() );
    return sock->connect(
        getHostAddress(),
        QUrl(getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT),
        nx::network::deprecated::kDefaultConnectTimeout);
}

void QnNetworkResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    //calling completionHandler(false) in aio_thread
    nx::network::SocketGlobals::aioService().post(std::bind(completionHandler, false));
}

QnUuid QnNetworkResource::physicalIdToId(const QString& physicalId)
{
    return nx::vms::api::CameraData::physicalIdToId(physicalId);
}

void QnNetworkResource::initializationDone()
{
    QnResource::initializationDone();

    if (getStatus() == Qn::Offline || getStatus() == Qn::Unauthorized || getStatus() == Qn::NotDefined)
        setStatus(Qn::Online);
}

QString QnNetworkResource::idForToStringFromPtr() const
{
    return getPhysicalId();
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

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "network_resource.h"

#include <memory>

#include <QCryptographicHash>
#include <QtNetwork/QAuthenticator>

#include <nx/network/aio/aio_service.h>
#include <nx/network/http/http_types.h>
#include <nx/network/nettools.h>
#include <nx/network/ping.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/socket.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/api/data/camera_data.h>
#include <recording/time_period_list.h>
#include <utils/common/sleep.h>

namespace {

static const QString kMediaPortParamName = "mediaPort";

QString calculateHostAddress(const QString& url)
{
    if (url.indexOf(QLatin1String("://")) == -1)
        return url;
    return QUrl(url).host();
}

} // namespace

QnNetworkResource::QnNetworkResource():
    m_httpPort(nx::network::http::DEFAULT_HTTP_PORT),
    m_cachedHostAddress([this] { return calculateHostAddress(getUrl()); })
{
    addFlags(Qn::network);
}

QnNetworkResource::~QnNetworkResource()
{
}

void QnNetworkResource::setUrl(const QString& url)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        if (!setUrlUnsafe(url))
            return;

        m_cachedHostAddress.reset();
    }

    emit urlChanged(toSharedPointer(this));
}

QString QnNetworkResource::getHostAddress() const
{
    // Secured by the same mutex as ::setUrl(), so thread-safe.
    return m_cachedHostAddress.get();
}

void QnNetworkResource::setHostAddress(const QString& ip)
{
    //NX_MUTEX_LOCKER mutex( &m_mutex );
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
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    return m_macAddress;
}

void QnNetworkResource::setMAC(const nx::utils::MacAddress &mac)
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    m_macAddress = mac;

    if (m_physicalId.isEmpty() && !mac.isNull())
        m_physicalId = mac.toString();
}

QString QnNetworkResource::getPhysicalId() const
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    return m_physicalId;
}

void QnNetworkResource::setPhysicalId(const QString &physicalId)
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    m_physicalId = physicalId;
}

void QnNetworkResource::setAuth(const QAuthenticator &auth)
{
    setProperty(ResourcePropertyKey::kCredentials,
        QString("%1:%2").arg(auth.user()) .arg(auth.password()));
}

void QnNetworkResource::setDefaultAuth(const QAuthenticator &auth)
{
    setProperty(ResourcePropertyKey::kDefaultCredentials,
        QString("%1:%2").arg(auth.user()).arg(auth.password()));
}

QAuthenticator QnNetworkResource::getAuth() const
{
    QString value = getProperty(ResourcePropertyKey::kCredentials);
    if (value.isNull())
        value = getProperty(ResourcePropertyKey::kDefaultCredentials);

    return parseAuth(value);
}

QAuthenticator QnNetworkResource::getDefaultAuth() const
{
    QString value = getProperty(ResourcePropertyKey::kDefaultCredentials);
    return parseAuth(value);
}

QAuthenticator QnNetworkResource::parseAuth(const QString& value)
{
    const auto delimiterPos = value.indexOf(":");
    QAuthenticator auth;
    auth.setUser(value);
    if (delimiterPos >= 0)
    {
        auth.setUser(value.left(delimiterPos));
        auth.setPassword(value.mid(delimiterPos+1));
    }
    return auth;
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
    if (value != mediaPort())
        setProperty(mediaPortKey(), value > 0 ? QString::number(value) : QString());
}

void QnNetworkResource::addNetworkStatus(NetworkStatus status)
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    m_networkStatus |= status;
}

void QnNetworkResource::removeNetworkStatus(NetworkStatus status)
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    m_networkStatus &= (~status);
}

bool QnNetworkResource::checkNetworkStatus(NetworkStatus status) const
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    return (m_networkStatus & status) == status;
}

void QnNetworkResource::setNetworkStatus(NetworkStatus status)
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    m_networkStatus = status;
}

QDateTime QnNetworkResource::getLastDiscoveredTime() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_lastDiscoveredTime;
}

void QnNetworkResource::setLastDiscoveredTime(const QDateTime& time)
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    m_lastDiscoveredTime = time;
}

void QnNetworkResource::setNetworkTimeout(unsigned int timeout)
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    m_networkTimeout = timeout;
}

unsigned int QnNetworkResource::getNetworkTimeout() const
{
    NX_MUTEX_LOCKER mutexLocker( &m_mutex );
    return m_networkTimeout;
}

void QnNetworkResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    m_cachedHostAddress.reset();
    base_type::updateInternal(source, notifiers);
    if (const auto otherNetwork = source.dynamicCast<QnNetworkResource>())
    {
        NX_ASSERT(getPhysicalId() == otherNetwork->getPhysicalId() || getPhysicalId().isNull());
        m_macAddress = otherNetwork->m_macAddress;
        m_lastDiscoveredTime = otherNetwork->m_lastDiscoveredTime;
    }
}

bool QnNetworkResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr& source)
{
    bool mergedSomething = false;
    if (const auto newUrl = source->getUrl(); newUrl != getUrl())
    {
        NX_VERBOSE(this,
            "Found camera has new url. Existing url=%1, new url=%2", getUrl(), newUrl);
        setUrl(newUrl);
        mergedSomething = true;
    }
    if (const auto newMac = source->getMAC(); !newMac.isNull() && newMac != getMAC())
    {
        NX_VERBOSE(this,
            "Found camera has new mac. Existing mac=%1, new mac=%2", getMAC(), newMac);
        setMAC(newMac);
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
    auto sock =
        nx::network::SocketFactory::createStreamSocket(nx::network::ssl::kAcceptAnyCertificate);
    return sock->connect(
        getHostAddress().toStdString(),
        QUrl(getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT),
        nx::network::deprecated::kDefaultConnectTimeout);
}

void QnNetworkResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    //calling completionHandler(false) in aio_thread
    nx::network::SocketGlobals::aioService().post(std::bind(completionHandler, false));
}

nx::Uuid QnNetworkResource::physicalIdToId(const QString& physicalId)
{
    return nx::vms::api::CameraData::physicalIdToId(physicalId);
}

QString QnNetworkResource::idForToStringFromPtr() const
{
    return NX_FMT("%1: %2", getId().toSimpleString(), getPhysicalId());
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

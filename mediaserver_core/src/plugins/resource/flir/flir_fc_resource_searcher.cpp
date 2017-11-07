#include "flir_fc_resource.h"
#include "flir_fc_resource_searcher.h"
#include "flir_parsing_utils.h"

#if defined(ENABLE_FLIR)

#include <core/resource/resource_fwd.h>
#include <nx/network/http/http_client.h>
#include <nx/network/system_socket.h>
#include <utils/common/synctime.h>

namespace {

const QString kFlirFcManufacture = lit("FLIR_FC");
const QString kFlirVendor = lit("FLIR");
const QString kFlirFcResourceTypeName = lit("FLIR_FC");
const QString kFLirFcModelPrefix = lit("FC-");
const QString kDeviceInfoUrlPath = lit("/page/factory/production/info");

const int kReceiveBufferSize = 50 * 1024;
const int kDefaultBroadcastPort = 1005;

const std::chrono::milliseconds kDeviceInfoRequestTimeout = std::chrono::seconds(40);
const std::chrono::milliseconds kDeviceInfoResponseTimeout = std::chrono::seconds(40);
const std::chrono::milliseconds kCacheExpirationTime = std::chrono::minutes(5);

const QString kFlirDefaultUsername = lit("admin");
const QString kFlirDefaultPassword = lit("admin");

} // namespace

namespace nx {
namespace plugins {
namespace flir {

FcResourceSearcher::FcResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    m_flirFcTypeId(qnResTypePool->getResourceTypeId(manufacture(), kFlirFcResourceTypeName, true)),
    m_terminated(false)
{
    QnMutexLocker lock(&m_mutex);
    initListenerUnsafe();
}

FcResourceSearcher::~FcResourceSearcher()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    m_receiveSocket->pleaseStopSync();
    for (auto& httpClient : m_httpClients)
        httpClient.second->pleaseStopSync();
}

QList<QnResourcePtr> FcResourceSearcher::checkHostAddr(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    bool doMultichannelCheck)
{
    QList<QnResourcePtr> result;
    nx_http::HttpClient httpClient;

    httpClient.setSendTimeoutMs(kDeviceInfoRequestTimeout.count());
    httpClient.setResponseReadTimeoutMs(kDeviceInfoResponseTimeout.count());

    auto deviceInfoUrl = url;
    deviceInfoUrl.setScheme("http");
    deviceInfoUrl.setPath(kDeviceInfoUrlPath);

    bool success = httpClient.doGet(deviceInfoUrl);

    if (!success)
        return QList<QnResourcePtr>();

    auto response = httpClient.response();

    if (response->statusLine.statusCode != nx_http::StatusCode::Value::ok)
        return QList<QnResourcePtr>();

    nx_http::BufferType messageBody;
    while (!httpClient.eof())
        messageBody.append(httpClient.fetchMessageBodyBuffer());

    auto deviceInfoString = QString::fromUtf8(messageBody);
    auto deviceInfo = parsePrivateDeviceInfo(deviceInfoString);

    if (!deviceInfo)
        return QList<QnResourcePtr>();

    QUrl deviceUrl;
    deviceUrl.setScheme(lit("http"));
    deviceUrl.setHost(url.host());
    deviceUrl.setPort(url.port(nx_http::DEFAULT_HTTP_PORT));
    deviceInfo->url = deviceUrl;

    auto resource = makeResource(deviceInfo.get(), auth);
    if (resource)
        result.push_back(resource);

    return result;
}

QnResourceList FcResourceSearcher::findResources()
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return QnResourceList();

    QnResourceList result;
    QAuthenticator auth;

    auth.setUser(kFlirDefaultUsername);
    auth.setPassword(kFlirDefaultPassword);

    auto now = qnSyncTime->currentMSecsSinceEpoch();
    auto itr = m_deviceInfoCache.begin();

    while (itr != m_deviceInfoCache.end())
    {
        auto timestamp = itr->second.timestamp;
        auto deviceInfo = itr->second.deviceInfo;
        if (now - timestamp > kCacheExpirationTime.count())
        {
            itr = m_deviceInfoCache.erase(itr);
            continue;
        }

        auto resource = makeResource(deviceInfo, auth);
        if (resource)
            result.push_back(resource);

        itr++;
    }

    return result;
}

bool FcResourceSearcher::isSequential() const
{
    return true;
}

QString FcResourceSearcher::manufacture() const
{
    return kFlirFcManufacture;
}

QnResourcePtr FcResourceSearcher::makeResource(
    const fc_private::DeviceInfo& info,
    const QAuthenticator& auth)
{
    if (!isDeviceSupported(info))
        return QnResourcePtr();

    QnFlirFcResourcePtr resource(new FcResource());

    resource->setName(info.model);
    resource->setModel(info.model);
    resource->setVendor(kFlirVendor);
    resource->setTypeId(m_flirFcTypeId);
    resource->setUrl(info.url.toString());
    resource->setPhysicalId(info.serialNumber);
    resource->setDefaultAuth(auth);

    return resource;
}

QnResourcePtr FcResourceSearcher::createResource(
    const QnUuid &resourceTypeId,
    const QnResourceParams &params)
{
    QnNetworkResourcePtr result;
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return result;

    if (resourceType->getManufacture() != manufacture())
        return result;

    result = QnPhysicalCameraResourcePtr(new FcResource());
    result->setTypeId(resourceTypeId);

    qDebug() << "Create FLIR (FC) camera resource. typeID:" << resourceTypeId.toString();

    return result;
}

void FcResourceSearcher::initListenerUnsafe()
{
    m_receiveBuffer.clear();
    m_receiveSocket.reset(new nx::network::UDPSocket(AF_INET));
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->setRecvBufferSize(kReceiveBufferSize);
    m_receiveSocket->bind(SocketAddress(HostAddress::anyHost, kDefaultBroadcastPort));
    m_receiveSocket->setNonBlockingMode(true);
    doNextReceiveUnsafe();
}

nx_http::AsyncHttpClientPtr FcResourceSearcher::createHttpClient() const
{
    nx_http::AuthInfo authInfo;

    authInfo.user.username = kFlirDefaultUsername;
    authInfo.user.authToken.setPassword(kFlirDefaultPassword);

    auto httpClientPtr = nx_http::AsyncHttpClient::create();
    httpClientPtr->setSendTimeoutMs(kDeviceInfoRequestTimeout.count());
    httpClientPtr->setResponseReadTimeoutMs(kDeviceInfoResponseTimeout.count());
    httpClientPtr->setAuth(authInfo);

    return httpClientPtr;
}

void FcResourceSearcher::doNextReceiveUnsafe()
{
    if (m_terminated)
        return;

    m_receiveBuffer.reserve(AbstractDatagramSocket::MAX_DATAGRAM_SIZE);
    m_receiveSocket->recvFromAsync(
        &m_receiveBuffer,
        [this](SystemError::ErrorCode erroCode, SocketAddress endpoint, std::size_t bytesRead)
        {
            receiveFromCallback(erroCode, endpoint, bytesRead);
        });
}

void FcResourceSearcher::receiveFromCallback(
    SystemError::ErrorCode errorCode,
    SocketAddress senderAddress,
    std::size_t bytesRead)
{
    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;

    if (errorCode != SystemError::noError)
        initListenerUnsafe();

    auto discoveryMessage = QString::fromUtf8(m_receiveBuffer.left(bytesRead));
    auto discoveryInfo = parseDeviceDiscoveryInfo(discoveryMessage);

    m_receiveBuffer.clear();

    if (!discoveryInfo || hasValidCacheUnsafe(senderAddress))
    {
        doNextReceiveUnsafe();
        return;
    }

    auto url = nx::utils::Url(lit("http://%1").arg(senderAddress.toString()));
    url.setPort(nx_http::DEFAULT_HTTP_PORT);
    url.setPath(kDeviceInfoUrlPath);

    if (m_requestsInProgress.find(senderAddress) == m_requestsInProgress.end())
    {
        auto onResponseReceived =
            [this, senderAddress](nx_http::AsyncHttpClientPtr httpClient)
        {
            QnMutexLocker lock(&m_mutex);
            handleDeviceInfoResponseUnsafe(senderAddress, httpClient);
        };

        if (m_httpClients.find(senderAddress) == m_httpClients.end())
            m_httpClients[senderAddress] = createHttpClient();

        m_requestsInProgress.insert(senderAddress);
        m_httpClients[senderAddress]->doGet(url, onResponseReceived);
    }

    doNextReceiveUnsafe();
}

bool FcResourceSearcher::hasValidCacheUnsafe(const SocketAddress& address) const
{
    auto itr = m_deviceInfoCache.find(address);

    if (itr == m_deviceInfoCache.end())
        return false;

    auto timestamp = itr->second.timestamp;
    auto now = qnSyncTime->currentMSecsSinceEpoch();
    if (now - timestamp > kCacheExpirationTime.count())
        return false;

    return true;
}

bool FcResourceSearcher::isDeviceSupported(const fc_private::DeviceInfo& deviceInfo) const
{
    return deviceInfo.model.startsWith(kFLirFcModelPrefix);
}

void FcResourceSearcher::cleanUpEndpointInfoUnsafe(const SocketAddress& endpoint)
{
    m_deviceInfoCache.erase(endpoint);
    m_requestsInProgress.erase(endpoint);
}

void FcResourceSearcher::handleDeviceInfoResponseUnsafe(
    const SocketAddress& senderAddress,
    nx_http::AsyncHttpClientPtr httpClient)
{
    if (m_terminated)
        return;

    if (httpClient->state() != nx_http::AsyncClient::State::sDone)
    {
        cleanUpEndpointInfoUnsafe(senderAddress);
        return;
    }

    auto response = httpClient->response();
    if (response->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        cleanUpEndpointInfoUnsafe(senderAddress);
        return;
    }

    auto deviceInfoString = QString::fromUtf8(httpClient->fetchMessageBodyBuffer());
    auto deviceInfo = parsePrivateDeviceInfo(deviceInfoString);

    if (!deviceInfo || !isDeviceSupported(deviceInfo.get()))
    {
        cleanUpEndpointInfoUnsafe(senderAddress);
        return;
    }

    auto url = QUrl(lit("http://%1").arg(senderAddress.toString()));
    url.setPort(nx_http::DEFAULT_HTTP_PORT);
    deviceInfo->url = url;

    TimestampedDeviceInfo tsDeviceInfo;
    tsDeviceInfo.deviceInfo = deviceInfo.get();
    tsDeviceInfo.timestamp = qnSyncTime->currentMSecsSinceEpoch();

    m_deviceInfoCache[senderAddress] = tsDeviceInfo;
    m_requestsInProgress.erase(senderAddress);
}

} // namespace flir
} // namespace plugins
} // namespace nx 

#endif // defined(ENABLE_FLIR)

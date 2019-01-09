#include "proxy_connection.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>

#include "api/app_server_connection.h"
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include "core/resource/network_resource.h"
#include <nx/network/http/custom_headers.h>
#include <transaction/message_bus_adapter.h>
#include "network/router.h"
#include "network/tcp_listener.h"
#include "proxy_connection_processor_p.h"

#include <nx/network/socket.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/utils/system_error.h>

#include "api/app_server_connection.h"
#include "core/resource/network_resource.h"

#include <nx/network/http/custom_headers.h>
#include "api/global_settings.h"
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/aio/unified_pollset.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/rtsp/rtsp_types.h>

#include <utils/common/app_info.h>
#include <core/resource/user_resource.h>
#include <nx/vms/auth/time_based_nonce_provider.h>

class QnTcpListener;

static const int kMaxProxyTtl = 8;
static const std::chrono::milliseconds kIoTimeout = std::chrono::minutes(16);
static const std::chrono::milliseconds kPollTimeout = std::chrono::milliseconds(1);
static const int kReadBufferSize = 1024 * 128; /* ~ 1 gbit/s */

namespace nx {
namespace vms {
namespace network {

// ----------------------------- QnProxyConnectionProcessor ----------------------------

ProxyConnectionProcessor::ProxyConnectionProcessor(
	::nx::vms::network::ReverseConnectionManager* reverseConnectionManager,
	std::unique_ptr<::nx::network::AbstractStreamSocket> socket,
	QnHttpConnectionListener* owner)
:
	QnTCPConnectionProcessor(new ProxyConnectionProcessorPrivate, std::move(socket), owner)
{
	Q_D(ProxyConnectionProcessor);
	d->connectTimeout = commonModule()->globalSettings()->proxyConnectTimeout();
	d->reverseConnectionManager = reverseConnectionManager;
}

ProxyConnectionProcessor::~ProxyConnectionProcessor()
{
	pleaseStop();
	stop();
}

bool ProxyConnectionProcessor::isStandardProxyNeeded(QnCommonModule* commonModule, const nx::network::http::Request& request)
{
	return isCloudRequest(request) || isProxyForCamera(commonModule, request);
}

bool ProxyConnectionProcessor::isCloudRequest(const nx::network::http::Request& request)
{
	return request.requestLine.url.host() == nx::network::SocketGlobals::cloud().cloudHost() ||
		request.requestLine.url.path().startsWith("/cdb") ||
		request.requestLine.url.path().startsWith("/nxcloud") ||
		request.requestLine.url.path().startsWith("/nxlicense");
}

bool ProxyConnectionProcessor::isProxyForCamera(
	QnCommonModule* commonModule,
	const nx::network::http::Request& request)
{
	nx::network::http::BufferType desiredCameraGuid;
	nx::network::http::HttpHeaders::const_iterator xCameraGuidIter = request.headers.find(Qn::CAMERA_GUID_HEADER_NAME);
	if (xCameraGuidIter != request.headers.end())
	{
		desiredCameraGuid = xCameraGuidIter->second;
	}
	else {
		desiredCameraGuid = request.getCookieValue(Qn::CAMERA_GUID_HEADER_NAME);
	}
	if (!desiredCameraGuid.isEmpty()) {
		QnResourcePtr camera = commonModule->resourcePool()->getResourceById(QnUuid::fromStringSafe(desiredCameraGuid));
		return camera != 0;
	}

	return false;
}

bool ProxyConnectionProcessor::isProtocol(const QString& protocol) const
{
	return nx::network::http::isUrlSheme(protocol)
		|| nx::network::rtsp::isUrlSheme(protocol);
}

int ProxyConnectionProcessor::getDefaultPortByProtocol(const QString& protocol)
{
	const auto port = nx::network::url::getDefaultPortForScheme(protocol);
	return port == 0 ? -1 : port;
}

bool ProxyConnectionProcessor::doProxyData(
	nx::network::AbstractStreamSocket* srcSocket,
	nx::network::AbstractStreamSocket* dstSocket,
	char* buffer,
	int bufferSize,
	bool* outSomeBytesRead)
{
	if (outSomeBytesRead)
		*outSomeBytesRead = false;
	int readed;
	if( !readSocketNonBlock(&readed, srcSocket, buffer, bufferSize) )
		return true;

	if (readed < 1)
	{
		if (readed != 0)
			NX_VERBOSE(this, lm("Error during proxying data"));
		return false;
	}

	if (outSomeBytesRead)
		*outSomeBytesRead = true;

	for( ;; )
	{
		int sended = dstSocket->send(buffer, readed);
		if( sended < 0 )
		{
			if( SystemError::getLastOSErrorCode() == SystemError::interrupted )
				continue;   //retrying interrupted call
            NX_DEBUG(this, lit("doProxyData. Socket error: %1").arg(SystemError::getLastOSErrorText()));
			return false;
		}
		if( sended == 0 )
			return false;   //socket closed
		if( sended == readed )
			return true;    //sent everything
		buffer += sended;
		readed -= sended;
	}
}

#ifdef PROXY_STRICT_IP
static bool isLocalAddress(const QString& addr)
{
	return addr == lit("localhost") || addr == lit("127.0.0.1");
}
#endif

QString ProxyConnectionProcessor::connectToRemoteHost(const QnRoute& route, const nx::utils::Url &url)
{
	Q_D(ProxyConnectionProcessor);
	d->dstSocket.reset();
	if (route.reverseConnect) {
		const auto& target = route.gatewayId.isNull() ? route.id : route.gatewayId;
		d->dstSocket = std::move(d->reverseConnectionManager->getProxySocket(
			target,
			d->connectTimeout));
	}

    if (!d->dstSocket)
	{

#ifdef PROXY_STRICT_IP
		QnServerMessageProcessor* processor = dynamic_cast<QnServerMessageProcessor*> (QnServerMessageProcessor::instance());
		if (processor && !processor->isKnownAddr(url.host()) && ! isLocalAddress(url.host()))
			return QString();
#endif

		if (url.host().isEmpty())
		{
			NX_DEBUG(this, lm("Empty target host detected. Cannot connect"));
			d->socket->close();
			return QString();
		}

        const bool useSsl = url.scheme() == nx::network::http::kSecureUrlSchemeName
            || url.scheme() == nx::network::rtsp::kSecureUrlSchemeName;
        d->dstSocket =
			nx::network::SocketFactory::createStreamSocket(useSsl);
		d->dstSocket->setRecvTimeout(d->connectTimeout.count());
		d->dstSocket->setSendTimeout(d->connectTimeout.count());
		if (!d->dstSocket->connect(
				nx::network::SocketAddress(url.host().toLatin1().data(), url.port()),
				nx::network::deprecated::kDefaultConnectTimeout))
		{
			d->socket->close();
			return QString(); // now answer from destination address
		}
		return url.toString();
	}
	else {
		d->dstSocket->setRecvTimeout(d->connectTimeout.count());
		d->dstSocket->setSendTimeout(d->connectTimeout.count());
		return route.id.toString();
	}

	d->dstSocket->setRecvTimeout(kIoTimeout);
	d->dstSocket->setSendTimeout(kIoTimeout);

	return QString();
}

/**
* Server nonce could be local since v.3.0. It cause authentication issue while proxing request.
* This function replace user information to the serverAuth key credentials to guarantee
* success authentication on the other server.
*/
bool ProxyConnectionProcessor::replaceAuthHeader()
{
	Q_D(ProxyConnectionProcessor);

	QByteArray authHeaderName;
	if (d->request.headers.find(nx::network::http::header::Authorization::NAME) != d->request.headers.end())
		authHeaderName = nx::network::http::header::Authorization::NAME;
	else if(d->request.headers.find("Proxy-Authorization") != d->request.headers.end())
		authHeaderName = QByteArray("Proxy-Authorization");
	else
		return true; //< not need to replace anything (proxy without authorization or auth by query items)

	nx::network::http::header::DigestAuthorization originalAuthHeader;
	if (!originalAuthHeader.parse(nx::network::http::getHeaderValue(d->request.headers, authHeaderName)))
		return false;
	if (isStandardProxyNeeded(commonModule(), d->request))
	{
		return true; //< no need to update, it is non server proxy request
	}

	if (auto ownServer = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID()))
	{
		// it's already authorized request.
		// Update authorization using local system user related to current server
		QString userName = ownServer->getId().toString();
		QString password = ownServer->getAuthKey();

		nx::network::http::header::WWWAuthenticate wwwAuthenticateHeader;
		nx::network::http::header::DigestAuthorization digestAuthorizationHeader;
		wwwAuthenticateHeader.authScheme = nx::network::http::header::AuthScheme::digest;
		wwwAuthenticateHeader.params["nonce"] = TimeBasedNonceProvider::generateTimeBasedNonce();
		wwwAuthenticateHeader.params["realm"] = nx::network::AppInfo::realm().toUtf8();

		if (!nx::network::http::calcDigestResponse(
			d->request.requestLine.method,
			userName.toUtf8(),
			password.toUtf8(),
			boost::none,
			d->request.requestLine.url.path().toUtf8(),
			wwwAuthenticateHeader,
			&digestAuthorizationHeader))
		{
			return false;
		}

		nx::network::http::HttpHeader authHeader(authHeaderName, digestAuthorizationHeader.serialized());
		QByteArray originalUserName;
		auto resPool = commonModule()->resourcePool();
		if (auto user = resPool->getResourceById<QnUserResource>(d->accessRights.userId))
			originalUserName = user->getName().toUtf8();
		if (originalUserName.isEmpty())
			originalUserName = originalAuthHeader.digest->userid;
		if (!originalUserName.isEmpty())
		{
			nx::network::http::HttpHeader userNameHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, originalUserName);
			nx::network::http::insertOrReplaceHeader(&d->request.headers, userNameHeader);
		}
		nx::network::http::insertOrReplaceHeader(&d->request.headers, authHeader);
	}

	return true;
}

void ProxyConnectionProcessor::cleanupProxyInfo(nx::network::http::Request* request)
{
	static const char* kProxyHeadersPrefix = "Proxy-";

	auto itr = request->headers.lower_bound(kProxyHeadersPrefix);
	while (itr != request->headers.end() && itr->first.startsWith(kProxyHeadersPrefix))
		itr = request->headers.erase(itr);

	request->requestLine.url = request->requestLine.url.toString(
		QUrl::RemoveScheme | QUrl::RemovePort | QUrl::RemoveAuthority);
}

static void fixServerUrlSchemeSecurity(nx::utils::Url* url, const QnGlobalSettings* settings)
{
    namespace http = nx::network::http;
    namespace rtsp = nx::network::rtsp;

    if (settings->isTrafficEncriptionForced() && url->scheme() == http::kUrlSchemeName)
        url->setScheme(http::kSecureUrlSchemeName);
    else
    if (settings->isVideoTrafficEncriptionForced() && url->scheme() == rtsp::kUrlSchemeName)
        url->setScheme(rtsp::kSecureUrlSchemeName);
}

bool ProxyConnectionProcessor::updateClientRequest(nx::utils::Url& dstUrl, QnRoute& dstRoute)
{
	Q_D(ProxyConnectionProcessor);

	if (isStandardProxyNeeded(commonModule(), d->request))
	{
		dstUrl = d->request.requestLine.url;
	}
	else
	{
		nx::utils::Url url = d->request.requestLine.url;
		QString host = url.host();
		QString urlPath = QString('/') + QnTcpListener::normalizedPath(url.path());

		// todo: this code is deprecated and isn't compatible with WEB client
		// It never used for WEB client purpose
		if (urlPath.startsWith("proxy") || urlPath.startsWith("/proxy"))
		{
			int proxyEndPos = urlPath.indexOf('/', 2); // remove proxy prefix
			int protocolEndPos = urlPath.indexOf('/', proxyEndPos + 1); // remove proxy prefix
			if (protocolEndPos == -1)
				return false;

			QString protocol = urlPath.mid(proxyEndPos + 1, protocolEndPos - proxyEndPos - 1);
			if (!isProtocol(protocol)) {
				protocol = url.scheme();
				if (protocol.isEmpty())
					protocol = "http";
				protocolEndPos = proxyEndPos;
			}

			int hostEndPos = urlPath.indexOf('/', protocolEndPos + 1); // remove proxy prefix
			if (hostEndPos == -1)
				hostEndPos = urlPath.size();

			host = urlPath.mid(protocolEndPos + 1, hostEndPos - protocolEndPos - 1);
			if (host.startsWith("{"))
				dstRoute.id = QnUuid::fromStringSafe(host);

			urlPath = urlPath.mid(hostEndPos);

			// get dst ip and port
			QStringList hostAndPort = host.split(':');
			int port = hostAndPort.size() > 1 ? hostAndPort[1].toInt() : getDefaultPortByProtocol(protocol);

			dstUrl = nx::utils::Url(lit("%1://%2:%3").arg(protocol).arg(hostAndPort[0]).arg(port));
		}
		else {
			QString scheme = url.scheme();
			if (scheme.isEmpty())
				scheme = lit("http");

			int defaultPort = getDefaultPortByProtocol(scheme);
			dstUrl = nx::utils::Url(lit("%1://%2:%3").arg(scheme).arg(url.host()).arg(url.port(defaultPort)));
		}

		if (urlPath.isEmpty())
			urlPath = "/";

		auto updatedUrl = urlPath;
		if (!url.query().isEmpty())
			updatedUrl += lit("?") + url.query();
		if (!url.fragment().isEmpty())
			updatedUrl += lit("#") + url.fragment();

		d->request.requestLine.url = updatedUrl;
	}

	nx::network::http::HttpHeaders::const_iterator xCameraGuidIter = d->request.headers.find( Qn::CAMERA_GUID_HEADER_NAME );
	QnUuid cameraGuid;
	if( xCameraGuidIter != d->request.headers.end() )
		cameraGuid = QnUuid::fromStringSafe(xCameraGuidIter->second);
	else
		cameraGuid = QnUuid::fromStringSafe(d->request.getCookieValue(Qn::CAMERA_GUID_HEADER_NAME));
	if (!cameraGuid.isNull()) {
		if (QnResourcePtr camera = resourcePool()->getResourceById(cameraGuid))
			dstRoute.id = camera->getParentId();
	}

	const auto itr = d->request.headers.find(Qn::SERVER_GUID_HEADER_NAME);
	if (itr != d->request.headers.end())
		dstRoute.id = QnUuid::fromStringSafe(itr->second);
	else if (!dstRoute.id.isNull())
		d->request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, dstRoute.id.toByteArray());
    else
        dstRoute.id = commonModule()->moduleGUID();

    if (!dstRoute.id.isNull()) //< Means dstUrl targets another server in the system.
        fixServerUrlSchemeSecurity(&dstUrl, commonModule()->globalSettings());

	if (dstRoute.id == commonModule()->moduleGUID())
	{
		if (!cameraGuid.isNull())
		{
			cleanupProxyInfo(&d->request);
			if (QnNetworkResourcePtr camera = resourcePool()->getResourceById<QnNetworkResource>(cameraGuid))
				dstRoute.addr = nx::network::SocketAddress(camera->getHostAddress(), camera->httpPort());
		}
		else if (isStandardProxyNeeded(commonModule(), d->request))
		{
			nx::utils::Url url = d->request.requestLine.url;
			int defaultPort = getDefaultPortByProtocol(dstUrl.scheme());
			dstRoute.addr = nx::network::SocketAddress(dstUrl.host(), dstUrl.port(defaultPort));
		}
		else
		{
			//proxying to ourself
			dstRoute.addr = nx::network::SocketAddress(nx::network::HostAddress::localhost, d->socket->getLocalAddress().port);
		}
	}
	else if (!dstRoute.id.isNull())
	{
		dstRoute = commonModule()->router()->routeTo(dstRoute.id);
	}
	else
	{
		// TODO: Add special case for RTSP?
		const auto defaultPort = nx::network::http::defaultPortForScheme(dstUrl.scheme().toUtf8());
		dstRoute.addr = nx::network::SocketAddress(dstUrl.host(), dstUrl.port(defaultPort));

		// No dst route Id means proxy to external resource.
		// All proxy hops have already been passed. Remove proxy-auth header.
		cleanupProxyInfo(&d->request);
	}

	if (!dstRoute.id.isNull() && dstRoute.id != commonModule()->moduleGUID())
	{
		if (!replaceAuthHeader())
			return false;
	}

	if (!dstRoute.addr.isNull())
	{
		if (!dstRoute.gatewayId.isNull())
		{
			nx::network::http::StringType ttlString = nx::network::http::getHeaderValue(d->request.headers, Qn::PROXY_TTL_HEADER_NAME);
			bool ok;
			int ttl = ttlString.toInt(&ok);
			if (!ok)
				ttl = kMaxProxyTtl;
			--ttl;

			if (ttl <= 0)
				return false;

			nx::network::http::insertOrReplaceHeader(&d->request.headers, nx::network::http::HttpHeader(Qn::PROXY_TTL_HEADER_NAME, QByteArray::number(ttl)));
			nx::network::http::StringType existAuthSession = nx::network::http::getHeaderValue(d->request.headers, Qn::AUTH_SESSION_HEADER_NAME);
			if (existAuthSession.isEmpty())
				nx::network::http::insertOrReplaceHeader(&d->request.headers, nx::network::http::HttpHeader(Qn::AUTH_SESSION_HEADER_NAME, authSession().toByteArray()));
		}
		dstUrl.setHost(dstRoute.addr.address.toString());
		dstUrl.setPort(dstRoute.addr.port);

		//adding entry corresponding to current server to Via header
		nx::network::http::header::Via via;
		auto viaHeaderIter = d->request.headers.find( "Via" );
		if( viaHeaderIter != d->request.headers.end() )
			via.parse( viaHeaderIter->second );

		nx::network::http::header::Via::ProxyEntry proxyEntry;
		proxyEntry.protoVersion = d->request.requestLine.version.version;
		proxyEntry.receivedBy = commonModule()->moduleGUID().toByteArray();
		via.entries.push_back( proxyEntry );
		nx::network::http::insertOrReplaceHeader(
			&d->request.headers,
			nx::network::http::HttpHeader( "Via", via.toString() ) );
	}

    if (!dstUrl.host().isEmpty())
    {
        // Destination host may be empty in case of reverse connection.
        auto hostIter = d->request.headers.find("Host");
        if (hostIter != d->request.headers.end())
        {
            int defaultPort = getDefaultPortByProtocol(dstUrl.scheme());
            hostIter->second = nx::network::SocketAddress(
                dstUrl.host(),
                dstUrl.port(defaultPort)).toString().toLatin1();
        }
    }

	//NOTE next hop should accept Authorization header already present
	//  in request since we use current time as nonce value

	d->clientRequest.clear();
	d->request.serialize(&d->clientRequest);

	return true;
}

bool ProxyConnectionProcessor::openProxyDstConnection()
{
	Q_D(ProxyConnectionProcessor);

	d->dstSocket.reset();

	// update source request
	nx::utils::Url dstUrl;
	QnRoute dstRoute;
	if (!updateClientRequest(dstUrl, dstRoute))
	{
		NX_VERBOSE(this, lm("Failed to find destination url"));
		d->socket->close();
		return false;
	}

	if (dstRoute.id.isNull() && d->accessRights.userId.isNull())
		return false; //< Do not allow direct IP connect for unauthorized users.

	NX_VERBOSE(this, lm("Found destination url %1").args(dstUrl));

	d->lastConnectedEndpoint = connectToRemoteHost(dstRoute, dstUrl);
	if (d->lastConnectedEndpoint.isEmpty())
	{
		NX_VERBOSE(this, lm("Failed to open connection to the target %1").args(dstUrl));
		return false; // invalid dst address
	}

	d->dstSocket->send(d->clientRequest.data(), d->clientRequest.size());

	return true;
}

void ProxyConnectionProcessor::run()
{
	Q_D(ProxyConnectionProcessor);

	d->socket->setRecvTimeout(kIoTimeout);
	d->socket->setSendTimeout(kIoTimeout);

	if (d->clientRequest.isEmpty()) {
		d->socket->close();
		return; // no input data
	}

	parseRequest();

	NX_VERBOSE(this, lm("Proxying request to %1").args(d->request.requestLine.url));

	if (!openProxyDstConnection())
		return;

	bool isWebSocket = nx::network::http::getHeaderValue( d->request.headers, "Upgrade").toLower() == lit("websocket");
	if (!isWebSocket && nx::network::http::isUrlSheme(d->protocol.toLower()))
	{
		NX_VERBOSE(this, lm("Smart proxy for %1").arg(d->request.requestLine));
		doSmartProxy();
	}
	else
	{
		NX_VERBOSE(this, lm("Raw proxy for %1").arg(d->request.requestLine));
		doRawProxy();
	}

	if (d->dstSocket)
		d->dstSocket->close();
	if (d->socket)
		d->socket->close();
}

void ProxyConnectionProcessor::doRawProxy()
{
	NX_VERBOSE(this, lm("Performing RAW proxy"));

	Q_D(ProxyConnectionProcessor);
	nx::Buffer buffer(kReadBufferSize, Qt::Uninitialized);

	// NOTE: Poll set would be more effective than a busy loop, but we can not use it because
	//     SSL sockets do not support it properly.
	while (!m_needStop)
	{
		const auto timeSinseLastIo = std::chrono::steady_clock::now() - d->lastIoTimePoint;
		if (m_needStop || timeSinseLastIo >= kIoTimeout)
			return;

		bool someBytesRead1 = false;
		bool someBytesRead2 = false;
		if (!doProxyData(d->socket.get(), d->dstSocket.get(), buffer.data(), buffer.size(), &someBytesRead1))
			return;

		if (!doProxyData(d->dstSocket.get(), d->socket.get(), buffer.data(), buffer.size(), &someBytesRead2))
			return;

		if (!someBytesRead1 && !someBytesRead2)
			std::this_thread::sleep_for(kPollTimeout);
	}
}

void ProxyConnectionProcessor::doProxyRequest()
{
	Q_D(ProxyConnectionProcessor);

	parseRequest();
	QString path = d->request.requestLine.url.path();
	// Parse next request and change dst if required.
	nx::utils::Url dstUrl;
	QnRoute dstRoute;
	updateClientRequest(dstUrl, dstRoute);
	bool isWebSocket = nx::network::http::getHeaderValue(d->request.headers, "Upgrade").toLower() == lit("websocket");
	bool isSameAddr = d->lastConnectedEndpoint == dstRoute.addr.toString() || d->lastConnectedEndpoint == dstUrl
		|| (dstRoute.reverseConnect && d->lastConnectedEndpoint == dstRoute.id.toByteArray());
	if (!isSameAddr)
	{
		// new server
		d->lastConnectedEndpoint = connectToRemoteHost(dstRoute, dstUrl);
		if (d->lastConnectedEndpoint.isEmpty())
		{
			NX_VERBOSE(this, lm("Failed to connect to destination %1 during \"smart\" proxying")
				.args(dstUrl));
			d->socket->close();
			return; // invalid dst address
		}
	}

	d->dstSocket->send(d->clientRequest);
	if (isWebSocket)
	{
		doRawProxy(); // switch to binary mode
		m_needStop = true;
	}
	d->clientRequest.clear();
}

void ProxyConnectionProcessor::doSmartProxy()
{
	NX_VERBOSE(this, lm("Performing \"smart\" proxy"));

	Q_D(ProxyConnectionProcessor);
	nx::Buffer buffer(kReadBufferSize, Qt::Uninitialized);
	d->clientRequest.clear();

	// NOTE: Poll set would be more effective than a busy loop, but we can not use it because
	//     SSL sockets do not support it properly.
	while (!m_needStop)
	{
		const auto timeSinseLastIo = std::chrono::steady_clock::now() - d->lastIoTimePoint;
		if (m_needStop || timeSinseLastIo >= kIoTimeout)
			return;

		bool someBytesRead1 = false;
		int readed;
		if (readSocketNonBlock(&readed, d->socket.get(), d->tcpReadBuffer, TCP_READ_BUFFER_SIZE))
		{
			if (readed < 1)
			{
				const auto systemErrorCode = SystemError::getLastOSErrorCode();
				NX_VERBOSE(this, lm("Proxying finished with system result code %1")
					.args(SystemError::toString(systemErrorCode)));
				return;
			}
			someBytesRead1 = true;

			d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
			const auto messageSize = isFullMessage(d->clientRequest);
			if (messageSize < 0)
				return;

			if (messageSize > 0)
				doProxyRequest();
			if (m_needStop)
				break;
		}

		bool someBytesRead2 = false;
		if (!doProxyData(d->dstSocket.get(), d->socket.get(), buffer.data(), kReadBufferSize, &someBytesRead2))
			return;

		if (!someBytesRead1 && !someBytesRead2)
			std::this_thread::sleep_for(kPollTimeout);
	}
}

bool ProxyConnectionProcessor::readSocketNonBlock(
	int* returnValue, nx::network::AbstractStreamSocket* socket, void* buffer, int bufferSize)
{
	*returnValue = socket->recv(buffer, bufferSize, MSG_DONTWAIT);
	if (*returnValue < 0)
	{
		auto code = SystemError::getLastOSErrorCode();
		if (code == SystemError::interrupted ||
			code == SystemError::wouldBlock ||
			code == SystemError::again)
		{
			return false;
		}
	}

	Q_D(ProxyConnectionProcessor);
	d->lastIoTimePoint = std::chrono::steady_clock::now();
	return true;
}

bool ProxyConnectionProcessor::isProxyNeeded(QnCommonModule* commonModule, const nx::network::http::Request& request)
{
	nx::network::http::HttpHeaders::const_iterator xServerGuidIter = request.headers.find(Qn::SERVER_GUID_HEADER_NAME);
	if (xServerGuidIter != request.headers.end())
	{
		// is proxy to other media server
		QnUuid desiredServerGuid(xServerGuidIter->second);
		if (desiredServerGuid != commonModule->moduleGUID())
		{
			NX_VERBOSE(typeid(ProxyConnectionProcessor),
				lm("Need proxy to another server for request [%1]").arg(request.requestLine));

			return true;
		}
	}
	return nx::vms::network::ProxyConnectionProcessor::isStandardProxyNeeded(commonModule, request);
}

} // namespace network
} // namespace vms
} // namespace nx

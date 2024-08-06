#include "rtsp_restreamer.h"

#include <core/resource/camera_resource.h>
#include <core/resource/network_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/login.h>

using namespace nx;

namespace {

CameraDiagnostics::Result probeStreamWithClient(
    const nx::utils::Url& url,
    std::chrono::seconds timeout,
    bool useCloud,
    const std::optional<nx::network::http::Credentials>& credentials,
    std::optional<nx::network::http::header::AuthScheme::Value> authScheme)
{
    QnRtspClient rtspClient(QnRtspClient::Config{});
    rtspClient.setTCPTimeout(timeout);
    if (useCloud)
        rtspClient.setCloudConnectEnabled(true);
    if (credentials && authScheme)
        rtspClient.setCredentials(*credentials, *authScheme);

    return rtspClient.open(url);
}

CameraDiagnostics::Result probeStreamWithTcpConnect(
    const nx::utils::Url& url,
    std::chrono::seconds timeout,
    bool useCloud)
{
    using namespace nx::network;
    bool isSslRequired =
        (url.scheme().toLower().toUtf8() == rtsp::kSecureUrlSchemeName);

    auto tcpSocket = SocketFactory::createStreamSocket(
        nx::network::ssl::kAcceptAnyCertificate,
        isSslRequired,
        useCloud ? NatTraversalSupport::enabled : NatTraversalSupport::disabled);
    SocketAddress targetAddress = url::getEndpoint(url, rtsp::DEFAULT_RTSP_PORT);
    if (!tcpSocket->connect(targetAddress, std::chrono::duration_cast<std::chrono::milliseconds>(timeout)))
        return CameraDiagnostics::CannotOpenCameraMediaPortResult(url, targetAddress.port);
    return CameraDiagnostics::NoErrorResult();
}

} // namespace

namespace nx::streaming {

RtspRestreamer::RtspRestreamer(
    const QnVirtualCameraResourcePtr& device,
    const nx::streaming::rtp::TimeOffsetPtr& timeOffset)
    :
    m_reader(device, timeOffset),
    m_camera(device)
{
}

RtspRestreamer::~RtspRestreamer()
{
}

const std::string& RtspRestreamer::cloudAddressTemplate()
{
    // {server_id}.{system_id}.relay.cloud.host
    static const std::string kCloudAddressTemplate =
        "(.+\\.)?([0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12})\\.(.+)*";
    return kCloudAddressTemplate;
}

bool RtspRestreamer::tryRewriteRequest(nx::utils::Url& url, bool force)
{
    bool useCloud = nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());
    if (!useCloud && !force)
        return false;
    if (!useCloud)
    {
        // If isn't in cloud format already, need to check if we can rewrite it into a cloud format.
        std::string host = url.host().toStdString();

        std::smatch match;
        try
        {
            std::regex cloudAddressRegex(
                cloudAddressTemplate(),
                std::regex_constants::ECMAScript | std::regex_constants::icase);
            if (!std::regex_match(
                host,
                match,
                cloudAddressRegex))
            {
                return false;
            }
        }
        catch (const std::regex_error& error)
        {
            NX_WARNING(
                NX_SCOPE_TAG,
                "Failed to match host %1 with pattern %2: error: %3",
                host,
                cloudAddressTemplate(),
                error.what());
            return false;
        }
        NX_ASSERT(match.size() == 4);

        url.setHost(match[2]);
    }
    if (url.port() < 0)
        url.setPort(443); //< Hope it wouldn't be changed.
    // Token auth can work only with secure connection.
    if (url.scheme().toLower() == nx::network::rtsp::kUrlSchemeName)
        url.setScheme(nx::network::rtsp::kSecureUrlSchemeName);
    return true;
}

void RtspRestreamer::setRequest(const QString& request)
{
    nx::utils::Url url(request);
    m_useCloud = tryRewriteRequest(url, false);
    m_url = url;

    m_reader.setRequest(url.toString());
}

QnAbstractMediaDataPtr RtspRestreamer::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr();
    return m_reader.getNextData();
}

bool RtspRestreamer::probeStream(
    const char* address,
    const char* login,
    const char* password,
    std::chrono::seconds timeout,
    bool fast)
{
    nx::utils::Url url(address);
    bool force = false;

    while (true)
    {
        bool useCloud = tryRewriteRequest(url, force);

        std::string token, fingerprint;
        if (useCloud)
        {
            auto result = requestToken(
                url, std::string(login), std::string(password), token, fingerprint);
            if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
                return false;
            NX_ASSERT(!token.empty());

        }
        std::optional<nx::network::http::Credentials> credentials;
        std::optional<nx::network::http::header::AuthScheme::Value> authScheme;
        if (useCloud)
        {
            credentials = nx::network::http::BearerAuthToken(token);
            authScheme = nx::network::http::header::AuthScheme::bearer;
        }
        else if (login && password)
        {
            QAuthenticator auth;
            auth.setUser(QString::fromUtf8(login));
            auth.setPassword(QString::fromUtf8(password));
            credentials = nx::network::http::Credentials(auth);
            authScheme = nx::network::http::header::AuthScheme::digest;
        }

        auto result = fast
            ? probeStreamWithTcpConnect(url, timeout, useCloud)
            : probeStreamWithClient(url, timeout, useCloud, credentials, authScheme);
        if (result.errorCode == CameraDiagnostics::ErrorCode::noError)
            return true;
        else if (result.errorCode != CameraDiagnostics::ErrorCode::unsupportedProtocol)
            return false;
        else if (force || useCloud)
            break; //< Already tried to connect with Cloud, break cycle.

        force = true;
    }
    return false;
}

CameraDiagnostics::Result RtspRestreamer::requestToken(
    const nx::utils::Url& rtspUrl,
    const std::string& username,
    const std::string& password,
    std::string& token,
    [[maybe_unused]] std::string& fingerprint)
{
    using namespace nx::network;
    // TODO Use own callback and fill fingerprint.
    http::HttpClient httpClient{ssl::kAcceptAnyCertificate};
    const auto requestData = QJson::serialized(
        nx::vms::api::LoginSessionRequest{
            .username = QString::fromStdString(username),
            .password = QString::fromStdString(password)});

    nx::utils::Url url;
    url.setScheme(http::kSecureUrlSchemeName); //< https.
    url.setHost(rtspUrl.host());
    url.setPort(rtspUrl.port());
    url.setPath("/rest/v4/login/sessions");
    if (!httpClient.doPost(url, http::header::ContentType::kJson, requestData))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to fetch session token for url %1", url.toString());
        return CameraDiagnostics::MediaServerUnavailableResult(url.toString());
    }

    const http::Response* response = httpClient.response();
    if (!response || response->statusLine.statusCode != http::StatusCode::ok)
    {
        NX_DEBUG(
            NX_SCOPE_TAG,
            "Unexpected http code %1 for url %2",
            response->statusLine.statusCode,
            url);
        return CameraDiagnostics::NotAuthorisedResult(url);
    }

    const auto body = httpClient.fetchEntireMessageBody().value_or(QByteArray());
    token = QJson::deserialized<nx::vms::api::LoginSession>(body).token;
    if (token.empty())
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Unexpected empty token after login for url %1",
            url.toString());
        return CameraDiagnostics::CameraResponseParseErrorResult(url, url.path());
    }
    NX_VERBOSE(
        NX_SCOPE_TAG,
        "Successfully set token credentionals for request %1",
        url.toString());
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result RtspRestreamer::openStream()
{
    m_openStreamResult = CameraDiagnostics::NoErrorResult();
    bool force = false;
    while (true)
    {
        if (force)
        {
            m_useCloud = tryRewriteRequest(m_url, true);
            if (m_useCloud)
                m_reader.setRequest(m_url.toString());
        }
        if (m_useCloud)
        {
            m_reader.setCloudConnectEnabled(true);

            const auto auth = m_camera->getAuth();
            std::string token, fingerprint;
            m_openStreamResult = requestToken(
                m_url, auth.user().toStdString(), auth.password().toStdString(), token, fingerprint);

            if (m_openStreamResult.errorCode != CameraDiagnostics::ErrorCode::noError)
                return m_openStreamResult;
            NX_ASSERT(!token.empty());

            m_reader.setPreferredAuthScheme(nx::network::http::header::AuthScheme::bearer);
            m_reader.setCredentials(nx::network::http::BearerAuthToken(token));
        }

        auto result = m_reader.openStream();
        if (result.errorCode == CameraDiagnostics::ErrorCode::noError)
            return result;
        else if (result.errorCode != CameraDiagnostics::ErrorCode::unsupportedProtocol)
            return result;
        else if (force || m_useCloud)
            return result; //< Already tried to connect with Cloud, break cycle.
        force = true;
    }
}

void RtspRestreamer::closeStream()
{
    m_reader.closeStream();
}

bool RtspRestreamer::isStreamOpened() const
{
    return m_reader.isStreamOpened();
}

CameraDiagnostics::Result RtspRestreamer::lastOpenStreamResult() const
{
    if (m_openStreamResult.errorCode == CameraDiagnostics::ErrorCode::noError)
        return m_reader.lastOpenStreamResult();
    return m_openStreamResult;
}

QnConstResourceVideoLayoutPtr RtspRestreamer::getVideoLayout() const
{
    return m_reader.getVideoLayout();
}

AudioLayoutConstPtr RtspRestreamer::getAudioLayout() const
{
    return m_reader.getAudioLayout();
}

void RtspRestreamer::setRole(Qn::ConnectionRole role)
{
    m_reader.setRole(role);
}

nx::utils::Url RtspRestreamer::getCurrentStreamUrl() const
{
    return m_reader.getCurrentStreamUrl();
}

void RtspRestreamer::pleaseStop()
{
    m_reader.pleaseStop();
}

} // namespace nx::vms::server

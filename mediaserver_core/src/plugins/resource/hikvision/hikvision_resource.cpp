#ifdef ENABLE_ONVIF

#include "hikvision_resource.h"
#include "hikvision_audio_transmitter.h"
#include "hikvision_utils.h"
#include "hikvision_hevc_stream_reader.h"

#include <QtXml/QDomElement>

#include <boost/optional.hpp>
#include <utils/camera/camera_diagnostics.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <onvif/soapMediaBindingProxy.h>
#include <nx/utils/log/log.h>
#include <nx/network/url/url_builder.h>

namespace {

const std::chrono::milliseconds kRequestTimeout(4000);

bool isResponseOK(const nx_http::HttpClient* client)
{
    if (!client->response())
        return false;
    return client->response()->statusLine.statusCode == nx_http::StatusCode::ok;
}

const std::array<Qn::ConnectionRole, 2> kRoles = {
    Qn::ConnectionRole::CR_LiveVideo,
    Qn::ConnectionRole::CR_SecondaryLiveVideo};

} // namespace

namespace nx {
namespace mediaserver_core {
namespace plugins {

using namespace nx::mediaserver_core::plugins::hikvision;

HikvisionResource::HikvisionResource():
    QnPlOnvifResource()
{
    m_audioTransmitter.reset(new HikvisionAudioTransmitter(this));
}

HikvisionResource::~HikvisionResource()
{
    m_audioTransmitter.reset();
}

CameraDiagnostics::Result HikvisionResource::initInternal()
{
    tryToEnableOnvifSupport(getDeviceOnvifUrl(), getAuth());

    auto result = QnPlOnvifResource::initInternal();
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
        return result;

    initialize2WayAudio();
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

QnAbstractStreamDataProvider* HikvisionResource::createLiveDataProvider()
{
    if (m_hevcSupported)
        return new plugins::HikvisionHevcStreamReader(toSharedPointer(this));
    
    return base_type::createLiveDataProvider();
}

CameraDiagnostics::Result HikvisionResource::initializeMedia(
    const CapabilitiesResp& onvifCapabilities)
{
    auto resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool hevcIsDisabled = resourceData.value<bool>(Qn::DISABLE_HEVC_PARAMETER_NAME, false);

    if (!hevcIsDisabled)
    {
        for (const auto& role : kRoles)
        {
            hikvision::ChannelCapabilities channelCapabilities;
            auto result = fetchChannelCapabilities(role, &channelCapabilities);
            if (!result)
                return result;

            m_channelCapabilitiesByRole[role] = channelCapabilities;
            m_hevcSupported = hikvision::codecSupported(
                AV_CODEC_ID_HEVC,
                channelCapabilities);
            if (m_hevcSupported && role == Qn::ConnectionRole::CR_LiveVideo)
            {
                setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, 1);
                if (!channelCapabilities.resolutions.empty())
                {
                    const auto primaryResolution = channelCapabilities.resolutions.front();
                    setPrimaryResolution(primaryResolution);
                    if (qFuzzyIsNull(customAspectRatio()))
                        setCustomAspectRatio(primaryResolution.width() / (qreal)primaryResolution.height());
                }
                if (!channelCapabilities.fps.empty())
                    setMaxFps(channelCapabilities.fps[0] / 100);
            }
        }
    }

    if (!m_hevcSupported)
        return base_type::initializeMedia(onvifCapabilities);

    return CameraDiagnostics::NoErrorResult();
}

static std::unique_ptr<nx_http::HttpClient> makeHttpClient(const QAuthenticator& authenticator)
{
    std::unique_ptr<nx_http::HttpClient> client(new nx_http::HttpClient);
    client->setResponseReadTimeoutMs((unsigned int) kRequestTimeout.count());
    client->setSendTimeoutMs((unsigned int) kRequestTimeout.count());
    client->setMessageBodyReadTimeoutMs((unsigned int) kRequestTimeout.count());
    client->setUserName(authenticator.user());
    client->setUserPassword(authenticator.password());
    return client;
}

std::unique_ptr<nx_http::HttpClient> HikvisionResource::getHttpClient()
{
    return makeHttpClient(getAuth());
}

CameraDiagnostics::Result HikvisionResource::fetchChannelCapabilities(
    Qn::ConnectionRole role,
    ChannelCapabilities* outCapabilities)
{
    auto url = QUrl(getUrl());
    url.setPath(kCapabilitiesRequestPathTemplate.arg(
        buildChannelNumber(role, getChannel())));

    nx::Buffer response;
    nx_http::StatusCode::Value statusCode;
    if (!doGetRequest(url, getAuth(), &response, &statusCode))
    {
        if (statusCode == nx_http::StatusCode::Value::unauthorized)
            return CameraDiagnostics::NotAuthorisedResult(url.toString());

        return CameraDiagnostics::RequestFailedResult(
            lit("Fetch channel capabilities"),
            lit("Request failed"));
    }

    if (!parseChannelCapabilitiesResponse(response, outCapabilities))
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            url.toString(),
            lit("Fetch camera capabilities"));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HikvisionResource::initialize2WayAudio()
{
    auto httpClient = getHttpClient();

    QUrl requestUrl(getUrl());
    requestUrl.setPath(lit("/ISAPI/System/TwoWayAudio/channels"));
    requestUrl.setHost(getHostAddress());
    requestUrl.setPort(QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT));

    if (!httpClient->doGet(requestUrl) || !isResponseOK(httpClient.get()))
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            requestUrl.toString(QUrl::RemovePassword),
            lit("Read two way audio info"));
    }

    QByteArray data;
    while (!httpClient->eof())
        data.append(httpClient->fetchMessageBodyBuffer());

    if (data.isEmpty())
        return CameraDiagnostics::NoErrorResult(); //< no 2-way-audio cap

    auto channels = parseAvailableChannelsResponse(data);

    if (channels.empty())
        return CameraDiagnostics::NoErrorResult(); //< no 2-way-audio cap

    boost::optional<ChannelStatusResponse> channel(boost::none);
    for (const auto& ch: channels)
    {
        if (!ch.id.isEmpty() && !ch.audioCompression.isEmpty())
        {
            channel = ch;
            break;
        }
    }

    if (!channel)
        return CameraDiagnostics::NoErrorResult();

    QnAudioFormat outputFormat = toAudioFormat(
        channel->audioCompression,
        channel->sampleRateKHz);

    if (m_audioTransmitter->isCompatible(outputFormat))
    {
        m_audioTransmitter->setOutputFormat(outputFormat);

        auto hikTransmitter = std::dynamic_pointer_cast<HikvisionAudioTransmitter>(
            m_audioTransmitter);

        if (hikTransmitter)
        {
            hikTransmitter->setChannelId(channel->id);
            hikTransmitter->setAudioUploadHttpMethod(nx_http::Method::put);
        }

        setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);
    }

    return CameraDiagnostics::NoErrorResult();
}

boost::optional<ChannelCapabilities> HikvisionResource::channelCapabilities(
    Qn::ConnectionRole role)
{
    auto itr = m_channelCapabilitiesByRole.find(role);
    if (itr == m_channelCapabilitiesByRole.end())
        return boost::none;

    return itr->second;
}

bool HikvisionResource::findDefaultPtzProfileToken()
{
    std::unique_ptr<MediaSoapWrapper> soapWrapper(new MediaSoapWrapper(
        getMediaUrl().toStdString(),
        getAuth().user(),
        getAuth().password(),
        getTimeDrift()));

    ProfilesReq request;
    ProfilesResp response;
    int soapRes = soapWrapper->getProfiles(request, response);
    if (soapRes != SOAP_OK)
    {
        NX_WARNING(this, lm("Can't read profile list from device %1").arg(getMediaUrl()));
        return false;
    }

    for (const auto& profile : response.Profiles)
    {
        if (!profile || !profile->PTZConfiguration)
            continue;
        QString ptzConfiguration = QString::fromStdString(profile->PTZConfiguration->token);
        if (ptzConfiguration == getPtzConfigurationToken())
        {
            setPtzProfileToken(QString::fromStdString(profile->token));
            return true;
        }
    }
    return false;
}

static const auto kIntegratePath = lit("/ISAPI/System/Network/Integrate");
static const auto kOnvifTag = lit("ONVIF");
static const auto kEnableTag = lit("enable");
static const auto kEnableOnvifXml = QString::fromUtf8(R"xml(
<?xml version:"1.0" encoding="UTF-8"?>
<Integrate>
    <ONVIF><enable>true</enable><certificateType/></ONVIF>
</Integrate>
)xml").trimmed();

static const auto kSetOnvifUserPath = lit("/ISAPI/Security/ONVIF/users/1");
static const auto kSetOnvifUserXml = QString::fromUtf8(R"xml(
<?xml version:"1.0" encoding="UTF-8"?>
<User>
    <id>1</id>
    <userName>%1</userName>
    <password>%2</password>
    <userType>administrator</userType>
</User>
)xml").trimmed();

bool HikvisionResource::tryToEnableOnvifSupport(const QUrl& url, const QAuthenticator& authenticator)
{
    const auto client = makeHttpClient(authenticator);
    if (!client->doGet(nx::network::url::Builder(url).setPath(kIntegratePath))
        || !isResponseOK(client.get()))
    {
        NX_VERBOSE(typeid(HikvisionResource), lm("Unable to send request %2").arg(client->url()));
        return false;
    }

    const auto responseBody = client->fetchEntireMessageBody();
    QDomDocument intergrateDocument;
    QString errorMessage;
    if (!intergrateDocument.setContent(responseBody, &errorMessage))
    {
        NX_VERBOSE(typeid(HikvisionResource), lm("Unable to parse response from %1: %2")
            .args(client->url(), errorMessage));
        return false;
    }

    const auto onvifElement = intergrateDocument.documentElement().firstChildElement(kOnvifTag);
    if (onvifElement.isNull())
    {
        NX_DEBUG(typeid(HikvisionResource), lm("Tag %1 is not found on %2")
            .args(kOnvifTag, client->url()));
        return false;
    }

    const auto enabledElement = onvifElement.firstChildElement(kEnableTag);
    if (onvifElement.isNull())
    {
        NX_DEBUG(typeid(HikvisionResource), lm("Tag %1 is not found on %2")
            .args(kOnvifTag, client->url()));
        return false;
    }

    const auto onvifEnabled = enabledElement.text();
    if (onvifEnabled == lit("false"))
    {
        // Enable ONVIF support, usernameand and password should be set automatically.
        const auto result = client->doPut(
            nx::network::url::Builder(url).setPath(kIntegratePath), "application/xml",
            kEnableOnvifXml.toUtf8())
                && isResponseOK(client.get());

        onvifEnabled == lit("true");
        NX_DEBUG(typeid(HikvisionResource), lm("Enable ONVIF result=%1 on %2")
            .args(result, client->url()));
    }
    else
    {
        NX_VERBOSE(typeid(HikvisionResource), lm("ONVIF is already enabled on %1")
            .args(client->url()));
    }

    if (onvifEnabled == lit("true"))
    {
        // Make sure onvif user/password is the same as main credentials.
        const auto result = client->doPut(
            nx::network::url::Builder(url).setPath(kSetOnvifUserPath), "application/xml",
            kSetOnvifUserXml.arg(authenticator.user(), authenticator.password()).toUtf8())
                && isResponseOK(client.get());

        NX_DEBUG(typeid(HikvisionResource), lm("Set ONVIF credentials result=%1 on %2")
            .args(result, client->url()));
        return result;
    }

    NX_DEBUG(typeid(HikvisionResource), lm("Unexpected ONVIF value: %1").arg(onvifEnabled));
    return false;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif  //ENABLE_ONVIF

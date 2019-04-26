#ifdef ENABLE_ONVIF

#include <algorithm>

#include "hikvision_audio_transmitter.h"
#include "hikvision_hevc_stream_reader.h"
#include "hikvision_ptz_controller.h"
#include "hikvision_request_helper.h"
#include "hikvision_resource.h"
#include "hikvision_utils.h"

#include <QtXml/QDomElement>

#include <boost/optional.hpp>
#include <utils/camera/camera_diagnostics.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <onvif/soapMediaBindingProxy.h>
#include <nx/utils/log/log.h>
#include <utils/media/av_codec_helper.h>

namespace {

const std::array<Qn::ConnectionRole, 2> kRoles =
{
    Qn::ConnectionRole::CR_LiveVideo,
    Qn::ConnectionRole::CR_SecondaryLiveVideo
};

static const nx::utils::log::Tag kHikvisionApiLogTag(QString("hikvision_api_protocols"));

} // namespace

namespace nx {
namespace vms::server {
namespace plugins {

using namespace nx::vms::server::plugins::hikvision;
using namespace nx::plugins::utils;

HikvisionResource::HikvisionResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule)
{
}

HikvisionResource::~HikvisionResource()
{
}

QString HikvisionResource::defaultCodec() const
{
    return QnAvCodecHelper::codecIdToString(AV_CODEC_ID_H265);
}

nx::vms::server::resource::StreamCapabilityMap HikvisionResource::getStreamCapabilityMapFromDriver(
    StreamIndex streamIndex)
{
    QnMutexLocker lock(&m_mutex);
    const auto capabilities = channelCapabilities(toConnectionRole(streamIndex));
    if (!capabilities)
        return base_type::getStreamCapabilityMapFromDriver(streamIndex);

    nx::vms::server::resource::StreamCapabilityMap result;
    for (const auto& codec: capabilities->codecs)
    {
        for (const auto& resolution: capabilities->resolutions)
        {
            auto& capability = result[{QnAvCodecHelper::codecIdToString(codec), resolution}];
            capability.minBitrateKbps = capabilities->bitrateRange.first;
            capability.maxBitrateKbps = capabilities->bitrateRange.second;

            const auto maxFps = capabilities->realMaxFps();
        }
    }

    return result;
}

CameraDiagnostics::Result HikvisionResource::initializeCameraDriver()
{
    m_integrationProtocols = tryToEnableIntegrationProtocols(
        getUrl(),
        getAuth(),
        /*isAdditionalSupportCheckNeeded*/ true);

    const auto result = QnPlOnvifResource::initializeCameraDriver();

    if (m_integrationProtocols[Protocol::isapi].enabled)
    {
        // We don't support multicast streaming for cameras integrated via ISAPI now.
        setCameraCapability(Qn::MulticastStreamCapability, false);
        saveProperties();
    }

    return result;
}

QnAbstractStreamDataProvider* HikvisionResource::createLiveDataProvider()
{
    if (m_integrationProtocols[Protocol::isapi].enabled)
        return new plugins::HikvisionHevcStreamReader(toSharedPointer(this));

    return base_type::createLiveDataProvider();
}

QnAbstractPtzController* HikvisionResource::createPtzControllerInternal() const
{
    if (resourceData().value<bool>(lit("useOnvifPtz"), false))
        return QnPlOnvifResource::createPtzControllerInternal();

    const auto isapi = m_integrationProtocols.find(Protocol::isapi);
    if (isapi != m_integrationProtocols.end() && isapi->second.enabled)
        return new hikvision::IsapiPtzController(toSharedPointer(this), getAuth());

    return base_type::createPtzControllerInternal();
}

CameraDiagnostics::Result HikvisionResource::initializeMedia(
    const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities)
{
    bool hevcIsDisabled = resourceData().value<bool>(ResourceDataKey::kDisableHevc, false);

    if (!hevcIsDisabled && m_integrationProtocols[Protocol::isapi].enabled)
    {
        for (const auto& role: kRoles)
        {
            hikvision::ChannelCapabilities channelCapabilities;
            auto result = fetchChannelCapabilities(role, &channelCapabilities);
            if (!result)
            {
                NX_DEBUG(this,
                    lm("Unable to fetch channel capabilities on %1 by ISAPI, fallback to ONVIF")
                    .args(getUrl()));

                return base_type::initializeMedia(onvifCapabilities);
            }

            m_channelCapabilitiesByRole[role] = channelCapabilities;
            m_hevcSupported = hikvision::codecSupported(AV_CODEC_ID_HEVC, channelCapabilities);
            if (m_hevcSupported)
            {
                if (role == Qn::ConnectionRole::CR_LiveVideo)
                {
                    setProperty(ResourcePropertyKey::kHasDualStreaming, 1);
                    if (!channelCapabilities.fpsInDeviceUnits.empty())
                        setMaxFps(channelCapabilities.realMaxFps());
                }
                if (!channelCapabilities.resolutions.empty())
                    setResolutionList(channelCapabilities, role);
            }
        }
    }
    if (m_hevcSupported)
    {
        fetchChannelCount();
        // Video properties has been read succesfully, time to read audio properties.
        fetchAndSetAudioSource();
        fetchAndSetAudioResourceOptions();

        m_audioTransmitter = initializeTwoWayAudio();
        if (m_audioTransmitter)
            setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);

        return CameraDiagnostics::NoErrorResult();
    }
    else
    {
        return base_type::initializeMedia(onvifCapabilities);
    }
}

void HikvisionResource::setResolutionList(
    const hikvision::ChannelCapabilities& channelCapabilities,
    Qn::ConnectionRole role)
{
    auto capabilities = role == Qn::CR_SecondaryLiveVideo
        ? secondaryVideoCapabilities() : primaryVideoCapabilities();
    capabilities.resolutions = QList<QSize>::fromVector(QVector<QSize>::fromStdVector(
        channelCapabilities.resolutions));
    capabilities.encoding = SupportedVideoEncoding::H264;
    if (role == Qn::CR_SecondaryLiveVideo)
        setSecondaryVideoCapabilities(capabilities);
    else
        setPrimaryVideoCapabilities(capabilities);
}

std::unique_ptr<nx::network::http::HttpClient> HikvisionResource::getHttpClient()
{
    return makeHttpClient(getAuth());
}

CameraDiagnostics::Result HikvisionResource::fetchChannelCapabilities(
    Qn::ConnectionRole role,
    ChannelCapabilities* outCapabilities)
{
    auto url = nx::utils::Url(getUrl());
    url.setPath(kCapabilitiesRequestPathTemplate.arg(
        buildChannelNumber(role, getChannel())));

    nx::Buffer response;
    nx::network::http::StatusCode::Value statusCode;
    if (!doGetRequest(url, getAuth(), &response, &statusCode))
    {
        if (statusCode == nx::network::http::StatusCode::Value::unauthorized)
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

QnAudioTransmitterPtr HikvisionResource::initializeTwoWayAudio()
{
    if (!m_integrationProtocols[Protocol::isapi].enabled)
        return QnPlOnvifResource::initializeTwoWayAudio();

    auto httpClient = getHttpClient();

    nx::utils::Url requestUrl(getUrl());
    requestUrl.setPath(lit("/ISAPI/System/TwoWayAudio/channels"));
    requestUrl.setHost(getHostAddress());
    requestUrl.setPort(nx::utils::Url(getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT));

    if (!httpClient->doGet(requestUrl) || !isResponseOK(httpClient.get()))
    {
        NX_WARNING(this, lm("Can't read two way audio info for camera %1").arg(getPhysicalId()));
        return QnAudioTransmitterPtr();
    }

    QByteArray data;
    while (!httpClient->eof())
        data.append(httpClient->fetchMessageBodyBuffer());

    if (data.isEmpty())
        return QnAudioTransmitterPtr(); //< no 2-way-audio cap

    auto channels = parseAvailableChannelsResponse(data);

    if (channels.empty())
        return QnAudioTransmitterPtr(); //< no 2-way-audio cap

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
        return QnAudioTransmitterPtr();

    QnAudioFormat outputFormat = toAudioFormat(
        channel->audioCompression,
        channel->sampleRateHz);

    auto audioTransmitter = std::make_shared<HikvisionAudioTransmitter>(this);
    if (audioTransmitter->isCompatible(outputFormat))
    {
        audioTransmitter->setOutputFormat(outputFormat);
        audioTransmitter->setChannelId(channel->id);
        audioTransmitter->setAudioUploadHttpMethod(nx::network::http::Method::put);
        return audioTransmitter;
    }
    return QnAudioTransmitterPtr();
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
    MediaSoapWrapper soapWrapper(this);
    ProfilesReq request;
    ProfilesResp response;
    int soapRes = soapWrapper.getProfiles(request, response);
    if (soapRes != SOAP_OK)
    {
        NX_WARNING(this, lm("Can't read profile list from device %1").arg(getMediaUrl()));
        return false;
    }

    for (const auto& profile : response.Profiles)
    {
        if (!profile || !profile->PTZConfiguration)
            continue;
        if (profile->PTZConfiguration->token == ptzConfigurationToken())
        {
            setPtzProfileToken(profile->token);
            return true;
        }
    }
    return false;
}

ProtocolStates HikvisionResource::tryToEnableIntegrationProtocols(
        const nx::utils::Url& url,
        const QAuthenticator& authenticator,
        bool isAdditionalSupportCheckNeeded)
{
    IsapiRequestHelper requestHelper(url, authenticator);
    auto supportedProtocols = requestHelper.fetchIntegrationProtocolInfo();
    for (auto& it: supportedProtocols)
        it.second.enabled = true;

    if (!supportedProtocols.empty())
    {
        if (!requestHelper.enableIntegrationProtocols(supportedProtocols))
        {
            NX_WARNING(
                kHikvisionApiLogTag,
                lm("Can't enable integration protocols, "
                    "maybe a device doesn't support 'Integrate' API call. "
                    "URL: %1").args(url));
        }
    }

    if (isAdditionalSupportCheckNeeded)
    {
        supportedProtocols[Protocol::isapi].enabled = requestHelper.checkIsapiSupport();
        supportedProtocols[Protocol::isapi].supported = supportedProtocols[Protocol::isapi].enabled;
    }

    const auto users = requestHelper.getOnvifUsers();
    if (!users)
    {
        supportedProtocols[Protocol::onvif].enabled = false;
        return supportedProtocols;
    }

    const auto existingUser = std::find_if(users->begin(), users->end(),
        [&](const std::pair<int, QString>& u) { return u.second == authenticator.user(); });

    const auto onvifCredentials = requestHelper.setOnvifCredentials(
        (existingUser != users->end())
            ? existingUser->first //< Override user permissions and password.
            : (users->empty() ? 1 : users->rbegin()->first + 1), //< New user.
        authenticator.user(), authenticator.password());

    if (!onvifCredentials)
        supportedProtocols[Protocol::onvif].enabled = false;

    return supportedProtocols;
}

CameraDiagnostics::Result HikvisionResource::fetchChannelCount(bool /*limitedByEncoders*/)
{
    return base_type::fetchChannelCount(/*limitedByEncoders*/ false);
}

} // namespace plugins
} // namespace vms::server
} // namespace nx

#endif  //ENABLE_ONVIF

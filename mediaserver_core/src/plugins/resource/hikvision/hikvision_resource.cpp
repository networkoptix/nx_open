#ifdef ENABLE_ONVIF

#include <algorithm>

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
#include <plugins/utils/xml_request_helper.h>
#include <utils/media/av_codec_helper.h>

namespace {

const std::array<Qn::ConnectionRole, 2> kRoles =
{
    Qn::ConnectionRole::CR_LiveVideo,
    Qn::ConnectionRole::CR_SecondaryLiveVideo
};

Qn::ConnectionRole toRole(Qn::StreamIndex streamIndex)
{
    return streamIndex == Qn::StreamIndex::primary ? Qn::CR_LiveVideo : Qn::CR_SecondaryLiveVideo;
}

static const nx::utils::log::Tag kHikvisionApiLogTag(QString("hikvision_api_protocols"));

} // namespace

namespace nx {
namespace mediaserver_core {
namespace plugins {

// TODO: Use enum class with serialization.
static const QString kOnvif = lit("ONVIF");
static const QString kIsapi = lit("ISAPI");
static const QString kCgi = lit("CGI");

using namespace nx::mediaserver_core::plugins::hikvision;
using namespace nx::plugins::utils;

HikvisionResource::HikvisionResource():
    QnPlOnvifResource()
{
}

HikvisionResource::~HikvisionResource()
{
}

QString HikvisionResource::defaultCodec() const
{
    return QnAvCodecHelper::codecIdToString(AV_CODEC_ID_H265);
}

nx::mediaserver::resource::StreamCapabilityMap HikvisionResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex streamIndex)
{
    QnMutexLocker lock(&m_mutex);
    const auto capabilities = channelCapabilities(toRole(streamIndex));
    if (!capabilities)
        return base_type::getStreamCapabilityMapFromDrives(streamIndex);

    nx::mediaserver::resource::StreamCapabilityMap result;
    for (const auto& codec: capabilities->codecs)
    {
        for (const auto& resolution: capabilities->resolutions)
        {
            auto& capability = result[{QnAvCodecHelper::codecIdToString(codec), resolution}];
            capability.minBitrateKbps = capabilities->bitrateRange.first;
            capability.maxBitrateKbps = capabilities->bitrateRange.second;

            const auto maxFps = std::max_element(capabilities->fps.begin(), capabilities->fps.end());
            if (maxFps != capabilities->fps.end())
                capability.maxFps = *maxFps;
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

    return QnPlOnvifResource::initializeCameraDriver();
}

QnAbstractStreamDataProvider* HikvisionResource::createLiveDataProvider()
{
    if (m_integrationProtocols[kIsapi].enabled)
        return new plugins::HikvisionHevcStreamReader(toSharedPointer(this));

    return base_type::createLiveDataProvider();
}

CameraDiagnostics::Result HikvisionResource::initializeMedia(
    const CapabilitiesResp& onvifCapabilities)
{
    auto resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool hevcIsDisabled = resourceData.value<bool>(Qn::DISABLE_HEVC_PARAMETER_NAME, false);

    if (!hevcIsDisabled && m_integrationProtocols[kIsapi].enabled)
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
                    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, 1);
                    if (!channelCapabilities.fps.empty())
                        setMaxFps(channelCapabilities.fps[0] / 100);
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
    capabilities.isH264 = true;
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
    if (!m_integrationProtocols[kIsapi].enabled)
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

static const auto kIntegratePath = "ISAPI/System/Network/Integrate";
static const auto kDeviceInfoPath = "ISAPI/System/deviceInfo";
static const auto kEnableProtocolsXmlTemplate = QString::fromUtf8(R"xml(
<?xml version:"1.0" encoding="UTF-8"?>
<Integrate>
    %1
</Integrate>
)xml").trimmed();

static const auto kSetOnvifUserPath = lit("ISAPI/Security/ONVIF/users/");
static const auto kSetOnvifUserXml = QString::fromUtf8(R"xml(
<?xml version:"1.0" encoding="UTF-8"?>
<User>
    <id>%1</id>
    <userName>%2</userName>
    <password>%3</password>
    <userType>administrator</userType>
</User>
)xml").trimmed();

static const std::map<QString, QString> kHikvisionIntegrationProtocols = {
    {kOnvif, lit("<ONVIF><enable>true</enable><certificateType/></ONVIF>")},
    {kIsapi, lit("<ISAPI><enable>true</enable></ISAPI>")},
    {kCgi, lit("<CGI><enable>true</enable><certificateType/></CGI>")}
};

class HikvisionRequestHelper: protected XmlRequestHelper
{
public:
    using XmlRequestHelper::XmlRequestHelper;

    std::map<QString, HikvisionResource::ProtocolState> fetchIntegrationProtocolInfo()
    {
        std::map<QString, HikvisionResource::ProtocolState> integrationProtocolStates;
        const auto document = get(kIntegratePath);
        if (!document)
            return integrationProtocolStates;

        for (const auto& entry: kHikvisionIntegrationProtocols)
        {
            const auto& protocolName = entry.first;
            const auto protocolElement = document->documentElement()
                .firstChildElement(protocolName);

            HikvisionResource::ProtocolState state;
            if (!protocolElement.isNull())
            {
                const auto value = protocolElement.firstChildElement(lit("enable")).text();
                state.supported = true;
                state.enabled = value == lit("true");
            }

            integrationProtocolStates.emplace(protocolName, state);
        }

        return integrationProtocolStates;
    }

    bool enableIntegrationProtocols(
        const std::map<QString, HikvisionResource::ProtocolState>& integrationProtocolStates)
    {
        QString enableProtocolsXmlString;
        for (const auto& protocolState: integrationProtocolStates)
        {
            if (!protocolState.second.supported || protocolState.second.enabled)
                continue;

            const auto itr = kHikvisionIntegrationProtocols.find(protocolState.first);
            NX_ASSERT(itr != kHikvisionIntegrationProtocols.cend());
            if (itr == kHikvisionIntegrationProtocols.cend())
                continue;

            enableProtocolsXmlString.append(itr->second);
        }

        if (enableProtocolsXmlString.isEmpty())
            return true;

        const auto result = put(
            kIntegratePath,
            kEnableProtocolsXmlTemplate.arg(enableProtocolsXmlString));

        NX_DEBUG(
            this,
            lm("Enable integration protocols result=%1 on %2")
                .args(result, m_client->url()));

        return result;
    }

    bool checkIsapiSupport()
    {
        const auto result = get(kDeviceInfoPath);
        return result != boost::none;
    };

    boost::optional<std::map<int, QString>> getOnvifUsers()
    {
        const auto document = get(kSetOnvifUserPath);
        if (!document)
            return boost::none;

        std::map<int, QString> users;
        const auto nodeList = document->documentElement().elementsByTagName(lit("User"));
        for (int nodeIndex = 0; nodeIndex < nodeList.size(); ++nodeIndex)
        {
            const auto element = nodeList.at(nodeIndex).toElement();
            const auto value =
                [&](const QString& s) { return element.elementsByTagName(s).at(0).toElement().text(); };

            bool isOk = false;
            users.emplace(value(lit("id")).toInt(&isOk), value(lit("userName")));
            if (!isOk || users.rbegin()->second.isEmpty())
            {
                NX_VERBOSE(this, lm("Unable to parse users from %1").arg(m_client->url()));
                return boost::none;
            }
        }

        NX_VERBOSE(this, lm("ONVIF users %1 on %2").container(users).arg(m_client->url()));
        return users;
    }

    bool setOnvifCredentials(int userId, const QString& login, const QString& password)
    {
        const auto userNumber = QString::number(userId);
        const auto result = put(
            kSetOnvifUserPath + userNumber,
            kSetOnvifUserXml.arg(userNumber, login, password));

        NX_DEBUG(this, lm("Set ONVIF credentials result=%1 on %2").args(result, m_client->url()));
        return result;
    }
};

std::map<QString, HikvisionResource::ProtocolState>
    HikvisionResource::tryToEnableIntegrationProtocols(
        const nx::utils::Url& url,
        const QAuthenticator& authenticator,
        bool isAdditionalSupportCheckNeeded)
{
    HikvisionRequestHelper requestHelper(url, authenticator);
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
        supportedProtocols[kIsapi].enabled = requestHelper.checkIsapiSupport();
        supportedProtocols[kIsapi].supported = supportedProtocols[kIsapi].enabled;
    }

    const auto users = requestHelper.getOnvifUsers();
    if (!users)
    {
        supportedProtocols[kOnvif].enabled = false;
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
        supportedProtocols[kOnvif].enabled = false;

    return supportedProtocols;
}

CameraDiagnostics::Result HikvisionResource::fetchChannelCount(bool /*limitedByEncoders*/)
{
    return base_type::fetchChannelCount(/*limitedByEncoders*/ false);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif  //ENABLE_ONVIF

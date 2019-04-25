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

} // namespace

namespace nx {
namespace mediaserver_core {
namespace plugins {

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

            const auto maxFps = capabilities->realMaxFps();
        }
    }

    return result;
}

CameraDiagnostics::Result HikvisionResource::initializeCameraDriver()
{
    tryToEnableIntegrationProtocols(getDeviceOnvifUrl(), getAuth());
    const auto result = QnPlOnvifResource::initializeCameraDriver();

    // We don't support multicast streaming for Hikvision ISAPI cameras.
    setCameraCapability(Qn::MulticastStreamCapability, false);
    saveParams();
    
    return result;
}

QnAbstractStreamDataProvider* HikvisionResource::createLiveDataProvider()
{
    return new plugins::HikvisionHevcStreamReader(toSharedPointer(this));
}

CameraDiagnostics::Result HikvisionResource::initializeMedia(
    const CapabilitiesResp& onvifCapabilities)
{
    auto resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool hevcIsDisabled = resourceData.value<bool>(Qn::DISABLE_HEVC_PARAMETER_NAME, false);

    if (!hevcIsDisabled)
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
            m_hevcSupported = hikvision::codecSupported(
                AV_CODEC_ID_HEVC,
                channelCapabilities);
            if (m_hevcSupported)
            {
                if (role == Qn::ConnectionRole::CR_LiveVideo)
                {
                    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, 1);
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
    capabilities.isH264 = true;
    if (role == Qn::CR_SecondaryLiveVideo)
        setSecondaryVideoCapabilities(capabilities);
    else
        setPrimaryVideoCapabilities(capabilities);
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

QnAudioTransmitterPtr HikvisionResource::initializeTwoWayAudio()
{
    auto httpClient = getHttpClient();

    QUrl requestUrl(getUrl());
    requestUrl.setPath(lit("/ISAPI/System/TwoWayAudio/channels"));
    requestUrl.setHost(getHostAddress());
    requestUrl.setPort(QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT));

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
        channel->sampleRateKHz);

    auto audioTransmitter = std::make_shared<HikvisionAudioTransmitter>(this);
    if (audioTransmitter->isCompatible(outputFormat))
    {
        audioTransmitter->setOutputFormat(outputFormat);
        audioTransmitter->setChannelId(channel->id);
        audioTransmitter->setAudioUploadHttpMethod(nx_http::Method::put);
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

static const auto kIntegratePath = lit("ISAPI/System/Network/Integrate");
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

struct HikvisionProtocolSwitch
{
    QString protocolName;
    bool supported = false;
    bool enabled = false;
};

static const std::map<QString, QString> kHikvisionIntegrationProtocols = {
    {lit("ONVIF"), lit("<ONVIF><enable>true</enable><certificateType/></ONVIF>")},
    {lit("ISAPI"), lit("<ISAPI><enable>true</enable></ISAPI>")},
    {lit("CGI"), lit("<CGI><enable>true</enable><certificateType/></CGI>")}
};

class HikvisionRequestHelper: protected XmlRequestHelper
{
public:
    using XmlRequestHelper::XmlRequestHelper;

    std::vector<HikvisionProtocolSwitch> fetchIntegrationProtocolInfo()
    {
        std::vector<HikvisionProtocolSwitch> integrationProtocolStates;
        const auto document = get(kIntegratePath);
        if (!document)
            return integrationProtocolStates;

        for (const auto& entry: kHikvisionIntegrationProtocols)
        {
            const auto& protocolName = entry.first;
            const auto protocolElement = document->documentElement()
                .firstChildElement(protocolName);

            HikvisionProtocolSwitch protocolSwitch;
            protocolSwitch.protocolName = protocolName;

            if (!protocolElement.isNull())
            {
                const auto value = protocolElement.firstChildElement(lit("enable")).text();
                protocolSwitch.supported = true;
                protocolSwitch.enabled = value == lit("true");
            }

            integrationProtocolStates.push_back(protocolSwitch);
        }

        return integrationProtocolStates;
    }

    bool enableIntegrationProtocols(
        const std::vector<HikvisionProtocolSwitch>& integrationProtocolStates)
    {
        QString enableProtocolsXmlString;
        for (const auto& protocolState: integrationProtocolStates)
        {
            if (!protocolState.supported || protocolState.enabled)
                continue;

            const auto itr = kHikvisionIntegrationProtocols.find(protocolState.protocolName);
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

bool HikvisionResource::tryToEnableIntegrationProtocols(
    const QUrl& url,
    const QAuthenticator& authenticator)
{
    HikvisionRequestHelper requestHelper(url, authenticator);
    const auto supportedProtocolSwitches = requestHelper.fetchIntegrationProtocolInfo();

    if (!supportedProtocolSwitches.empty())
    {
        if (!requestHelper.enableIntegrationProtocols(supportedProtocolSwitches))
            return false;
    }

    const auto users = requestHelper.getOnvifUsers();
    if (!users)
        return false;

    const auto existingUser = std::find_if(users->begin(), users->end(),
        [&](const std::pair<int, QString>& u) { return u.second == authenticator.user(); });

    return requestHelper.setOnvifCredentials(
        (existingUser != users->end())
            ? existingUser->first //< Override user permissions and password.
            : (users->empty() ? 1 : users->rbegin()->first + 1), //< New user.
        authenticator.user(), authenticator.password());
}

CameraDiagnostics::Result HikvisionResource::fetchChannelCount(bool /*limitedByEncoders*/)
{
    return base_type::fetchChannelCount(/*limitedByEncoders*/ false);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif  //ENABLE_ONVIF

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
#include <plugins/utils/xml_request_helper.h>
#include <utils/media/av_codec_helper.h>
#include <utils/media/av_codec_helper.h>

namespace {

const std::array<Qn::ConnectionRole, 2> kRoles = {
    Qn::ConnectionRole::CR_LiveVideo,
    Qn::ConnectionRole::CR_SecondaryLiveVideo};

} // namespace

namespace nx {
namespace mediaserver_core {
namespace plugins {

using namespace nx::mediaserver_core::plugins::hikvision;
using namespace nx::plugins::utils;

HikvisionResource::HikvisionResource():
    QnPlOnvifResource()
{
    m_audioTransmitter.reset(new HikvisionAudioTransmitter(this));
}

HikvisionResource::~HikvisionResource()
{
    m_audioTransmitter.reset();
}

nx::mediaserver::resource::StreamCapabilityMap HikvisionResource::getStreamCapabilityMapFromDrives(
    bool primaryStream)
{
    using namespace nx::mediaserver::resource;
    auto result = base_type::getStreamCapabilityMapFromDrives(primaryStream);
    if (m_hevcSupported)
    {
        StreamCapabilityMap resultCopy = result;
        for (const auto& onvifKeys: resultCopy.keys())
        {
            StreamCapabilityKey key;
            key.codec = QnAvCodecHelper::codecIdToString(AV_CODEC_ID_HEVC);
            key.resolution = onvifKeys.resolution;
            result.insert(key, nx::media::CameraStreamCapability());
        }
    }
    return result;
}

CameraDiagnostics::Result HikvisionResource::initializeCameraDriver()
{
    tryToEnableOnvifSupport(getDeviceOnvifUrl(), getAuth());

    auto result = QnPlOnvifResource::initializeCameraDriver();
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
                    auto capabilities = primaryVideoCapabilities();
                    capabilities.resolutions = QList<QSize>::fromVector(QVector<QSize>::fromStdVector(
                        channelCapabilities.resolutions));
                    setPrimaryVideoCapabilities(capabilities);
                }
                if (!channelCapabilities.fps.empty())
                    setMaxFps(channelCapabilities.fps[0] / 100);
            }
        }
    }

    return base_type::initializeMedia(onvifCapabilities);
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

static const auto kIntegratePath = lit("ISAPI/System/Network/Integrate");
static const auto kEnableOnvifXml = QString::fromUtf8(R"xml(
<?xml version:"1.0" encoding="UTF-8"?>
<Integrate>
    <ONVIF><enable>true</enable><certificateType/></ONVIF>
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

class HikvisionRequestHelper: protected XmlRequestHelper
{
public:
    using XmlRequestHelper::XmlRequestHelper;

    boost::optional<bool> isOnvifSupported()
    {
        const auto document = get(kIntegratePath);
        if (!document)
            return boost::none;

        const auto value = document->documentElement()
            .firstChildElement(lit("ONVIF")).firstChildElement(lit("enable")).text();

        if (value == lit("true"))
            return true;

        if (value == lit("false"))
            return false;

        NX_WARNING(this, lm("Unexpected ONVIF.enable value '%1' on %2").args(value, m_client->url()));
        return boost::none;
    }

    bool enableOnvif()
    {
        const auto result = put(kIntegratePath, kEnableOnvifXml);
        NX_DEBUG(this, lm("Enable ONVIF result=%1 on %2").args(result, m_client->url()));
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

bool HikvisionResource::tryToEnableOnvifSupport(const QUrl& url, const QAuthenticator& authenticator)
{
    HikvisionRequestHelper requestHelper(url, authenticator);
    const auto isOnvifSupported = requestHelper.isOnvifSupported();
    if (!isOnvifSupported)
        return false;

    if (*isOnvifSupported == false)
    {
        if (!requestHelper.enableOnvif())
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

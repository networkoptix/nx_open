#ifdef ENABLE_ONVIF

#include "onvif_stream_reader.h"

#include <variant>
#include <type_traits>

#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>

#include <network/tcp_connection_priv.h>

#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/nalUnits.h>

#include <common/common_module.h>

#include <nx/fusion/model_functions.h>

#include <core/resource_management/resource_data_pool.h>
#include <core/resource/resource_data_structures.h>
#include <core/resource/param.h>
#include <core/resource_management/resource_properties.h>
#include <core/onvif/onvif_config_data.h>

#include <nx/vms/api/types/rtp_types.h>

#include <nx/utils/app_info.h>

#include <nx/streaming/rtp/parsers/base_metadata_rtp_parser_factory.h>

#include "onvif/soapMediaBindingProxy.h"
#include "onvif_resource.h"
#include "profile_helper.h"

using namespace nx::vms::server::plugins::onvif;

static const int MAX_CAHCE_URL_TIME = 1000 * 15;

struct DefaultProfileInfo
{
    QString primaryProfileName;
    QString secondaryProfileName;
    QString primaryProfileToken;
    QString secondaryProfileToken;
};

DefaultProfileInfo nxDefaultProfileInfo(const QnPlOnvifResourcePtr& device)
{
    DefaultProfileInfo info;

    const auto brand = nx::utils::AppInfo::brand();

    const auto forcedOnvifParams =
        device->resourceData().value<QnOnvifConfigDataPtr>("forcedOnvifParams");

    const int channel = device->getChannel();
    const bool forcedProfilesAreDefined = forcedOnvifParams
        && device->getChannel() < forcedOnvifParams->profiles.size();

    if (forcedProfilesAreDefined)
    {
        const QStringList forcedProfiles =
            forcedOnvifParams->profiles[channel].split(',', QString::SkipEmptyParts);

        if (forcedProfiles.size() > 0)
            info.primaryProfileName = forcedProfiles[0];

        if (forcedProfiles.size() > 1)
            info.secondaryProfileName = forcedProfiles[1];

        if (forcedProfiles.size() > 2)
        {
            NX_WARNING(NX_SCOPE_TAG,
                "The size of profile list is more than 2, ignoring excessive profiles, Device %1",
                device);
        }

        NX_DEBUG(NX_SCOPE_TAG,
            "Forced profiles with names %1 and %2 are defined for device %3",
            info.primaryProfileName, info.secondaryProfileName, device);
    }
    else
    {
        info.primaryProfileName = NX_FMT("%1 Primary", brand);
        info.secondaryProfileName = NX_FMT("%1 Secondary", brand);
    }

    info.primaryProfileToken = NX_FMT("%1P", brand);
    info.secondaryProfileToken = NX_FMT("%1S", brand);

    if (channel > 0)
    {
        QString postfix = NX_FMT("-%1", channel);

        if (!forcedProfilesAreDefined)
        {
            info.primaryProfileName += postfix;
            info.secondaryProfileName += postfix;
        }

        info.primaryProfileToken += postfix;
        info.secondaryProfileToken += postfix;
    }

    return info;
}

static onvifXsd__StreamType streamTypeFromNxRtpTransportType(
    nx::vms::api::RtpTransportType rtpTransportType)
{
    if (rtpTransportType == nx::vms::api::RtpTransportType::multicast)
        return onvifXsd__StreamType::RTP_Multicast;

    return onvifXsd__StreamType::RTP_Unicast;
}

static std::string media2TransportProtocolFromNxRtpTransportType(
    nx::vms::api::RtpTransportType rtpTransportType)
{
    if (rtpTransportType == nx::vms::api::RtpTransportType::multicast)
        return "RtspMulticast";

    if (rtpTransportType == nx::vms::api::RtpTransportType::udp)
        return "RtspUnicast";

    return "RTSP";
}

const bool isMedia2UsageForced(const QnPlOnvifResourcePtr& device)
{
    const QnResourceData resData = device->resourceData();
    return resData.value<bool>(ResourceDataKey::kUseMedia2ToFetchProfiles, false)
        && !device->getMedia2Url().isEmpty();
}

struct CameraInfoParams
{
    CameraInfoParams() {}

    std::string videoEncoderConfigurationToken;
    std::string videoSourceConfigurationToken;
    std::string audioEncoderConfigurationToken;
    std::string audioSourceConfigurationToken;

    std::string ptzConfigurationToken;

    std::string videoSourceToken;
    std::string audioSourceToken;

    std::string profileToken;

    template<typename Profile>
    static CameraInfoParams fromProfile(const Profile* profile)
    {
        CameraInfoParams result;

        result.profileToken = profile->token;
        result.videoSourceToken = ProfileHelper::videoSourceToken(profile);
        result.videoSourceConfigurationToken =
            ProfileHelper::videoSourceConfigurationToken(profile);
        result.videoEncoderConfigurationToken =
            ProfileHelper::videoEncoderConfigurationToken(profile);

        result.audioSourceToken = ProfileHelper::audioSourceToken(profile);
        result.audioSourceConfigurationToken =
            ProfileHelper::audioSourceConfigurationToken(profile);
        result.audioEncoderConfigurationToken =
            ProfileHelper::audioEncoderConfigurationToken(profile);

        result.ptzConfigurationToken = ProfileHelper::ptzConfigurationToken(profile);

        return result;
    }

    QString toString() const
    {
        return NX_FMT(
            "[profileToken: %1, "
            "videoSourceToken: %2, "
            "videoSourceConfigurationToken: %3, "
            "videoEncoderConfigurationToken: %4, "
            "audioSourceToken: %5, "
            "audioSourceConfigurationToken: %6, "
            "audioEncoderConfigurationToken: %7, "
            "ptzConfigurationToken: %8]",
            profileToken,
            videoSourceToken,
            videoSourceConfigurationToken,
            videoEncoderConfigurationToken,
            audioSourceToken,
            audioSourceConfigurationToken,
            audioEncoderConfigurationToken,
            ptzConfigurationToken);
    }
};

struct ProfileSelectionResult
{
    ProfileSelectionResult(CameraDiagnostics::Result error):
        error(std::move(error))
    {
    }

    ProfileSelectionResult(std::optional<CameraInfoParams> result):
        error(CameraDiagnostics::NoErrorResult()),
        result(std::move(result))
    {
    }

    const CameraDiagnostics::Result error;
    const std::optional<CameraInfoParams> result;
};

template<typename ProfilesFetcher>
ProfileSelectionResult tryToChooseExistingProfileInternal(
    const QnPlOnvifResourcePtr& device,
    const CameraInfoParams& info,
    const bool isPrimary)
{
    ProfilesFetcher profiles(device);
    profiles.receiveBySoap();
    if (!profiles)
        return profiles.requestFailedResult();

    const DefaultProfileInfo defaultProfileInfo = nxDefaultProfileInfo(device);

    const auto* profile = selectExistingProfile(
        profiles.get()->Profiles, isPrimary, info, defaultProfileInfo);

    if (!profile)
        return ProfileSelectionResult(std::nullopt);

    return ProfileSelectionResult(CameraInfoParams::fromProfile(profile));
}

static ProfileSelectionResult tryToChooseExistingProfile(
    const QnPlOnvifResourcePtr& device,
    const CameraInfoParams& info,
    bool isPrimary)
{
    if (isMedia2UsageForced(device))
        return tryToChooseExistingProfileInternal<Media2::Profiles>(device, info, isPrimary);

    return tryToChooseExistingProfileInternal<Media::Profiles>(device, info, isPrimary);
}

template<typename Profile>
Profile* selectExistingProfile(
    const std::vector<Profile*>& profiles,
    bool isPrimary,
    const CameraInfoParams& info,
    const DefaultProfileInfo& defaultProfileInfo)
{
    QStringList availableProfileTokens;
    const QString noProfileName = isPrimary
        ? defaultProfileInfo.primaryProfileName
        : defaultProfileInfo.secondaryProfileName;

    const QString filteredProfileName = isPrimary
        ? defaultProfileInfo.secondaryProfileName
        : defaultProfileInfo.primaryProfileName;

    for (const auto& profile: profiles)
    {
        if (profile->token.empty())
            continue;
        else if (profile->Name == noProfileName.toStdString())
            return profile;
        else if (profile->Name == filteredProfileName.toStdString())
            continue;

        if (!info.videoSourceToken.empty())
        {
            if (ProfileHelper::videoSourceToken(profile) != info.videoSourceToken)
                continue;
        }
        availableProfileTokens.push_back(QString::fromStdString(profile->token));
    }

    // Try to select profile with necessary VideoEncoder to avoid change video encoder inside
    // profile. (Some cameras doesn't support it. It can be checked via getCompatibleVideoEncoders
    // from profile)
    for (const auto& profile: profiles)
    {
        if (!profile || !availableProfileTokens.contains(QString::fromStdString(profile->token)))
            continue;

        const bool vSourceMatched =
            ProfileHelper::videoSourceToken(profile) == info.videoSourceToken;

        const bool vEncoderMatched = ProfileHelper::videoEncoderConfigurationToken(profile) ==
            info.videoEncoderConfigurationToken;

        if (vSourceMatched && vEncoderMatched)
            return profile;
    }

    // try to select profile by is lexicographical order
    std::sort(availableProfileTokens.begin(), availableProfileTokens.end());
    const int profileIndex = isPrimary ? 0 : 1;
    if (availableProfileTokens.size() <= profileIndex)
        return 0; // no existing profile matched

    const QString reqProfileToken = availableProfileTokens[profileIndex];
    for (const auto& profile: profiles)
    {
        if (profile->token == reqProfileToken.toStdString())
            return profile;
    }

    return nullptr;
}

static CameraDiagnostics::Result createNewProfile(
    const QnPlOnvifResourcePtr& device,
    std::string name,
    std::string token)
{
    Media::ProfileCreator profileCreator(device);

    Media::ProfileCreator::Request request;
    request.Name = std::move(name);
    request.Token = &token;
    profileCreator.performRequest(request);
    if (!profileCreator)
        return profileCreator.requestFailedResult();

    return CameraDiagnostics::NoErrorResult();
}

//
// QnOnvifStreamReader
//

QnOnvifStreamReader::QnOnvifStreamReader(const QnPlOnvifResourcePtr& res):
    CLServerPushStreamReader(res),
    m_multiCodec(res, res->getTimeOffset()),
    m_onvifRes(res)
{
    m_multiCodec.setCustomTrackParserFactory(
        std::make_unique<nx::streaming::rtp::BaseMetadataRtpParserFactory>(
            QnPlOnvifResource::kSupportedMetadataCodecs));
}

QnOnvifStreamReader::~QnOnvifStreamReader()
{
    stop();
}

void QnOnvifStreamReader::preStreamConfigureHook(bool /*isCameraControlRequired*/)
{
}

void QnOnvifStreamReader::postStreamConfigureHook(bool isCameraControlRequired)
{
    if (!isCameraControlRequired)
        return;

    const auto resData = m_onvifRes->resourceData();
    const auto delayMs = resData.value<int>("afterConfigureStreamDelayMs");
    if (delayMs > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
}

CameraDiagnostics::Result QnOnvifStreamReader::openStreamInternal(
    bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    NX_VERBOSE(this, "try to get stream URL for camera %1 for role %2", m_resource->getUrl(), getRole());
    if (isStreamOpened())
    {
        NX_VERBOSE(this, "Stream is already opened for camera %1 for role %2", m_resource->getUrl(), getRole());
        return CameraDiagnostics::NoErrorResult();
    }

    preStreamConfigureHook(isCameraControlRequired);
    executePreConfigurationRequests();

    if (m_onvifRes->preferredRtpTransport() == nx::vms::api::RtpTransportType::multicast)
    {
        const auto result = m_onvifRes->ensureMulticastIsEnabled(toStreamIndex(getRole()));
        if (!result)
        {
            NX_DEBUG(this,
                "Device [%1] has incorrect multicast parameters "
                "and the Server is unable to fix them",
                m_onvifRes);

            return result;
        }
    }

    QString streamUrl;
    CameraDiagnostics::Result result = updateCameraAndFetchStreamUrl(
        &streamUrl, isCameraControlRequired, params);
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
    {
        NX_DEBUG(this,
            "Can not get stream URL for camera %1 for role %2. Request failed with code %3",
            m_resource->getUrl(), getRole(), result.toString(resourcePool()));
        return result;
    }

    postStreamConfigureHook(isCameraControlRequired);

    auto resData = m_onvifRes->resourceData();
    if (resData.contains(ResourceDataKey::kPreferredAuthScheme))
    {
        auto authScheme = nx::network::http::header::AuthScheme::fromString(
            resData.value<QString>(ResourceDataKey::kPreferredAuthScheme)
                .toLatin1()
                .constData());

        if (authScheme != nx::network::http::header::AuthScheme::none)
            m_multiCodec.setPrefferedAuthScheme(authScheme);
    }

    m_multiCodec.setRole(getRole());
    m_multiCodec.setRequest(streamUrl);

    m_onvifRes->updateSourceUrl(m_multiCodec.getCurrentStreamUrl(), getRole());

    result = m_multiCodec.openStream();
    return result;
}

void QnOnvifStreamReader::setCameraControlDisabled(bool value)
{
    if (!value)
        m_previousStreamParams = QnLiveStreamParams();
    CLServerPushStreamReader::setCameraControlDisabled(value);
}

CameraDiagnostics::Result QnOnvifStreamReader::updateCameraAndFetchStreamUrl(
    QString* const streamUrl, bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    if (!m_streamUrl.isEmpty() && m_cachedTimer.isValid() && m_cachedTimer.elapsed() < MAX_CAHCE_URL_TIME)
    {
        if (!isCameraControlRequired)
        {
            NX_VERBOSE(this, lm("For %1 use unconfigured cached URL: %2").args(
                m_onvifRes->getPhysicalId(), m_streamUrl));

            *streamUrl = m_streamUrl;
            return CameraDiagnostics::NoErrorResult();
        }

        if (params == m_previousStreamParams)
        {
            NX_VERBOSE(this, lm("For %1 use configured cached URL: %2").args(
                m_onvifRes->getPhysicalId(), m_streamUrl));

            *streamUrl = m_streamUrl;
            return m_onvifRes->customStreamConfiguration(getRole(), params);
        }
    }

    m_onvifRes->beforeConfigureStream(getRole());
    CameraDiagnostics::Result result = updateCameraAndFetchStreamUrl(
        getRole() == Qn::CR_LiveVideo, streamUrl, isCameraControlRequired, params);
    if (isCameraControlRequired)
        m_onvifRes->customStreamConfiguration(getRole(), params);
    m_onvifRes->afterConfigureStream(getRole());

    if (result)
    {
        NX_VERBOSE(this, lm("For %1 new URL: %2").args(m_onvifRes->getPhysicalId(), *streamUrl));

        // cache value
        m_streamUrl = *streamUrl;
        m_cachedTimer.restart();
        if (isCameraControlRequired)
            m_previousStreamParams = params;
        else
            m_previousStreamParams = QnLiveStreamParams();
    }
    else
    {
        NX_VERBOSE(this, lm("For %1 unable to update stream URL: %2").args(
            m_onvifRes->getPhysicalId(), result.toString(nullptr)));
    }
    return result;
}

CameraDiagnostics::Result QnOnvifStreamReader::updateCameraAndFetchStreamUrl(
    bool isPrimary,
    QString* outStreamUrl,
    bool isCameraControlRequired,
    const QnLiveStreamParams& params) const
{

    CameraInfoParams info;
    CameraDiagnostics::Result result;

    result = fetchUpdateVideoEncoder(&info, isPrimary, isCameraControlRequired, params);
    if (!result)
        return result;

    info.videoSourceToken = m_onvifRes->videoSourceToken();
    info.videoSourceConfigurationToken =
        m_onvifRes->videoSourceConfigurationToken();

    // If audio encoder updating fails we ignore it.
    fetchUpdateAudioEncoder(&info, isPrimary, isCameraControlRequired);

    info.audioSourceToken = m_onvifRes->audioSourceToken();
    info.audioSourceConfigurationToken =
        m_onvifRes->audioSourceConfigurationToken();

    result = fetchUpdateProfile(info, isPrimary, isCameraControlRequired);
    if (!result)
    {
        // LOG: qWarning() << "ONVIF camera " << getResource()->getUrl() << ": can't prepare profile";
        return result;
    }

    result = fetchStreamUrl(info.profileToken, isPrimary, outStreamUrl);
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
        return result;

    NX_INFO(this, lit("got stream URL %1 for camera %2 for role %3")
        .arg(*outStreamUrl)
        .arg(m_resource->getUrl())
        .arg(getRole()));

    return result;
}

void QnOnvifStreamReader::closeStream()
{
    m_multiCodec.closeStream();
}

bool QnOnvifStreamReader::isStreamOpened() const
{
    return m_multiCodec.isStreamOpened();
}

QnMetaDataV1Ptr QnOnvifStreamReader::getCameraMetadata()
{
    return QnMetaDataV1Ptr(0);
}

bool QnOnvifStreamReader::isGotFrame(QnCompressedVideoDataPtr videoData)
{
    const quint8* curNal = (const quint8*) videoData->data();
    const quint8* end = curNal + videoData->dataSize();
    curNal = NALUnit::findNextNAL(curNal, end);

    const quint8* nextNal = curNal;
    for (;curNal < end; curNal = nextNal)
    {
        nextNal = NALUnit::findNextNAL(curNal, end);
        quint8 nalUnitType = *curNal & 0x1f;
        if (nalUnitType >= nuSliceNonIDR && nalUnitType <= nuSliceIDR)
            return true;
    }
    return false;
}

QnAbstractMediaDataPtr QnOnvifStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetadata())
        return getMetadata();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 2 && !rez; ++i)
        rez = m_multiCodec.getNextData();

    if (!rez)
        closeStream();

    return rez;
}

void QnOnvifStreamReader::printProfile(const onvifXsd__Profile& profile, bool isPrimary) const
{
    #if defined(PL_ONVIF_DEBUG)
        qDebug() << "ONVIF device (UniqueId: "
            << m_onvifRes->getUniqueId() << ") has the following "
            << (isPrimary? "primary": "secondary") << " profile:";
        qDebug() << "Name: " << profile.Name.c_str();
        qDebug() << "Token: " << profile.token.c_str();
        if (!profile.VideoEncoderConfiguration)
        {
            qDebug() << "profile.VideoEncoderConfiguration is nullptr";
            return;
        }
        qDebug() << "Encoder Name: " << profile.VideoEncoderConfiguration->Name.c_str();
        qDebug() << "Encoder Token: " << profile.VideoEncoderConfiguration->token.c_str();
        qDebug() << "Quality: " << profile.VideoEncoderConfiguration->Quality;
        if(profile.VideoEncoderConfiguration->Resolution)
        {
            qDebug() << "Resolution: " << profile.VideoEncoderConfiguration->Resolution->Width
                << "x" << profile.VideoEncoderConfiguration->Resolution->Height;
        }
        else
        {
            qDebug() << "profile.VideoEncoderConfiguration->Resolution is nullptr";
        }

        if (profile.VideoEncoderConfiguration->RateControl)
            qDebug() << "FPS: " << profile.VideoEncoderConfiguration->RateControl->FrameRateLimit;
        else
            qDebug() << "profile.VideoEncoderConfiguration->RateControl is nullptr";
#else
        Q_UNUSED(profile)
        Q_UNUSED(isPrimary)
    #endif
}

bool QnOnvifStreamReader::executePreConfigurationRequests()
{
    auto resData = m_onvifRes->resourceData();

    auto requests = resData.value<QnHttpConfigureRequestList>(
        ResourceDataKey::kPreStreamConfigureRequests);

    if (requests.empty())
        return true;

    CLSimpleHTTPClient http(
        m_onvifRes->getHostAddress(),
        QUrl(m_onvifRes->getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT),
        3000, //<  TODO: #dmishin move to constant
        m_onvifRes->getAuth());

    CLHttpStatus status;
    for (const auto& request: requests)
    {
        if (request.method == lit("GET"))
        {
            qDebug() << request.templateString;
            status = http.doGET(request.templateString);
        }
        else if (request.method == lit("POST"))
            status = http.doPOST(request.templateString, request.body);
        else
            return false;

        if (status != CL_HTTP_SUCCESS && !request.isAllowedToFail)
            return false;
    }

    return true;
}

/**
 * Heuristic algorithm, that tries to fix incorrect URI of media stream for Dahua cameras.

 * Many Dahua cameras have a bug in ONVIF API implementation: GetMediaUri returns the same URL for
 * different profiles (namely the ULR for the zero profile): channel number and subtype may be
 * wrong.
 * Common stream URL looks like the following:
 * rtsp://217.171.200.130:5542/cam/realmonitor?channel=2&subtype=1&unicast=true&proto=Onvif
 * If incoming URL does not look like it, the function does nothing (otherwise it can corrupt
 * the correct URL).
*/
void QnOnvifStreamReader::fixDahuaStreamUrl(
    QString* urlString, const std::string& profileToken) const
{
    NX_ASSERT(urlString);

    // 1. Try to detect the Profile index. Common profile name is something like "MediaProfile102"
    // for cameras or something like "MediaProfile00102" for NVRs/DVRs.
    // The two lower digits - is subtype number (subtype number corresponds to videoEncoder token.
    // The higher digits is a channel number minus 1 .

    constexpr int kShortFormatDigitCount = 3;
    constexpr int kLongFormatDigitCount = 5;

    const auto digitsIterator = std::find_if_not(profileToken.crbegin(), profileToken.crend(),
        isdigit).base();
    const int digitsCount = profileToken.end() - digitsIterator;
    if ((digitsCount < kShortFormatDigitCount) || (digitsCount > kLongFormatDigitCount))
        return;

    const int code = atoi(&*digitsIterator);
    const int neededSubtypeNumber = code % 100;
    const int neededChannelNumber = code / 100 + 1;

    constexpr auto kPath("/cam/realmonitor");
    constexpr auto kChannel("channel");
    constexpr auto kSubtype("subtype");

    // Fixable url should look somewhat like this:
    // rtsp://217.171.200.130:5542/cam/realmonitor?channel=2&subtype=1&unicast=true&proto=Onvif
    // Let's check it.

    QUrl url(*urlString);
    const QString path = url.path();
    QUrlQuery query(url.query());

    if (path != kPath)
        return; //< Unknown url format => url should not be fixed.

    bool isNumber = false;
    const int currentChannelNumber = query.queryItemValue(kChannel).toInt(&isNumber);
    if (!isNumber)
        return; //< Unknown url format => url should not be fixed.
    const int currentSubtypeNumber = query.queryItemValue(kSubtype).toInt(&isNumber);
    if (!isNumber)
        return; //< Unknown url format => url should not be fixed.

    // Fix channel and subtype.
    QList<QPair<QString, QString>> queryParams = query.queryItems();
    for (auto& param: queryParams)
    {
        if (param.first == kChannel)
            param.second = QString::number(neededChannelNumber);
        else if (param.first == kSubtype)
            param.second = QString::number(neededSubtypeNumber);
    }
    query.setQueryItems(queryParams);

    url.setQuery(query.toString());
    *urlString = url.toString();
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchStreamUrl(
    const std::string& profileToken, bool isPrimary, QString* mediaUrl) const
{
    MediaSoapWrapper soapWrapper(m_onvifRes);

    Q_UNUSED(isPrimary);

    if (isMedia2UsageForced(m_onvifRes))
    {
        Media2::StreamUri streamUriFetcher(m_onvifRes);
        Media2::StreamUri::Request request;

        request.Protocol = media2TransportProtocolFromNxRtpTransportType(
            m_onvifRes->preferredRtpTransport());
        request.ProfileToken = profileToken;

        if (!streamUriFetcher.receiveBySoap(request))
            return streamUriFetcher.requestFailedResult();

        *mediaUrl = QString::fromStdString(streamUriFetcher.get()->Uri);
        return CameraDiagnostics::NoErrorResult();
    }


    StreamUriResp response;
    StreamUriReq request;
    onvifXsd__StreamSetup streamSetup;
    onvifXsd__Transport transport;

    request.StreamSetup = &streamSetup;
    request.StreamSetup->Transport = &transport;
    request.StreamSetup->Stream = streamTypeFromNxRtpTransportType(
        m_onvifRes->preferredRtpTransport());
    request.StreamSetup->Transport->Tunnel = 0;
    request.StreamSetup->Transport->Protocol = onvifXsd__TransportProtocol::RTSP;
    request.ProfileToken = profileToken;

    int soapRes = soapWrapper.getStreamUri(request, response);
    if (soapRes != SOAP_OK) {
    #if defined(PL_ONVIF_DEBUG)
        qCritical() << "QnOnvifStreamReader::fetchStreamUrl (primary stream = " << isPrimary
            << "): can't get stream URL of ONVIF device (URL: " << m_onvifRes->getMediaUrl()
            << ", UniqueId: " << m_onvifRes->getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastErrorDescription();
    #endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getStreamUri"), soapWrapper.getLastErrorDescription());
    }

    if (!response.MediaUri)
    {
    #if defined(PL_ONVIF_DEBUG)
        qCritical() << "QnOnvifStreamReader::fetchStreamUrl (primary stream = "  << isPrimary
            << "): can't get stream URL of ONVIF device (URL: " << m_onvifRes->getMediaUrl()
            << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: got empty response.";
    #endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getStreamUri"), QLatin1String("empty media uri"));
    }

    #if defined(PL_ONVIF_DEBUG)
        qDebug() << "URL of ONVIF device stream (UniqueId: " << m_onvifRes->getUniqueId()
            << ") successfully fetched: " << response.MediaUri->Uri.c_str();
    #endif
    QUrl relutUrl(m_onvifRes->fromOnvifDiscoveredUrl(response.MediaUri->Uri, false));

    if (relutUrl.host().size()==0)
    {
        QString temp = relutUrl.toString();
        relutUrl.setHost(m_onvifRes->getHostAddress());
    #if defined(PL_ONVIF_DEBUG)
        qCritical() << "pure URL(error) " << temp<< " Trying to fix: " << relutUrl.toString();
    #endif
    }

    *mediaUrl = relutUrl.toString();

    const bool isDahua = m_onvifRes->getVendor().toLower() == "dahua";
    const bool fixWrongUri = m_onvifRes->resourceData().value<bool>(ResourceDataKey::kFixWrongUri);

    if (isDahua || fixWrongUri)
        fixDahuaStreamUrl(mediaUrl, request.ProfileToken);

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateVideoEncoder(
    CameraInfoParams* outInfo,
    bool isPrimary,
    bool isCameraControlRequired,
    const QnLiveStreamParams& params) const
{
    if (m_onvifRes->commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    outInfo->videoEncoderConfigurationToken = isPrimary
        ? m_onvifRes->primaryVideoCapabilities().videoEncoderToken
        : m_onvifRes->secondaryVideoCapabilities().videoEncoderToken;

    if (!isCameraControlRequired)
        return CameraDiagnostics::NoErrorResult(); //< Do not update video encoder configuration.

    bool isMedia2Supported = !m_onvifRes->getMedia2Url().isEmpty();
    if (isMedia2Supported)
    {
        // Use Media2 interface.
        Media2::VideoEncoderConfigurations veConfigurations(m_onvifRes);
        veConfigurations.receiveBySoap();
        if (!veConfigurations)
        {
            //LOG.
            if (veConfigurations.innerWrapper().lastErrorIsNotAuthenticated())
                return CameraDiagnostics::NotAuthorisedResult(veConfigurations.endpoint());
            else
                isMedia2Supported = false;
        }
        else
        {
            // The pointer currentVEConfig points to the object that is valid until
            // veConfigurations destructor is called.
            onvifXsd__VideoEncoder2Configuration* currentVEConfig = selectVideoEncoder2Config(
                veConfigurations.get()->Configurations, isPrimary);

            if (!currentVEConfig)
                return CameraDiagnostics::RequestFailedResult("selectVideoEncoder2Config", QString());

            outInfo->videoEncoderConfigurationToken = currentVEConfig->token;

            auto streamIndex = isPrimary
                ? nx::vms::api::StreamIndex::primary
                : nx::vms::api::StreamIndex::secondary;
            m_onvifRes->updateVideoEncoder2(*currentVEConfig, streamIndex, params);

            int triesLeft = m_onvifRes->getMaxOnvifRequestTries();
            CameraDiagnostics::Result result = CameraDiagnostics::UnknownErrorResult();

            while ((result.errorCode != CameraDiagnostics::ErrorCode::noError) && --triesLeft >= 0)
            {
                result = m_onvifRes->sendVideoEncoder2ToCameraEx(*currentVEConfig, streamIndex, params);
                if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
                    msleep(300);
            }

            return result;
        }
    }

    // isMedia2Supported could have been changed in the previous if
    if (!isMedia2Supported)
    {
        // Use old Media1 interface.
        Media::VideoEncoderConfigurations veConfigurations(m_onvifRes);
        veConfigurations.receiveBySoap();
        if (!veConfigurations)
        {
            //LOG.
            if (veConfigurations.innerWrapper().lastErrorIsNotAuthenticated())
                return CameraDiagnostics::NotAuthorisedResult(veConfigurations.endpoint());
            else
                return veConfigurations.requestFailedResult();
        }

        // The pointer currentVEConfig points to the object that is valid until
        // veConfigurations destructor is called.
        onvifXsd__VideoEncoderConfiguration* currentVEConfig = selectVideoEncoderConfig(
            veConfigurations.get()->Configurations, isPrimary);

        if (!currentVEConfig)
            return CameraDiagnostics::RequestFailedResult("selectVideoEncoderConfig", QString());

        auto streamIndex = isPrimary
            ? nx::vms::api::StreamIndex::primary
            : nx::vms::api::StreamIndex::secondary;
        m_onvifRes->updateVideoEncoder1(*currentVEConfig, streamIndex, params);

        // Usually getMaxOnvifRequestTries returns 1, but for some OnvifResource descendants it's
        // larger. In case of failure request are repeated.
        int triesLeft = m_onvifRes->getMaxOnvifRequestTries();
        constexpr std::chrono::milliseconds kTryIntervalMs(300);

        // Some DW cameras need two same sendVideoEncoder requests with 1 second interval.
        // Both responses are Ok, but only the second request really succeeds.
        auto resourceData = m_onvifRes->resourceData();
        const int repeatIntervalForSendVideoEncoderMs =
            resourceData.value<int>(ResourceDataKey::kRepeatIntervalForSendVideoEncoderMS, 0);

        CameraDiagnostics::Result result = CameraDiagnostics::UnknownErrorResult();
        while ((result.errorCode != CameraDiagnostics::ErrorCode::noError) && --triesLeft >= 0)
        {
            result = m_onvifRes->sendVideoEncoderToCameraEx(*currentVEConfig, streamIndex, params);
            if (repeatIntervalForSendVideoEncoderMs)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(repeatIntervalForSendVideoEncoderMs));
                result = m_onvifRes->sendVideoEncoderToCameraEx(
                    *currentVEConfig, streamIndex, params);
            }

            if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
                std::this_thread::sleep_for(kTryIntervalMs);
        }

        return result;
    }

    // This code is never reached, it's written to eliminate compiler warning/error.
    return CameraDiagnostics::NoErrorResult();
}

onvifXsd__VideoEncoderConfiguration* QnOnvifStreamReader::selectVideoEncoderConfig(
    std::vector<onvifXsd__VideoEncoderConfiguration *>& configs, bool isPrimary) const
{
    const std::string videoEncoderToken = isPrimary
        ? m_onvifRes->primaryVideoCapabilities().videoEncoderToken
        : m_onvifRes->secondaryVideoCapabilities().videoEncoderToken;

    for (auto itr = configs.begin(); itr != configs.end(); ++itr)
    {
        onvifXsd__VideoEncoderConfiguration* conf = *itr;
        if (conf && videoEncoderToken == conf->token)
            return conf;
    }

    return nullptr;
}

onvifXsd__VideoEncoder2Configuration* QnOnvifStreamReader::selectVideoEncoder2Config(
    std::vector<onvifXsd__VideoEncoder2Configuration *>& configs, bool isPrimary) const
{
    const std::string videoEncoderToken = isPrimary
        ? m_onvifRes->primaryVideoCapabilities().videoEncoderToken
        : m_onvifRes->secondaryVideoCapabilities().videoEncoderToken;

    for (auto itr = configs.begin(); itr != configs.end(); ++itr)
    {
        onvifXsd__VideoEncoder2Configuration* conf = *itr;
        if (conf && videoEncoderToken == conf->token)
            return conf;
    }

    return nullptr;
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateProfile(
    CameraInfoParams& info,
    bool isPrimary,
    bool isCameraControlRequired) const
{
    if (m_onvifRes->commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    const QnResourceData resData = m_onvifRes->resourceData();
    const bool useExistingProfiles = resData.value<bool>(
        ResourceDataKey::kUseExistingOnvifProfiles);

    const ProfileSelectionResult profileSelectionResult =
        tryToChooseExistingProfile(m_onvifRes, info, isPrimary);

    if (profileSelectionResult.error.errorCode != CameraDiagnostics::ErrorCode::noError)
    {
        NX_DEBUG(this,
            "Failed to fetch information about profiles from Device %1 (%2), channel %3, error %4",
            m_onvifRes->getUserDefinedName(), m_onvifRes->getId(), m_onvifRes->getChannel(),
            profileSelectionResult.error.toString(resourcePool()));

        return profileSelectionResult.error;
    }

    if (profileSelectionResult.result)
    {
        NX_DEBUG(this,
            "Found suitable profile on the Device %1 (%2), channel %3. Profile info: %5",
            m_onvifRes->getUserDefinedName(), m_onvifRes->getId(), m_onvifRes->getChannel(),
            *profileSelectionResult.result);

        info.profileToken = profileSelectionResult.result->profileToken;
    }
    else
    {
        const DefaultProfileInfo defaultProfileInfo = nxDefaultProfileInfo(m_onvifRes);
        const std::string noProfileName = isPrimary
            ? defaultProfileInfo.primaryProfileName.toStdString()
            : defaultProfileInfo.secondaryProfileName.toStdString();

        info.profileToken = isPrimary
            ? defaultProfileInfo.primaryProfileToken.toStdString()
            : defaultProfileInfo.secondaryProfileToken.toStdString();

        NX_DEBUG(this,
            "Suitable profile has not been found on the Device %1 (%2), channel %3"
            "Attempting to create a new profile with name %4 and token %5",
            m_onvifRes->getUserDefinedName(), m_onvifRes->getId(), m_onvifRes->getChannel(),
            noProfileName, info.profileToken);

        const CameraDiagnostics::Result result =
            createNewProfile(m_onvifRes, noProfileName, info.profileToken);

        if (!result)
        {
            NX_DEBUG(this,
                "Failed to create new profile for device %1 (%2), channel %3, error: %4",
                m_onvifRes->getUserDefinedName(), m_onvifRes->getId(), m_onvifRes->getChannel(),
                result.toString(resourcePool()));
            return result;
        }
    }

    if (useExistingProfiles)
    {
        NX_DEBUG(this, "Using existing profiles for Device %1 (%2), channel %3",
            m_onvifRes->getUserDefinedName(), m_onvifRes->getId(), m_onvifRes->getChannel());
        return CameraDiagnostics::NoErrorResult();
    }

    if (getRole() == Qn::CR_LiveVideo)
        m_onvifRes->setPtzProfileToken(info.profileToken);

    if (!isCameraControlRequired)
    {
        NX_DEBUG(this,
            "Device %1 (%2), channel %3 is not controlled by the Server, using current settings",
            m_onvifRes->getUserDefinedName(), m_onvifRes->getId(), m_onvifRes->getChannel());
        return CameraDiagnostics::NoErrorResult();
    }
    else
    {
        return updateProfileConfigurations(
            info,
            profileSelectionResult.result ? *profileSelectionResult.result : CameraInfoParams());
    }
}

ConfigurationSet QnOnvifStreamReader::calculateConfigurationsToUpdate(
    const CameraInfoParams& desiredParameters,
    const CameraInfoParams& actualParameters) const
{
    ConfigurationSet result;
    result.profileToken = desiredParameters.profileToken;

    if (actualParameters.videoSourceToken != desiredParameters.videoSourceToken);
    {
        result.configurations.emplace(
            ConfigurationType::videoSource,
            desiredParameters.videoSourceConfigurationToken);
    }

    if (actualParameters.videoEncoderConfigurationToken !=
        desiredParameters.videoEncoderConfigurationToken)
    {
        result.configurations.emplace(
            ConfigurationType::videoEncoder,
            desiredParameters.videoEncoderConfigurationToken);
    }

    if (getRole() == Qn::ConnectionRole::CR_LiveVideo
        && !m_onvifRes->getPtzUrl().isEmpty()
        && !m_onvifRes->ptzConfigurationToken().empty()
        && actualParameters.ptzConfigurationToken.empty())
    {
        result.configurations.emplace(
            ConfigurationType::ptz,
            desiredParameters.ptzConfigurationToken);
    }

    if (!desiredParameters.audioSourceToken.empty()
        && !desiredParameters.audioEncoderConfigurationToken.empty())
    {
        if (actualParameters.audioSourceToken != desiredParameters.audioSourceToken)
        {
            result.configurations.emplace(
                ConfigurationType::audioSource,
                desiredParameters.audioSourceConfigurationToken);
        }

        if (actualParameters.audioEncoderConfigurationToken
            != desiredParameters.audioEncoderConfigurationToken)
        {
            result.configurations.emplace(
                ConfigurationType::audioEncoder,
                desiredParameters.audioEncoderConfigurationToken);
        }
    }

    return result;
}

CameraDiagnostics::Result QnOnvifStreamReader::updateProfileConfigurations(
    const CameraInfoParams& desiredParameters,
    const CameraInfoParams& actualParameters) const
{
    ConfigurationSet configurationSet = calculateConfigurationsToUpdate(
        desiredParameters,
        actualParameters);

    CameraDiagnostics::Result result = isMedia2UsageForced(m_onvifRes)
        ? ProfileHelper::addMedia2Configurations(m_onvifRes, std::move(configurationSet))
        : ProfileHelper::addMediaConfigurations(m_onvifRes, configurationSet);

    if (!result)
        return result;

    if (!m_onvifRes->audioOutputConfigurationToken().empty())
    {
        NX_DEBUG(this, "Device %1 (%2), channel %3, binding two-way audio to profile %4",
            m_onvifRes->getUserDefinedName(), m_onvifRes->getId(), m_onvifRes->getChannel(),
            desiredParameters.profileToken);

        const auto result = bindTwoWayAudioToProfile(desiredParameters.profileToken);
        if (!result)
        {
            const auto errorMessage = result.toString(
                m_onvifRes->serverModule()->commonModule()->resourcePool());

            NX_DEBUG(this,
                "Error binding two way audio to profile %1 for camera %2. Error: %3",
                desiredParameters.profileToken, m_onvifRes->getUrl(), errorMessage);
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifStreamReader::bindTwoWayAudioToProfile(
    const std::string& profileToken) const
{
    MediaSoapWrapper soapWrapper(m_onvifRes);

    AddAudioOutputConfigurationReq addAudioOutputConfigurationRequest;
    AddAudioOutputConfigurationResp addAudioOutputConfigurationResponse;

    addAudioOutputConfigurationRequest.ProfileToken = profileToken;
    addAudioOutputConfigurationRequest.ConfigurationToken =
        m_onvifRes->audioOutputConfigurationToken();
    int soapRes = soapWrapper.addAudioOutputConfiguration(
        addAudioOutputConfigurationRequest, addAudioOutputConfigurationResponse);
    if (soapRes != SOAP_OK)
    {
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("addAudioOutputConfiguration"), soapWrapper.getLastErrorDescription());
    }

    GetCompatibleAudioDecoderConfigurationsReq audioDecodersRequest;
    GetCompatibleAudioDecoderConfigurationsResp audioDecodersResponse;
    audioDecodersRequest.ProfileToken = profileToken;
    soapRes = soapWrapper.getCompatibleAudioDecoderConfigurations(audioDecodersRequest, audioDecodersResponse);
    if (soapRes != SOAP_OK)
    {
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getCompatibleAudioDecoderConfigurations"), soapWrapper.getLastErrorDescription());
    }
    if (!audioDecodersResponse.Configurations.empty() && audioDecodersResponse.Configurations[0])
    {
        AddAudioDecoderConfigurationReq addDecoderRequest;
        AddAudioDecoderConfigurationResp addDecoderResponse;
        addDecoderRequest.ProfileToken = profileToken;
        auto configuration = audioDecodersResponse.Configurations[0];
        addDecoderRequest.ConfigurationToken = configuration->token;

        soapRes = soapWrapper.addAudioDecoderConfiguration(addDecoderRequest, addDecoderResponse);
        if (soapRes != SOAP_OK)
        {
            return CameraDiagnostics::RequestFailedResult(
                QLatin1String("addAudioDecoderConfiguration"), soapWrapper.getLastErrorDescription());
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateAudioEncoder(
    CameraInfoParams* outInfo, bool isPrimary, bool isCameraControlRequired) const
{
    if (m_onvifRes->commonModule()->isNeedToStop())
        return CameraDiagnostics::ServerTerminatedResult();

    Media::AudioEncoderConfigurations aeConfigurations(m_onvifRes);
    aeConfigurations.receiveBySoap();
    if (!aeConfigurations)
    {
        // LOG.
        return aeConfigurations.requestFailedResult();
    }

    onvifXsd__AudioEncoderConfiguration* currentAEConfig = selectAudioEncoderConfig(
        aeConfigurations.get()->Configurations, isPrimary);
    if (!currentAEConfig)
        return CameraDiagnostics::RequestFailedResult("selectAudioEncoderConfig", QString());

    // TODO: #rvasilenko UTF unuse std::string
    outInfo->audioEncoderConfigurationToken = currentAEConfig->token;

    if (!isCameraControlRequired)
        return CameraDiagnostics::NoErrorResult(); //< Do not update audio encoder params.

    return sendAudioEncoderToCamera(*currentAEConfig);
}

onvifXsd__AudioEncoderConfiguration* QnOnvifStreamReader::selectAudioEncoderConfig(
    std::vector<onvifXsd__AudioEncoderConfiguration *>& configs, bool /*isPrimary*/) const
{
    std::string id = m_onvifRes->audioEncoderConfigurationToken();
    if (id.empty())
        return nullptr;

    for (auto iter = configs.begin(); iter != configs.end(); ++iter)
    {
        if (*iter && id == (*iter)->token)
            return *iter;
    }

    return nullptr;
}

void QnOnvifStreamReader::updateAudioEncoder(AudioEncoder& encoder) const
{
    QnPlOnvifResource::AUDIO_CODEC codec = m_onvifRes->getAudioCodec();
    if (codec <= QnPlOnvifResource::AUDIO_NONE || codec >= QnPlOnvifResource::SIZE_OF_AUDIO_CODECS)
    {
        #if defined(PL_ONVIF_DEBUG)
            qWarning() << "QnOnvifStreamReader::updateAudioEncoder: "
                << "codec type is undefined. UniqueId: " << m_onvifRes->getUniqueId();
        #endif
        return;
    }

    switch (codec)
    {
        case QnPlOnvifResource::G711:
            encoder.Encoding = onvifXsd__AudioEncoding::G711; // = 0
            break;
        case QnPlOnvifResource::G726:
            encoder.Encoding = onvifXsd__AudioEncoding::G726; // = 1
            break;
        case QnPlOnvifResource::AAC:
            encoder.Encoding = onvifXsd__AudioEncoding::AAC; // = 2
            break;
        case QnPlOnvifResource::AMR:
            encoder.Encoding = onvifXsd__AudioEncoding::AMR; // = 3
            break;
        default:
            #if defined(PL_ONVIF_DEBUG)
                qWarning() << "QnOnvifStreamReader::updateAudioEncoder: codec type is unknown: "
                    << codec << ". UniqueId: " << m_onvifRes->getUniqueId();
            #endif
            return;
    }

    // Value 0 means "undefined".
    int bitrate = m_onvifRes->getAudioBitrate();
    if (bitrate)
        encoder.Bitrate = bitrate;

    // Value 0 means "undefined".
    int samplerate = m_onvifRes->getAudioSamplerate();
    if (samplerate)
        encoder.SampleRate = samplerate;
}

CameraDiagnostics::Result QnOnvifStreamReader::sendAudioEncoderToCamera(
    onvifXsd__AudioEncoderConfiguration& encoderConfig) const
{
    Media::AudioEncoderConfigurationSetter setter(m_onvifRes);
    Media::AudioEncoderConfigurationSetter::Request request;
    request.Configuration = &encoderConfig;
    request.ForcePersistence = false;

    setter.performRequest(request);
    if (!setter)
    {
        // LOG.
        return setter.requestFailedResult();
    }
    return CameraDiagnostics::NoErrorResult();
}

QnConstResourceAudioLayoutPtr QnOnvifStreamReader::getDPAudioLayout() const
{
    return m_multiCodec.getAudioLayout();
}

void QnOnvifStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_multiCodec.pleaseStop();
}

QnConstResourceVideoLayoutPtr QnOnvifStreamReader::getVideoLayout() const
{
    return m_multiCodec.getVideoLayout();
}

#endif //ENABLE_ONVIF

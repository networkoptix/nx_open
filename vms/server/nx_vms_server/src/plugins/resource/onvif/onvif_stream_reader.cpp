#ifdef ENABLE_ONVIF

#include "onvif_stream_reader.h"

#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>

#include <network/tcp_connection_priv.h>

#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/app_info.h>
#include <utils/media/nalUnits.h>

#include <common/common_module.h>

#include <core/resource_management/resource_data_pool.h>
#include <core/resource/resource_data_structures.h>
#include <core/resource/param.h>
#include <core/resource_management/resource_properties.h>

#include <nx/vms/api/types/rtp_types.h>

#include "onvif/soapMediaBindingProxy.h"
#include "onvif_resource.h"

static const int MAX_CAHCE_URL_TIME = 1000 * 15;

static onvifXsd__StreamType streamTypeFromRtpTransport(nx::vms::api::RtpTransportType rtpTransport)
{
    if (rtpTransport == nx::vms::api::RtpTransportType::multicast)
        return onvifXsd__StreamType::RTP_Multicast;

    return onvifXsd__StreamType::RTP_Unicast;
}

struct CameraInfoParams
{
    CameraInfoParams() {}
    std::string videoEncoderConfigurationToken;
    std::string videoSourceConfigurationToken;
    std::string videoSourceToken;

    std::string audioEncoderConfigurationToken;
    std::string audioSourceConfigurationToken;
    std::string audioSourceToken;

    std::string profileToken;
};

//
// QnOnvifStreamReader
//

QnOnvifStreamReader::QnOnvifStreamReader(const QnPlOnvifResourcePtr& res):
    CLServerPushStreamReader(res),
    m_multiCodec(res, res->getTimeOffset()),
    m_onvifRes(res)
{
}

QnOnvifStreamReader::~QnOnvifStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnOnvifStreamReader::openStreamInternal(
    bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    NETOPTIX_PRIMARY_NAME = QString(lit("%1 Primary")).arg(QnAppInfo::productNameShort()).toUtf8();
    NETOPTIX_SECONDARY_NAME = QString(lit("%1 Secondary")).arg(QnAppInfo::productNameShort()).toUtf8();
    NETOPTIX_PRIMARY_TOKEN = QString(lit("%1P")).arg(QnAppInfo::productNameShort()).toUtf8();
    NETOPTIX_SECONDARY_TOKEN = QString(lit("%1S")).arg(QnAppInfo::productNameShort()).toUtf8();

    int channel = m_onvifRes->getChannel();
    if (channel > 0) {
        QByteArray postfix = QByteArray("-") + QByteArray::number(channel);
        NETOPTIX_PRIMARY_NAME += postfix;
        NETOPTIX_SECONDARY_NAME += postfix;
        NETOPTIX_PRIMARY_TOKEN += postfix;
        NETOPTIX_SECONDARY_TOKEN += postfix;
    }

    preStreamConfigureHook();
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
        #if defined(PL_ONVIF_DEBUG)
            qCritical() << "QnOnvifStreamReader::openStream: "
                << "can't fetch stream URL for resource with UniqueId: "
                << m_onvifRes->getUniqueId();
        #endif
        return result;
    }

    postStreamConfigureHook();

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
            return m_onvifRes->customStreamConfiguration(getRole());
        }
    }

    m_onvifRes->beforeConfigureStream(getRole());
    CameraDiagnostics::Result result = updateCameraAndFetchStreamUrl(
        getRole() == Qn::CR_LiveVideo, streamUrl, isCameraControlRequired, params);
    m_onvifRes->customStreamConfiguration(getRole());
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
    info.videoSourceConfigurationToken = m_onvifRes->videoSourceConfigurationToken();

    // If audio encoder updating fails we ignore it.
    fetchUpdateAudioEncoder(&info, isPrimary, isCameraControlRequired);

    info.audioSourceToken = m_onvifRes->audioSourceToken();
    info.audioSourceConfigurationToken = m_onvifRes->audioSourceConfigurationToken();

    result = fetchUpdateProfile(info, isPrimary, isCameraControlRequired);
    if (!result)
    {
        // LOG: qWarning() << "ONVIF camera " << getResource()->getUrl() << ": can't prepare profile";
        return result;
    }

    {
        MediaSoapWrapper soapWrapper(m_onvifRes);

        result = fetchStreamUrl(soapWrapper, info.profileToken, isPrimary, outStreamUrl);
        if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
            return result;

        NX_INFO(this, lit("got stream URL %1 for camera %2 for role %3")
            .arg(*outStreamUrl)
            .arg(m_resource->getUrl())
            .arg(getRole()));
    }
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

void QnOnvifStreamReader::fixStreamUrl(QString* mediaUrl, const std::string& profileToken) const
{
    // Try to detect the Profile index. Common profile name is something like "MediaProfile002".
    const QString token = QString::fromStdString(profileToken);
    bool isNumber = false;
    int profileIndex = token.right(3).toInt(&isNumber);
    if (!isNumber)
        return; //< Failed to detect index => can not fix Uri.

    if (profileIndex == 0)
        return; //< Uri is always correct for zero profile.

    const QString kBrokenSubstring("subtype=0");
    int position = mediaUrl->indexOf(kBrokenSubstring);
    if (position == -1)
        return; //< No substring to fix found in Uri.

    position += (kBrokenSubstring.length() - 1); // Position of the symbol to fix.

    mediaUrl->replace(position, 1, QString::number(profileIndex));
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchStreamUrl(MediaSoapWrapper& soapWrapper,
    const std::string& profileToken, bool isPrimary, QString* mediaUrl) const
{
    Q_UNUSED(isPrimary);

    StreamUriResp response;
    StreamUriReq request;
    onvifXsd__StreamSetup streamSetup;
    onvifXsd__Transport transport;

    request.StreamSetup = &streamSetup;
    request.StreamSetup->Transport = &transport;
    request.StreamSetup->Stream = streamTypeFromRtpTransport(m_onvifRes->preferredRtpTransport());
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

    const bool fixWrongUri = m_onvifRes->resourceData().value<bool>(QString("fixWrongUri"));
    if (fixWrongUri)
        fixStreamUrl(mediaUrl, request.ProfileToken);

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

    if (m_onvifRes->getMedia2Url().isEmpty())
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
    else
    {
        // Use old Media2 interface.
        Media2::VideoEncoderConfigurations veConfigurations(m_onvifRes);
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

    auto resData = m_onvifRes->resourceData();
    auto useExistingProfiles = resData.value<bool>(
        ResourceDataKey::kUseExistingOnvifProfiles);

    Media::Profiles profiles(m_onvifRes);
    profiles.receiveBySoap();
    if (!profiles)
    {
        // LOG.
        return profiles.requestFailedResult();
    }

    onvifXsd__Profile* profile = selectExistingProfile(
        profiles.get()->Profiles, isPrimary, info);
    if (profile)
    {
        info.profileToken = profile->token;
    }
    else
    {
        std::string noProfileName = isPrimary
            ? NETOPTIX_PRIMARY_NAME.toStdString()
            : NETOPTIX_SECONDARY_NAME.toStdString();
        info.profileToken = isPrimary
            ? NETOPTIX_PRIMARY_TOKEN.toStdString()
            : NETOPTIX_SECONDARY_TOKEN.toStdString();
        CameraDiagnostics::Result result = createNewProfile(noProfileName, info.profileToken);
        if (!result)
            return result;
    }

    if (useExistingProfiles)
        return CameraDiagnostics::NoErrorResult();

    if (getRole() == Qn::CR_LiveVideo)
        m_onvifRes->setPtzProfileToken(info.profileToken);
    if (!isCameraControlRequired)
        return CameraDiagnostics::NoErrorResult();
    else
        return sendProfileToCamera(info, profile);
}

CameraDiagnostics::Result QnOnvifStreamReader::createNewProfile(
    std::string name, std::string token) const
{
    Media::ProfileCreator profileCreator(m_onvifRes);

    Media::ProfileCreator::Request request;
    request.Name = std::move(name);
    request.Token = &token;
    profileCreator.performRequest(request);
    if (!profileCreator)
    {
        // LOG.
        return profileCreator.requestFailedResult();
    }

    return CameraDiagnostics::NoErrorResult();
}

onvifXsd__Profile* QnOnvifStreamReader::selectExistingProfile(
    std::vector<onvifXsd__Profile *>& profiles, bool isPrimary, CameraInfoParams& info) const
{
    QStringList availableProfiles;
    QString noProfileName = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_NAME) : QLatin1String(NETOPTIX_SECONDARY_NAME);
    QString filteredProfileName = isPrimary ? QLatin1String(NETOPTIX_SECONDARY_NAME) : QLatin1String(NETOPTIX_PRIMARY_NAME);

    for (auto iter = profiles.begin(); iter != profiles.end(); ++iter)
    {
        onvifXsd__Profile* profile = *iter;
        if (profile->token.empty())
            continue;
        else if (profile->Name == noProfileName.toStdString())
            return profile;
        else if (profile->Name == filteredProfileName.toStdString())
            continue;

        if (!info.videoSourceToken.empty())
        {
            if (!profile->VideoSourceConfiguration
                || (profile->VideoSourceConfiguration->SourceToken != info.videoSourceToken))
            {
                continue;
            }
        }
        availableProfiles << QString::fromStdString(profile->token);
    }

    // Try to select profile with necessary VideoEncoder to avoid change video encoder inside
    // profile. (Some cameras doesn't support it. It can be checked
    // via getCompatibleVideoEncoders from profile)

    for (auto iter = profiles.begin(); iter != profiles.end(); ++iter)
    {
        onvifXsd__Profile* profile = *iter;
        if (!profile || !availableProfiles.contains(QString::fromStdString(profile->token)))
            continue;
        bool vSourceMatched = profile->VideoSourceConfiguration
            && profile->VideoSourceConfiguration->SourceToken == info.videoSourceToken;
        bool vEncoderMatched = profile->VideoEncoderConfiguration
            && profile->VideoEncoderConfiguration->token == info.videoEncoderConfigurationToken;
        if (vSourceMatched && vEncoderMatched)
            return profile;
    }

    // try to select profile by is lexicographical order
    std::sort(availableProfiles.begin(), availableProfiles.end());
    int profileIndex = isPrimary ? 0 : 1;
    if (availableProfiles.size() <= profileIndex)
        return 0; // no existing profile matched

    QString reqProfileToken = availableProfiles[profileIndex];
    for (auto iter = profiles.begin(); iter != profiles.end(); ++iter)
    {
        onvifXsd__Profile* profile = *iter;
        if (profile->token == reqProfileToken.toStdString())
            return profile;
    }

    return 0;
}

CameraDiagnostics::Result QnOnvifStreamReader::bindTwoWayAudioToProfile(
    MediaSoapWrapper& soapWrapper, const std::string& profileToken) const
{
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

CameraDiagnostics::Result QnOnvifStreamReader::sendProfileToCamera(
    CameraInfoParams& info, onvifXsd__Profile* profile) const
{
    if (!profile)
        return CameraDiagnostics::NoErrorResult();

    MediaSoapWrapper soapWrapper(m_onvifRes);

    bool vSourceMatched = profile && profile->VideoSourceConfiguration
        && profile->VideoSourceConfiguration->SourceToken == info.videoSourceToken;
    if (!vSourceMatched)
    {
        AddVideoSrcConfigReq request;
        AddVideoSrcConfigResp response;

        request.ProfileToken = info.profileToken;
        request.ConfigurationToken = info.videoSourceConfigurationToken;

        int soapRes = soapWrapper.addVideoSourceConfiguration(request, response);
        if (soapRes != SOAP_OK)
        {
            #if defined(PL_ONVIF_DEBUG)
                qCritical() << "QnOnvifStreamReader::addVideoSourceConfiguration: "
                    << "can't add video source to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getLastErrorDescription()
                    << ". URL: " << soapWrapper.endpoint()
                    << ", uniqueId: " << m_onvifRes->getUniqueId() <<
                    "current vSourceID=" << profile->VideoSourceConfiguration->token.data()
                    << "requested vSourceID=" << info.videoSourceToken;
            #endif
            if (m_onvifRes->getMaxChannels() > 1)
            {
                    return CameraDiagnostics::RequestFailedResult(
                        QLatin1String("addVideoSourceConfiguration"), soapWrapper.getLastErrorDescription());
            }
        }
    }

    // Adding video encoder.
    bool vEncoderMatched = profile && profile->VideoEncoderConfiguration
        && profile->VideoEncoderConfiguration->token == info.videoEncoderConfigurationToken;
    if (!vEncoderMatched)
    {
        AddVideoConfigReq request;
        AddVideoConfigResp response;

        request.ProfileToken = info.profileToken;
        request.ConfigurationToken = info.videoEncoderConfigurationToken;

        int soapRes = soapWrapper.addVideoEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK)
        {
            #if defined(PL_ONVIF_DEBUG)
                qCritical() << "QnOnvifStreamReader::addVideoEncoderConfiguration: "
                    << "can't add video encoder to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getLastErrorDescription()
                    << ". URL: " << soapWrapper.endpoint()
                    << ", uniqueId: " << m_onvifRes->getUniqueId();
            #endif
            return CameraDiagnostics::RequestFailedResult(
                QLatin1String("addVideoEncoderConfiguration"), soapWrapper.getLastErrorDescription());
        }
    }

    if (getRole() == Qn::CR_LiveVideo)
    {
        if (!m_onvifRes->audioOutputConfigurationToken().empty())
        {
            auto result = bindTwoWayAudioToProfile(soapWrapper, info.profileToken);
            if (!result)
            {
                const auto errorMessage = result.toString(m_onvifRes->serverModule()->commonModule()->resourcePool());
                NX_WARNING(this,
                    lm("Error binding two way audio to profile %1 for camera %2. Error: %3")
                    .args(info.profileToken, m_onvifRes->getUrl(), errorMessage));
            }
        }

        if(!m_onvifRes->getPtzUrl().isEmpty() && !m_onvifRes->ptzConfigurationToken().empty())
        {
            bool ptzMatched = profile && profile->PTZConfiguration;
            if (!ptzMatched)
            {
                AddPTZConfigReq request;
                AddPTZConfigResp response;

                request.ProfileToken = info.profileToken;
                request.ConfigurationToken = m_onvifRes->ptzConfigurationToken();

                int soapRes = soapWrapper.addPTZConfiguration(request, response);
                if (soapRes != SOAP_OK)
                {
                    #if defined(PL_ONVIF_DEBUG)
                        qCritical() << "QnOnvifStreamReader::addPTZConfiguration: "
                            << "can't add ptz configuration to profile. Gsoap error: "
                            << soapRes << ", description: " << soapWrapper.getLastErrorDescription()
                            << ". URL: " << soapWrapper.endpoint()
                            << ", uniqueId: " << m_onvifRes->getUniqueId();
                    #endif
                    return CameraDiagnostics::RequestFailedResult(
                        QLatin1String("addPTZConfiguration"), soapWrapper.getLastErrorDescription());
                }
            }
        }
    }

    //Adding audio source
    if (!info.audioSourceToken.empty() && !info.audioEncoderConfigurationToken.empty())
    {
        bool audioSrcMatched = profile && profile->AudioSourceConfiguration
            && profile->AudioSourceConfiguration->SourceToken == info.audioSourceToken;
        if (!audioSrcMatched)
        {
            AddAudioSrcConfigReq request;
            AddAudioSrcConfigResp response;

            request.ProfileToken = info.profileToken;
            request.ConfigurationToken = info.audioSourceConfigurationToken;

            int soapRes = soapWrapper.addAudioSourceConfiguration(request, response);
            if (soapRes != SOAP_OK)
            {
                #if defined(PL_ONVIF_DEBUG)
                qCritical() << "QnOnvifStreamReader::addAudioSourceConfiguration: "
                    << "can't add audio source to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getLastErrorDescription()
                    << ". URL: " << soapWrapper.endpoint()
                    << ", uniqueId: " << m_onvifRes->getUniqueId()
                    << "profile=" << info.profileToken << "audioSourceConfigurationToken=" << info.audioSourceConfigurationToken;
                #endif
                // Ignore audio error and do not return here.
            }
        }

        // Adding audio encoder.
        bool audioEncoderMatched = profile && profile->AudioEncoderConfiguration
            && profile->AudioEncoderConfiguration->token == info.audioEncoderConfigurationToken;
        if (!audioEncoderMatched)
        {
            AddAudioConfigReq request;
            AddAudioConfigResp response;

            request.ProfileToken = info.profileToken;
            request.ConfigurationToken = info.audioEncoderConfigurationToken;

            int soapRes = soapWrapper.addAudioEncoderConfiguration(request, response);
            if (soapRes != SOAP_OK)
            {
                #if defined(PL_ONVIF_DEBUG)
                    qCritical() << "QnOnvifStreamReader::addAudioEncoderConfiguration: "
                        << "can't add audio encoder to profile. Gsoap error: "
                        << soapRes << ", description: " << soapWrapper.endpoint()
                        << ". URL: " << soapWrapper.endpoint()
                        << ", uniqueId: " << m_onvifRes->getUniqueId()
                        << "profile=" << info.profileToken
                        << "audioEncoderConfigurationToken=" << info.audioEncoderConfigurationToken;
                #endif
                //result = false; //< Sound can be absent, so ignoring;

            }
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

    // TODO: #vasilenko UTF unuse std::string
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

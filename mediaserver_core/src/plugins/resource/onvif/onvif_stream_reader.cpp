#ifdef ENABLE_ONVIF

#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/utils/log/log.h>
#include "onvif_stream_reader.h"
#include "onvif/soapMediaBindingProxy.h"
#include "onvif_resource.h"

#include "nx/utils/log/log.h"
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <nx/network/http/http_types.h>
#include <utils/media/nalUnits.h>
#include <utils/common/app_info.h>
#include <network/tcp_connection_priv.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/resource_data_structures.h>
#include <core/resource/param.h>
#include <core/resource_management/resource_properties.h>
#include <common/static_common_module.h>

static const int MAX_CAHCE_URL_TIME = 1000 * 300;

struct CameraInfoParams
{
    CameraInfoParams() {}
    QString videoEncoderId;
    QString videoSourceId;
    QString audioEncoderId;
    QString audioSourceId;

    QString profileToken;
};

//
// QnOnvifStreamReader
//

QnOnvifStreamReader::QnOnvifStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_multiCodec(res),
    m_mustNotConfigureResource(false)
{
    m_onvifRes = getResource().dynamicCast<QnPlOnvifResource>();
    m_tmpH264Conf = new onvifXsd__H264Configuration();
}

QnOnvifStreamReader::~QnOnvifStreamReader()
{
    stop();
    delete m_tmpH264Conf;
}

CameraDiagnostics::Result QnOnvifStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
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

    QString streamUrl;
    CameraDiagnostics::Result result = updateCameraAndFetchStreamUrl( &streamUrl, isCameraControlRequired, params );
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
    {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::openStream: can't fetch stream URL for resource with UniqueId: " << m_onvifRes->getUniqueId();
#endif
        return result;
    }

    postStreamConfigureHook();

    auto resData = qnStaticCommon->dataPool()->data(m_onvifRes);
    if (resData.contains(Qn::PREFERRED_AUTH_SCHEME_PARAM_NAME))
    {
        auto authScheme = nx_http::header::AuthScheme::fromString(
            resData.value<QString>(Qn::PREFERRED_AUTH_SCHEME_PARAM_NAME)
                .toLatin1()
                .constData());

        if (authScheme != nx_http::header::AuthScheme::none)
            m_multiCodec.setPrefferedAuthScheme(authScheme);
    }

    m_multiCodec.setRole(getRole());
    m_multiCodec.setRequest(streamUrl);

	m_onvifRes->updateSourceUrl(m_multiCodec.getCurrentStreamUrl(), getRole());

    result = m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED && canChangeStatus())
        m_resource->setStatus(Qn::Unauthorized);
    return result;
}

void QnOnvifStreamReader::setCameraControlDisabled(bool value)
{
    if (!value)
        m_previousStreamParams = QnLiveStreamParams();
    CLServerPushStreamReader::setCameraControlDisabled(value);
}

CameraDiagnostics::Result QnOnvifStreamReader::updateCameraAndFetchStreamUrl( QString* const streamUrl, bool isCameraControlRequired, const QnLiveStreamParams& params )
{
    //QnMutexLocker lock( m_onvifRes->getStreamConfMutex() );


    if (!m_streamUrl.isEmpty() && !isCameraControlRequired)
    {
        *streamUrl = m_streamUrl;
        return CameraDiagnostics::NoErrorResult();
    }

    if (!m_streamUrl.isEmpty() &&
        params == m_previousStreamParams &&
        m_cachedTimer.elapsed() < MAX_CAHCE_URL_TIME)
    {
        *streamUrl = m_streamUrl;
        return m_onvifRes->customStreamConfiguration(getRole());
    }

    m_onvifRes->beforeConfigureStream(getRole());
    CameraDiagnostics::Result result = updateCameraAndFetchStreamUrl(getRole() == Qn::CR_LiveVideo, streamUrl, isCameraControlRequired, params);
    m_onvifRes->customStreamConfiguration(getRole());
    m_onvifRes->afterConfigureStream(getRole());

    if (result.errorCode == CameraDiagnostics::ErrorCode::noError)
    {
        // cache value
        m_streamUrl = *streamUrl;
        if (isCameraControlRequired)
            m_previousStreamParams = params;
        else
            m_previousStreamParams = QnLiveStreamParams();
    }
    return result;
}

CameraDiagnostics::Result QnOnvifStreamReader::updateCameraAndFetchStreamUrl(
    bool isPrimary,
    QString* const streamUrl,
    bool isCameraControlRequired,
    const QnLiveStreamParams& params ) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(
        m_onvifRes->getMediaUrl().toStdString().c_str(),
        auth.user(),
        auth.password(),
        m_onvifRes->getTimeDrift());

    auto proxy = soapWrapper.getProxy();
    auto onvifRes = m_resource.dynamicCast<QnPlOnvifResource>();

    if (onvifRes)
    {
        proxy->soap->recv_timeout = onvifRes->getOnvifRequestsRecieveTimeout();
        proxy->soap->send_timeout = onvifRes->getOnvifRequestsSendTimeout();
    }

    CameraInfoParams info;

    if (QnResource::isStopping())
        return CameraDiagnostics::ServerTerminatedResult();

    CameraDiagnostics::Result result = fetchUpdateVideoEncoder(soapWrapper, info, isPrimary, isCameraControlRequired, params);
    if( !result  )
        return result;
    info.videoSourceId = m_onvifRes->getVideoSourceId();

    if (QnResource::isStopping())
        return CameraDiagnostics::ServerTerminatedResult();

    fetchUpdateAudioEncoder(soapWrapper, info, isPrimary, isCameraControlRequired);

    if (QnResource::isStopping())
        return CameraDiagnostics::ServerTerminatedResult();

    result = fetchUpdateProfile(soapWrapper, info, isPrimary, isCameraControlRequired);
    if( !result ) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "ONVIF camera " << getResource()->getUrl() << ": can't prepare profile";
#endif
        return result;
    }

    if (QnResource::isStopping())
        return CameraDiagnostics::ServerTerminatedResult();

    ////Printing chosen profile
    //if (cl_log.logLevel() >= cl_logDEBUG1) {
    //    printProfile(*info.finalProfile, isPrimary);
    //}
    result = fetchStreamUrl( soapWrapper, info.profileToken, isPrimary, streamUrl );
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    NX_LOG(lit("got stream URL %1 for camera %2 for role %3")
        .arg(*streamUrl)
        .arg(m_resource->getUrl())
        .arg(getRole()),
        cl_logINFO);

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

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 2 && !rez; ++i)
        rez = m_multiCodec.getNextData();

    if (!rez)
        closeStream();

    return rez;
}

void QnOnvifStreamReader::printProfile(const Profile& profile, bool isPrimary) const
{
#ifdef PL_ONVIF_DEBUG
    qDebug() << "ONVIF device (UniqueId: " << m_onvifRes->getUniqueId() << ") has the following "
             << (isPrimary? "primary": "secondary") << " profile:";
    qDebug() << "Name: " << profile.Name.c_str();
    qDebug() << "Token: " << profile.token.c_str();
    qDebug() << "Encoder Name: " << profile.VideoEncoderConfiguration->Name.c_str();
    qDebug() << "Encoder Token: " << profile.VideoEncoderConfiguration->token.c_str();
    qDebug() << "Quality: " << profile.VideoEncoderConfiguration->Quality;
    qDebug() << "Resolution: " << profile.VideoEncoderConfiguration->Resolution->Width << "x"
             << profile.VideoEncoderConfiguration->Resolution->Height;
    qDebug() << "FPS: " << profile.VideoEncoderConfiguration->RateControl->FrameRateLimit;
#else
    Q_UNUSED(profile)
    Q_UNUSED(isPrimary)
#endif
}

bool QnOnvifStreamReader::executePreConfigurationRequests()
{
    auto resData = qnStaticCommon->dataPool()->data(m_onvifRes);

    auto requests = resData.value<QnHttpConfigureRequestList>(
        Qn::PRE_SRTEAM_CONFIGURE_REQUESTS_PARAM_NAME);

    if (requests.empty())
        return true;

    CLSimpleHTTPClient http(
        m_onvifRes->getHostAddress(),
        QUrl(m_onvifRes->getUrl()).port(nx_http::DEFAULT_HTTP_PORT),
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

void QnOnvifStreamReader::updateVideoEncoder(VideoEncoder& encoder, bool isPrimary, const QnLiveStreamParams& streamParams) const
{
    QnLiveStreamParams params = streamParams;
    auto resData = qnStaticCommon->dataPool()->data(m_onvifRes);
    bool useEncodingInterval = resData.value<bool>
        (Qn::CONTROL_FPS_VIA_ENCODING_INTERVAL_PARAM_NAME);

    encoder.Encoding = m_onvifRes->getCodec(isPrimary) == QnPlOnvifResource::H264? onvifXsd__VideoEncoding__H264: onvifXsd__VideoEncoding__JPEG;
    //encoder.Name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;

    Qn::StreamQuality quality = params.quality;
    QSize resolution = isPrimary? m_onvifRes->getPrimaryResolution(): m_onvifRes->getSecondaryResolution();

    if (encoder.Encoding == onvifXsd__VideoEncoding__H264)
    {
        if (encoder.H264 == 0)
            encoder.H264 = m_tmpH264Conf;

        encoder.H264->GovLength = m_onvifRes->getGovLength();
        int profile = isPrimary? m_onvifRes->getPrimaryH264Profile(): m_onvifRes->getSecondaryH264Profile();
        if (profile != -1)
            encoder.H264->H264Profile = onvifXsd__H264Profile(profile);
        if (encoder.RateControl)
            encoder.RateControl->EncodingInterval = 1;
    }

    if (!encoder.RateControl)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: RateControl is NULL. UniqueId: " << m_onvifRes->getUniqueId();
#endif
    }
    else
    {
        if (!useEncodingInterval)
        {
            encoder.RateControl->FrameRateLimit = params.fps;
        }
        else
        {
            int fpsBase = resData.value<int>(Qn::FPS_BASE_PARAM_NAME);
            params.fps = m_onvifRes->getClosestAvailableFps(params.fps);
            encoder.RateControl->FrameRateLimit = fpsBase;
            encoder.RateControl->EncodingInterval = static_cast<int>(
                fpsBase / params.fps + 0.5);
        }

        encoder.RateControl->BitrateLimit = m_onvifRes
            ->suggestBitrateKbps(resolution, params, getRole());
    }


    if (quality != Qn::QualityPreSet)
    {
        encoder.Quality = m_onvifRes->innerQualityToOnvif(quality);
    }

    if (!encoder.Resolution)
    {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: Resolution is NULL. UniqueId: " << m_onvifRes->getUniqueId();
#endif
    }
    else
    {

        if (resolution == EMPTY_RESOLUTION_PAIR)
        {
#ifdef PL_ONVIF_DEBUG
            qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: : Can't determine (" << (isPrimary? "primary": "secondary")
                << ") resolution " << "for ONVIF device (UniqueId: " << m_onvifRes->getUniqueId() << "). Default resolution will be used.";
#endif
        }
        else
        {

            encoder.Resolution->Width = resolution.width();
            encoder.Resolution->Height = resolution.height();
        }
    }
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchStreamUrl(MediaSoapWrapper& soapWrapper, const QString& profileToken, bool isPrimary, QString* const mediaUrl) const
{
    Q_UNUSED( isPrimary );

    StreamUriResp response;
    StreamUriReq request;
    onvifXsd__StreamSetup streamSetup;
    onvifXsd__Transport transport;

    request.StreamSetup = &streamSetup;
    request.StreamSetup->Transport = &transport;
    request.StreamSetup->Stream = onvifXsd__StreamType__RTP_Unicast;
    request.StreamSetup->Transport->Tunnel = 0;
    request.StreamSetup->Transport->Protocol = onvifXsd__TransportProtocol__RTSP;
    request.ProfileToken = profileToken.toStdString();

    int soapRes = soapWrapper.getStreamUri(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::fetchStreamUrl (primary stream = " << isPrimary
            << "): can't get stream URL of ONVIF device (URL: " << m_onvifRes->getMediaUrl()
            << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: SOAP request failed. GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
#endif
        return CameraDiagnostics::RequestFailedResult( QLatin1String("getStreamUri"), soapWrapper.getLastError() );
    }

    if (!response.MediaUri) {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::fetchStreamUrl (primary stream = "  << isPrimary
            << "): can't get stream URL of ONVIF device (URL: " << m_onvifRes->getMediaUrl()
            << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: got empty response.";
#endif
        return CameraDiagnostics::RequestFailedResult( QLatin1String("getStreamUri"), QLatin1String("empty media uri") );
    }

#ifdef PL_ONVIF_DEBUG
    qDebug() << "URL of ONVIF device stream (UniqueId: " << m_onvifRes->getUniqueId()
        << ") successfully fetched: " << response.MediaUri->Uri.c_str();
#endif

    QUrl relutUrl(m_onvifRes->fromOnvifDiscoveredUrl(response.MediaUri->Uri, false));


    if (relutUrl.host().size()==0)
    {
        QString temp = relutUrl.toString();
        relutUrl.setHost(m_onvifRes->getHostAddress());
#ifdef PL_ONVIF_DEBUG
        qCritical() << "pure URL(error) " << temp<< " Trying to fix: " << relutUrl.toString();
#endif
    }

    *mediaUrl = relutUrl.toString();
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateVideoEncoder(
    MediaSoapWrapper& soapWrapper,
    CameraInfoParams& info,
    bool isPrimary,
    bool isCameraControlRequired,
    const QnLiveStreamParams& params) const
{
    VideoConfigsReq request;
    VideoConfigsResp response;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK)
    {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::fetchUpdateVideoEncoder: can't get video encoders from camera ("
            << (isPrimary? "primary": "secondary") << ") Gsoap error: " << soapRes << ". Description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
#endif
        if (soapWrapper.isNotAuthenticated() && canChangeStatus()) {
            m_onvifRes->setStatus(Qn::Unauthorized);
            return CameraDiagnostics::NotAuthorisedResult( soapWrapper.getEndpointUrl() );
        }
        return CameraDiagnostics::RequestFailedResult( QLatin1String("getVideoEncoderConfigurations"), soapWrapper.getLastError() );
    }

    VideoEncoder* encoderParamsToSet = fetchVideoEncoder(response, isPrimary);
    if( !encoderParamsToSet )
        return CameraDiagnostics::RequestFailedResult( QLatin1String("fetchVideoEncoder"), QString() );

    // TODO: #vasilenko UTF unuse std::string
    info.videoEncoderId = QString::fromStdString(encoderParamsToSet->token);

    if (!isCameraControlRequired || m_mustNotConfigureResource)
        return CameraDiagnostics::NoErrorResult(); // do not update video encoder params

    updateVideoEncoder(*encoderParamsToSet, isPrimary, params);

    int triesLeft = m_onvifRes->getMaxOnvifRequestTries();
    CameraDiagnostics::Result result = CameraDiagnostics::UnknownErrorResult();
    while ((result.errorCode != CameraDiagnostics::ErrorCode::noError) && --triesLeft >= 0) {
        result = m_onvifRes->sendVideoEncoderToCamera(*encoderParamsToSet);
        if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
            msleep(300);
    }

    return result;
}

VideoEncoder* QnOnvifStreamReader::fetchVideoEncoder(VideoConfigsResp& response, bool isPrimary) const
{
    std::vector<VideoEncoder*>::const_iterator iter = response.Configurations.begin();
    QString id = isPrimary  ? m_onvifRes->getPrimaryVideoEncoderId() : m_onvifRes->getSecondaryVideoEncoderId();

    for (;iter != response.Configurations.end(); ++iter)
    {
        VideoEncoder* conf = *iter;
        // TODO: #vasilenko UTF unuse std::string
        if (conf && id == QString::fromStdString(conf->token)) {
            /*
            if (!isPrimary && m_onvifRes->forcePrimaryEncoderCodec())
            {
                // get codec list from primary encoder, but use ID of secondary encoder
                conf->token = m_onvifRes->getSecondaryVideoEncoderId().toStdString();
            }
            */
            return conf;
        }
    }

    return 0;
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateProfile(
    MediaSoapWrapper& soapWrapper,
    CameraInfoParams& info,
    bool isPrimary,
    bool isCameraControlRequired) const
{
    ProfilesReq request;
    ProfilesResp response;

    auto resData = qnStaticCommon->dataPool()->data(m_onvifRes);
    bool useExistingProfiles = resData.value<bool>(
        Qn::USE_EXISTING_ONVIF_PROFILES_PARAM_NAME);

    int soapRes = soapWrapper.getProfiles(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::fetchUpdateProfile: can't get profiles from camera ("
            << (isPrimary? "primary": "secondary") << "). Gsoap error: " << soapRes << ". Description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
#endif
        return CameraDiagnostics::RequestFailedResult( QLatin1String("getProfiles"), soapWrapper.getLastError() );
    }

    Profile* profile = fetchExistingProfile(response, isPrimary, info);
    if (profile) {
        info.profileToken = QString::fromStdString(profile->token);
    }
    else {
        QString noProfileName = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_NAME) : QLatin1String(NETOPTIX_SECONDARY_NAME);
        info.profileToken = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_TOKEN) : QLatin1String(NETOPTIX_SECONDARY_TOKEN);
        CameraDiagnostics::Result result = createNewProfile(noProfileName, info.profileToken);
        if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
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

CameraDiagnostics::Result QnOnvifStreamReader::createNewProfile(const QString& name, const QString& token) const
{
    QAuthenticator auth = m_onvifRes->getAuth();
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_onvifRes->getTimeDrift());
    std::string stdStrToken = token.toStdString();

    CreateProfileReq request;
    CreateProfileResp response;

    request.Name = name.toStdString();
    request.Token = &stdStrToken;

    int soapRes = soapWrapper.createProfile(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::sendProfileToCamera: can't create profile " << request.Name.c_str() << "Gsoap error: "
            << soapRes << ", description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
#endif
        return CameraDiagnostics::RequestFailedResult( QLatin1String("createProfile"), soapWrapper.getLastError() );
    }
    return CameraDiagnostics::NoErrorResult();
}

Profile* QnOnvifStreamReader::fetchExistingProfile(const ProfilesResp& response, bool isPrimary, CameraInfoParams& info) const
{
    QStringList availableProfiles;
    QString noProfileName = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_NAME) : QLatin1String(NETOPTIX_SECONDARY_NAME);
    QString filteredProfileName = isPrimary ? QLatin1String(NETOPTIX_SECONDARY_NAME) : QLatin1String(NETOPTIX_PRIMARY_NAME);

    std::vector<Profile*>::const_iterator iter = response.Profiles.begin();
    for (; iter != response.Profiles.end(); ++iter) {
        Profile* profile = *iter;
        if (profile->token.empty())
            continue;
        else if (profile->Name == noProfileName.toStdString())
            return profile;
        else if (profile->Name == filteredProfileName.toStdString())
            continue;
        availableProfiles << QString::fromStdString(profile->token);
    }

    // try to select profile with necessary VideoEncoder to avoid change video encoder inside profile (some cameras doesn't support it. It can be checked via getCompatibleVideoEncoders from profile)
    iter = response.Profiles.begin();
    for (; iter != response.Profiles.end(); ++iter)
    {
        Profile* profile = *iter;
        if (!profile || !availableProfiles.contains(QString::fromStdString(profile->token)))
            continue;
        bool vSourceMatched = profile->VideoSourceConfiguration && profile->VideoSourceConfiguration->token == info.videoSourceId.toStdString();
        bool vEncoderMatched = profile->VideoEncoderConfiguration && profile->VideoEncoderConfiguration->token == info.videoEncoderId.toStdString();
        if (vSourceMatched && vEncoderMatched)
            return profile;
    }

    // try to select profile by is lexicographical order
    std::sort(availableProfiles.begin(), availableProfiles.end());
    int profileIndex = isPrimary ? 0 : 1;
    profileIndex += m_onvifRes->getChannel()* (m_onvifRes->hasDualStreaming() ? 2 : 1);
    if (availableProfiles.size() <= profileIndex)
        return 0; // no existing profile matched


    QString reqProfileToken = availableProfiles[profileIndex];
    for (iter = response.Profiles.begin(); iter != response.Profiles.end(); ++iter) {
        Profile* profile = *iter;
        if (profile->token == reqProfileToken.toStdString()) {
            return profile;
        }
    }

    return 0;
}

CameraDiagnostics::Result QnOnvifStreamReader::sendProfileToCamera(CameraInfoParams& info, Profile* profile) const
{
    QAuthenticator auth = m_onvifRes->getAuth();
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_onvifRes->getTimeDrift());

    if (!profile)
        return CameraDiagnostics::NoErrorResult();

    bool vSourceMatched = profile && profile->VideoSourceConfiguration && profile->VideoSourceConfiguration->token == info.videoSourceId.toStdString();
    if (!vSourceMatched)
    {
        AddVideoSrcConfigReq request;
        AddVideoSrcConfigResp response;

        request.ProfileToken = info.profileToken.toStdString();
        request.ConfigurationToken = info.videoSourceId.toStdString();

        int soapRes = soapWrapper.addVideoSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
            qCritical() << "QnOnvifStreamReader::addVideoSourceConfiguration: can't add video source to profile. Gsoap error: "
                << soapRes << ", description: " << soapWrapper.getLastError()
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId() <<
                "current vSourceID=" << profile->VideoSourceConfiguration->token.data() << "requested vSourceID=" << info.videoSourceId;
#endif
            if (m_onvifRes->getMaxChannels() > 1)
                return CameraDiagnostics::RequestFailedResult( QLatin1String("addVideoSourceConfiguration"), soapWrapper.getLastError() );
        }
    }

    //Adding video encoder
    bool vEncoderMatched = profile && profile->VideoEncoderConfiguration && profile->VideoEncoderConfiguration->token == info.videoEncoderId.toStdString();
    if (!vEncoderMatched)
    {
        AddVideoConfigReq request;
        AddVideoConfigResp response;

        request.ProfileToken = info.profileToken.toStdString();
        request.ConfigurationToken = info.videoEncoderId.toStdString();

        int soapRes = soapWrapper.addVideoEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
            qCritical() << "QnOnvifStreamReader::addVideoEncoderConfiguration: can't add video encoder to profile. Gsoap error: "
                << soapRes << ", description: " << soapWrapper.getLastError()
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
#endif
            return CameraDiagnostics::RequestFailedResult( QLatin1String("addVideoEncoderConfiguration"), soapWrapper.getLastError() );
        }
    }

    if (getRole() == Qn::CR_LiveVideo)
    {
        if(!m_onvifRes->getPtzUrl().isEmpty() && !m_onvifRes->getPtzConfigurationToken().isEmpty())
        {
            bool ptzMatched = profile && profile->PTZConfiguration;
            if (!ptzMatched)
            {
                AddPTZConfigReq request;
                AddPTZConfigResp response;

                request.ProfileToken = info.profileToken.toStdString();
                request.ConfigurationToken = m_onvifRes->getPtzConfigurationToken().toStdString();

                int soapRes = soapWrapper.addPTZConfiguration(request, response);
                if (soapRes != SOAP_OK)
                {
#ifdef PL_ONVIF_DEBUG
                    qCritical() << "QnOnvifStreamReader::addPTZConfiguration: can't add ptz configuration to profile. Gsoap error: "
                        << soapRes << ", description: " << soapWrapper.getLastError()
                        << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
#endif
                    return CameraDiagnostics::RequestFailedResult( QLatin1String("addPTZConfiguration"), soapWrapper.getLastError() );
                }
            }
        }
    }

    //Adding audio source
    if (!info.audioSourceId.isEmpty() && !info.audioEncoderId.isEmpty())
    {
        bool audioSrcMatched = profile && profile->AudioSourceConfiguration && profile->AudioSourceConfiguration->token == info.audioSourceId.toStdString();
        if (!audioSrcMatched) {
            AddAudioSrcConfigReq request;
            AddAudioSrcConfigResp response;

            request.ProfileToken = info.profileToken.toStdString();
            request.ConfigurationToken = info.audioSourceId.toStdString();

            int soapRes = soapWrapper.addAudioSourceConfiguration(request, response);
            if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
                qCritical() << "QnOnvifStreamReader::addAudioSourceConfiguration: can't add audio source to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getLastError()
                    << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId()
                    << "profile=" << info.profileToken << "audioSourceId=" << info.audioSourceId;
#endif
                // return false; // ignore audio error
            }
        }

        //Adding audio encoder
        bool audioEncoderMatched = profile && profile->AudioEncoderConfiguration && profile->AudioEncoderConfiguration->token == info.audioEncoderId.toStdString();
        if (!audioEncoderMatched)
        {
            AddAudioConfigReq request;
            AddAudioConfigResp response;

            request.ProfileToken = info.profileToken.toStdString();
            request.ConfigurationToken = info.audioEncoderId.toStdString();

            int soapRes = soapWrapper.addAudioEncoderConfiguration(request, response);
            if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
                qCritical() << "QnOnvifStreamReader::addAudioEncoderConfiguration: can't add audio encoder to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getEndpointUrl()
                    << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId()
                    << "profile=" << info.profileToken << "audioEncoderId=" << info.audioEncoderId;
#endif
                //result = false; //Sound can be absent, so ignoring;

            }
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateAudioEncoder(MediaSoapWrapper& soapWrapper, CameraInfoParams& info, bool isPrimary, bool isCameraControlRequired) const
{
    AudioConfigsReq request;
    AudioConfigsResp response;

    int soapRes = soapWrapper.getAudioEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qCritical() << "QnOnvifStreamReader::fetchUpdateAudioEncoder: can't get audio encoders from camera ("
            << (isPrimary? "primary": "secondary")
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
#endif
        return CameraDiagnostics::RequestFailedResult( QLatin1String("getAudioEncoderConfigurations"), soapWrapper.getLastError() );
    }

    AudioEncoder* result = fetchAudioEncoder(response, isPrimary);
    if( !result )
        return CameraDiagnostics::RequestFailedResult( QLatin1String("fetchAudioEncoder"), QString() );

    // TODO: #vasilenko UTF unuse std::string
    info.audioEncoderId = QString::fromStdString(result->token);

    if (!isCameraControlRequired)
        return CameraDiagnostics::NoErrorResult();    // do not update audio encoder params

    updateAudioEncoder(*result, isPrimary);
    return sendAudioEncoderToCamera(*result);
}

AudioEncoder* QnOnvifStreamReader::fetchAudioEncoder(AudioConfigsResp& response, bool /*isPrimary*/) const
{
    QString id = m_onvifRes->getAudioEncoderId();
    if (id.isEmpty()) {
        return 0;
    }

    std::vector<onvifXsd__AudioEncoderConfiguration*>::const_iterator iter = response.Configurations.begin();
    for (; iter != response.Configurations.end(); ++iter) {
        // TODO: #vasilenko UTF unuse std::string
        if (*iter && id == QString::fromStdString((*iter)->token)) {
            return *iter;
        }
    }

    return 0;
}

void QnOnvifStreamReader::updateAudioEncoder(AudioEncoder& encoder, bool isPrimary) const
{
    Q_UNUSED(isPrimary)
    //encoder.Name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;

    QnPlOnvifResource::AUDIO_CODECS codec = m_onvifRes->getAudioCodec();
    if (codec <= QnPlOnvifResource::AUDIO_NONE || codec >= QnPlOnvifResource::SIZE_OF_AUDIO_CODECS) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::updateAudioEncoder: codec type is undefined. UniqueId: " << m_onvifRes->getUniqueId();
#endif
        return;
    }

    //onvifXsd__AudioEncoding__G711 = 0, onvifXsd__AudioEncoding__G726 = 1, onvifXsd__AudioEncoding__AAC = 2
    switch (codec)
    {
        case QnPlOnvifResource::G711:
            encoder.Encoding = onvifXsd__AudioEncoding__G711;
            break;
        case QnPlOnvifResource::G726:
            encoder.Encoding = onvifXsd__AudioEncoding__G726;
            break;
        case QnPlOnvifResource::AAC:
            encoder.Encoding = onvifXsd__AudioEncoding__AAC;
            break;
        case QnPlOnvifResource::AMR:
            encoder.Encoding = onvifXsd__AudioEncoding__AMR;
            break;
        default:
#ifdef PL_ONVIF_DEBUG
            qWarning() << "QnOnvifStreamReader::updateAudioEncoder: codec type is unknown: " << codec
                << ". UniqueId: " << m_onvifRes->getUniqueId();
#endif
            return;
    }

    //value 0 means "undefined"
    int bitrate = m_onvifRes->getAudioBitrate();
    if (bitrate) {
        encoder.Bitrate = bitrate;
    }

    //value 0 means "undefined"
    int samplerate = m_onvifRes->getAudioSamplerate();
    if (samplerate) {
        encoder.SampleRate = samplerate;
    }
}

CameraDiagnostics::Result QnOnvifStreamReader::sendAudioEncoderToCamera(AudioEncoder& encoder) const
{
    QAuthenticator auth = m_onvifRes->getAuth();
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user(), auth.password(), m_onvifRes->getTimeDrift());

    SetAudioConfigReq request;
    SetAudioConfigResp response;
    request.Configuration = &encoder;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setAudioEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK) {
#ifdef PL_ONVIF_DEBUG
        qWarning() << "QnOnvifStreamReader::sendAudioEncoderToCamera: can't set required values into ONVIF physical device (URL: "
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId()
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError()
            << "configuration token=" << request.Configuration->token.c_str();
#endif
        return CameraDiagnostics::RequestFailedResult( QLatin1String("setAudioEncoderConfiguration"), soapWrapper.getLastError() );
    }

    return CameraDiagnostics::NoErrorResult();
}

AudioSource* QnOnvifStreamReader::fetchAudioSource(AudioSrcConfigsResp& response, bool /*isPrimary*/) const
{
    QString id = m_onvifRes->getAudioSourceId();
    if (id.isEmpty()) {
        return 0;
    }

    std::vector<AudioSource*>::const_iterator it = response.Configurations.begin();
    for (; it != response.Configurations.end(); ++it) {
        // TODO: #vasilenko UTF unuse std::string
        if (*it && id == QString::fromStdString((*it)->token)) {
            return *it;
        }
    }

    return 0;
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

bool QnOnvifStreamReader::secondaryResolutionIsLarge() const
{
    return m_onvifRes->secondaryResolutionIsLarge();
}

QnConstResourceVideoLayoutPtr QnOnvifStreamReader::getVideoLayout() const
{
    return m_multiCodec.getVideoLayout();
}

void QnOnvifStreamReader::setMustNotConfigureResource(bool mustNotConfigureResource)
{
    m_mustNotConfigureResource = mustNotConfigureResource;
}

#endif //ENABLE_ONVIF

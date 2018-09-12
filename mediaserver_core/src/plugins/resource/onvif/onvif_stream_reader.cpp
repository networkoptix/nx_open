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
#include <common/static_common_module.h>

#include <core/resource_management/resource_data_pool.h>
#include <core/resource/resource_data_structures.h>
#include <core/resource/param.h>
#include <core/resource_management/resource_properties.h>

#include "onvif/soapMediaBindingProxy.h"
#include "onvif_resource.h"

static const int MAX_CAHCE_URL_TIME = 1000 * 15;

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

QnOnvifStreamReader::QnOnvifStreamReader(const QnPlOnvifResourcePtr& res):
    CLServerPushStreamReader(res),
    m_multiCodec(res),
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

    QString streamUrl;
    CameraDiagnostics::Result result = updateCameraAndFetchStreamUrl(
        &streamUrl, isCameraControlRequired, params);
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
    {
        #if defined(PL_ONVIF_DEBUG)
            qCritical() << "QnOnvifStreamReader::openStream: "
                << "can't fetch stream URL for resource with UniqueId: "
                << m_onvifRes->getUniqueId();
        #endif
        return result;
    }

    postStreamConfigureHook();

    auto resData = qnStaticCommon->dataPool()->data(m_onvifRes);
    if (resData.contains(Qn::PREFERRED_AUTH_SCHEME_PARAM_NAME))
    {
        auto authScheme = nx::network::http::header::AuthScheme::fromString(
            resData.value<QString>(Qn::PREFERRED_AUTH_SCHEME_PARAM_NAME)
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
    info.videoSourceId = m_onvifRes->getVideoSourceId();

    // If audio encoder updating fails we ignore it.
    fetchUpdateAudioEncoder(&info, isPrimary, isCameraControlRequired);

    result = fetchUpdateProfile(info, isPrimary, isCameraControlRequired);
    if (!result)
    {
        // LOG: qWarning() << "ONVIF camera " << getResource()->getUrl() << ": can't prepare profile";
        return result;
    }

    ////Printing chosen profile
    //if (cl_log.logLevel() >= cl_logDEBUG1) {
    //    printProfile(*info.finalProfile, isPrimary);
    //}

    {
        QAuthenticator auth(m_onvifRes->getAuth());
        MediaSoapWrapper soapWrapper(
            m_onvifRes->onvifTimeouts(),
            m_onvifRes->getMediaUrl().toStdString().c_str(),
            auth.user(),
            auth.password(),
            m_onvifRes->getTimeDrift());

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

CameraDiagnostics::Result QnOnvifStreamReader::fetchStreamUrl(MediaSoapWrapper& soapWrapper,
    const QString& profileToken, bool isPrimary, QString* const mediaUrl) const
{
    Q_UNUSED( isPrimary );

    StreamUriResp response;
    StreamUriReq request;
    onvifXsd__StreamSetup streamSetup;
    onvifXsd__Transport transport;

    request.StreamSetup = &streamSetup;
    request.StreamSetup->Transport = &transport;
    request.StreamSetup->Stream = onvifXsd__StreamType::RTP_Unicast;
    request.StreamSetup->Transport->Tunnel = 0;
    request.StreamSetup->Transport->Protocol = onvifXsd__TransportProtocol::RTSP;
    request.ProfileToken = profileToken.toStdString();

    int soapRes = soapWrapper.getStreamUri(request, response);
    if (soapRes != SOAP_OK) {
    #if defined(PL_ONVIF_DEBUG)
        qCritical() << "QnOnvifStreamReader::fetchStreamUrl (primary stream = " << isPrimary
            << "): can't get stream URL of ONVIF device (URL: " << m_onvifRes->getMediaUrl()
            << ", UniqueId: " << m_onvifRes->getUniqueId()
            << "). Root cause: SOAP request failed. GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
    #endif
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getStreamUri"), soapWrapper.getLastError());
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
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateVideoEncoder(
    CameraInfoParams* outInfo,
    bool isPrimary,
    bool isCameraControlRequired,
    const QnLiveStreamParams& params) const
{
    if (QnResource::isStopping())
        return CameraDiagnostics::ServerTerminatedResult();

    if (!isCameraControlRequired || m_mustNotConfigureResource)
        return CameraDiagnostics::NoErrorResult(); //< Do not update video encoder configuration.

    if (m_onvifRes->m_serviceUrls.media2ServiceUrl.isEmpty())
    {
        // Use old Media1 interface.
        Media::VideoEncoderConfigurations veConfigurations(m_onvifRes->makeRequestParams());
        veConfigurations.receiveBySoap();
        if (!veConfigurations)
        {
            //LOG.
            if (veConfigurations.innerWrapper().isNotAuthenticated())
            {
                return CameraDiagnostics::NotAuthorisedResult(veConfigurations.endpoint());
            }
            return veConfigurations.requestFailedResult();
        }

        // The pointer currentVEConfig points to the object that is valid until
        // veConfigurations destructor is called.
        onvifXsd__VideoEncoderConfiguration* currentVEConfig = selectVideoEncoderConfig(
            veConfigurations.getEphemeralReference().Configurations, isPrimary);

        if (!currentVEConfig)
            return CameraDiagnostics::RequestFailedResult("selectVideoEncoderConfig", QString());

        // TODO: #vasilenko UTF unuse std::string
        outInfo->videoEncoderId = QString::fromStdString(currentVEConfig->token);

        auto streamIndex = isPrimary ? Qn::StreamIndex::primary : Qn::StreamIndex::secondary;
        m_onvifRes->updateVideoEncoder(*currentVEConfig, streamIndex, params);

        int triesLeft = m_onvifRes->getMaxOnvifRequestTries();
        CameraDiagnostics::Result result = CameraDiagnostics::UnknownErrorResult();
        while ((result.errorCode != CameraDiagnostics::ErrorCode::noError) && --triesLeft >= 0)
        {
            result = m_onvifRes->sendVideoEncoderToCameraEx(*currentVEConfig, streamIndex, params);
            if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
                msleep(300);
        }

        return result;
    }
    else
    {
        // Use old Media2 interface.
        Media2::VideoEncoderConfigurations veConfigurations(m_onvifRes->makeRequestParams());
        veConfigurations.receiveBySoap();
        if (!veConfigurations)
        {
            //LOG.
            if (veConfigurations.innerWrapper().isNotAuthenticated() && canChangeStatus())
            {
                m_onvifRes->setStatus(Qn::Unauthorized);
                return CameraDiagnostics::NotAuthorisedResult(veConfigurations.endpoint());
            }
            return veConfigurations.requestFailedResult();
        }

        // The pointer currentVEConfig points to the object that is valid until
        // veConfigurations destructor is called.
        onvifXsd__VideoEncoder2Configuration* currentVEConfig = selectVideoEncoder2Config(
            veConfigurations.getEphemeralReference().Configurations, isPrimary);

        if (!currentVEConfig)
            return CameraDiagnostics::RequestFailedResult("selectVideoEncoder2Config", QString());

        outInfo->videoEncoderId = QString::fromStdString(currentVEConfig->token);

        auto streamIndex = isPrimary ? Qn::StreamIndex::primary : Qn::StreamIndex::secondary;
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
    const QString id = isPrimary
        ? m_onvifRes->primaryVideoCapabilities().id
        : m_onvifRes->secondaryVideoCapabilities().id;

    for (auto itr = configs.begin(); itr != configs.end(); ++itr)
    {
        onvifXsd__VideoEncoderConfiguration* conf = *itr;
        if (conf && id == QString::fromStdString(conf->token))
            return conf;
    }

    return nullptr;
}

onvifXsd__VideoEncoder2Configuration* QnOnvifStreamReader::selectVideoEncoder2Config(
    std::vector<onvifXsd__VideoEncoder2Configuration *>& configs, bool isPrimary) const
{
    const QString id = isPrimary
        ? m_onvifRes->primaryVideoCapabilities().id
        : m_onvifRes->secondaryVideoCapabilities().id;

    for (auto itr = configs.begin(); itr != configs.end(); ++itr)
    {
        onvifXsd__VideoEncoder2Configuration* conf = *itr;
        if (conf && id == QString::fromStdString(conf->token))
            return conf;
    }

    return nullptr;
}

CameraDiagnostics::Result QnOnvifStreamReader::fetchUpdateProfile(
    CameraInfoParams& info,
    bool isPrimary,
    bool isCameraControlRequired) const
{
    if (QnResource::isStopping())
        return CameraDiagnostics::ServerTerminatedResult();

    auto resData = qnStaticCommon->dataPool()->data(m_onvifRes);
    bool useExistingProfiles = resData.value<bool>(
        Qn::USE_EXISTING_ONVIF_PROFILES_PARAM_NAME);

    Media::Profiles profiles(m_onvifRes->makeRequestParams());
    profiles.receiveBySoap();
    if (!profiles)
    {
        // LOG.
        return profiles.requestFailedResult();
    }

    onvifXsd__Profile* profile = selectExistingProfile(
        profiles.getEphemeralReference().Profiles, isPrimary, info);
    if (profile)
    {
        info.profileToken = QString::fromStdString(profile->token);
    }
    else
    {
        QString noProfileName = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_NAME) : QLatin1String(NETOPTIX_SECONDARY_NAME);
        info.profileToken = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_TOKEN) : QLatin1String(NETOPTIX_SECONDARY_TOKEN);
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
    const QString& name, const QString& token) const
{
    Media::ProfileCreator profileCreator(m_onvifRes->makeRequestParams());
    std::string stdStrToken = token.toStdString();

    Media::ProfileCreator::Request request;
    request.Name = name.toStdString();
    request.Token = &stdStrToken;
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
            && profile->VideoSourceConfiguration->token == info.videoSourceId.toStdString();
        bool vEncoderMatched = profile->VideoEncoderConfiguration
            && profile->VideoEncoderConfiguration->token == info.videoEncoderId.toStdString();
        if (vSourceMatched && vEncoderMatched)
            return profile;
    }

    // try to select profile by is lexicographical order
    std::sort(availableProfiles.begin(), availableProfiles.end());
    int profileIndex = isPrimary ? 0 : 1;
    profileIndex += m_onvifRes->getChannel()* (m_onvifRes->hasDualStreamingInternal() ? 2 : 1);
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
    MediaSoapWrapper& soapWrapper, const QString& profileToken) const
{
    AddAudioOutputConfigurationReq addAudioOutputConfigurationRequest;
    AddAudioOutputConfigurationResp addAudioOutputConfigurationResponse;

    addAudioOutputConfigurationRequest.ProfileToken = profileToken.toStdString();
    addAudioOutputConfigurationRequest.ConfigurationToken =
        m_onvifRes->audioOutputConfigurationToken().toStdString();
    int soapRes = soapWrapper.addAudioOutputConfiguration(
        addAudioOutputConfigurationRequest, addAudioOutputConfigurationResponse);
    if (soapRes != SOAP_OK)
    {
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("addAudioOutputConfiguration"), soapWrapper.getLastError());
    }

    GetCompatibleAudioDecoderConfigurationsReq audioDecodersRequest;
    GetCompatibleAudioDecoderConfigurationsResp audioDecodersResponse;
    audioDecodersRequest.ProfileToken = profileToken.toStdString();
    soapRes = soapWrapper.getCompatibleAudioDecoderConfigurations(audioDecodersRequest, audioDecodersResponse);
    if (soapRes != SOAP_OK)
    {
        return CameraDiagnostics::RequestFailedResult(
            QLatin1String("getCompatibleAudioDecoderConfigurations"), soapWrapper.getLastError());
    }
    if (!audioDecodersResponse.Configurations.empty() && audioDecodersResponse.Configurations[0])
    {
        AddAudioDecoderConfigurationReq addDecoderRequest;
        AddAudioDecoderConfigurationResp addDecoderResponse;
        addDecoderRequest.ProfileToken = profileToken.toStdString();
        auto configuration = audioDecodersResponse.Configurations[0];
        addDecoderRequest.ConfigurationToken = configuration->token;

        soapRes = soapWrapper.addAudioDecoderConfiguration(addDecoderRequest, addDecoderResponse);
        if (soapRes != SOAP_OK)
        {
            return CameraDiagnostics::RequestFailedResult(
                QLatin1String("addAudioDecoderConfiguration"), soapWrapper.getLastError());
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifStreamReader::sendProfileToCamera(
    CameraInfoParams& info, onvifXsd__Profile* profile) const
{
    QAuthenticator auth = m_onvifRes->getAuth();
    MediaSoapWrapper soapWrapper(
        m_onvifRes->onvifTimeouts(),
        m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user(),
        auth.password(), m_onvifRes->getTimeDrift());

    if (!profile)
        return CameraDiagnostics::NoErrorResult();

    bool vSourceMatched = profile && profile->VideoSourceConfiguration
        && profile->VideoSourceConfiguration->token == info.videoSourceId.toStdString();
    if (!vSourceMatched)
    {
        AddVideoSrcConfigReq request;
        AddVideoSrcConfigResp response;

        request.ProfileToken = info.profileToken.toStdString();
        request.ConfigurationToken = info.videoSourceId.toStdString();

        int soapRes = soapWrapper.addVideoSourceConfiguration(request, response);
        if (soapRes != SOAP_OK)
        {
            #if defined(PL_ONVIF_DEBUG)
                qCritical() << "QnOnvifStreamReader::addVideoSourceConfiguration: "
                    << "can't add video source to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getLastError()
                    << ". URL: " << soapWrapper.getEndpointUrl()
                    << ", uniqueId: " << m_onvifRes->getUniqueId() <<
                    "current vSourceID=" << profile->VideoSourceConfiguration->token.data()
                    << "requested vSourceID=" << info.videoSourceId;
            #endif
            if (m_onvifRes->getMaxChannels() > 1)
            {
                    return CameraDiagnostics::RequestFailedResult(
                        QLatin1String("addVideoSourceConfiguration"), soapWrapper.getLastError());
            }
        }
    }

    // Adding video encoder.
    bool vEncoderMatched = profile && profile->VideoEncoderConfiguration
        && profile->VideoEncoderConfiguration->token == info.videoEncoderId.toStdString();
    if (!vEncoderMatched)
    {
        AddVideoConfigReq request;
        AddVideoConfigResp response;

        request.ProfileToken = info.profileToken.toStdString();
        request.ConfigurationToken = info.videoEncoderId.toStdString();

        int soapRes = soapWrapper.addVideoEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK)
        {
            #if defined(PL_ONVIF_DEBUG)
                qCritical() << "QnOnvifStreamReader::addVideoEncoderConfiguration: "
                    << "can't add video encoder to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getLastError()
                    << ". URL: " << soapWrapper.getEndpointUrl()
                    << ", uniqueId: " << m_onvifRes->getUniqueId();
            #endif
            return CameraDiagnostics::RequestFailedResult(
                QLatin1String("addVideoEncoderConfiguration"), soapWrapper.getLastError());
        }
    }

    if (getRole() == Qn::CR_LiveVideo)
    {
        if (!m_onvifRes->audioOutputConfigurationToken().isEmpty())
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
                    #if defined(PL_ONVIF_DEBUG)
                        qCritical() << "QnOnvifStreamReader::addPTZConfiguration: "
                            << "can't add ptz configuration to profile. Gsoap error: "
                            << soapRes << ", description: " << soapWrapper.getLastError()
                            << ". URL: " << soapWrapper.getEndpointUrl()
                            << ", uniqueId: " << m_onvifRes->getUniqueId();
                    #endif
                    return CameraDiagnostics::RequestFailedResult(
                        QLatin1String("addPTZConfiguration"), soapWrapper.getLastError());
                }
            }
        }
    }

    //Adding audio source
    if (!info.audioSourceId.isEmpty() && !info.audioEncoderId.isEmpty())
    {
        bool audioSrcMatched = profile && profile->AudioSourceConfiguration
            && profile->AudioSourceConfiguration->token == info.audioSourceId.toStdString();
        if (!audioSrcMatched)
        {
            AddAudioSrcConfigReq request;
            AddAudioSrcConfigResp response;

            request.ProfileToken = info.profileToken.toStdString();
            request.ConfigurationToken = info.audioSourceId.toStdString();

            int soapRes = soapWrapper.addAudioSourceConfiguration(request, response);
            if (soapRes != SOAP_OK)
            {
                #if defined(PL_ONVIF_DEBUG)
                qCritical() << "QnOnvifStreamReader::addAudioSourceConfiguration: "
                    << "can't add audio source to profile. Gsoap error: "
                    << soapRes << ", description: " << soapWrapper.getLastError()
                    << ". URL: " << soapWrapper.getEndpointUrl()
                    << ", uniqueId: " << m_onvifRes->getUniqueId()
                    << "profile=" << info.profileToken << "audioSourceId=" << info.audioSourceId;
                #endif
                //return false; //< ignore audio error
            }
        }

        // Adding audio encoder.
        bool audioEncoderMatched = profile && profile->AudioEncoderConfiguration
            && profile->AudioEncoderConfiguration->token == info.audioEncoderId.toStdString();
        if (!audioEncoderMatched)
        {
            AddAudioConfigReq request;
            AddAudioConfigResp response;

            request.ProfileToken = info.profileToken.toStdString();
            request.ConfigurationToken = info.audioEncoderId.toStdString();

            int soapRes = soapWrapper.addAudioEncoderConfiguration(request, response);
            if (soapRes != SOAP_OK)
            {
                #if defined(PL_ONVIF_DEBUG)
                    qCritical() << "QnOnvifStreamReader::addAudioEncoderConfiguration: "
                        << "can't add audio encoder to profile. Gsoap error: "
                        << soapRes << ", description: " << soapWrapper.getEndpointUrl()
                        << ". URL: " << soapWrapper.getEndpointUrl()
                        << ", uniqueId: " << m_onvifRes->getUniqueId()
                        << "profile=" << info.profileToken
                        << "audioEncoderId=" << info.audioEncoderId;
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
    if (QnResource::isStopping())
        return CameraDiagnostics::ServerTerminatedResult();

    Media::AudioEncoderConfigurations aeConfigurations(m_onvifRes->makeRequestParams());
    aeConfigurations.receiveBySoap();
    if (!aeConfigurations)
    {
        // LOG.
        return aeConfigurations.requestFailedResult();
    }

    onvifXsd__AudioEncoderConfiguration* currentAEConfig = selectAudioEncoderConfig(
        aeConfigurations.getEphemeralReference().Configurations, isPrimary);
    if (!currentAEConfig)
        return CameraDiagnostics::RequestFailedResult("selectAudioEncoderConfig", QString());

    // TODO: #vasilenko UTF unuse std::string
    outInfo->audioEncoderId = QString::fromStdString(currentAEConfig->token);

    if (!isCameraControlRequired)
        return CameraDiagnostics::NoErrorResult(); //< Do not update audio encoder params.

    return sendAudioEncoderToCamera(*currentAEConfig);
}

onvifXsd__AudioEncoderConfiguration* QnOnvifStreamReader::selectAudioEncoderConfig(
    std::vector<onvifXsd__AudioEncoderConfiguration *>& configs, bool /*isPrimary*/) const
{
    QString id = m_onvifRes->getAudioEncoderId();
    if (id.isEmpty())
        return nullptr;

    for (auto iter = configs.begin(); iter != configs.end(); ++iter)
    {
        // TODO: #vasilenko UTF unuse std::string
        if (*iter && id == QString::fromStdString((*iter)->token))
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
    Media::AudioEncoderConfigurationSetter setter(m_onvifRes->makeRequestParams());
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

AudioSource* QnOnvifStreamReader::fetchAudioSource(
    AudioSrcConfigsResp& response, bool /*isPrimary*/) const
{
    QString id = m_onvifRes->getAudioSourceId();
    if (id.isEmpty()) {
        return 0;
    }

    std::vector<AudioSource*>::const_iterator it = response.Configurations.begin();
    for (; it != response.Configurations.end(); ++it)
    {
        // TODO: #vasilenko UTF unuse std::string
        if (*it && id == QString::fromStdString((*it)->token))
        {
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

QnConstResourceVideoLayoutPtr QnOnvifStreamReader::getVideoLayout() const
{
    return m_multiCodec.getVideoLayout();
}

void QnOnvifStreamReader::setMustNotConfigureResource(bool mustNotConfigureResource)
{
    m_mustNotConfigureResource = mustNotConfigureResource;
}

#endif //ENABLE_ONVIF

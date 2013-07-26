#include <QTextStream>
#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "onvif/soapMediaBindingProxy.h"
#include "utils/network/tcp_connection_priv.h"

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

QnOnvifStreamReader::QnOnvifStreamReader(QnResourcePtr res):
    CLServerPushStreamReader(res),
    m_multiCodec(res),
    m_cachedFps(-1),
    m_cachedQuality(QnQualityNotDefined)
{
    m_onvifRes = getResource().dynamicCast<QnPlOnvifResource>();
    m_tmpH264Conf = new onvifXsd__H264Configuration();
}

QnOnvifStreamReader::~QnOnvifStreamReader()
{
    stop();
    delete m_tmpH264Conf;
}

CameraDiagnostics::ErrorCode::Value QnOnvifStreamReader::openStream()
{
    if (isStreamOpened())
        return CameraDiagnostics::ErrorCode::noError;

    NETOPTIX_PRIMARY_NAME = "Netoptix Primary";
    NETOPTIX_SECONDARY_NAME = "Netoptix Secondary";
    NETOPTIX_PRIMARY_TOKEN = "netoptixP";
    NETOPTIX_SECONDARY_TOKEN = "netoptixS";

    int channel = m_onvifRes->getChannel();
    if (channel > 0) {
        QByteArray postfix = QByteArray("-") + QByteArray::number(channel);
        NETOPTIX_PRIMARY_NAME += postfix;
        NETOPTIX_SECONDARY_NAME += postfix;
        NETOPTIX_PRIMARY_TOKEN += postfix;
        NETOPTIX_SECONDARY_TOKEN += postfix;
    }

    /*
    if (!m_onvifRes->isSoapAuthorized()) {
        m_onvifRes->setStatus(QnResource::Unauthorized);
        return;
    }
    */

    QString streamUrl = updateCameraAndFetchStreamUrl();

    /*
    QUrl url(streamUrlFull);

    QString streamUrl = url.path();
    int port = url.port();

    if (port > 0)
    {
        streamUrl = QString(":") + QString::number(port) + streamUrl;
    }
    */  


    

    if (streamUrl.isEmpty()) {
        qCritical() << "QnOnvifStreamReader::openStream: can't fetch stream URL for resource with UniqueId: " << m_onvifRes->getUniqueId();
        return CameraDiagnostics::ErrorCode::noMediaTrack;
    }


    m_multiCodec.setRequest(streamUrl);
    const CameraDiagnostics::ErrorCode::Value result = m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED && canChangeStatus())
        m_resource->setStatus(QnResource::Unauthorized);
    return result;
}

const QString QnOnvifStreamReader::updateCameraAndFetchStreamUrl()
{
    //QMutexLocker lock(m_onvifRes->getStreamConfMutex());

    int currentFps = getFps();
    QnStreamQuality currentQuality = getQuality();

    if (!m_streamUrl.isEmpty() && m_onvifRes->isCameraControlDisabled())
    {
        m_cachedFps = -1;
        return m_streamUrl;
    }

    if (!m_streamUrl.isEmpty() && currentFps == m_cachedFps && currentQuality == m_cachedQuality && m_cachedTimer.elapsed() < MAX_CAHCE_URL_TIME)
    {
        return m_streamUrl;
    }

    m_onvifRes->beforeConfigureStream();
    QString result = updateCameraAndFetchStreamUrl(getRole() == QnResource::Role_LiveVideo);
    m_onvifRes->afterConfigureStream();

    if (!result.isEmpty()) {
        // cache value
        m_streamUrl = result;
        m_cachedQuality = currentQuality;
        m_cachedFps = currentFps;
        m_cachedTimer.restart();
    }
    return result;
}

const QString QnOnvifStreamReader::updateCameraAndFetchStreamUrl(bool isPrimary) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_onvifRes->getTimeDrift());
    CameraInfoParams info;

    if (!fetchUpdateVideoEncoder(soapWrapper, info, isPrimary)) {
        return QString();
    }
    info.videoSourceId = m_onvifRes->getVideoSourceId();

    fetchUpdateAudioEncoder(soapWrapper, info, isPrimary);

    if (!fetchUpdateProfile(soapWrapper, info, isPrimary)) {
        qWarning() << "ONVIF camera " << getResource()->getUrl() << ": can't prepare profile";
        return QString();
    }

    ////Printing chosen profile
    //if (cl_log.logLevel() >= cl_logDEBUG1) {
    //    printProfile(*info.finalProfile, isPrimary);
    //}
    QString result = fetchStreamUrl(soapWrapper, info.profileToken, isPrimary);
    qDebug() << "got stream URL for camera" << m_resource->getUrl() << "for profile" << info.profileToken;
    qDebug() << "rtsp=" << result;
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
    const quint8* curNal = (const quint8*) videoData->data.data();
    const quint8* end = curNal + videoData->data.size();
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
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 2 && !rez; ++i)
        rez = m_multiCodec.getNextData();
    
    if (!rez)
        closeStream();
    
    return rez;
}

void QnOnvifStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnOnvifStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

void QnOnvifStreamReader::printProfile(const Profile& profile, bool isPrimary) const
{
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
}

void QnOnvifStreamReader::updateVideoEncoder(VideoEncoder& encoder, bool isPrimary) const
{

    encoder.Encoding = m_onvifRes->getCodec(isPrimary) == QnPlOnvifResource::H264? onvifXsd__VideoEncoding__H264: onvifXsd__VideoEncoding__JPEG;
    //encoder.Name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;

    QnStreamQuality quality = getQuality();
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
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: RateControl is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } else 
    {
        encoder.RateControl->FrameRateLimit = getFps();
        encoder.RateControl->BitrateLimit = m_onvifRes->suggestBitrateKbps(quality, resolution, encoder.RateControl->FrameRateLimit);
    }

    
    if (quality != QnQualityPreSet) 
    {
        encoder.Quality = m_onvifRes->innerQualityToOnvif(quality);
    }

    if (!encoder.Resolution) 
    {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: Resolution is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } 
    else 
    {
       
        if (resolution == EMPTY_RESOLUTION_PAIR) 
        {
            qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: : Can't determine (" << (isPrimary? "primary": "secondary") 
                << ") resolution " << "for ONVIF device (UniqueId: " << m_onvifRes->getUniqueId() << "). Default resolution will be used.";
        } 
        else 
        {

            encoder.Resolution->Width = resolution.width();
            encoder.Resolution->Height = resolution.height();
        }
    }
}

const QString QnOnvifStreamReader::fetchStreamUrl(MediaSoapWrapper& soapWrapper, const QString& profileToken, bool isPrimary) const
{
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
        qCritical() << "QnOnvifStreamReader::fetchStreamUrl (primary stream = " << isPrimary 
            << "): can't get stream URL of ONVIF device (URL: " << m_onvifRes->getMediaUrl() 
            << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: SOAP request failed. GSoap error code: "
            << soapRes << ". " << soapWrapper.getLastError();
        return QString();
    }

    if (!response.MediaUri) {
        qCritical() << "QnOnvifStreamReader::fetchStreamUrl (primary stream = "  << isPrimary 
            << "): can't get stream URL of ONVIF device (URL: " << m_onvifRes->getMediaUrl() 
            << ", UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: got empty response.";
        return QString();
    }

    qDebug() << "URL of ONVIF device stream (UniqueId: " << m_onvifRes->getUniqueId()
        << ") successfully fetched: " << response.MediaUri->Uri.c_str();

    
    QUrl relutUrl(m_onvifRes->fromOnvifDiscoveredUrl(response.MediaUri->Uri, false));
    

    if (relutUrl.host().size()==0)
    {
        QString temp = relutUrl.toString();
        relutUrl.setHost(m_onvifRes->getHostAddress());
        qCritical() << "pure URL(error) " << temp<< " Trying to fix: " << relutUrl.toString();
    }

    return relutUrl.toString();
}

bool QnOnvifStreamReader::fetchUpdateVideoEncoder(MediaSoapWrapper& soapWrapper, CameraInfoParams& info, bool isPrimary) const
{
    VideoConfigsReq request;
    VideoConfigsResp response;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateVideoEncoder: can't get video encoders from camera (" 
            << (isPrimary? "primary": "secondary") << ") Gsoap error: " << soapRes << ". Description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        if (soapWrapper.isNotAuthenticated() && canChangeStatus()) {
            m_onvifRes->setStatus(QnResource::Unauthorized);
        }
        return false;
    }

    VideoEncoder* result = fetchVideoEncoder(response, isPrimary);

    if (result) {
        //TODO: #vasilenko UTF unuse std::string
        info.videoEncoderId = QString::fromStdString(result->token);

        if (m_onvifRes->isCameraControlDisabled())
            return true; // do not update video encoder params

        updateVideoEncoder(*result, isPrimary);

        int triesLeft = m_onvifRes->getMaxOnvifRequestTries();
        bool bResult = false;
        while (!bResult && --triesLeft >= 0) {
            bResult = m_onvifRes->sendVideoEncoderToCamera(*result) == SOAP_OK;
            if (!bResult)
                msleep(300);
        }

        return bResult;
    }

    return false;
}

VideoEncoder* QnOnvifStreamReader::fetchVideoEncoder(VideoConfigsResp& response, bool isPrimary) const
{
    std::vector<VideoEncoder*>::const_iterator iter = response.Configurations.begin();
    QString id = isPrimary  ? m_onvifRes->getPrimaryVideoEncoderId() : m_onvifRes->getSecondaryVideoEncoderId();

    for (;iter != response.Configurations.end(); ++iter) 
    {
        VideoEncoder* conf = *iter;
        //TODO: #vasilenko UTF unuse std::string
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

bool QnOnvifStreamReader::fetchUpdateProfile(MediaSoapWrapper& soapWrapper, CameraInfoParams& info, bool isPrimary) const
{
    ProfilesReq request;
    ProfilesResp response;

    int soapRes = soapWrapper.getProfiles(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateProfile: can't get profiles from camera (" 
            << (isPrimary? "primary": "secondary") << "). Gsoap error: " << soapRes << ". Description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return false;
    }

    Profile* profile = fetchExistingProfile(response, isPrimary);
    if (profile) {
        info.profileToken = QString::fromStdString(profile->token);
    }
    else {
        QString noProfileName = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_NAME) : QLatin1String(NETOPTIX_SECONDARY_NAME);
        info.profileToken = isPrimary ? QLatin1String(NETOPTIX_PRIMARY_TOKEN) : QLatin1String(NETOPTIX_SECONDARY_TOKEN);
        if (!createNewProfile(noProfileName, info.profileToken))
            return false;
    }
    return sendProfileToCamera(info, profile);
}

bool QnOnvifStreamReader::createNewProfile(const QString& name, const QString& token) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_onvifRes->getTimeDrift());
    std::string stdStrToken = token.toStdString();

    CreateProfileReq request;
    CreateProfileResp response;

    request.Name = name.toStdString();
    request.Token = &stdStrToken;

    int soapRes = soapWrapper.createProfile(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::sendProfileToCamera: can't create profile " << request.Name.c_str() << "Gsoap error: " 
            << soapRes << ", description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
    }
    return soapRes == SOAP_OK;
}

Profile* QnOnvifStreamReader::fetchExistingProfile(const ProfilesResp& response, bool isPrimary) const
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

    qSort(availableProfiles);
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

bool QnOnvifStreamReader::sendProfileToCamera(CameraInfoParams& info, Profile* profile) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_onvifRes->getTimeDrift());

    bool vSourceMatched = profile && profile->VideoSourceConfiguration && profile->VideoSourceConfiguration->token == info.videoSourceId.toStdString();
    if (!vSourceMatched)
    {
        AddVideoSrcConfigReq request;
        AddVideoSrcConfigResp response;

        request.ProfileToken = info.profileToken.toStdString();
        request.ConfigurationToken = info.videoSourceId.toStdString();

        int soapRes = soapWrapper.addVideoSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addVideoSourceConfiguration: can't add video source to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId() <<
                "current vSourceID=" << profile->VideoSourceConfiguration->token.data() << "requested vSourceID=" << info.videoSourceId;
            if (m_onvifRes->getMaxChannels() > 1)
                return false;
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
            qCritical() << "QnOnvifStreamReader::addVideoEncoderConfiguration: can't add video encoder to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            return false;
        }
    }

    if (getRole() == QnResource::Role_LiveVideo)
    {
        if(QnOnvifPtzController *ptzController = dynamic_cast<QnOnvifPtzController *>(m_onvifRes->getPtzController())) // TODO: #Elric EVIL!
        {
            bool ptzMatched = profile && profile->PTZConfiguration;
            if (!ptzMatched)
            {
                AddPTZConfigReq request;
                AddPTZConfigResp response;

                request.ProfileToken = info.profileToken.toStdString();
                request.ConfigurationToken = ptzController->getPtzConfigurationToken().toStdString();

                int soapRes = soapWrapper.addPTZConfiguration(request, response);
                if (soapRes == SOAP_OK) {
                    ptzController->setMediaProfileToken(QString::fromStdString(profile->token));
                }
                else {
                    qCritical() << "QnOnvifStreamReader::addPTZConfiguration: can't add ptz configuration to profile. Gsoap error: " 
                        << soapRes << ", description: " << soapWrapper.getLastError() 
                        << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

                    return false;
                }
            }
            else {
                ptzController->setMediaProfileToken(QString::fromStdString(profile->token));
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
                qCritical() << "QnOnvifStreamReader::addAudioSourceConfiguration: can't add audio source to profile. Gsoap error: " 
                    << soapRes << ", description: " << soapWrapper.getLastError() 
                    << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId()
                    << "profile=" << info.profileToken << "audioSourceId=" << info.audioSourceId;
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
                qCritical() << "QnOnvifStreamReader::addAudioEncoderConfiguration: can't add audio encoder to profile. Gsoap error: " 
                    << soapRes << ", description: " << soapWrapper.getEndpointUrl() 
                    << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId()
                    << "profile=" << info.profileToken << "audioEncoderId=" << info.audioEncoderId;

                //result = false; //Sound can be absent, so ignoring;

            }
        }
    }

    return true;
}

bool QnOnvifStreamReader::fetchUpdateAudioEncoder(MediaSoapWrapper& soapWrapper, CameraInfoParams& info, bool isPrimary) const
{
    AudioConfigsReq request;
    AudioConfigsResp response;

    int soapRes = soapWrapper.getAudioEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateAudioEncoder: can't get audio encoders from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return false;
    }

    AudioEncoder* result = fetchAudioEncoder(response, isPrimary);

    if (result) {
        //TODO: #vasilenko UTF unuse std::string
        info.audioEncoderId = QString::fromStdString(result->token);

        if (m_onvifRes->isCameraControlDisabled())
            return true; // do not update audio encoder params

        updateAudioEncoder(*result, isPrimary);
        return sendAudioEncoderToCamera(*result);
    }

    return false;
}

AudioEncoder* QnOnvifStreamReader::fetchAudioEncoder(AudioConfigsResp& response, bool /*isPrimary*/) const
{
    QString id = m_onvifRes->getAudioEncoderId();
    if (id.isEmpty()) {
        return 0;
    }

    std::vector<onvifXsd__AudioEncoderConfiguration*>::const_iterator iter = response.Configurations.begin();
    for (; iter != response.Configurations.end(); ++iter) {
        //TODO: #vasilenko UTF unuse std::string
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
        qWarning() << "QnOnvifStreamReader::updateAudioEncoder: codec type is undefined. UniqueId: " << m_onvifRes->getUniqueId();

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
            qWarning() << "QnOnvifStreamReader::updateAudioEncoder: codec type is unknown: " << codec
                << ". UniqueId: " << m_onvifRes->getUniqueId();
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

bool QnOnvifStreamReader::sendAudioEncoderToCamera(AudioEncoder& encoder) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_onvifRes->getTimeDrift());

    SetAudioConfigReq request;
    SetAudioConfigResp response;
    request.Configuration = &encoder;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setAudioEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::sendAudioEncoderToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError()
            << "configuration token=" << request.Configuration->token.c_str();
        return false;
    }

    return true;
}

AudioSource* QnOnvifStreamReader::fetchAudioSource(AudioSrcConfigsResp& response, bool /*isPrimary*/) const
{
    QString id = m_onvifRes->getAudioSourceId();
    if (id.isEmpty()) {
        return 0;
    }

    std::vector<AudioSource*>::const_iterator it = response.Configurations.begin();
    for (; it != response.Configurations.end(); ++it) {
        //TODO: #vasilenko UTF unuse std::string
        if (*it && id == QString::fromStdString((*it)->token)) {
            return *it;
        }
    }

    return 0;
}

const QnResourceAudioLayout* QnOnvifStreamReader::getDPAudioLayout() const
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

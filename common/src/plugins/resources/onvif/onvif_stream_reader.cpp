#include <QTextStream>
#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "onvif/soapMediaBindingProxy.h"
#include "utils/network/tcp_connection_priv.h"

//
// struct ProfilePair
//

struct ProfilePair
{
    Profile tmp; //if we will not find our profile in camera, netoptix ptr should point to this var
    bool profileAbsent;
    Profile* netoptix;
    Profile* predefined;

    ProfilePair(): profileAbsent(false), netoptix(0), predefined(0) {}
};

//
// struct CameraInfo
//

struct CameraInfoParams
{
    CameraInfoParams(): profileAbsent(false) {}
    QString predefinedProfileId;
    QString netoptixProfileId;
    QString videoEncoderId;
    QString audioEncoderId;

    QString finalProfileId; //Ptr to predefined or netoptix (if creation was successful) profile
    bool profileAbsent; //Create netoptix profile or not
};

//
// QnOnvifStreamReader
//

QnOnvifStreamReader::QnOnvifStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    QnLiveStreamProvider(res),
    m_multiCodec(res)
{
    m_onvifRes = getResource().dynamicCast<QnPlOnvifResource>();
    m_tmpH264Conf = new onvifXsd__H264Configuration();
}

QnOnvifStreamReader::~QnOnvifStreamReader()
{
    stop();
    delete m_tmpH264Conf;
}

void QnOnvifStreamReader::openStream()
{
    if (isStreamOpened())
        return;

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
        return;
    }


    m_multiCodec.setRequest(streamUrl);
    m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED)
        m_resource->setStatus(QnResource::Unauthorized);
}

const QString QnOnvifStreamReader::updateCameraAndFetchStreamUrl() const
{
    return updateCameraAndFetchStreamUrl(getRole() == QnResource::Role_LiveVideo);
}

const QString QnOnvifStreamReader::updateCameraAndFetchStreamUrl(bool isPrimary) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_onvifRes->getTimeDrift());
    CameraInfoParams info;

    if (!fetchUpdateVideoEncoder(soapWrapper, info, isPrimary)) {
        return QString();
    }

    fetchUpdateAudioEncoder(soapWrapper, info, isPrimary);

    if (!fetchUpdateProfile(soapWrapper, info, isPrimary)) {
        return QString();
    }

    ////Printing chosen profile
    //if (cl_log.logLevel() >= cl_logDEBUG1) {
    //    printProfile(*info.finalProfile, isPrimary);
    //}
    QString result = fetchStreamUrl(soapWrapper, info.finalProfileId, isPrimary);
    qDebug() << "ONVIF camera " << getResource()->getUrl() << ": got stream URL=" << result;
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
    int errorCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_multiCodec.getNextData();
        if (rez) 
        {
            QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
            //ToDo: if (videoData)
            //    parseMotionInfo(videoData);
            
            //if (!videoData || isGotFrame(videoData))
            break;
        }
        else {
            errorCount++;
            if (errorCount > 1) {
                closeStream();
                break;
            }
        }
    }
    
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
        if (soapWrapper.isNotAuthenticated()) {
            m_onvifRes->setStatus(QnResource::Unauthorized);
        }
        return false;
    }

    VideoEncoder* result = fetchVideoEncoder(response, isPrimary);

    if (result) {
        //TODO:UTF unuse std::string
        info.videoEncoderId = QString::fromStdString(result->token);
        updateVideoEncoder(*result, isPrimary);

        int triesLeft = m_onvifRes->getMaxOnvifRequestTries();
        bool bResult = false;
        while (!bResult && --triesLeft >= 0) {
            bResult = sendVideoEncoderToCamera(*result);
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
        //TODO:UTF unuse std::string
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

    ProfilePair profiles;
    fetchProfile(response, profiles, isPrimary);

    bool result = false;
    if (profiles.netoptix) {
        //TODO:UTF unuse std::string
        info.netoptixProfileId = QString::fromStdString(profiles.netoptix->token);
        updateProfile(*profiles.netoptix, isPrimary);
        result = sendProfileToCamera(info, *profiles.netoptix, profiles.profileAbsent);
        info.finalProfileId = result? info.netoptixProfileId : QString();
    }

    if (!result && profiles.predefined) {
        //TODO:UTF unuse std::string
        info.predefinedProfileId = QString::fromStdString(profiles.predefined->token);
        updateProfile(*profiles.predefined, isPrimary);
        qDebug() << "ONVIF camera " << getResource()->getUrl() << ": can't create user profile. Try to use predefined profile";
        result = sendProfileToCamera(info, *profiles.predefined, false);
        info.finalProfileId = result? info.predefinedProfileId : QString();
    }

    //result = false if we failed to add encoders / sources to profile, but there is a big
    //possibility, that desired encoders / sources attached to previously created profile, so ignore
    //and try to get stream url further
    if (!result) {
        qDebug() << "ONVIF camera " << getResource()->getUrl() << ": can't update predefined profile";
        result = true;
        info.finalProfileId = info.predefinedProfileId.isEmpty() ? info.netoptixProfileId : info.predefinedProfileId;
    }

    return result;
}

void QnOnvifStreamReader::fetchProfile(ProfilesResp& response, ProfilePair& profiles, bool isPrimary) const
{
    std::vector<Profile*>::const_iterator iter = response.Profiles.begin();

    const char* token = isPrimary? NETOPTIX_PRIMARY_TOKEN: NETOPTIX_SECONDARY_TOKEN;
    std::string videoSourceId = m_onvifRes->getVideoSourceId().toStdString();
    std::string encoderToken = isPrimary? m_onvifRes->getPrimaryVideoEncoderId().toStdString(): m_onvifRes->getSecondaryVideoEncoderId().toStdString();

    for (; iter != response.Profiles.end(); ++iter) {
        Profile* profile = *iter;
        if (!profile)
            continue;
        if (profile->VideoSourceConfiguration && profile->VideoSourceConfiguration->token != videoSourceId)
            continue;

        if (profile->token == token) {
            profiles.netoptix = profile;
        }
        else if (!profiles.predefined && !QByteArray(profile->token.c_str()).startsWith("netoptix") && 
                 profile->VideoEncoderConfiguration && profile->VideoEncoderConfiguration->token == encoderToken)
            profiles.predefined = profile;
    }

    //Netoptix profile not found. Trying to create it.
    if (!profiles.netoptix) {
        profiles.profileAbsent = true;

        profiles.netoptix = &profiles.tmp;
        profiles.netoptix->token = token;
    }
}

void QnOnvifStreamReader::updateProfile(Profile& profile, bool isPrimary) const
{
    profile.Name = isPrimary? std::string(NETOPTIX_PRIMARY_NAME.data()): std::string(NETOPTIX_SECONDARY_NAME.data());
}

bool QnOnvifStreamReader::sendProfileToCamera(CameraInfoParams& info, Profile& profile, bool create) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_onvifRes->getTimeDrift());

    if (create) {
        CreateProfileReq request;
        CreateProfileResp response;

        request.Name = profile.Name;
        request.Token = &profile.token;

        int soapRes = soapWrapper.createProfile(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::sendProfileToCamera: can't create profile " << request.Name.c_str() << "Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError()
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
            return false;
        }
    }

    std::string videoSourceId = m_onvifRes->getVideoSourceId().toStdString();
    std::string audioSourceId = m_onvifRes->getAudioSourceId().toStdString();
    bool result  = true;
    //Adding video source
    if (profile.VideoSourceConfiguration == 0 || profile.VideoSourceConfiguration->token != videoSourceId)
    {
        AddVideoSrcConfigReq request;
        AddVideoSrcConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = videoSourceId;

        int soapRes = soapWrapper.addVideoSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addVideoSourceConfiguration: can't add video source to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
            
            result = false;
        }
    }

    //Adding video encoder
    if (!info.videoEncoderId.isEmpty())
    {
        if (profile.VideoEncoderConfiguration == 0 || profile.VideoEncoderConfiguration->token != info.videoEncoderId.toStdString())
        {
            AddVideoConfigReq request;
            AddVideoConfigResp response;

            request.ProfileToken = profile.token;
            request.ConfigurationToken = info.videoEncoderId.toStdString();

            int soapRes = soapWrapper.addVideoEncoderConfiguration(request, response);
            if (soapRes != SOAP_OK) {
                qCritical() << "QnOnvifStreamReader::addVideoEncoderConfiguration: can't add video encoder to profile. Gsoap error: " 
                    << soapRes << ", description: " << soapWrapper.getLastError() 
                    << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

                return false;
            }
        }
    } else {
        return false;
    }

    if (getRole() == QnResource::Role_LiveVideo && m_onvifRes->getPtzController())
    {
        if (profile.PTZConfiguration == 0)
        {
            AddPTZConfigReq request;
            AddPTZConfigResp response;

            request.ProfileToken = profile.token;
            request.ConfigurationToken = m_onvifRes->getPtzController()->getPtzConfigurationToken().toStdString();

            int soapRes = soapWrapper.addPTZConfiguration(request, response);
            if (soapRes == SOAP_OK) {
                m_onvifRes->getPtzController()->setMediaProfileToken(QString::fromStdString(profile.token));
            }
            else {
                qCritical() << "QnOnvifStreamReader::addPTZConfiguration: can't add ptz configuration to profile. Gsoap error: " 
                    << soapRes << ", description: " << soapWrapper.getLastError() 
                    << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

                return false;
            }
        }
        else {
            m_onvifRes->getPtzController()->setMediaProfileToken(QString::fromStdString(profile.token));
        }
    }


    //Adding audio source
    if (!audioSourceId.empty() && (profile.AudioSourceConfiguration == 0 || profile.AudioSourceConfiguration->token != audioSourceId))
    {
        AddAudioSrcConfigReq request;
        AddAudioSrcConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = audioSourceId;

        int soapRes = soapWrapper.addAudioSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addAudioSourceConfiguration: can't add audio source to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            //Sound can be absent, so ignoring;
            //result = false;
        }
    } else {
        //Sound can be absent, so ignoring;
        //result = false;
    }


    //Adding audio encoder
    if (!info.audioEncoderId.isEmpty() && (profile.AudioEncoderConfiguration == 0 || profile.AudioEncoderConfiguration->token != info.audioEncoderId.toStdString()))
    {
        AddAudioConfigReq request;
        AddAudioConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = info.audioEncoderId.toStdString();

        int soapRes = soapWrapper.addAudioEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addAudioEncoderConfiguration: can't add audio encoder to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getEndpointUrl() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            //Sound can be absent, so ignoring;
            //result = false;

        }
    } else {
        //Sound can be absent, so ignoring;
        //result = false;
    }

    return result;
}

bool QnOnvifStreamReader::sendVideoEncoderToCamera(VideoEncoder& encoder) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_onvifRes->getTimeDrift());

    SetVideoConfigReq request;
    SetVideoConfigResp response;
    request.Configuration = &encoder;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::sendVideoEncoderToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId()
            << "encoder token=" << request.Configuration->token.c_str()
            << "encoder name=" << request.Configuration->Name.c_str()
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
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
        //TODO:UTF unuse std::string
        info.audioEncoderId = QString::fromStdString(result->token);
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
        //TODO:UTF unuse std::string
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
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
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
        //TODO:UTF unuse std::string
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


/*
for (;it != response.Configurations.end(); ++it) 
{
if (!(*it) || !(*it)->Bounds)
{
continue;
}

if ((*it)->Name == name) 
{
return *it;
}

unsigned long curSquare = (*it)->Bounds->height * (*it)->Bounds->width;
if (curSquare > square) {
square = curSquare;
result = *it;
}
}
*/

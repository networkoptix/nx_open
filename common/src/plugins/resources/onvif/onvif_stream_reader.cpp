#include <QTextStream>
#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "onvif/soapMediaBindingProxy.h"
#include "utils/network/tcp_connection_priv.h"

//
// struct CameraInfo
//

struct ProfilePair
{
    bool profileAbsent;
    Profile* netoptix;
    Profile* predefined;

    ProfilePair(): profileAbsent(false), netoptix(0), predefined(0) {}
};

struct CameraInfo
{
    VideoConfigsResp videoEncodersBox;
    VideoSrcConfigsResp videoSourcesBox;
    AudioConfigsResp audioEncodersBox;
    AudioSrcConfigsResp audioSourcesBox;
    ProfilesResp profileBox;

    VideoEncoder* videoEncoder;
    VideoSource* videoSource;
    AudioEncoder* audioEncoder;
    AudioSource* audioSource;
    Profile* predefinedProfile;
    Profile* netoptixProfile;
    Profile* finalProfile;

    bool profileAbsent;


    CameraInfo():
        videoEncoder(0),
        videoSource(0),
        audioEncoder(0),
        audioSource(0),
        predefinedProfile(0),
        netoptixProfile(0),
        finalProfile(0),
        profileAbsent(false)
    {

    }
};

//
// QnOnvifStreamReader
//

const char* QnOnvifStreamReader::NETOPTIX_PRIMARY_NAME = "Netoptix Primary";
const char* QnOnvifStreamReader::NETOPTIX_SECONDARY_NAME = "Netoptix Secondary";
const char* QnOnvifStreamReader::NETOPTIX_PRIMARY_TOKEN = "netoptixP";
const char* QnOnvifStreamReader::NETOPTIX_SECONDARY_TOKEN = "netoptixS";

QnOnvifStreamReader::QnOnvifStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    m_multiCodec(res)
{
    m_onvifRes = getResource().dynamicCast<QnPlOnvifResource>();
}

QnOnvifStreamReader::~QnOnvifStreamReader()
{

}

void QnOnvifStreamReader::openStream()
{
    if (isStreamOpened())
        return;

    if (!m_onvifRes->isSoapAuthorized()) {
        m_onvifRes->setStatus(QnResource::Unauthorized);
        return;
    }

    QString streamUrl = updateCameraAndFetchStreamUrl();
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
    QnResource::ConnectionRole role = getRole();

    if (role == QnResource::Role_LiveVideo)
    {
        return updateCameraAndFetchStreamUrl(true);
    }

    if (role == QnResource::Role_SecondaryLiveVideo) {
        return updateCameraAndFetchStreamUrl(false);
    }

    qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl: got unexpected role: " << role 
        << ". UniqueId: " << m_onvifRes->getUniqueId() << "). Root cause: there is no appropriate profile";

    return QString();
}

const QString QnOnvifStreamReader::updateCameraAndFetchStreamUrl(bool isPrimary) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString());
    CameraInfo info;

    info.videoEncoder = fetchUpdateVideoEncoder(soapWrapper, info.videoEncodersBox, isPrimary);
    if (!info.videoEncoder) {
        return QString();
    }

    info.videoSource = fetchUpdateVideoSource(soapWrapper, info.videoSourcesBox, isPrimary);
    info.audioEncoder = fetchUpdateAudioEncoder(soapWrapper, info.audioEncodersBox, isPrimary);
    info.audioSource = fetchUpdateAudioSource(soapWrapper, info.audioSourcesBox, isPrimary);

    ProfilePair profiles = fetchUpdateProfile(soapWrapper, info.profileBox, isPrimary);
    info.profileAbsent = profiles.profileAbsent;
    info.netoptixProfile = profiles.netoptix;
    info.predefinedProfile = profiles.predefined;
    if (!info.netoptixProfile && info.predefinedProfile) {
        return QString();
    }

    if (!sendConfigToCamera(soapWrapper, info)) {
        return QString();
    }

    //Printing chosen profile
    if (cl_log.logLevel() >= cl_logDEBUG1) {
        printProfile(*info.finalProfile, isPrimary);
    }

    return fetchStreamUrl(soapWrapper, info.finalProfile->token, isPrimary);
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
            
            if (!videoData || isGotFrame(videoData))
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

//Deleting protocol, host and port from URL assuming that input URLs cannot be without
//protocol, but with host specified.
const QString QnOnvifStreamReader::normalizeStreamSrcUrl(const std::string& src) const
{
    qDebug() << "QnOnvifStreamReader::normalizeStreamSrcUrl: initial URL: '" << src.c_str() << "'";

    QString result(QString::fromStdString(src));
    int pos = result.indexOf("://");
    if (pos != -1) {
        pos += sizeof("://");
        pos = result.indexOf("/", pos);
        result = pos != -1? result.right(result.size() - pos - 1): QString();
    }

    qDebug() << "QnOnvifStreamReader::normalizeStreamSrcUrl: normalized URL: '" << result << "'";
    return result;
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
    encoder.Encoding = m_onvifRes->getCodec() == QnPlOnvifResource::H264? onvifXsd__VideoEncoding__H264: onvifXsd__VideoEncoding__JPEG;
    encoder.Name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;

    if (!encoder.RateControl) {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: RateControl is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } else {
        encoder.RateControl->FrameRateLimit = getFps();
    }

    QnStreamQuality quality = getQuality();
    if (quality != QnQualityPreSeted) {
        encoder.Quality = m_onvifRes->innerQualityToOnvif(quality);
    }

    if (!encoder.Resolution) {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: Resolution is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } else {

        ResolutionPair resolution = isPrimary? m_onvifRes->getPrimaryResolution(): m_onvifRes->getSecondaryResolution();
        if (resolution == EMPTY_RESOLUTION_PAIR) {
            qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: : Can't determine (" << (isPrimary? "primary": "secondary") 
                << ") resolution " << "for ONVIF device (UniqueId: " << m_onvifRes->getUniqueId() << "). Default resolution will be used.";
        } else {

            encoder.Resolution->Width = resolution.first;
            encoder.Resolution->Height = resolution.second;
        }
    }
}

const QString QnOnvifStreamReader::fetchStreamUrl(MediaSoapWrapper& soapWrapper, const std::string& profileToken, bool isPrimary) const
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
    request.ProfileToken = profileToken;

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
    return QString(normalizeStreamSrcUrl(response.MediaUri->Uri));
}

VideoEncoder* QnOnvifStreamReader::fetchUpdateVideoEncoder(MediaSoapWrapper& soapWrapper, VideoConfigsResp& response, bool isPrimary) const
{
    VideoConfigsReq request;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateVideoEncoder: can't get video encoders from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return 0;
    }

    VideoEncoder* result = fetchUpdateVideoEncoder(response, isPrimary);

    if (result) {
        updateVideoEncoder(*result, isPrimary);
    }

    return result;
}

VideoEncoder* QnOnvifStreamReader::fetchUpdateVideoEncoder(VideoConfigsResp& response, bool isPrimary) const
{
    std::vector<VideoEncoder*>::const_iterator iter = response.Configurations.begin();
    VideoEncoder* result = 0;
    const char* name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;
    const char* filteredName = isPrimary? NETOPTIX_SECONDARY_NAME: NETOPTIX_PRIMARY_NAME;

    while (iter != response.Configurations.end()) {
        VideoEncoder* conf = *iter;

        if (conf) {

            if (conf->Name != filteredName) {
                result = conf;
            }
            if (conf->Name == name) {
                return conf;
            }

        }

        ++iter;
    }

    return result;
}

ProfilePair QnOnvifStreamReader::fetchUpdateProfile(MediaSoapWrapper& soapWrapper, ProfilesResp& response, bool isPrimary) const
{
    ProfilesReq request;

    int soapRes = soapWrapper.getProfiles(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateProfile: can't get profiles from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return ProfilePair();
    }

    ProfilePair result = fetchUpdateProfile(response, isPrimary);

    if (result.netoptix) {
        updateProfile(*result.netoptix, isPrimary);
    }

    if (result.predefined) {
        updateProfile(*result.predefined, isPrimary);
    }

    return result;
}

ProfilePair QnOnvifStreamReader::fetchUpdateProfile(ProfilesResp& response, bool isPrimary) const
{
    std::vector<Profile*>::const_iterator iter = response.Profiles.begin();
    ProfilePair result;
    Profile* tmp = 0;
    const char* name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;
    const char* filteredName = isPrimary? NETOPTIX_SECONDARY_NAME: NETOPTIX_PRIMARY_NAME;

    //Trying to find our and some predefined profile
    while (iter != response.Profiles.end()) {
        Profile* profile = *iter;

        if (profile) {

            if (profile->Name != filteredName) {
                if (result.predefined && !tmp) {
                    tmp = profile;
                }
                if (!result.predefined) {
                    result.predefined = profile;
                }
            }
            if (profile->Name == name) {
                result.netoptix = profile;
                break;
            }

        }

        ++iter;
    }

    if (result.netoptix || !tmp) {
        return result;
    }

    //Not Found. Trying to create profile
    result.profileAbsent = true;
    result.netoptix = tmp;
    result.netoptix->token = isPrimary? NETOPTIX_PRIMARY_TOKEN: NETOPTIX_SECONDARY_TOKEN;

    return result;
}

void QnOnvifStreamReader::updateProfile(Profile& profile, bool isPrimary) const
{
    profile.Name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;
}

VideoSource* QnOnvifStreamReader::fetchUpdateVideoSource(MediaSoapWrapper& soapWrapper, VideoSrcConfigsResp& response, bool isPrimary) const
{
    VideoSrcConfigsReq request;

    int soapRes = soapWrapper.getVideoSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateVideoSource: can't get video sources from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return 0;
    }

    VideoSource* result = fetchUpdateVideoSource(response, isPrimary);

    if (result) {
        updateVideoSource(*result, isPrimary);
    }

    return result;
}

VideoSource* QnOnvifStreamReader::fetchUpdateVideoSource(VideoSrcConfigsResp& response, bool /*isPrimary*/) const
{
    unsigned long square = 0;
    std::vector<VideoSource*>::const_iterator it = response.Configurations.begin();
    VideoSource* result = 0;
    //One name for 2 streams
    const char* name = NETOPTIX_PRIMARY_NAME;

    while (it != response.Configurations.end()) {
        if (!(*it) || !(*it)->Bounds) {
            continue;
        }

        if ((*it)->Name == name) {
            return *it;
        }

        unsigned long curSquare = (*it)->Bounds->height * (*it)->Bounds->width;
        if (curSquare > square) {
            square = curSquare;
            result = *it;
        }

        ++it;
    }

    return result;
}

void QnOnvifStreamReader::updateVideoSource(VideoSource& source, bool /*isPrimary*/) const
{
    //One name for primary and secondary
    source.Name = NETOPTIX_PRIMARY_NAME;

    if (!source.Bounds) {
        qWarning() << "QnOnvifStreamReader::updateVideoSource: rectangle object is NULL. UniqueId: " << m_onvifRes->getUniqueId();
        return;
    }

    CameraPhysicalWindowSize size = m_onvifRes->getPhysicalWindowSize();
    if (!size.isValid()) {
        return;
    }

    source.Bounds->x = size.x;
    source.Bounds->y = size.y;
    source.Bounds->height = size.height;
    source.Bounds->width = size.width;
}

bool QnOnvifStreamReader::sendConfigToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info) const
{
    bool result = false;
    if (info.netoptixProfile && sendProfileToCamera(soapWrapper, info, *info.netoptixProfile, info.profileAbsent)) {
        result = true;
        info.finalProfile = info.netoptixProfile;
    }

    if (!result) {
        result = sendProfileToCamera(soapWrapper, info, *info.predefinedProfile, false);
        info.finalProfile = info.predefinedProfile;
    }

    if (!result) {
        return false;
    }

    if (!sendVideoEncoderToCamera(soapWrapper, info, *info.finalProfile))
    {
        return false;
    }

    //This block of functions: is not critical if failed
    sendVideoSourceToCamera(soapWrapper, info, *info.finalProfile);
    sendAudioEncoderToCamera(soapWrapper, info, *info.finalProfile);
    sendAudioSourceToCamera(soapWrapper, info, *info.finalProfile);

    return true;
}

bool QnOnvifStreamReader::sendProfileToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile, bool create) const
{
    if (create) {
        CreateProfileReq request;
        CreateProfileResp response;

        request.Name = profile.Name;
        request.Token = &profile.token;

        int soapRes = soapWrapper.createProfile(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::sendProfileToCamera: can't create profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getEndpointUrl()
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
            return false;
        }
    }

    //Adding video encoder
    if (info.videoEncoder) {
        AddVideoConfigReq request;
        AddVideoConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = info.videoEncoder->token;

        int soapRes = soapWrapper.addVideoEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addVideoEncoderConfiguration: can't add video encoder to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            return false;
        } else {
            profile.VideoEncoderConfiguration = info.videoEncoder;
        }
    } else {
        return false;
    }

    //Adding video source
    if (info.videoSource) {
        AddVideoSrcConfigReq request;
        AddVideoSrcConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = info.videoSource->token;

        int soapRes = soapWrapper.addVideoSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addVideoSourceConfiguration: can't add video source to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
            
            if (!profile.VideoSourceConfiguration) {
                return false;
            }

        } else {
            profile.VideoSourceConfiguration = info.videoSource;
        }
    } else if (!profile.VideoSourceConfiguration) {
        return false;
    }

    //Adding audio encoder
    if (info.audioEncoder) {
        AddAudioConfigReq request;
        AddAudioConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = info.audioEncoder->token;

        int soapRes = soapWrapper.addAudioEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addAudioEncoderConfiguration: can't add audio encoder to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getEndpointUrl() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            //Sound can be absent, so ignoring;
            //return false;

        } else {
            profile.AudioEncoderConfiguration = info.audioEncoder;
        }
    } else {
        //Sound can be absent, so ignoring;
        //return false;
    }

    //Adding audio source
    if (info.audioSource) {
        AddAudioSrcConfigReq request;
        AddAudioSrcConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = info.audioSource->token;

        int soapRes = soapWrapper.addAudioSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addAudioSourceConfiguration: can't add audio source to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            if (!profile.AudioSourceConfiguration) {
                return false;
            }

        } else {
            profile.AudioSourceConfiguration = info.audioSource;
        }
    } else {
        //Sound can be absent, so ignoring;
        //return false;
    }

    return true;
}

bool QnOnvifStreamReader::sendVideoEncoderToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& /*profile*/) const
{
    SetVideoConfigReq request;
    SetVideoConfigResp response;
    request.Configuration = info.videoEncoder;

    int soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::sendVideoEncoderToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    return true;
}

bool QnOnvifStreamReader::sendVideoSourceToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& /*profile*/) const
{
    SetVideoSrcConfigReq request;
    SetVideoSrcConfigResp response;
    request.Configuration = info.videoSource;

    int soapRes = soapWrapper.setVideoSourceConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::setVideoSourceConfiguration: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    return true;
}

AudioEncoder* QnOnvifStreamReader::fetchUpdateAudioEncoder(MediaSoapWrapper& soapWrapper, AudioConfigsResp& response, bool isPrimary) const
{
    AudioConfigsReq request;

    int soapRes = soapWrapper.getAudioEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateAudioEncoder: can't get video encoders from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return 0;
    }

    AudioEncoder* result = fetchUpdateAudioEncoder(response, isPrimary);

    if (result) {
        updateAudioEncoder(*result, isPrimary);
    }

    return result;
}

AudioEncoder* QnOnvifStreamReader::fetchUpdateAudioEncoder(AudioConfigsResp& response, bool isPrimary) const
{
    std::vector<AudioEncoder*>::const_iterator iter = response.Configurations.begin();
    AudioEncoder* result = 0;
    const char* name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;
    const char* filteredName = isPrimary? NETOPTIX_SECONDARY_NAME: NETOPTIX_PRIMARY_NAME;

    while (iter != response.Configurations.end()) {
        AudioEncoder* conf = *iter;

        if (conf) {

            if (conf->Name != filteredName) {
                result = conf;
            }
            if (conf->Name == name) {
                return conf;
            }

        }

        ++iter;
    }

    return result;
}

void QnOnvifStreamReader::updateAudioEncoder(AudioEncoder& encoder, bool isPrimary) const
{
    encoder.Name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;

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

bool QnOnvifStreamReader::sendAudioEncoderToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& /*profile*/) const
{
    SetAudioConfigReq request;
    SetAudioConfigResp response;
    request.Configuration = info.audioEncoder;

    int soapRes = soapWrapper.setAudioEncoderConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::sendAudioEncoderToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    return true;
}

AudioSource* QnOnvifStreamReader::fetchUpdateAudioSource(MediaSoapWrapper& soapWrapper, AudioSrcConfigsResp& response, bool isPrimary) const
{
    AudioSrcConfigsReq request;

    int soapRes = soapWrapper.getAudioSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateAudioSource: can't get audio sources from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return 0;
    }

    AudioSource* result = fetchUpdateAudioSource(response, isPrimary);

    if (result) {
        updateAudioSource(*result, isPrimary);
    }

    return result;
}

AudioSource* QnOnvifStreamReader::fetchUpdateAudioSource(AudioSrcConfigsResp& response, bool isPrimary) const
{
    std::vector<AudioSource*>::const_iterator it = response.Configurations.begin();
    AudioSource* result = 0;
    //One name for 2 streams
    const char* name = NETOPTIX_PRIMARY_NAME;

    while (it != response.Configurations.end()) {
        if (!(*it)) {
            continue;
        }

        if ((*it)->Name == name) {
            return *it;
        }

        result = *it;
        ++it;
    }

    return result;
}

void QnOnvifStreamReader::updateAudioSource(AudioSource& source, bool isPrimary) const
{
    //One name for 2 streams
    source.Name = NETOPTIX_PRIMARY_NAME;
}

bool QnOnvifStreamReader::sendAudioSourceToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& /*profile*/) const
{
    SetAudioSrcConfigReq request;
    SetAudioSrcConfigResp response;
    request.Configuration = info.audioSource;

    int soapRes = soapWrapper.setAudioSourceConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::sendAudioSourceToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    return true;
}

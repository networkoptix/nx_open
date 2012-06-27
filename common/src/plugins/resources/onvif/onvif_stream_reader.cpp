#include <QTextStream>
#include "onvif_resource.h"
#include "onvif_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "onvif/soapMediaBindingProxy.h"
#include "utils/network/tcp_connection_priv.h"

//
// LocalVideoEncoder
//

class LocalVideoEncoder: public VideoEncoder
{
public:
    LocalVideoEncoder()
    {
        Resolution = new onvifXsd__VideoResolution();
        RateControl = new onvifXsd__VideoRateControl();
        MPEG4 = NULL;
        H264 = NULL;
        Multicast = NULL;
        __anyAttribute = NULL;
    }

    ~LocalVideoEncoder()
    {
        delete Resolution;
        delete RateControl;
        if (MPEG4) delete MPEG4;
        if (H264) delete H264;
        if (Multicast)
        {
            if (Multicast->Address)
            {
                if (Multicast->Address->IPv4Address) {
                    delete Multicast->Address->IPv4Address;
                }

                if (Multicast->Address->IPv6Address) {
                    delete Multicast->Address->IPv6Address;
                }

                delete Multicast->Address;
            }
            delete Multicast;
        }
    }

    void setValues(const VideoEncoder& src)
    {
        if (src.MPEG4) {
            MPEG4 = new onvifXsd__Mpeg4Configuration();

            MPEG4->GovLength = src.MPEG4->GovLength;
            MPEG4->Mpeg4Profile = src.MPEG4->Mpeg4Profile;
            MPEG4->__item = NULL;
        }

        if (src.H264) {
            H264 = new onvifXsd__H264Configuration();

            H264->GovLength = src.H264->GovLength;
            H264->H264Profile = src.H264->H264Profile;
            H264->__item = NULL;
        }

        if (src.Multicast) {
            Multicast = new onvifXsd__MulticastConfiguration();

            if (src.Multicast->Address) {
                Multicast->Address = new onvifXsd__IPAddress();
                Multicast->Address->Type = src.Multicast->Address->Type;
                Multicast->Address->__item = NULL;
                Multicast->Address->IPv4Address = NULL;
                Multicast->Address->IPv6Address = NULL;

                if (src.Multicast->Address->IPv4Address) {
                    Multicast->Address->IPv4Address = new std::string();
                    *(Multicast->Address->IPv4Address) = *(src.Multicast->Address->IPv4Address);
                }

                if (src.Multicast->Address->IPv6Address) {
                    Multicast->Address->IPv6Address = new std::string();
                    *(Multicast->Address->IPv6Address) = *(src.Multicast->Address->IPv6Address);
                }
            }

            Multicast->Port = src.Multicast->Port;
            Multicast->TTL = src.Multicast->TTL;
            Multicast->AutoStart = src.Multicast->AutoStart;
            Multicast->__anyAttribute = NULL;
        }

        //if (src.__anyAttribute) {
        //    __anyAttribute = src.__anyAttribute;
        //}

        Encoding = src.Encoding;
        SessionTimeout = src.SessionTimeout;
        __any = src.__any;
        Name = src.Name;
        UseCount = src.UseCount;
        token = src.token;
    }
};

//
// LocalVideoSource
//

class LocalVideoSource: public VideoSource
{
public:
    LocalVideoSource()
    {
        Bounds = new onvifXsd__IntRectangle();
        Bounds->x = -1;
        Bounds->y = -1;
        Bounds->width = -1;
        Bounds->height = -1;

        Extension = NULL;
        __anyAttribute = NULL;
    }

    ~LocalVideoSource()
    {
        delete Bounds;
    }

    void setValues(const VideoSource& src)
    {
        /*if (src.Extension) {
            Extension = src.Extension;
        }

        if (src.__anyAttribute) {
            __anyAttribute = src.__anyAttribute;
        }*/

        SourceToken = src.SourceToken;
        Name = src.Name;
        UseCount = src.UseCount;
        token = src.token;
    }
};

//
// LocalAudioEncoder
//

class LocalAudioEncoder: public AudioEncoder
{
public:
    LocalAudioEncoder()
    {
        Multicast = NULL;
        __anyAttribute = NULL;
    }

    void setValues(const AudioEncoder& src)
    {
        /*if (src.Multicast) {
            Multicast = src.Multicast;
        }

        if (src.__anyAttribute) {
            __anyAttribute = src.__anyAttribute;
        }*/

        Encoding = src.Encoding;
        Bitrate = src.Bitrate;
        SampleRate = src.SampleRate;
        SessionTimeout = src.SessionTimeout;
        Name = src.Name;
        UseCount = src.UseCount;
        token = src.token;
    }

};

//
// LocalAudioSource
//

class LocalAudioSource: public AudioSource
{
public:
    LocalAudioSource()
    {
        __anyAttribute = NULL;
    }

    void setValues(const AudioSource& src)
    {
        /*if (src.__anyAttribute) {
            __anyAttribute = src.__anyAttribute;
        }*/

        SourceToken = src.SourceToken;
        __any = src.__any;
        Name = src.Name;
        UseCount = src.UseCount;
        token = src.token;
    }
};

//
// LocalProfile
//

class LocalProfile: public Profile
{
public:
    LocalProfile()
    {
        VideoSourceConfiguration = NULL;
        AudioSourceConfiguration = NULL;
        VideoEncoderConfiguration = NULL;
        AudioEncoderConfiguration = NULL;
        VideoAnalyticsConfiguration = NULL;
        PTZConfiguration = NULL;
        MetadataConfiguration = NULL;
        Extension = NULL;
        fixed = NULL;
        __anyAttribute = NULL;
        __item = NULL;
    }

    void setValues(const Profile& src)
    {
        /*if (src.__anyAttribute) {
            __anyAttribute = src.__anyAttribute;
        }

        if (src.fixed) {
            fixed = src.fixed;
        }*/

        Name = src.Name;
        token = src.token;
    }
};

//
// struct ProfilePair
//

struct ProfilePair
{
    LocalProfile tmp; //if we will not find our profile in camera, netoptix ptr should point to this var
    bool profileAbsent;
    Profile* netoptix;
    Profile* predefined;

    ProfilePair(): profileAbsent(false), netoptix(0), predefined(0) {}
};

//
// struct CameraInfo
//

struct CameraInfo
{
    LocalProfile predefinedProfile;
    LocalProfile netoptixProfile;
    LocalVideoEncoder videoEncoderLocal;
    LocalVideoSource videoSourceLocal;
    LocalAudioEncoder audioEncoderLocal;
    LocalAudioSource audioSourceLocal;

    LocalProfile* finalProfile; //Ptr to predefined or netoptix (if creation was successful) profile
    bool profileAbsent; //Create netoptix profile or not


    CameraInfo():
        finalProfile(0),
        profileAbsent(false)
    {
        
    }

    void setAudioEncoderToProfiles()
    {
        predefinedProfile.AudioEncoderConfiguration = &audioEncoderLocal;
        netoptixProfile.AudioEncoderConfiguration = &audioEncoderLocal;
    }

    void setAudioSourceToProfiles()
    {
        predefinedProfile.AudioSourceConfiguration = &audioSourceLocal;
        netoptixProfile.AudioSourceConfiguration = &audioSourceLocal;
    }

    void setVideoEncoderToProfiles()
    {
        predefinedProfile.VideoEncoderConfiguration = &videoEncoderLocal;
        netoptixProfile.VideoEncoderConfiguration = &videoEncoderLocal;
    }

    void setVideoSourceToProfiles()
    {
        predefinedProfile.VideoSourceConfiguration = &videoSourceLocal;
        netoptixProfile.VideoSourceConfiguration = &videoSourceLocal;
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
    QnLiveStreamProvider(res),
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
    QnResource::ConnectionRole role = getRole();

    if (role == QnResource::Role_LiveVideo)
    {
        return updateCameraAndFetchStreamUrl(true);
    }

    if (role == QnResource::Role_SecondaryLiveVideo) {
        return updateCameraAndFetchStreamUrl(false);
    }

    qCritical() << "QnOnvifStreamReader::updateCameraAndfetchStreamUrl: got unexpected role: " << role 
        << ". UniqueId: " << m_onvifRes->getUniqueId() << ")";

    return QString();
}

const QString QnOnvifStreamReader::updateCameraAndFetchStreamUrl(bool isPrimary) const
{
    QAuthenticator auth(m_onvifRes->getAuth());
    MediaSoapWrapper soapWrapper(m_onvifRes->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString());
    CameraInfo info;

    if (!fetchUpdateVideoEncoder(soapWrapper, info, isPrimary)) {
        return QString();
    }

    fetchUpdateVideoSource(soapWrapper, info, isPrimary);
    fetchUpdateAudioEncoder(soapWrapper, info, isPrimary);
    fetchUpdateAudioSource(soapWrapper, info, isPrimary);

    if (!fetchUpdateProfile(soapWrapper, info, isPrimary)) {
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

    QnStreamQuality quality = getQuality();
    ResolutionPair resolution = isPrimary? m_onvifRes->getPrimaryResolution(): m_onvifRes->getSecondaryResolution();

    if (!encoder.H264)
    {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: H264 object is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } else 
    {
        encoder.H264->GovLength = m_onvifRes->getGovLength();
        encoder.H264->H264Profile = isPrimary ? onvifXsd__H264Profile__High : onvifXsd__H264Profile__Baseline;
    }

    if (!encoder.RateControl) 
    {
        qWarning() << "QnOnvifStreamReader::updateVideoEncoderParams: RateControl is NULL. UniqueId: " << m_onvifRes->getUniqueId();
    } else 
    {
        encoder.RateControl->FrameRateLimit = getFps();
        encoder.RateControl->BitrateLimit = m_onvifRes->suggestBitrateKbps(quality, QSize(resolution.first, resolution.second), encoder.RateControl->FrameRateLimit);
    }

    
    if (quality != QnQualityPreSeted) 
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

    return QString::fromStdString(response.MediaUri->Uri);
}

bool QnOnvifStreamReader::fetchUpdateVideoEncoder(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const
{
    VideoConfigsReq request;
    VideoConfigsResp response;

    int soapRes = soapWrapper.getVideoEncoderConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateVideoEncoder: can't get video encoders from camera (" 
            << (isPrimary? "primary": "secondary") << ") Gsoap error: " << soapRes << ". Description: " << soapWrapper.getLastError()
            << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return false;
    }

    VideoEncoder* result = fetchVideoEncoder(response, isPrimary);

    if (result) {
        info.videoEncoderLocal.setValues(*result);
        info.setVideoEncoderToProfiles();

        updateVideoEncoder(info.videoEncoderLocal, isPrimary);

        return true;
    }

    return false;
}

VideoEncoder* QnOnvifStreamReader::fetchVideoEncoder(VideoConfigsResp& response, bool isPrimary) const
{
    std::vector<VideoEncoder*>::const_iterator iter = response.Configurations.begin();
    QString id = isPrimary ? m_onvifRes->getPrimaryVideoEncoderId() : m_onvifRes->getSecondaryVideoEncoderId();

    while (iter != response.Configurations.end()) {
        VideoEncoder* conf = *iter;

        if (conf && id == conf->token.c_str()) {
            return conf;
        }

        ++iter;
    }

    return 0;
}

bool QnOnvifStreamReader::fetchUpdateProfile(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const
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
        info.netoptixProfile.setValues(*profiles.netoptix);
        updateProfile(info.netoptixProfile, isPrimary);
        result = true;
    }

    if (profiles.predefined) {
        info.netoptixProfile.setValues(*profiles.predefined);
        updateProfile(*profiles.predefined, isPrimary);
        result = true;
    }

    return result;
}

void QnOnvifStreamReader::fetchProfile(ProfilesResp& response, ProfilePair& profiles, bool isPrimary) const
{
    std::vector<Profile*>::const_iterator iter = response.Profiles.begin();
    Profile* additionalProfile = 0;

    const char* token = isPrimary? NETOPTIX_PRIMARY_TOKEN: NETOPTIX_SECONDARY_TOKEN;
    const char* filteredToken = isPrimary? NETOPTIX_SECONDARY_TOKEN: NETOPTIX_PRIMARY_TOKEN;

    QString encoderToken = isPrimary? m_onvifRes->getPrimaryVideoEncoderId(): m_onvifRes->getSecondaryVideoEncoderId();
    QString filteredEncoderToken = isPrimary? m_onvifRes->getSecondaryVideoEncoderId(): m_onvifRes->getPrimaryVideoEncoderId();

    //Trying to find our and some predefined profile
    while (iter != response.Profiles.end()) {
        Profile* profile = *iter;

        if (profile && profile->token != filteredToken) {

            if (profile->token == token) {
                profiles.netoptix = profile;
                return;
            }

            bool finish = false;
            if (profile->VideoEncoderConfiguration)
            {
                if (encoderToken == profile->VideoEncoderConfiguration->token.c_str()) {
                    profiles.predefined = profile;
                } else if (filteredEncoderToken == profile->VideoEncoderConfiguration->token.c_str()) {
                    finish = true;
                }
            }

            //If there are no our profiles and no profiles with our (choosen) encoders
            //choose any other. To avoid race between primary and secondary: primary will used 
            //the first, and secondary - the last
            if (!finish && (!additionalProfile || !isPrimary)) {
                additionalProfile = profile;
            }
        }

        ++iter;
    }

    if (!profiles.predefined) {
        profiles.predefined = additionalProfile;
    }

    //Netoptix profile not found. Trying to create it.
    profiles.profileAbsent = true;

    profiles.netoptix = &profiles.tmp;
    profiles.netoptix->token = isPrimary? NETOPTIX_PRIMARY_TOKEN: NETOPTIX_SECONDARY_TOKEN;
}

void QnOnvifStreamReader::updateProfile(Profile& profile, bool isPrimary) const
{
    profile.Name = isPrimary? NETOPTIX_PRIMARY_NAME: NETOPTIX_SECONDARY_NAME;
}

bool QnOnvifStreamReader::fetchUpdateVideoSource(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const
{
    VideoSrcConfigsReq request;
    VideoSrcConfigsResp response;

    int soapRes = soapWrapper.getVideoSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateVideoSource: can't get video sources from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return false;
    }

    VideoSource* result = fetchVideoSource(response, isPrimary);

    if (result) {
        info.videoSourceLocal.setValues(*result);
        info.setVideoSourceToProfiles();

        updateVideoSource(info.videoSourceLocal, isPrimary);

        return true;
    }

    return false;
}

VideoSource* QnOnvifStreamReader::fetchVideoSource(VideoSrcConfigsResp& response, bool /*isPrimary*/) const
{
    unsigned long square = 0;
    std::vector<VideoSource*>::const_iterator it = response.Configurations.begin();
    VideoSource* result = 0;
    //One name for 2 streams
    const char* name = NETOPTIX_PRIMARY_NAME;

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
    if (!info.netoptixProfile.token.empty() &&
        sendProfileToCamera(soapWrapper, info, info.netoptixProfile, info.profileAbsent))
    {
        result = true;
        info.finalProfile = &info.netoptixProfile;
    }

    if (!result && !info.predefinedProfile.token.empty()) {
        result = sendProfileToCamera(soapWrapper, info, info.predefinedProfile, false);
        info.finalProfile = &info.predefinedProfile;
    }

    //result = false if we failed to add encoders / sources to profile, but there is a big
    //possibility, that desired encoders / sources attached to predefined profile, so ignore
    //and try to get stream url further
    //ToDo: think more about this situation
    if (!result) {
        //return false;
        info.finalProfile = &info.predefinedProfile;
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

bool QnOnvifStreamReader::sendProfileToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& /*info*/, Profile& profile, bool create) const
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
    if (profile.VideoEncoderConfiguration)
    {
        AddVideoConfigReq request;
        AddVideoConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = profile.VideoEncoderConfiguration->token;

        int soapRes = soapWrapper.addVideoEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addVideoEncoderConfiguration: can't add video encoder to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            return false;
        }
    } else {
        return false;
    }

    //Adding video source
    if (profile.VideoSourceConfiguration)
    {
        AddVideoSrcConfigReq request;
        AddVideoSrcConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = profile.VideoSourceConfiguration->token;

        int soapRes = soapWrapper.addVideoSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addVideoSourceConfiguration: can't add video source to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
            
            return false;
        }
    } else {
        return false;
    }

    //Adding audio encoder
    if (profile.AudioEncoderConfiguration)
    {
        AddAudioConfigReq request;
        AddAudioConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = profile.AudioEncoderConfiguration->token;

        int soapRes = soapWrapper.addAudioEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addAudioEncoderConfiguration: can't add audio encoder to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getEndpointUrl() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            //Sound can be absent, so ignoring;
            //return false;

        }
    } else {
        //Sound can be absent, so ignoring;
        //return false;
    }

    //Adding audio source
    if (profile.AudioSourceConfiguration)
    {
        AddAudioSrcConfigReq request;
        AddAudioSrcConfigResp response;

        request.ProfileToken = profile.token;
        request.ConfigurationToken = profile.AudioSourceConfiguration->token;

        int soapRes = soapWrapper.addAudioSourceConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            qCritical() << "QnOnvifStreamReader::addAudioSourceConfiguration: can't add audio source to profile. Gsoap error: " 
                << soapRes << ", description: " << soapWrapper.getLastError() 
                << ". URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();

            //Sound can be absent, so ignoring;
            //return false;
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
    request.Configuration = &info.videoEncoderLocal;
    request.ForcePersistence = false;

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
    request.Configuration = &info.videoSourceLocal;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setVideoSourceConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::setVideoSourceConfiguration: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    return true;
}

bool QnOnvifStreamReader::fetchUpdateAudioEncoder(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const
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
        info.audioEncoderLocal.setValues(*result);
        info.setAudioEncoderToProfiles();

        updateAudioEncoder(info.audioEncoderLocal, isPrimary);

        return true;
    }

    return false;
}

AudioEncoder* QnOnvifStreamReader::fetchAudioEncoder(AudioConfigsResp& response, bool isPrimary) const
{
    std::vector<AudioEncoder*>::const_iterator iter = response.Configurations.begin();
    AudioEncoder* result = 0;
    const char* name = NETOPTIX_PRIMARY_NAME;

    while (iter != response.Configurations.end()) {
        AudioEncoder* conf = *iter;

        if (conf) {

            if (conf->Name == name) {
                return conf;
            }
            result = conf;
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
    request.Configuration = &info.audioEncoderLocal;
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

bool QnOnvifStreamReader::fetchUpdateAudioSource(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const
{
    AudioSrcConfigsReq request;
    AudioSrcConfigsResp response;

    int soapRes = soapWrapper.getAudioSourceConfigurations(request, response);
    if (soapRes != SOAP_OK) {
        qCritical() << "QnOnvifStreamReader::fetchUpdateAudioSource: can't get audio sources from camera (" 
            << (isPrimary? "primary": "secondary") 
            << "). URL: " << soapWrapper.getEndpointUrl() << ", uniqueId: " << m_onvifRes->getUniqueId();
        return false;
    }

    AudioSource* result = fetchAudioSource(response, isPrimary);

    if (result) {
        info.audioSourceLocal.setValues(*result);
        info.setAudioSourceToProfiles();

        updateAudioSource(info.audioSourceLocal, isPrimary);

        return true;
    }

    return false;
}

AudioSource* QnOnvifStreamReader::fetchAudioSource(AudioSrcConfigsResp& response, bool isPrimary) const
{
    std::vector<AudioSource*>::const_iterator it = response.Configurations.begin();
    AudioSource* result = 0;
    //One name for 2 streams
    const char* name = NETOPTIX_PRIMARY_NAME;

    while (it != response.Configurations.end()) {
        if (!(*it)) {
            ++it;
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
    request.Configuration = &info.audioSourceLocal;
    request.ForcePersistence = false;

    int soapRes = soapWrapper.setAudioSourceConfiguration(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifStreamReader::sendAudioSourceToCamera: can't set required values into ONVIF physical device (URL: " 
            << soapWrapper.getEndpointUrl() << ", UniqueId: " << m_onvifRes->getUniqueId() 
            << "). Root cause: SOAP failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
        return false;
    }

    return true;
}

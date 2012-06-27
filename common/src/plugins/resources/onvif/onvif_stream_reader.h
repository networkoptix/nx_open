#ifndef onvif_stream_reader_h
#define onvif_stream_reader_h

#include "onvif_helper.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "soap_wrapper.h"

struct CameraInfo;
struct ProfilePair;
class onvifXsd__Profile;
class onvifXsd__VideoEncoderConfiguration;
class onvifXsd__VideoSourceConfiguration;
class onvifXsd__AudioEncoderConfiguration;
class onvifXsd__AudioSourceConfiguration;

typedef onvifXsd__Profile Profile;
typedef onvifXsd__VideoEncoderConfiguration VideoEncoder;
typedef onvifXsd__VideoSourceConfiguration VideoSource;
typedef onvifXsd__AudioEncoderConfiguration AudioEncoder;
typedef onvifXsd__AudioSourceConfiguration AudioSource;

class QnOnvifStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
    static const char* NETOPTIX_PRIMARY_NAME;
    static const char* NETOPTIX_SECONDARY_NAME;
    static const char* NETOPTIX_PRIMARY_TOKEN;
    static const char* NETOPTIX_SECONDARY_TOKEN;

public:
    QnOnvifStreamReader(QnResourcePtr res);
    virtual ~QnOnvifStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
private:

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    void processTriggerData(const quint8* payload, int len);

    bool isGotFrame(QnCompressedVideoDataPtr videoData);

    const QString updateCameraAndFetchStreamUrl() const;
    const QString updateCameraAndFetchStreamUrl(bool isPrimary) const;

    //Returned pointers are valid while response object is living. (For all functions in the following block)
    bool fetchUpdateVideoEncoder(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const;
    bool fetchUpdateAudioEncoder(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const;
    bool fetchUpdateProfile(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const;
    bool fetchUpdateVideoSource(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const;
    bool fetchUpdateAudioSource(MediaSoapWrapper& soapWrapper, CameraInfo& info, bool isPrimary) const;

    //Returned pointers are valid while response object is living. (For all functions in the following block)
    VideoEncoder* fetchVideoEncoder(VideoConfigsResp& response, bool isPrimary) const;
    AudioEncoder* fetchAudioEncoder(AudioConfigsResp& response, bool isPrimary) const;
    ProfilePair fetchProfile(ProfilesResp& response, bool isPrimary) const;
    VideoSource* fetchVideoSource(VideoSrcConfigsResp& response, bool isPrimary) const;
    AudioSource* fetchAudioSource(AudioSrcConfigsResp& response, bool isPrimary) const;

    void updateVideoEncoder(VideoEncoder& encoder, bool isPrimary) const;
    void updateAudioEncoder(AudioEncoder& encoder, bool isPrimary) const;
    void updateProfile(Profile& profile, bool isPrimary) const;
    void updateVideoSource(VideoSource& source, bool isPrimary) const;
    void updateAudioSource(AudioSource& source, bool isPrimary) const;

    bool sendConfigToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info) const;
    bool sendProfileToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile, bool create = false) const;
    bool sendVideoEncoderToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile) const;
    bool sendVideoSourceToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile) const;
    bool sendAudioEncoderToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile) const;
    bool sendAudioSourceToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile) const;

    const QString fetchStreamUrl(MediaSoapWrapper& soapWrapper, const std::string& profileToken, bool isPrimary) const;

    void updateVideoEncoderParams(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) const;

    void printProfile(const Profile& profile, bool isPrimary) const;
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_multiCodec;
    QnPlOnvifResourcePtr m_onvifRes;
};

#endif // onvif_stream_reader_h

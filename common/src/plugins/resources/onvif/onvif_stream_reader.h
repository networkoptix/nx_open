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

typedef onvifXsd__Profile Profile;
typedef onvifXsd__VideoEncoderConfiguration VideoEncoder;
typedef onvifXsd__VideoSourceConfiguration VideoSource;

class QnOnvifStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
    static const int MAX_VIDEO_PARAMS_RESET_TRIES;
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
    VideoEncoder* fetchUpdateVideoEncoder(MediaSoapWrapper& soapWrapper, VideoConfigsResp& response, bool isPrimary) const;
    ProfilePair fetchUpdateProfile(MediaSoapWrapper& soapWrapper, ProfilesResp& response, bool isPrimary) const;
    VideoSource* fetchUpdateVideoSource(MediaSoapWrapper& soapWrapper, VideoSrcConfigsResp& response, bool isPrimary) const;

    //Returned pointers are valid while response object is living. (For all functions in the following block)
    VideoEncoder* fetchUpdateVideoEncoder(VideoConfigsResp& response, bool isPrimary) const;
    ProfilePair fetchUpdateProfile(ProfilesResp& response, bool isPrimary) const;
    VideoSource* fetchUpdateVideoSource(VideoSrcConfigsResp& response, bool isPrimary) const;

    void updateVideoEncoder(VideoEncoder& encoder, bool isPrimary) const;
    void updateProfile(Profile& profile, bool isPrimary) const;
    void updateVideoSource(VideoSource& source, bool isPrimary) const;

    bool sendConfigToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info) const;
    bool sendProfileToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile, bool create = false) const;
    bool sendVideoEncoderToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile) const;
    bool sendVideoSourceToCamera(MediaSoapWrapper& soapWrapper, CameraInfo& info, Profile& profile) const;

    const QString fetchStreamUrl(MediaSoapWrapper& soapWrapper, const std::string& profileToken, bool isPrimary) const;

    void updateVideoEncoderParams(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) const;
    const QString normalizeStreamSrcUrl(const std::string& src) const;
    void printProfile(const Profile& profile, bool isPrimary) const;
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_multiCodec;
    QnPlOnvifResourcePtr m_onvifRes;
};

#endif // onvif_stream_reader_h

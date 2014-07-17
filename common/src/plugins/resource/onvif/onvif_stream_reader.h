#ifndef onvif_stream_reader_h
#define onvif_stream_reader_h

#ifdef ENABLE_ONVIF

#include "onvif_helper.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "soap_wrapper.h"
#include "onvif_resource.h"

struct CameraInfoParams;
struct ProfilePair;
class onvifXsd__Profile;
class onvifXsd__VideoEncoderConfiguration;
class onvifXsd__AudioEncoderConfiguration;
class onvifXsd__AudioSourceConfiguration;
class onvifXsd__H264Configuration;

typedef onvifXsd__Profile Profile;
typedef onvifXsd__AudioEncoderConfiguration AudioEncoder;
typedef onvifXsd__AudioSourceConfiguration AudioSource;

class QnOnvifStreamReader: public CLServerPushStreamReader
{
public:
    /*
    static const char* NETOPTIX_PRIMARY_NAME;
    static const char* NETOPTIX_SECONDARY_NAME;
    static const char* NETOPTIX_PRIMARY_TOKEN;
    static const char* NETOPTIX_SECONDARY_TOKEN;
    */

    QnOnvifStreamReader(const QnResourcePtr& res);
    virtual ~QnOnvifStreamReader();
    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
    virtual void pleaseStop() override;
    virtual bool secondaryResolutionIsLarge() const override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void setCameraControlDisabled(bool value) override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
private:

    
    
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    void processTriggerData(const quint8* payload, int len);

    bool isGotFrame(QnCompressedVideoDataPtr videoData);

    CameraDiagnostics::Result updateCameraAndFetchStreamUrl( QString* const streamUrl );
    CameraDiagnostics::Result updateCameraAndFetchStreamUrl( bool isPrimary, QString* const streamUrl ) const;

    //Returned pointers are valid while response object is living. (For all functions in the following block)
    CameraDiagnostics::Result fetchUpdateVideoEncoder(MediaSoapWrapper& soapWrapper, CameraInfoParams& info, bool isPrimary) const;
    CameraDiagnostics::Result fetchUpdateAudioEncoder(MediaSoapWrapper& soapWrapper, CameraInfoParams& info, bool isPrimary) const;
    
    CameraDiagnostics::Result fetchUpdateProfile(MediaSoapWrapper& soapWrapper, CameraInfoParams& info, bool isPrimary) const;
    Profile* fetchExistingProfile(const ProfilesResp& response, bool isPrimary, CameraInfoParams& info) const;
    CameraDiagnostics::Result sendProfileToCamera(CameraInfoParams& info, Profile* profile) const;
    CameraDiagnostics::Result createNewProfile(const QString& name, const QString& token) const;


    //Returned pointers are valid while response object is living. (For all functions in the following block)
    VideoEncoder* fetchVideoEncoder(VideoConfigsResp& response, bool isPrimary) const;
    AudioEncoder* fetchAudioEncoder(AudioConfigsResp& response, bool isPrimary) const;
    void fetchProfile(ProfilesResp& response, ProfilePair& profiles, bool isPrimary) const;
    AudioSource* fetchAudioSource(AudioSrcConfigsResp& response, bool isPrimary) const;

    void updateVideoEncoder(VideoEncoder& encoder, bool isPrimary) const;
    void updateAudioEncoder(AudioEncoder& encoder, bool isPrimary) const;

    CameraDiagnostics::Result sendProfileToCamera(CameraInfoParams& info, Profile& profile, bool create = false) const;
    CameraDiagnostics::Result sendVideoSourceToCamera(VideoSource& source) const;
    CameraDiagnostics::Result sendAudioEncoderToCamera(AudioEncoder& encoder) const;

    CameraDiagnostics::Result fetchStreamUrl(MediaSoapWrapper& soapWrapper, const QString& profileToken, bool isPrimary, QString* const mediaUrl) const;

    void updateVideoEncoderParams(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) const;

    void printProfile(const Profile& profile, bool isPrimary) const;
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_multiCodec;
    QnPlOnvifResourcePtr m_onvifRes;

    QByteArray NETOPTIX_PRIMARY_NAME;
    QByteArray NETOPTIX_SECONDARY_NAME;
    QByteArray NETOPTIX_PRIMARY_TOKEN;
    QByteArray NETOPTIX_SECONDARY_TOKEN;
    onvifXsd__H264Configuration* m_tmpH264Conf;

    QString m_streamUrl;
    int m_cachedFps;
    Qn::StreamQuality m_cachedQuality;
    QElapsedTimer m_cachedTimer;
    Qn::StreamQuality m_cachedSecondaryQuality;
};

#endif //ENABLE_ONVIF

#endif // onvif_stream_reader_h

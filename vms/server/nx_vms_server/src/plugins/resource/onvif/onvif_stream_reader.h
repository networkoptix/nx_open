#pragma once

#ifdef ENABLE_ONVIF

#include "onvif_helper.h"
#include <providers/spush_media_stream_provider.h>
#include "network/multicodec_rtp_reader.h"
#include "soap_wrapper.h"
#include "onvif_resource.h"

struct CameraInfoParams;
struct ProfilePair;
class tt__Profile;
class tt__VideoEncoderConfiguration;
class tt__AudioEncoderConfiguration;
class tt__AudioSourceConfiguration;
class tt__H264Configuration;

typedef tt__AudioEncoderConfiguration AudioEncoder;

class QnOnvifStreamReader: public CLServerPushStreamReader
{
public:
    /*
    static const char* NETOPTIX_PRIMARY_NAME;
    static const char* NETOPTIX_SECONDARY_NAME;
    static const char* NETOPTIX_PRIMARY_TOKEN;
    static const char* NETOPTIX_SECONDARY_TOKEN;
    */

    QnOnvifStreamReader(const QnPlOnvifResourcePtr& res);
    virtual ~QnOnvifStreamReader();
    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const override;
    virtual void pleaseStop() override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;

    void setMustNotConfigureResource(bool mustNotConfigureResource);
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;
    virtual void preStreamConfigureHook() {}
    virtual void postStreamConfigureHook() {}

    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void setCameraControlDisabled(bool value) override;
private:
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    void processTriggerData(const quint8* payload, int len);

    bool isGotFrame(QnCompressedVideoDataPtr videoData);

    CameraDiagnostics::Result updateCameraAndFetchStreamUrl(QString* const streamUrl,
        bool isCameraControlRequired, const QnLiveStreamParams& params);
    CameraDiagnostics::Result updateCameraAndFetchStreamUrl(
        bool isPrimary, QString* outStreamUrl, bool isCameraControlRequired,
        const QnLiveStreamParams& params) const;

    // Returned pointers are valid while response object is living.
    // (For all functions in the following block.)
    CameraDiagnostics::Result fetchUpdateVideoEncoder(
        CameraInfoParams* outInfo, bool isPrimary, bool isCameraControlRequired,
        const QnLiveStreamParams& params) const;
    CameraDiagnostics::Result fetchUpdateAudioEncoder(
        CameraInfoParams* outInfo, bool isPrimary, bool isCameraControlRequired) const;

    CameraDiagnostics::Result fetchUpdateProfile(
        CameraInfoParams& info, bool isPrimary, bool isCameraControlRequired) const;
    tt__Profile* selectExistingProfile(
        std::vector<tt__Profile *>& profiles, bool isPrimary, CameraInfoParams& info) const;
    CameraDiagnostics::Result sendProfileToCamera(CameraInfoParams& info, tt__Profile* profile) const;
    CameraDiagnostics::Result createNewProfile(std::string name, std::string token) const;

    // Returned pointers are valid while response object is living.
    // (For all functions in the following block.)
    tt__VideoEncoderConfiguration* selectVideoEncoderConfig(
        std::vector<tt__VideoEncoderConfiguration *>& configs, bool isPrimary) const;
    tt__VideoEncoder2Configuration* selectVideoEncoder2Config(
        std::vector<tt__VideoEncoder2Configuration *>& configs, bool isPrimary) const;

    tt__AudioEncoderConfiguration* selectAudioEncoderConfig(
        std::vector<tt__AudioEncoderConfiguration *>& configs, bool isPrimary) const;

    void updateAudioEncoder(AudioEncoder& encoder) const;

    CameraDiagnostics::Result sendAudioEncoderToCamera(
        tt__AudioEncoderConfiguration& encoderConfig) const;

    void fixStreamUrl(QString* mediaUrl, const std::string& profileToken) const;

    CameraDiagnostics::Result fetchStreamUrl(MediaSoapWrapper& soapWrapper,
        const std::string& profileToken, bool isPrimary, QString* mediaUrl) const;

    void printProfile(const tt__Profile& profile, bool isPrimary) const;

    bool executePreConfigurationRequests();
    CameraDiagnostics::Result bindTwoWayAudioToProfile(
        MediaSoapWrapper& soapWrapper, const std::string& profileToken) const;
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_multiCodec;
    QnPlOnvifResourcePtr m_onvifRes;

    QByteArray NETOPTIX_PRIMARY_NAME;
    QByteArray NETOPTIX_SECONDARY_NAME;
    QByteArray NETOPTIX_PRIMARY_TOKEN;
    QByteArray NETOPTIX_SECONDARY_TOKEN;

    QString m_streamUrl;
    QElapsedTimer m_cachedTimer;
    QnLiveStreamParams m_previousStreamParams;
};

#endif //ENABLE_ONVIF

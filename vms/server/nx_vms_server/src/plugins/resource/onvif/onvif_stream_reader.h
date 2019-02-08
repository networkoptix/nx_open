#pragma once

#ifdef ENABLE_ONVIF

#include "onvif_helper.h"
#include <providers/spush_media_stream_provider.h>
#include "network/multicodec_rtp_reader.h"
#include "soap_wrapper.h"
#include "onvif_resource.h"

struct CameraInfoParams;
struct ProfilePair;
class onvifXsd__Profile;
class onvifXsd__VideoEncoderConfiguration;
class onvifXsd__AudioEncoderConfiguration;
class onvifXsd__AudioSourceConfiguration;
class onvifXsd__H264Configuration;

typedef onvifXsd__AudioEncoderConfiguration AudioEncoder;

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
    bool needConfigureProvider() const override;
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
    onvifXsd__Profile* selectExistingProfile(
        std::vector<onvifXsd__Profile *>& profiles, bool isPrimary, CameraInfoParams& info) const;
    CameraDiagnostics::Result sendProfileToCamera(CameraInfoParams& info, onvifXsd__Profile* profile) const;
    CameraDiagnostics::Result createNewProfile(std::string name, std::string token) const;

    // Returned pointers are valid while response object is living.
    // (For all functions in the following block.)
    onvifXsd__VideoEncoderConfiguration* selectVideoEncoderConfig(
        std::vector<onvifXsd__VideoEncoderConfiguration *>& configs, bool isPrimary) const;
    onvifXsd__VideoEncoder2Configuration* selectVideoEncoder2Config(
        std::vector<onvifXsd__VideoEncoder2Configuration *>& configs, bool isPrimary) const;

    onvifXsd__AudioEncoderConfiguration* selectAudioEncoderConfig(
        std::vector<onvifXsd__AudioEncoderConfiguration *>& configs, bool isPrimary) const;

    void updateAudioEncoder(AudioEncoder& encoder) const;

    CameraDiagnostics::Result sendAudioEncoderToCamera(
        onvifXsd__AudioEncoderConfiguration& encoderConfig) const;

    CameraDiagnostics::Result fetchStreamUrl(MediaSoapWrapper& soapWrapper,
        const std::string& profileToken, bool isPrimary, QString* const mediaUrl) const;

    void printProfile(const onvifXsd__Profile& profile, bool isPrimary) const;

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
    bool m_mustNotConfigureResource = false;
};

#endif //ENABLE_ONVIF

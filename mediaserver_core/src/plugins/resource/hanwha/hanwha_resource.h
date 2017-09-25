#pragma once

#if defined(ENABLE_HANWHA)

#include <core/ptz/ptz_limits.h>

#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/resource/hanwha/hanwha_attributes.h>
#include <plugins/resource/hanwha/hanwha_stream_limits.h>
#include <plugins/resource/hanwha/hanwha_advanced_parameter_info.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_codec_limits.h>

#include <core/ptz/ptz_auxilary_trait.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaResource: public QnPlOnvifResource
{
    using base_type = QnPlOnvifResource;

public:
    HanwhaResource() = default;
    virtual ~HanwhaResource() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual bool getParamPhysical(const QString &id, QString &value) override;

    virtual bool getParamsPhysical(
        const QSet<QString> &idList,
        QnCameraAdvancedParamValueList& result) override;

    virtual bool setParamPhysical(const QString &id, const QString& value) override;

    virtual bool setParamsPhysical(
        const QnCameraAdvancedParamValueList &values,
        QnCameraAdvancedParamValueList &result) override;

public:
    int maxProfileCount() const;

    AVCodecID streamCodec(Qn::ConnectionRole role) const;
    QString streamCodecProfile(AVCodecID codec, Qn::ConnectionRole role) const;
    QSize streamResolution(Qn::ConnectionRole role) const;
    int streamFrameRate(Qn::ConnectionRole role, int desiredFps) const;
    int streamGovLength(Qn::ConnectionRole role) const;
    Qn::BitrateControl streamBitrateControl(Qn::ConnectionRole role) const;
    int streamBitrate(Qn::ConnectionRole role, const QnLiveStreamParams& streamParams) const;

    int closestFrameRate(Qn::ConnectionRole role, int desiredFrameRate) const;

    int profileByRole(Qn::ConnectionRole role) const;

    CameraDiagnostics::Result findProfiles(
        int* outPrimaryProfileNumber,
        int* outSecondaryProfileNumber,
        int* totalProfileNumber,
        std::set<int>* profilesToRemoveIfProfilesExhausted);

    CameraDiagnostics::Result removeProfile(int profileNumber);

    CameraDiagnostics::Result createProfile(int* outProfileNumber, Qn::ConnectionRole role);

    void updateToChannel(int value);

    bool isNvr() const;
protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractPtzController* createPtzControllerInternal() override;

private:
    CameraDiagnostics::Result init();
    CameraDiagnostics::Result initSystem();
    CameraDiagnostics::Result initAttributes();
    CameraDiagnostics::Result initMedia();
    CameraDiagnostics::Result initIo();
    CameraDiagnostics::Result initPtz();
    CameraDiagnostics::Result initAdvancedParameters();
    CameraDiagnostics::Result initTwoWayAudio();
    CameraDiagnostics::Result initRemoteArchive();

    CameraDiagnostics::Result createNxProfiles();
    CameraDiagnostics::Result createNxProfile(
        Qn::ConnectionRole role,
        int* outNxProfile,
        int totalProfileNumber,
        std::set<int>* inOutProfilesToRemove);

    int chooseProfileToRemove(
        int totalProfileNumber,
        const std::set<int>& inOutProfilesToRemove) const;

    CameraDiagnostics::Result fetchStreamLimits(HanwhaStreamLimits* outStreamLimits);

    CameraDiagnostics::Result fetchCodecInfo(HanwhaCodecInfo* outCodecInfo);

    void sortResolutions(std::vector<QSize>* resolutions) const;

    CameraDiagnostics::Result fetchPtzLimits(QnPtzLimits* outPtzLimits);

    AVCodecID defaultCodecForStream(Qn::ConnectionRole role) const;
    QSize defaultResolutionForStream(Qn::ConnectionRole role) const;
    int defaultGovLengthForStream(Qn::ConnectionRole role) const;
    Qn::BitrateControl defaultBitrateControlForStream(Qn::ConnectionRole role) const;
    int defaultBitrateForStream(Qn::ConnectionRole role) const;
    Qn::EntropyCoding defaultEntropyCodingForStream(Qn::ConnectionRole role) const;
    QString defaultCodecProfileForStream(Qn::ConnectionRole role) const;
    int defaultFrameRateForStream(Qn::ConnectionRole role) const;

    QString defaultValue(const QString& parameter, Qn::ConnectionRole role) const;

    QString suggestCodecProfile(AVCodecID codec, QString desiredProfile) const;

    QSize bestSecondaryResolution(
        const QSize& primaryResolution,
        const std::vector<QSize>& resolutionList) const;

    QnCameraAdvancedParams filterParameters(const QnCameraAdvancedParams& allParameters) const;

    bool fillRanges(
        QnCameraAdvancedParams* inOutParameters,
        const HanwhaCgiParameters& cgiParameters) const;

    bool addSpecificRanges(
        QnCameraAdvancedParameter* inOutParameter) const;

    bool addBitrateRanges(
        QnCameraAdvancedParameter* inOutParameter,
        const HanwhaAdavancedParameterInfo& info) const;

    bool addFrameRateRanges(
        QnCameraAdvancedParameter* inOutParameter,
        const HanwhaAdavancedParameterInfo& info) const;

    using CreateDependencyFunc = 
        std::function<QnCameraAdvancedParameterDependency(
            const HanwhaCodecLimits& codecLimits,
            AVCodecID codec,
            const QSize& resolution,
            const QString& bitrateControl)>;

    bool addDependencies(
        QnCameraAdvancedParameter* inOutParameter,
        const HanwhaAdavancedParameterInfo& info,
        CreateDependencyFunc createDependencyFunc) const;

    boost::optional<HanwhaAdavancedParameterInfo> advancedParameterInfo(const QString& id) const;

    boost::optional<QString> tryToGetSpecificParameterDefault(
        const QString& parameterString,
        const HanwhaResponse& response) const;

    boost::optional<int> calculateDefaultBitrate(
        const QString& parmeterString,
        const HanwhaResponse& response) const;

    boost::optional<int> calculateDefaultGovLength(
        const QString& parameterString,
        const HanwhaResponse& response) const;

    std::tuple<int, int, AVCodecID> channelProfileCodec(const QString& parameterString) const;
    
    QString toHanwhaAdvancedParameterValue(
        const QnCameraAdvancedParameter& parameter,
        const HanwhaAdavancedParameterInfo& parameterInfo,
        const QString& str) const;

    QString fromHanwhaAdvancedParameterValue(
        const QnCameraAdvancedParameter& parameter,
        const HanwhaAdavancedParameterInfo& parameterInfo,
        const QString& str) const;

    void reopenStreams(bool reopenPrimary, bool reopenSecondary);

    int suggestBitrate(
        const HanwhaCodecLimits& limits,
        Qn::BitrateControl bitrateControl,
        double coefficient,
        int framerate) const;

    bool isBitrateInLimits(
        const HanwhaCodecLimits& limits,
        Qn::BitrateControl bitrateControl,
        int bitrate) const;

    QnCameraAdvancedParamValueList filterGroupParameters(
        const QnCameraAdvancedParamValueList& values);

    QString groupLead(const QString& groupName) const;

    boost::optional<QnCameraAdvancedParamValue> findButtonParameter(
        const QnCameraAdvancedParamValueList) const;

    bool executeCommand(const QnCameraAdvancedParamValue& command);
    void initMediaStreamCapabilities();
    nx::media::CameraStreamCapability mediaCapabilityForRole(Qn::ConnectionRole role);
    bool executeServiceCommand(
        const QnCameraAdvancedParameter& parameter,
        const HanwhaAdavancedParameterInfo& info);

    bool resetProfileToDefault(Qn::ConnectionRole role);

    QString propertyByPrameterAndRole(const QString& parameter, Qn::ConnectionRole role) const;

private:
    using AdvancedParameterId = QString;

    mutable QnMutex m_mutex;
    int m_maxProfileCount = 0;
    HanwhaStreamLimits m_streamLimits;
    HanwhaCodecInfo m_codecInfo;
    std::map<Qn::ConnectionRole, int> m_profileByRole;

    Ptz::Capabilities m_ptzCapabilities = Ptz::NoPtzCapabilities;
    QnPtzLimits m_ptzLimits;
    QnPtzAuxilaryTraitList m_ptzTraits;

    std::map<AdvancedParameterId, HanwhaAdavancedParameterInfo> m_advancedParameterInfos;
    HanwhaAttributes m_attributes;
    HanwhaCgiParameters m_cgiParameters;
    bool m_isNvr = false;

    nx::media::CameraMediaCapability m_capabilities;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)

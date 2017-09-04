#pragma once

#if defined(ENABLE_HANWHA)

#include <core/ptz/ptz_limits.h>

#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/resource/hanwha/hanwha_stream_limits.h>
#include <plugins/resource/hanwha/hanwha_advanced_parameter_info.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>

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
    HanwhaResource();
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
    QSize streamResolution(Qn::ConnectionRole role) const;
    int streamGovLength(Qn::ConnectionRole role) const;
    int closestFrameRate(Qn::ConnectionRole role, int desiredFrameRate) const;

    int profileByRole(Qn::ConnectionRole role) const;

    CameraDiagnostics::Result findProfiles(
        int* outPrimaryProfileNumber,
        int* outSecondaryProfileNumber,
        int* totalProfileNumber,
        std::set<int>* profilesToRemoveIfProfilesExhausted);

    CameraDiagnostics::Result removeProfile(int profileNumber);

    CameraDiagnostics::Result createProfile(int* outProfileNumber, Qn::ConnectionRole role);

protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractPtzController* createPtzControllerInternal() override;

private:
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
    void sortResolutions(std::vector<QSize>* resolutions) const;

    CameraDiagnostics::Result fetchPtzLimits(QnPtzLimits* outPtzLimits);

    AVCodecID defaultCodecForStream(Qn::ConnectionRole role) const;
    QSize defaultResolutionForStream(Qn::ConnectionRole role) const;
    int defaultGovLengthForStream(Qn::ConnectionRole role) const;

    QSize bestSecondaryResolution(
        const QSize& primaryResolution,
        const std::vector<QSize>& resolutionList) const;

    QnCameraAdvancedParams filterParameters(const QnCameraAdvancedParams& allParameters) const;

    bool fillRanges(
        QnCameraAdvancedParams* inOutParameters,
        const HanwhaCgiParameters& cgiParameters) const;

    boost::optional<HanwhaAdavancedParameterInfo> advancedParameterInfo(const QString& id) const;

private:
    using AdvancedParameterId = QString;

    mutable QnMutex m_mutex;
    int m_maxProfileCount = 0;
    HanwhaStreamLimits m_streamLimits;
    std::map<Qn::ConnectionRole, int> m_profileByRole;

    Ptz::Capabilities m_ptzCapabilities;
    QnPtzLimits m_ptzLimits;

    std::map<AdvancedParameterId, HanwhaAdavancedParameterInfo> m_advancedParameterInfos;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)

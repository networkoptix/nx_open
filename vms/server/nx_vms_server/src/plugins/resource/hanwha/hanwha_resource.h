#pragma once

#include <boost/optional.hpp>

#include <core/ptz/ptz_limits.h>

#include <plugins/resource/hanwha/hanwha_advanced_parameter_info.h>
#include <plugins/resource/hanwha/hanwha_archive_delegate.h>
#include <plugins/resource/hanwha/hanwha_attributes.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_codec_limits.h>
#include <plugins/resource/hanwha/hanwha_ptz_common.h>
#include <plugins/resource/hanwha/hanwha_range.h>
#include <plugins/resource/hanwha/hanwha_remote_archive_manager.h>
#include <plugins/resource/hanwha/hanwha_request_helper.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>
#include <plugins/resource/onvif/onvif_resource.h>

#include <core/ptz/ptz_auxiliary_trait.h>
#include <nx/core/ptz/type.h>
#include <nx/utils/timer_holder.h>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx {

namespace vms::server { namespace resource { class SharedContextPool; } }

namespace vms::server {
namespace plugins {

enum class HanwhaProfileParameterFlag
{
    newProfile = 1,
    audioSupported = 1 << 1
};

Q_DECLARE_FLAGS(HanwhaProfileParameterFlags, HanwhaProfileParameterFlag);

class HanwhaResource: public QnPlOnvifResource
{
    using base_type = QnPlOnvifResource;
public:
    HanwhaResource(QnMediaServerModule* serverModule);
    virtual ~HanwhaResource() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual nx::core::resource::AbstractRemoteArchiveManager* remoteArchiveManager() override;

    virtual QnCameraAdvancedParamValueMap getApiParameters(const QSet<QString>& ids) override;
    virtual QSet<QString> setApiParameters(const QnCameraAdvancedParamValueMap& values) override;

    virtual bool setOutputPortState(
        const QString& outputId,
        bool activate,
        unsigned int autoResetTimeoutMs = 0) override;

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

    virtual bool captureEvent(const nx::vms::event::AbstractEventPtr& event) override;

    virtual bool isAnalyticsDriverEvent(nx::vms::api::EventType eventType) const override;

    QnTimePeriodList getDtsTimePeriods(
        qint64 startTimeMs,
        qint64 endTimeMs,
        int detailLevel,
        bool keepSmalChunks,
        int limit,
        Qt::SortOrder sortOrder);

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

    SessionContextPtr session(
        HanwhaSessionType sessionType,
        const QnUuid& clientId,
        bool generateNewOne = false);

    std::unique_ptr<QnAbstractArchiveDelegate> remoteArchiveDelegate();

    bool isVideoSourceActive();

    int maxProfileCount() const;

    AVCodecID streamCodec(Qn::ConnectionRole role) const;
    QString streamCodecProfile(AVCodecID codec, Qn::ConnectionRole role) const;
    QSize streamResolution(Qn::ConnectionRole role) const;
    int streamFrameRate(Qn::ConnectionRole role, int desiredFps) const;
    int streamGovLength(Qn::ConnectionRole role) const;
    Qn::BitrateControl streamBitrateControl(Qn::ConnectionRole role) const;
    int streamBitrate(Qn::ConnectionRole role, const QnLiveStreamParams& streamParams) const;

    int closestFrameRate(Qn::ConnectionRole role, int desiredFrameRate) const;

    int profileByRole(Qn::ConnectionRole role, bool isBypassProfile = false) const;

    CameraDiagnostics::Result findProfiles(
        boost::optional<HanwhaVideoProfile>* outPrimaryProfile,
        boost::optional<HanwhaVideoProfile>* outSecondaryProfile,
        int* totalProfileNumber,
        std::set<int>* profilesToRemoveIfProfilesExhausted,
        bool useBypass = false);

    CameraDiagnostics::Result fetchProfiles(
        HanwhaProfileMap* outProfiles,
        bool useBypass = false);

    CameraDiagnostics::Result removeProfile(int profileNumber);

    CameraDiagnostics::Result createProfile(int* outProfileNumber, Qn::ConnectionRole role);

    // Returns profile number for role if profile was found and has been considered as correct,
    // otherwise returns boost::none.
    boost::optional<int> verifyProfile(Qn::ConnectionRole role);

    CameraDiagnostics::Result updateProfileNameIfNeeded(
        Qn::ConnectionRole role,
        const HanwhaVideoProfile& profile);

    void updateToChannel(int value);

    bool isNvr() const;
    bool isProxiedAnalogEncoder() const;
    HanwhaDeviceType deviceType() const;
    HanwhaDeviceType bypassDeviceType() const;

    bool hasSerialPort() const;

    QString nxProfileName(
        Qn::ConnectionRole role,
        boost::optional<int> forcedProfileNameLength = boost::none) const;

    bool needToReplaceProfile(
        const boost::optional<HanwhaVideoProfile>& nxProfileToReplace,
        Qn::ConnectionRole role) const;

    std::shared_ptr<HanwhaSharedResourceContext> sharedContext() const;

    virtual bool setCameraCredentialsSync(const QAuthenticator& auth, QString* outErrorString = nullptr) override;

    bool isConnectedViaSunapi() const;

    HanwhaProfileParameters makeProfileParameters(
        Qn::ConnectionRole role,
        const QnLiveStreamParams& parameters,
        HanwhaProfileParameterFlags flags) const;

    bool isBypassSupported() const;
    boost::optional<int> bypassChannel() const;

    CameraDiagnostics::Result enableAudioInput();

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractPtzController* createPtzControllerInternal() const override;
    virtual QnAbstractArchiveDelegate* createArchiveDelegate() override;
    virtual bool allowRtspVideoLayout() const override { return false; }

    virtual void setSupportedAnalyticsEventTypeIds(
        QnUuid engineId, QSet<QString> supportedEvents) override;

private:
    CameraDiagnostics::Result initDevice();
    CameraDiagnostics::Result initSystem(const HanwhaInformation& information);
    CameraDiagnostics::Result initBypass();

    CameraDiagnostics::Result initMedia();
    CameraDiagnostics::Result setProfileSessionPolicy();

    CameraDiagnostics::Result initIo();
    CameraDiagnostics::Result initPtz();
    CameraDiagnostics::Result initConfigurationalPtz();
    CameraDiagnostics::Result initRedirectedAreaZoomPtz();
    CameraDiagnostics::Result initAdvancedParameters();
    CameraDiagnostics::Result initTwoWayAudio();
    CameraDiagnostics::Result initRemoteArchive();

    CameraDiagnostics::Result handleProxiedDeviceInfo(const HanwhaResponse& deviceInfoResponse);
    CameraDiagnostics::Result createNxProfiles();
    CameraDiagnostics::Result fetchExistingProfiles();
    CameraDiagnostics::Result createNxProfile(
        Qn::ConnectionRole role,
        int* outNxProfile,
        int totalProfileNumber,
        std::set<int>* inOutProfilesToRemove);

    int chooseProfileToRemove(
        int totalProfileNumber,
        const std::set<int>& inOutProfilesToRemove) const;

    CameraDiagnostics::Result setUpProfilePolicies(
        int primaryProfile,
        int secondaryProfile);

    CameraDiagnostics::Result fetchPtzLimits(QnPtzLimits* outPtzLimits);

    CameraDiagnostics::Result fetchCodecInfo(HanwhaCodecInfo* outCodecInfo);

    void cleanUpOnProxiedDeviceChange();

    HanwhaPtzRangeMap fetchPtzRanges();
    QnPtzAuxiliaryTraitList calculatePtzTraits() const;
    QnPtzAuxiliaryTraitList calculateCameraOnlyTraits() const;
    void calculateAutoFocusSupport(QnPtzAuxiliaryTraitList* outTraitList) const;

    AVCodecID defaultCodecForStream(Qn::ConnectionRole role) const;
    QSize defaultResolutionForStream(Qn::ConnectionRole role) const;
    int defaultGovLengthForStream(Qn::ConnectionRole role) const;
    Qn::BitrateControl defaultBitrateControlForStream(Qn::ConnectionRole role) const;
    int defaultBitrateForStream(Qn::ConnectionRole role) const;
    Qn::EntropyCoding defaultEntropyCodingForStream(Qn::ConnectionRole role) const;
    QString defaultCodecProfileForStream(Qn::ConnectionRole role) const;
    int defaultFrameRateForStream(Qn::ConnectionRole role) const;

    QString defaultValue(const QString& parameter, Qn::ConnectionRole role) const;

    QString suggestCodecProfile(
        AVCodecID codec,
        Qn::ConnectionRole role,
        const QString& desiredProfile) const;

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

    bool addResolutionRanges(
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

    QnCameraAdvancedParamValueList addAssociatedParameters(
        const QnCameraAdvancedParamValueList& values);

    QString groupLead(const QString& groupName) const;

    boost::optional<QnCameraAdvancedParamValue> findButtonParameter(
        const QnCameraAdvancedParamValueList) const;

    bool executeCommand(const QnCameraAdvancedParamValue& command);
    bool executeCommandInternal(
        const HanwhaAdavancedParameterInfo& info,
        const HanwhaRequestHelper::Parameters& parameters);

    void initMediaStreamCapabilities();

    nx::media::CameraStreamCapability mediaCapabilityForRole(Qn::ConnectionRole role) const;

    bool executeServiceCommand(
        const QnCameraAdvancedParameter& parameter,
        const HanwhaAdavancedParameterInfo& info);

    bool resetProfileToDefault(Qn::ConnectionRole role);

    QString propertyByPrameterAndRole(const QString& parameter, Qn::ConnectionRole role) const;

    struct HanwhaPortInfo
    {
        QString submenu;
        QString number;
        QString prefix;
        bool isProxied = false;
    };

    boost::optional<HanwhaPortInfo> portInfoFromId(const QString& id) const;

    bool setOutputPortStateInternal(const QString& outputId, bool activate);

    const HanwhaAttributes& attributes() const;
    const HanwhaCgiParameters& cgiParameters() const;

    // Proxied id is an id of a device connected to some proxy (e.g. NVR)
    virtual QString proxiedId() const;
    virtual void setProxiedId(const QString& proxiedId);

    bool isProxiedMultisensorCamera() const;

    void setDirectProfile(Qn::ConnectionRole role, int profileNumber);
    void setBypassProfile(Qn::ConnectionRole role, int profileNumber);

    Ptz::Capabilities ptzCapabilities(nx::core::ptz::Type ptzType) const;

    void setPtzCalibarionTimer();

private:
    using AdvancedParameterId = QString;

    struct ProfileNumbers
    {
        // TODO: #dmishin move to std::optional one day.

        // Profile number available via NVR CGI or standalone camera CGI.
        int directNumber = kHanwhaInvalidProfile;

        // Profile numbera available via bypass. Not relevant for standalone cameras
        int bypassNumber = kHanwhaInvalidProfile;
    };

    mutable QnMutex m_mutex;
    int m_maxProfileCount = 0;
    HanwhaCodecInfo m_codecInfo;
    std::map<Qn::ConnectionRole, ProfileNumbers> m_profileByRole;

    QnPtzLimits m_ptzLimits;
    QnPtzAuxiliaryTraitList m_ptzTraits;
    HanwhaPtzRangeMap m_ptzRanges;
    HanwhaPtzCapabilitiesMap m_ptzCapabilities = {
        {nx::core::ptz::Type::operational, Ptz::NoPtzCapabilities},
        {nx::core::ptz::Type::configurational, Ptz::NoPtzCapabilities}
    };

    std::map<AdvancedParameterId, HanwhaAdavancedParameterInfo> m_advancedParameterInfos;

    bool m_isBypassSupported = false;
    int m_proxiedDeviceChannelCount = 1;
    bool m_hasSerialPort = false;

    HanwhaAttributes m_attributes;
    HanwhaAttributes m_bypassDeviceAttributes;

    HanwhaCgiParameters m_cgiParameters;
    HanwhaCgiParameters m_bypassDeviceCgiParameters;

    HanwhaDeviceType m_deviceType = HanwhaDeviceType::unknown;
    HanwhaDeviceType m_bypassDeviceType = HanwhaDeviceType::unknown;
    bool m_isChannelConnectedViaSunapi = false;

    nx::media::CameraMediaCapability m_capabilities;
    QMap<QString, QnIOPortData> m_ioPortTypeById;
    std::atomic<bool> m_areInputPortsMonitored{false};
    QString m_defaultOutputPortId;

    nx::utils::TimerHolder m_timerHolder;
    std::shared_ptr<HanwhaSharedResourceContext> m_sharedContext;
    std::unique_ptr<HanwhaRemoteArchiveManager> m_remoteArchiveManager;
    std::unique_ptr<HanwhaArchiveDelegate> m_remoteArchiveDelegate;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx

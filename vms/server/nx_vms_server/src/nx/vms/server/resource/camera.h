#pragma once

#include <core/resource/camera_resource.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/media_stream_capability.h>
#include <core/dataconsumer/audio_data_transmitter.h>
#include <media_server/media_server_module.h>

#include "resource_fwd.h"
#include <nx/vms/server/server_module_aware.h>

typedef std::shared_ptr<QnAbstractAudioTransmitter> QnAudioTransmitterPtr;
class QnAbstractPtzController;

namespace nx::vms::server::resource { struct MulticastParameters; }

namespace nx {
namespace vms::server {
namespace resource {

class StreamCapabilityAdvancedParametersProvider;

struct StreamCapabilityKey
{
    QString codec;
    QSize resolution;

    StreamCapabilityKey() = default;
    StreamCapabilityKey(const QString& codec, const QSize& resolution):
        codec(codec),
        resolution(resolution)
    {
    }

    bool operator<(const StreamCapabilityKey& other) const
    {
        if (codec != other.codec)
            return codec < other.codec;
        if (resolution.width() != other.resolution.width())
            return resolution.width() < other.resolution.width();
        return resolution.height() < other.resolution.height();
    }

    QString toString() const
    {
        return lm("%1(%2x%3)").args(codec, resolution.width(), resolution.height());
    }
};
using StreamCapabilityMap = QMap<StreamCapabilityKey, nx::media::CameraStreamCapability>;
using StreamCapabilityMaps = QMap<nx::vms::api::StreamIndex, StreamCapabilityMap>;

class Camera:
    public QnVirtualCameraResource,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
    using base_type = QnVirtualCameraResource;
public:
    static const float kMaxEps;

    Camera(QnMediaServerModule* serverModule);
    virtual ~Camera() override;

    /*!
        Calls \a QnResource::init. If \a QnResource::init is already running in another thread, this method waits for it to complete
    */
    void blockingInit();

    /**
     * The difference between desired and real is that camera can have multiple clients we do not
     * know about or big exposure time.
     */
    int getPrimaryStreamRealFps() const;
    virtual QString defaultCodec() const;

    virtual void setUrl(const QString &url) override;
    virtual int getChannel() const override;

    /**
     * @return Driver (built-in or external) name, used to manage camera. This can be "axis",
     * "dlink", "onvif", etc.
    */
    virtual QString getDriverName() const = 0;

    QnAbstractPtzController* createPtzController() const;

    /** Returns id-value pairs. */
    QnCameraAdvancedParamValueMap getAdvancedParameters(const QSet<QString>& ids);
    boost::optional<QString> getAdvancedParameter(const QString& id);

    /** Returns ids of successfully set parameters. */
    QSet<QString> setAdvancedParameters(const QnCameraAdvancedParamValueMap& values);
    bool setAdvancedParameter(const QString& id, const QString& value);

    virtual QnAdvancedStreamParams advancedLiveStreamParams() const override;

    static float getResolutionAspectRatio(const QSize& resolution); // find resolution helper function

    /** Gets supported codecs and their resolution list. */
    StreamCapabilityMap getStreamCapabilityMap(nx::vms::api::StreamIndex streamIndex);

    /**
     * @param resolution Resolution for which we want to find the closest one.
     * @param aspectRatio Ideal aspect ratio for resolution we want to find. If 0 than this
     *  this parameter is not taken in account.
     * @param maxResolutionArea Maximum area of found resolution.
     * @param resolutionList List of resolutions from which we should choose closest
     * @param outCoefficient Output parameter. Measure of similarity for original resolution
     *  and a found one.
     * @return Closest resolution or empty QSize if nothing found.
     */
    static QSize getNearestResolution(
        const QSize& resolution,
        float desiredAspectRatio,
        double maxResolutionArea,
        const QList<QSize>& resolutionList,
        double* outCoefficient = 0);

    /**
     * Simple wrapper for getNearestResolution(). Calls getNearestResolution() twice.
     * First time it calls it with provided aspectRatio. If appropriate resolution is not found
     * getNearestResolution() is called with 0 aspectRatio.
     */
    static QSize closestResolution(
        const QSize& idealResolution,
        float desiredAspectRatio,
        const QSize& maxResolution,
        const QList<QSize>& resolutionList,
        double* outCoefficient = 0);

    static QSize closestSecondaryResolution(
        float desiredAspectRatio,
        const QList<QSize>& resolutionList);

    class AdvancedParametersProvider
    {
    public:
        virtual ~AdvancedParametersProvider() = default;

        /** @return supported parameters descriptions. */
        virtual QnCameraAdvancedParams descriptions() = 0;

        /** @return id-value pairs. */
        virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) = 0;

        /** @return ids of successfully set parameters. */
        virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) = 0;
    };

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

    virtual QnAudioTransmitterPtr getAudioTransmitter();

    void setLastMediaIssue(const CameraDiagnostics::Result& issue);
    CameraDiagnostics::Result getLastMediaIssue() const;

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);
    int getMaxChannels() const;

    void inputPortListenerAttached();
    void inputPortListenerDetached();

    /** Override to support output ports. */
    virtual bool setOutputPortState(
        const QString& portId,
        bool value,
        unsigned int autoResetTimeoutMs = 0);

    /** Override for IO modules. */
    virtual QnIOStateDataList ioPortStates() const;

    void reopenStream(nx::vms::api::StreamIndex streamIndex);

    nx::streaming::rtp::TimeOffsetPtr getTimeOffset() { return m_timeOffset; }

    /*
     * Return time periods from resource based archive (direct to storage)
     */
    virtual QnTimePeriodList getDtsTimePeriods(
        qint64 startTimeMs,
        qint64 endTimeMs,
        int detailLevel,
        bool keepSmalChunks,
        int limit,
        Qt::SortOrder sortOrder);

    enum class Role
    {
        regular,
        subchannel
    };
    void setRole(Role role) { m_role = role; }
    Role getRole() const { return m_role; }

    bool fixMulticastParametersIfNeeded(
        nx::vms::server::resource::MulticastParameters* inOutMulticastParameters,
        nx::vms::api::StreamIndex streamIndex);

signals:
    /** Emit on camera or IO module input change. */
    void inputPortStateChanged(
        const QnResourcePtr& resource,
        const QString& portId,
        bool value,
        qint64 timestamp);

    /** Emit on IO modules only. */
    void outputPortStateChanged(
        const QnResourcePtr& resource,
        const QString& portId,
        bool value,
        qint64 timestamp);

protected:
    virtual int getMaxChannelsFromDriver() const { return 1; }

    virtual CameraDiagnostics::Result initInternal() override;
    virtual void initializationDone() override;

    /** Is called during initInternal(). */
    virtual CameraDiagnostics::Result initializeCameraDriver() = 0;

    /** Override to add support for advanced parameters. */
    virtual std::vector<AdvancedParametersProvider*>  advancedParametersProviders();

    /**
     * Gets supported codecs and their resolution list.
     * For each key optional CameraStreamCapability could be provided.
     * CameraStreamCapability could be null. That case it is auto-filled with default values.
     */
    virtual StreamCapabilityMap getStreamCapabilityMapFromDriver(
        nx::vms::api::StreamIndex streamIndex);

    /**
     * @return stream capability traits
     *  (e.g. availability of second stream resolution depending on primary stream resolution)
     */
    virtual nx::media::CameraTraits mediaTraits() const;

    virtual QnAbstractPtzController* createPtzControllerInternal() const;

    /** Override to support input port monitoring. */
    virtual void startInputPortStatesMonitoring();
    virtual void stopInputPortStatesMonitoring();

private:
    CameraDiagnostics::Result initializeAdvancedParametersProviders();
    void fixInputPortMonitoring();

protected:
    QnAudioTransmitterPtr m_audioTransmitter;

private:
    using StreamCapabilityAdvancedParameterProviders = std::map<
        nx::vms::api::StreamIndex,
        std::unique_ptr<StreamCapabilityAdvancedParametersProvider>>;

    int m_channelNumber; // video/audio source number
    nx::streaming::rtp::TimeOffsetPtr m_timeOffset;
    QElapsedTimer m_lastInitTime;
    QAuthenticator m_lastCredentials;
    AdvancedParametersProvider* m_defaultAdvancedParametersProvider = nullptr;
    std::map<QString, AdvancedParametersProvider*> m_advancedParametersProvidersByParameterId;
    StreamCapabilityAdvancedParameterProviders m_streamCapabilityAdvancedProviders;
    CameraDiagnostics::Result m_lastMediaIssue = CameraDiagnostics::NoErrorResult();
    nx::media::CameraTraits m_mediaTraits;

    std::atomic<size_t> m_inputPortListenerCount = 0;
    bool m_inputPortListeningInProgress = false;
    QnMutex m_ioPortStatesMutex;
    std::map<QString, QnIOStateData> m_ioPortStatesCache;
    Role m_role = Role::regular;
};

} // namespace resource
} // namespace vms::server
} // namespace nx

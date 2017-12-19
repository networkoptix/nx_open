#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include <deque>

#include <QtCore/QMetaType>
#include <QtCore/QElapsedTimer>

#include <nx_ec/ec_api_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <utils/camera/camera_diagnostics.h>
#include <utils/common/aspect_ratio.h>

#include <core/resource/camera_media_stream_info.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/security_cam_resource.h>

class CameraMediaStreams;
class CameraBitrates;
class CameraBitrateInfo;

class QnVirtualCameraResource : public QnSecurityCamResource
{
    Q_OBJECT
    Q_FLAGS(Qn::CameraCapabilities)
    Q_PROPERTY(Qn::CameraCapabilities cameraCapabilities READ getCameraCapabilities WRITE setCameraCapabilities)
    using base_type = QnSecurityCamResource;

public:
    QnVirtualCameraResource(QnCommonModule* commonModule = nullptr);

    virtual QString getUniqueId() const override;

    virtual QStringList searchFilters() const override;
    void forceEnableAudio();
    void forceDisableAudio();
    bool isForcedAudioSupported() const;
    virtual int saveAsync();
    void updateDefaultAuthIfEmpty(const QString& login, const QString& password);

    //! Camera source URL, commonly - rtsp link.
    QString sourceUrl(Qn::ConnectionRole role) const;
    void updateSourceUrl(const QString& url, Qn::ConnectionRole role);

    static int issuesTimeoutMs();

    void issueOccured();
    void cleanCameraIssues();

    CameraMediaStreams mediaStreams() const;
    CameraMediaStreamInfo defaultStream() const;
    CameraMediaStreamInfo secondaryStream() const;

    QnAspectRatio aspectRatio() const;

    // TODO: saveMediaStreamInfoIfNeeded and saveBitrateIfNeeded should be moved into
    // nx::mediaserver::resource::Camera, as soon as QnLiveStreamProvider moved into nx::mediaserver.

    /** @return true if mediaStreamInfo differs from existing and has been saved. */
    bool saveMediaStreamInfoIfNeeded( const CameraMediaStreamInfo& mediaStreamInfo );
    bool saveMediaStreamInfoIfNeeded( const CameraMediaStreams& streams );

    /** @return true if bitrateInfo.encoderIndex is not already saved. */
    bool saveBitrateIfNeeded( const CameraBitrateInfo& bitrateInfo );

    // TODO: Move to nx::mediaserver::resource::Camera, it should be used only on server cameras!
    /**
     * Returns advanced live stream parameters. These parameters are configured on advanced tab.
     * For primary stream this parameters are merged with parameters on record schedule.
     */
    virtual QnAdvancedStreamParams advancedLiveStreamParams() const;

private:
    void saveResolutionList( const CameraMediaStreams& supportedNativeStreams );

private:
    int m_issueCounter;
    QElapsedTimer m_lastIssueTimer;
    std::map<Qn::ConnectionRole, QString> m_cachedStreamUrls;
    QnMutex m_mediaStreamsMutex;
};

const QSize EMPTY_RESOLUTION_PAIR(0, 0);
const QSize SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 360);
const QSize SECONDARY_STREAM_MAX_RESOLUTION(1024, 768);
const QSize UNLIMITED_RESOLUTION(INT_MAX, INT_MAX);

Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);

#endif // QN_CAMERA_RESOURCE_H

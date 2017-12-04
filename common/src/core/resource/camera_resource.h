#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include <deque>

#include <QtCore/QMetaType>
#include <QtCore/QElapsedTimer>

#include <nx_ec/ec_api_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <utils/camera/camera_diagnostics.h>
#include <utils/common/aspect_ratio.h>

#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_media_stream_info.h>

class QnAbstractDTSFactory;

class CameraMediaStreams;
class CameraBitrates;
class CameraBitrateInfo;

class QN_EXPORT QnVirtualCameraResource : public QnSecurityCamResource
{
    Q_OBJECT
    Q_FLAGS(Qn::CameraCapabilities)
    Q_PROPERTY(Qn::CameraCapabilities cameraCapabilities READ getCameraCapabilities WRITE setCameraCapabilities)
    using base_type = QnSecurityCamResource;
public:
    QnVirtualCameraResource(QnCommonModule* commonModule = nullptr);

    QnAbstractDTSFactory* getDTSFactory();
    void setDTSFactory(QnAbstractDTSFactory* factory);
    void lockDTSFactory();
    void unLockDTSFactory();

    virtual QString getUniqueId() const override;

    virtual QString toSearchString() const override;
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

    QnAspectRatio aspectRatio() const;

private:
    QnAbstractDTSFactory* m_dtsFactory;
    int m_issueCounter;
    QElapsedTimer m_lastIssueTimer;
    std::map<Qn::ConnectionRole, QString> m_cachedStreamUrls;
};


const QSize EMPTY_RESOLUTION_PAIR(0, 0);
const QSize SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 316); // 316 is average between 272&360
const QSize SECONDARY_STREAM_MAX_RESOLUTION(1024, 768);

class QN_EXPORT QnPhysicalCameraResource : public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnPhysicalCameraResource(QnCommonModule* commonModule = nullptr);

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    float rawSuggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const;
    virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps, Qn::ConnectionRole role = Qn::CR_Default) const;

    virtual void setUrl(const QString &url) override;
    virtual int getChannel() const override;

    /*!
        \return \a true if \a mediaStreamInfo differs from existing and has been saved
    */
    bool saveMediaStreamInfoIfNeeded( const CameraMediaStreamInfo& mediaStreamInfo );
    bool saveMediaStreamInfoIfNeeded( const CameraMediaStreams& streams );

    /*!
     *  \return \a true if \param bitrateInfo.encoderIndex is not already saved
     */
    bool saveBitrateIfNeeded( const CameraBitrateInfo& bitrateInfo );

    static float getResolutionAspectRatio(const QSize& resolution); // find resolution helper function
    static QSize getNearestResolution(const QSize& resolution, float aspectRatio, double maxResolutionSquare, const QList<QSize>& resolutionList, double* coeff = 0); // find resolution helper function

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    bool initializeBasicTwoWayAudio();
private:
    void saveResolutionList( const CameraMediaStreams& supportedNativeStreams );
private:
    QnMutex m_mediaStreamsMutex;
    int m_channelNumber; // video/audio source number
    QElapsedTimer m_lastInitTime;
    QAuthenticator m_lastCredentials;
};



Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);

#endif // QN_CAMERA_RESOURCE_H

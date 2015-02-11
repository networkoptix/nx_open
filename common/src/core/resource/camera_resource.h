#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include <deque>

#include <QtCore/QMetaType>

#include <nx_ec/ec_api_fwd.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/camera/camera_diagnostics.h>

#include "security_cam_resource.h"


class QnAbstractDTSFactory;

class QN_EXPORT QnVirtualCameraResource : public QnSecurityCamResource
{
    Q_OBJECT
    Q_FLAGS(Qn::CameraCapabilities)
    Q_PROPERTY(Qn::CameraCapabilities cameraCapabilities READ getCameraCapabilities WRITE setCameraCapabilities)

public:
    QnVirtualCameraResource();

    QnAbstractDTSFactory* getDTSFactory();
    void setDTSFactory(QnAbstractDTSFactory* factory);
    void lockDTSFactory();
    void unLockDTSFactory();

    virtual QString getUniqueId() const override;

    QString toSearchString() const override;
    void forceEnableAudio();
    void forceDisableAudio();
    bool isForcedAudioSupported() const;
    void saveParams();
    void saveParamsAsync();
    int saveAsync();
public slots:
    void issueOccured();
    void noCameraIssues();
private slots:
    void at_saveAsyncFinished(int, ec2::ErrorCode, const QnVirtualCameraResourceList &);
protected:

private:
    QnAbstractDTSFactory* m_dtsFactory;
    std::deque<qint64> m_issueTimes;
};


const QSize EMPTY_RESOLUTION_PAIR(0, 0);
const QSize SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 316); // 316 is average between 272&360
const QSize SECONDARY_STREAM_MAX_RESOLUTION(1024, 768);

class CameraMediaStreams;
class CameraMediaStreamInfo;

class QN_EXPORT QnPhysicalCameraResource : public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnPhysicalCameraResource();

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const;

    virtual void setUrl(const QString &url) override;
    virtual int getChannel() const override;

    /*!
        \return \a true if \a mediaStreamInfo differs from existing and has been saved
    */
    bool saveMediaStreamInfoIfNeeded( const CameraMediaStreamInfo& mediaStreamInfo );

    static float getResolutionAspectRatio(const QSize& resolution); // find resolution helper function
    static QSize getNearestResolution(const QSize& resolution, float aspectRatio, double maxResolutionSquare, const QList<QSize>& resolutionList, double* coeff = 0); // find resolution helper function

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    void saveResolutionList( const CameraMediaStreams& supportedNativeStreams );

private:
    QnMutex m_mediaStreamsMutex;
    int m_channelNumber; // video/audio source number
};

class CameraMediaStreamInfo
{
public:
    //!0 - primary stream, 1 - secondary stream
    int encoderIndex;
    //!has format "1920x1080" or "*" to notify that any resolution is supported
    QString resolution;
    //!transport method that can be used to receive this stream
    /*!
        Possible values:\n
            - rtsp
            - webm (webm over http)
            - hls (Apple Http Live Streaming)
            - mjpg (motion jpeg over http)
    */
    std::vector<QString> transports;
    //!if \a true this stream is produced by transcoding one of native (having this flag set to \a false) stream
    bool transcodingRequired;
    int codec;

    CameraMediaStreamInfo();
    CameraMediaStreamInfo(
        int _encoderIndex,
        const QSize& _resolution,
        CodecID _codec );

    bool operator==( const CameraMediaStreamInfo& rhs ) const;
    bool operator!=( const CameraMediaStreamInfo& rhs ) const;
};
#define CameraMediaStreamInfo_Fields (encoderIndex)(resolution)(transports)(transcodingRequired)(codec)


class CameraMediaStreams
{
public:
    std::vector<CameraMediaStreamInfo> streams;
};
#define CameraMediaStreams_Fields (streams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (CameraMediaStreamInfo)(CameraMediaStreams), (json) )

Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);

#endif // QN_CAMERA_RESOURCE_H

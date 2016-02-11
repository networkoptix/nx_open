#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include <deque>

#include <QtCore/QMetaType>
#include <QtCore/QElapsedTimer>

#include <nx_ec/ec_api_fwd.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/camera/camera_diagnostics.h>

#include "security_cam_resource.h"

class QnAbstractDTSFactory;

class CameraMediaStreams;
class CameraMediaStreamInfo;
class CameraBitrates;
class CameraBitrateInfo;

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

    static int issuesTimeoutMs();

    void issueOccured();
    void cleanCameraIssues();

    CameraMediaStreams mediaStreams() const;
protected:

private:
    QnAbstractDTSFactory* m_dtsFactory;
    int m_issueCounter;
    QElapsedTimer m_lastIssueTimer;
};


const QSize EMPTY_RESOLUTION_PAIR(0, 0);
const QSize SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 316); // 316 is average between 272&360
const QSize SECONDARY_STREAM_MAX_RESOLUTION(1024, 768);

class QN_EXPORT QnPhysicalCameraResource : public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnPhysicalCameraResource();

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    float rawSuggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const;
    virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const;

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
private:
    void saveResolutionList( const CameraMediaStreams& supportedNativeStreams );
private:
    QnMutex m_mediaStreamsMutex;
    int m_channelNumber; // video/audio source number
};

class CameraMediaStreamInfo
{
public:
    static const int PRIMARY_STREAM_INDEX = 0;
    static const int SECONDARY_STREAM_INDEX = 1;

    static const QLatin1String anyResolution;
    static QString resolutionToString( const QSize& resolution = QSize() );

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
    std::map<QString, QString> customStreamParams;

    CameraMediaStreamInfo(
        int _encoderIndex = -1,
        const QSize& _resolution = QSize(),
        CodecID _codec = CODEC_ID_NONE)
    :
        encoderIndex( _encoderIndex ),
        resolution( resolutionToString( _resolution ) ),
        transcodingRequired( false ),
        codec( _codec )
    {
        //TODO #ak delegate to next constructor after moving to vs2013
    }

    template<class CustomParamDictType>
        CameraMediaStreamInfo(
            int _encoderIndex,
            const QSize& _resolution,
            CodecID _codec,
            CustomParamDictType&& _customStreamParams )
    :
        encoderIndex( _encoderIndex ),
        resolution( _resolution.isValid()
            ? QString::fromLatin1("%1x%2").arg(_resolution.width()).arg(_resolution.height())
            : anyResolution ),
        transcodingRequired( false ),
        codec( _codec ),
        customStreamParams( std::forward<CustomParamDictType>(_customStreamParams) )
    {
    }

    bool operator==( const CameraMediaStreamInfo& rhs ) const;
    bool operator!=( const CameraMediaStreamInfo& rhs ) const;
    QSize getResolution() const;
};
#define CameraMediaStreamInfo_Fields (encoderIndex)(resolution)(transports)(transcodingRequired)(codec)(customStreamParams)

class CameraMediaStreams
{
public:
    std::vector<CameraMediaStreamInfo> streams;
};
#define CameraMediaStreams_Fields (streams)

class CameraBitrateInfo
{
public:
    int encoderIndex;
    QString timestamp;

    float rawSuggestedBitrate;
    float suggestedBitrate;
    float actualBitrate;

    Qn::BitratePerGopType bitratePerGop;
    float bitrateFactor;

    int fps;
    float actualFps;
    float averageGopSize;
    QString resolution;
    int numberOfChannels;
    bool isConfigured;

    CameraBitrateInfo(int index = -1, QString time = QString())
        : encoderIndex(index)
        , timestamp(time)
        , rawSuggestedBitrate(-1)
        , suggestedBitrate(-1)
        , actualBitrate(-1)
        , bitratePerGop(Qn::BPG_None)
        , bitrateFactor(-1)
        , fps(-1)
        , actualFps(-1)
        , numberOfChannels(-1)
        , isConfigured(false)
    {
    }
};
#define CameraBitrateInfo_Fields (encoderIndex)(timestamp) \
    (rawSuggestedBitrate)(suggestedBitrate)(actualBitrate) \
    (bitratePerGop)(bitrateFactor) \
    (fps)(actualFps)(averageGopSize)(resolution)(numberOfChannels)(isConfigured)

class CameraBitrates
{
public:
    std::vector<CameraBitrateInfo> streams;
};
#define CameraBitrates_Fields (streams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( \
        (CameraMediaStreamInfo)(CameraMediaStreams) \
        (CameraBitrateInfo)(CameraBitrates), \
        (json) )

Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);

#endif // QN_CAMERA_RESOURCE_H

#pragma once

#include <QtNetwork/QHostAddress>
#include <nx/utils/thread/mutex.h>
#include "network/tcp_connection_processor.h"
#include <core/resource/resource_fwd.h>
#include "nx/streaming/media_data_packet.h"
#include "rtsp/abstract_rtsp_encoder.h"
#include <nx/network/rtsp/rtsp_types.h>

class QnAbstractStreamDataProvider;
class QnResourceVideoLayout;
class QnArchiveStreamReader;

struct RtspServerTrackInfo
{
    enum class MediaType
    {
        Undefined,
        Video,
        Audio,
        Subtitles,
        MetaData
    };

    RtspServerTrackInfo():
        clientPort(-1),
        clientRtcpPort(0),
        sequence(0),
        firstRtpTime(-1),
        mediaSocket(0),
        rtcpSocket(0),
        mediaType(MediaType::Undefined)
    {

    }
    ~RtspServerTrackInfo()
    {
        delete mediaSocket;
        delete rtcpSocket;
    }

    bool openServerSocket(const QString& peerAddress);

    void setEncoder(AbstractRtspEncoderPtr value)
    {
        QnMutexLocker lock(&m_mutex);
        encoder = std::move(value);
    }

    AbstractRtspEncoderPtr getEncoder() const
    {
        QnMutexLocker lock(&m_mutex);
        return encoder;
    }

    int clientPort;
    int clientRtcpPort;
    quint16 sequence;
    qint64 firstRtpTime;
    nx::network::AbstractDatagramSocket* mediaSocket;
    nx::network::AbstractDatagramSocket* rtcpSocket;
    MediaType mediaType;

private:
    AbstractRtspEncoderPtr encoder;
    mutable QnMutex m_mutex;
    static QnMutex m_createSocketMutex;
};

typedef QSharedPointer<RtspServerTrackInfo> RtspServerTrackInfoPtr;
typedef QMap<int, RtspServerTrackInfoPtr> ServerTrackInfoMap;

class QnRtspConnectionProcessorPrivate;
class QnRtspFfmpegEncoder;
class QnMediaServerModule;
enum class PlaybackMode;

class QnRtspConnectionProcessor: public QnTCPConnectionProcessor
{
    Q_OBJECT

public:
    static bool doesPathEndWithCameraId() { return true; } //< See the base class method.

    QnRtspConnectionProcessor(
        QnMediaServerModule* serverModule,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnRtspConnectionProcessor();
    qint64 getRtspTime();
    void setRtspTime(qint64 time);
    void resetTrackTiming();
    bool isTcpMode() const;
    QnMediaResourcePtr getResource() const;
    bool isLiveDP(QnAbstractStreamDataProvider* dp);

    void setQuality(MediaQuality quality);
    bool isSecondaryLiveDPSupported() const;
    QHostAddress getPeerAddress() const;
    QByteArray getRangeStr();
    int getMetadataChannelNum() const;
    int getAVTcpChannel(int trackNum) const;
    RtspServerTrackInfo* getTrackInfo(int trackNum) const;
    int getTracksCount() const;
    QnMediaServerModule* serverModule() const;
protected:
    virtual void run();
    void addResponseRangeHeader();
private slots:
    void at_camera_parentIdChanged(const QnResourcePtr & /*resource*/);
    void at_camera_resourceChanged(const QnResourcePtr & /*resource*/);

private:
    void checkQuality();
    void processRequest();
    bool parseRequestParams();
    void initResponse(
        nx::network::rtsp::StatusCodeValue code = nx::network::http::StatusCode::ok,
        const QString& message = "OK");
    void generateSessionId();
    void sendResponse(nx::network::rtsp::StatusCodeValue code, const QByteArray& contentType);
    PlaybackMode getStreamingMode() const;

    int numOfVideoChannels();
    nx::network::rtsp::StatusCodeValue composeDescribe();
    nx::network::rtsp::StatusCodeValue composeSetup();
    nx::network::rtsp::StatusCodeValue composePlay();
    nx::network::rtsp::StatusCodeValue composePause();
    int extractTrackId(const QString& path);
    nx::network::rtsp::StatusCodeValue composeTeardown();
    void processRangeHeader();
    nx::network::rtsp::StatusCodeValue composeSetParameter();
    nx::network::rtsp::StatusCodeValue composeGetParameter();
    void createDataProvider();
    void putLastIFrameToQueue();
    //QnAbstractMediaStreamDataProvider* getLiveDp();
    void setQualityInternal(MediaQuality quality);
    AbstractRtspEncoderPtr createEncoderByMediaData(
        QnConstAbstractMediaDataPtr mediaHigh,
        QnConstAbstractMediaDataPtr mediaLow,
        MediaQuality quality);
    QnConstAbstractMediaDataPtr getCameraData(
        QnAbstractMediaData::DataType dataType, MediaQuality quality);
    static int isFullBinaryMessage(const QByteArray& data);
    void processBinaryRequest();
    void createPredefinedTracks(QSharedPointer<const QnResourceVideoLayout> videoLayout);
    void updatePredefinedTracks();
    void notifyMediaRangeUsed(qint64 timestampUsec);
    QnRtspFfmpegEncoder* createRtspFfmpegEncoder(bool isVideo);
    QnConstMediaContextPtr getAudioCodecContext(int audioTrackIndex) const;
    nx::vms::api::StreamDataFilters streamFilterFromHeaders(
        nx::vms::api::StreamDataFilters oldFilters) const;
private:
    Q_DECLARE_PRIVATE(QnRtspConnectionProcessor);
    friend class QnRtspDataConsumer;
};

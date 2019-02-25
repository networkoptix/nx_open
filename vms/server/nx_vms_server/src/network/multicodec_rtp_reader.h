#pragma once
#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/QElapsedTimer>

#include <vector>
#include <atomic>

#include <core/dataprovider/abstract_media_stream_provider.h>
#include <core/resource/resource_consumer.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/media_data_packet.h>
#include <utils/camera/camera_diagnostics.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/safe_direct_connection.h>

#include <nx/streaming/rtsp_client.h>

#include <nx/vms/event/event_fwd.h>
#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>
#include <nx/streaming/rtp/camera_time_helper.h>

namespace nx::streaming::rtp  { class StreamParser; }

class QnMulticodecRtpReader:
    public QObject,
    public QnResourceConsumer,
    public QnAbstractMediaStreamProvider,
    public QnStoppable,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    using OnSocketReadTimeoutCallback = nx::utils::MoveOnlyFunc<QnAbstractMediaDataPtr()>;

    QnMulticodecRtpReader(
        const QnResourcePtr& resource,
        const nx::streaming::rtp::TimeOffsetPtr& timeOffset,
        std::unique_ptr<nx::network::AbstractStreamSocket> tcpSock = std::unique_ptr<nx::network::AbstractStreamSocket>());
    virtual ~QnMulticodecRtpReader();

    /** Implementation of QnAbstractMediaStreamProvider::getNextData. */
    virtual QnAbstractMediaDataPtr getNextData() override;

    /** Implementation of QnAbstractMediaStreamProvider::openStream. */
    virtual CameraDiagnostics::Result openStream() override;

    /** Implementation of QnAbstractMediaStreamProvider::closeStream. */
    virtual void closeStream() override;

    /** Implementation of QnAbstractMediaStreamProvider::isStreamOpened. */
    virtual bool isStreamOpened() const override;
    virtual CameraDiagnostics::Result lastOpenStreamResult() const override;

    /** Implementation of QnAbstractMediaStreamProvider::getLastResponseCode. */
    nx::network::rtsp::StatusCodeValue getLastResponseCode() const;

    /** Implementation of QnAbstractMediaStreamProvider::getAudioLayout. */
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() const override;

    /** Implementation of QnStoppable::pleaseStop. */
    virtual void pleaseStop() override;

    void setRequest(const QString& request);
    void setRole(Qn::ConnectionRole role);
    void setPrefferedAuthScheme(const nx::network::http::header::AuthScheme::Value scheme);

    static void setDefaultTransport(const QString& defaultTransportToUse);
    void setRtpTransport(RtspTransport value);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;
    void setUserAgent(const QString& value);

    nx::utils::Url getCurrentStreamUrl() const;

    void setPositionUsec(qint64 value);

    void setDateTimeFormat(const QnRtspClient::DateTimeFormat& format);

    QnRtspClient& rtspClient();
    void addRequestHeader(const QString& requestName, const nx::network::http::HttpHeader& header);
    void setRtpFrameTimeoutMs(int value);

    /**
     * This callback is called every time socket read timeout happens.
     * Connection may be closed on socket read error, media frame timeout
     * or by RTSP session timeout (usually 1 minute).
     * If callback returns valid packet it is returned from getNextDataTcp, otherwise timeout is skipped.
     */
    void setOnSocketReadTimeoutCallback(
        std::chrono::milliseconds timeout,
        OnSocketReadTimeoutCallback callback);

    RtspTransport getRtpTransport() const;

signals:
    void networkIssue(
        const QnResourcePtr&,
        qint64 timeStamp,
        nx::vms::api::EventReason reasonCode,
        const QString& reasonParamsEncoded);
private:

    struct TrackInfo
    {
        QnRtspIoDevice* ioDevice = nullptr; //< External reference; do not delete.
        std::shared_ptr<nx::streaming::rtp::StreamParser> parser;
        nx::streaming::Sdp::MediaType mediaType;
        std::optional<std::chrono::microseconds> onvifExtensionTimestamp;
        int rtcpChannelNumber = 0;
        int logicalChannelNum = 0;
    };

    nx::streaming::rtp::StreamParser* createParser(const QString& codecName);
    bool gotKeyData(const QnAbstractMediaDataPtr& mediaData);
    void clearKeyData(int channelNum);
    QnAbstractMediaDataPtr getNextDataUDP();
    QnAbstractMediaDataPtr getNextDataTCP();
    void processTcpRtcp(
        quint8* buffer, int bufferSize, int bufferCapacity);
    void buildClientRTCPReport(quint8 chNumber);
    QnAbstractMediaDataPtr getNextDataInternal();
    void processCameraTimeHelperEvent(nx::streaming::rtp::CameraTimeHelper::EventType event);

    void calcStreamUrl();

    void updateOnvifTime(
        int rtpBufferOffset,
        int rtpPacketSize,
        TrackInfo& track,
        int rtpChannel);

private slots:
    void at_packetLost(quint32 prev, quint32 next);
    void at_propertyChanged(const QnResourcePtr& res, const QString& key);
    void createTrackParsers();

private:
    QnRtspClient m_RtpSession;
    QVector<bool> m_gotKeyDataInfo;
    QVector<TrackInfo> m_tracks;
    std::map<int, int> m_trackIndices;

    QString m_request;

    std::vector<QnByteArray*> m_demuxedData;
    int m_numberOfVideoChannels;
    nx::streaming::rtp::CameraTimeHelper m_timeHelper;
    bool m_pleaseStop;
    QElapsedTimer m_rtcpReportTimer;
    bool m_gotSomeFrame;
    Qn::ConnectionRole m_role;

    QnCustomResourceVideoLayoutPtr m_customVideoLayout;
    mutable QnMutex m_layoutMutex;
    QnConstResourceAudioLayoutPtr m_audioLayout;
    bool m_gotData;
    QElapsedTimer m_dataTimer;
    bool m_rtpStarted;
    nx::network::http::header::AuthScheme::Value m_prefferedAuthScheme;
    nx::utils::Url m_currentStreamUrl;
    RtspTransport m_rtpTransport;

    int m_maxRtpRetryCount{0};
    int m_rtpFrameTimeoutMs{0};
    std::atomic<qint64> m_positionUsec{AV_NOPTS_VALUE};
    OnSocketReadTimeoutCallback m_onSocketReadTimeoutCallback;
    std::chrono::milliseconds m_callbackTimeout{0};
    CameraDiagnostics::Result m_openStreamResult;

    static nx::utils::Mutex s_defaultTransportMutex;
    static RtspTransport s_defaultTransportToUse;
};

#endif // defined(ENABLE_DATA_PROVIDERS)

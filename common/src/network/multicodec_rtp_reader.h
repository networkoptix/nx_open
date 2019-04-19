#pragma once
#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/QElapsedTimer>

#include <vector>
#include <atomic>
#include <set>

#include <core/dataprovider/abstract_media_stream_provider.h>
#include <core/resource/resource_consumer.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/media_data_packet.h>
#include <utils/camera/camera_diagnostics.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/safe_direct_connection.h>

#include <nx/streaming/rtsp_client.h>

#include <nx/vms/event/event_fwd.h>
#include <nx/streaming/rtp_stream_parser.h>

class QnRtpStreamParser;
class QnRtpAudioStreamParser;
class QnRtpVideoStreamParser;
class QnResourceAudioLayout;

class QnMulticodecRtpReader:
    public QObject,
    public QnResourceConsumer,
    public QnAbstractMediaStreamProvider,
    public QnStoppable,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    using OnSocketReadTimeoutCallback = nx::utils::MoveOnlyFunc<QnAbstractMediaDataPtr()>;

    QnMulticodecRtpReader(
        const QnResourcePtr& resource,
        std::unique_ptr<AbstractStreamSocket> tcpSock = std::unique_ptr<AbstractStreamSocket>());
    virtual ~QnMulticodecRtpReader();

    /** Implementation of QnAbstractMediaStreamProvider::getNextData. */
    virtual QnAbstractMediaDataPtr getNextData() override;

    /** Implementation of QnAbstractMediaStreamProvider::openStream. */
    virtual CameraDiagnostics::Result openStream() override;

    /** Implementation of QnAbstractMediaStreamProvider::closeStream. */
    virtual void closeStream() override;

    /** Implementation of QnAbstractMediaStreamProvider::isStreamOpened. */
    virtual bool isStreamOpened() const override;

    /** Implementation of QnAbstractMediaStreamProvider::getLastResponseCode. */
    virtual int getLastResponseCode() const override;

    /** Implementation of QnAbstractMediaStreamProvider::getAudioLayout. */
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() const override;

    /** Implementation of QnStoppable::pleaseStop. */
    virtual void pleaseStop() override;

    void setRequest(const QString& request);
    void setRole(Qn::ConnectionRole role);
    void setPrefferedAuthScheme(const nx_http::header::AuthScheme::Value scheme);

    static void setDefaultTransport(nx::vms::api::RtpTransportType defaultTransportToUse);
    void setRtpTransport(nx::vms::api::RtpTransportType value);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;
    void setUserAgent(const QString& value);

    QString getCurrentStreamUrl() const;

    void setPositionUsec(qint64 value);

    void setDateTimeFormat(const QnRtspClient::DateTimeFormat& format);

    QnRtspClient& rtspClient();

    /**
     * Trust to camera NPT clock if value is true. Remember difference between camera and local clock otherwise.
     * Default value is false.
     */
    void setTrustToCameraTime(bool value);

    void setTimePolicy(TimePolicy timePolicy);

    void addRequestHeader(const QString& requestName, const nx_http::HttpHeader& header);
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
signals:
    void networkIssue(
        const QnResourcePtr&,
        qint64 timeStamp,
        nx::vms::event::EventReason reasonCode,
        const QString& reasonParamsEncoded);

private:

    struct TrackInfo
    {
        TrackInfo(): ioDevice(0), parser(0) {}
        ~TrackInfo() { }
        QnRtspIoDevice* ioDevice; //< External reference; do not delete.
        std::shared_ptr<QnRtpStreamParser> parser;
        int rtcpChannelNumber = 0;
    };

    QnRtpStreamParser* createParser(const QString& codecName);
    bool gotKeyData(const QnAbstractMediaDataPtr& mediaData);
    void clearKeyData(int channelNum);
    QnAbstractMediaDataPtr getNextDataUDP();
    QnAbstractMediaDataPtr getNextDataTCP();
    void processTcpRtcp(
        quint8* buffer, int bufferSize, int bufferCapacity);
    void buildClientRTCPReport(quint8 chNumber);
    QnAbstractMediaDataPtr getNextDataInternal();
    nx::vms::api::RtpTransportType getRtpTransport() const;

    void calcStreamUrl();

    CameraDiagnostics::Result registerMulticastAddress(
        const QnRtspIoDevice::AddressInfo& addressInfo);
    void unregisterMulticastAddresses();

    boost::optional<std::chrono::microseconds> parseOnvifNtpExtensionTime(
        quint8* bufferStart,
        int length) const;

    bool isOnvifNtpExtensionId(uint16_t id) const;

    QnRtspStatistic rtspStatistics(
        int rtpBufferOffset,
        int rtpPacketSize,
        int track,
        int rtpChannel);

private slots:
    void at_packetLost(quint32 prev, quint32 next);
    void at_propertyChanged(const QnResourcePtr& res, const QString& key);
    void createTrackParsers();
private:
    QnRtspClient m_RtpSession;
    QVector<bool> m_gotKeyDataInfo;
    QVector<TrackInfo> m_tracks;

    QString m_request;

    std::vector<QnByteArray*> m_demuxedData;
    int m_numberOfVideoChannels;
    QnRtspTimeHelper m_timeHelper;
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
    nx_http::header::AuthScheme::Value m_prefferedAuthScheme;
    QString m_currentStreamUrl;
    nx::vms::api::RtpTransportType m_rtpTransport;

    int m_maxRtpRetryCount{0};
    int m_rtpFrameTimeoutMs{0};
    std::atomic<qint64> m_positionUsec{AV_NOPTS_VALUE};
    boost::optional<std::chrono::microseconds> m_lastOnvifNtpExtensionTime;
    OnSocketReadTimeoutCallback m_onSocketReadTimeoutCallback;
    std::chrono::milliseconds m_callbackTimeout{0};

    static QnMutex s_defaultTransportMutex;
    static nx::vms::api::RtpTransportType s_defaultTransportToUse;
    std::set<SocketAddress> m_registeredMulticastAddresses;
};

#endif // defined(ENABLE_DATA_PROVIDERS)

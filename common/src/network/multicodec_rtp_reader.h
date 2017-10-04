#pragma once

#ifdef ENABLE_DATA_PROVIDERS

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
#include <nx/streaming/rtp_stream_parser.h>


namespace RtpTransport
{
    typedef QString Value;

    //!Server selects best suitable transport

    static const QLatin1String _auto( "AUTO" );
    static const QLatin1String udp( "UDP" );
    static const QLatin1String tcp( "TCP" );

    Value fromString(const QString& str);
};

class QnRtpStreamParser;
class QnRtpAudioStreamParser;
class QnRtpVideoStreamParser;
class QnResourceAudioLayout;

class QnMulticodecRtpReader
:
    public QObject,
    public QnResourceConsumer,
    public QnAbstractMediaStreamProvider,
    public QnStoppable,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

private:
    enum {BLOCK_SIZE = 1460};

public:
    QnMulticodecRtpReader(
        const QnResourcePtr& res,
        std::unique_ptr<AbstractStreamSocket> tcpSock = std::unique_ptr<AbstractStreamSocket>() );
    virtual ~QnMulticodecRtpReader();

    //!Implementation of QnAbstractMediaStreamProvider::getNextData
    virtual QnAbstractMediaDataPtr getNextData() override;
    //!Implementation of QnAbstractMediaStreamProvider::openStream
    virtual CameraDiagnostics::Result openStream() override;
    //!Implementation of QnAbstractMediaStreamProvider::closeStream
    virtual void closeStream() override;
    //!Implementation of QnAbstractMediaStreamProvider::isStreamOpened
    virtual bool isStreamOpened() const override;
    //!Implementation of QnAbstractMediaStreamProvider::getLastResponseCode
    virtual int getLastResponseCode() const override;
    //!Implementation of QnAbstractMediaStreamProvider::getAudioLayout
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() const override;
    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setRequest(const QString& request);
    void setRole(Qn::ConnectionRole role);
    void setPrefferedAuthScheme(const nx_http::header::AuthScheme::Value scheme);

    static void setDefaultTransport( const RtpTransport::Value& defaultTransportToUse );
    void setRtpTransport(const RtpTransport::Value& value);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;
    void setUserAgent(const QString& value);

    QString getCurrentStreamUrl() const;

    void setPositionUsec(qint64 value);

    void setDateTimeFormat(const QnRtspClient::DateTimeFormat& format);

    /**
     * Trust to camera NPT clock if value is true. Remember difference between camera and local clock otherwise.
     * Default value is false.
     */
    void setTrustToCameraTime(bool value);
signals:
    void networkIssue(const QnResourcePtr&, qint64 timeStamp, nx::vms::event::EventReason reasonCode, const QString& reasonParamsEncoded);

private:

    struct TrackInfo
    {
        TrackInfo(): ioDevice(0), parser(0) {}
        ~TrackInfo() { delete parser; }
        QnRtspIoDevice* ioDevice; // external reference. do not delete
        QnRtpStreamParser* parser;
    };

    QnRtpStreamParser* createParser(const QString& codecName);
    bool gotKeyData(const QnAbstractMediaDataPtr& mediaData);
    void clearKeyData(int channelNum);
    QnAbstractMediaDataPtr getNextDataUDP();
    QnAbstractMediaDataPtr getNextDataTCP();
    void processTcpRtcp(QnRtspIoDevice* ioDevice, quint8* buffer, int bufferSize, int bufferCapacity);
    void buildClientRTCPReport(quint8 chNumber);
    QnAbstractMediaDataPtr getNextDataInternal();
    QnRtspClient::TransportType getRtpTransport() const;

    void calcStreamUrl();
private slots:
    void at_packetLost(quint32 prev, quint32 next);
    void at_propertyChanged(const QnResourcePtr & res, const QString & key);
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
    QString m_rtpTransport;

    int m_maxRtpRetryCount{0};
    int m_rtpFrameTimeoutMs{0};
    std::atomic<qint64> m_positionUsec{AV_NOPTS_VALUE};
};

#endif // ENABLE_DATA_PROVIDERS

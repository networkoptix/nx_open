#ifndef __MULTI_CODEC_RTP_READER__
#define __MULTI_CODEC_RTP_READER__

#ifdef ENABLE_DATA_PROVIDERS

#include <vector>

#include "core/dataprovider/abstract_media_stream_provider.h"
#include "core/resource/resource_consumer.h"
#include "core/resource/resource_media_layout.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/camera/camera_diagnostics.h"
#include "utils/common/stoppable.h"
#include "utils/network/rtpsession.h"

#include <business/business_fwd.h>
#include "rtp_stream_parser.h"


namespace RtpTransport
{
    typedef QString Value;

    //!Server selects best suitable transport
    static QLatin1String _auto( "AUTO" );
    static QLatin1String udp( "UDP" );
    static QLatin1String tcp( "TCP" );
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
    public QnStoppable
{
    Q_OBJECT

private:
    enum {BLOCK_SIZE = 1460};

public:
    QnMulticodecRtpReader( const QnResourcePtr& res );
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

    static void setDefaultTransport( const RtpTransport::Value& defaultTransportToUse );

    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;
signals:
    void networkIssue(const QnResourcePtr&, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonParamsEncoded);

private:

    struct TrackInfo
    {
        TrackInfo(): ioDevice(0), parser(0), gotKeyData(false) {}
        ~TrackInfo() { delete parser; }
        RTPIODevice* ioDevice; // external reference. do not delete
        QnRtpStreamParser* parser;
        bool gotKeyData;
    };

    QnRtpStreamParser* createParser(const QString& codecName);
    bool gotKeyData(TrackInfo& track, const QnAbstractMediaDataPtr& mediaData);
    QnAbstractMediaDataPtr getNextDataUDP();
    QnAbstractMediaDataPtr getNextDataTCP();
    void processTcpRtcp(RTPIODevice* ioDevice, quint8* buffer, int bufferSize, int bufferCapacity);
    void buildClientRTCPReport(quint8 chNumber);
    QnAbstractMediaDataPtr getNextDataInternal();
private slots:
    void at_packetLost(quint32 prev, quint32 next);
    void at_propertyChanged(const QnResourcePtr & res, const QString & key);
private:
    RTPSession m_RtpSession;

    /*
    RTPIODevice* m_videoIOs[CL_MAX_CHANNELS];
    RTPIODevice* m_audioIO;
    QnRtpVideoStreamParser* m_videoParsers[CL_MAX_CHANNELS];
    QnRtpAudioStreamParser* m_audioParser;
    QnAbstractMediaDataPtr m_lastVideoData;
    QList<QnAbstractMediaDataPtr> m_lastAudioData;
    */
    QVector<TrackInfo> tracks;

    QString m_request;

    std::vector<QnByteArray*> m_demuxedData;
    int m_numberOfVideoChannels;
    QnRtspTimeHelper m_timeHelper;
    bool m_pleaseStop;
    QElapsedTimer m_rtcpReportTimer;
    bool m_gotSomeFrame;
    Qn::ConnectionRole m_role;

    QnCustomResourceVideoLayoutPtr m_customVideoLayout;
    mutable QMutex m_layoutMutex;
    QnConstResourceAudioLayoutPtr m_audioLayout;
    bool m_gotData;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //__MULTI_CODEC_RTP_READER__

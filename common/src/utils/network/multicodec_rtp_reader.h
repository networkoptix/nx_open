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
    void setRole(QnResource::ConnectionRole role);

    static void setDefaultTransport( const RtpTransport::Value& defaultTransportToUse );

signals:
    void networkIssue(const QnResourcePtr&, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonParamsEncoded);

private:
    QnRtpStreamParser* createParser(const QString& codecName);
    void initIO(RTPIODevice** ioDevice, QnRtpStreamParser* parser, RTPSession::TrackType mediaType);
    void setNeedKeyData();
    void checkIfNeedKeyData();
    QnAbstractMediaDataPtr getNextDataUDP();
    QnAbstractMediaDataPtr getNextDataTCP();
    void processTcpRtcp(RTPIODevice* ioDevice, quint8* buffer, int bufferSize, int bufferCapacity);
    void buildClientRTCPReport(quint8 chNumber);

private slots:
    void at_packetLost(quint32 prev, quint32 next);
    void at_propertyChanged(const QnResourcePtr & res, const QString & key);
private:
    RTPSession m_RtpSession;
    RTPIODevice* m_videoIO;
    RTPIODevice* m_audioIO;
    QnRtpVideoStreamParser* m_videoParser;
    QnRtpAudioStreamParser* m_audioParser;

    QString m_request;

    std::vector<QnByteArray*> m_demuxedData;
    QnAbstractMediaDataPtr m_lastVideoData;
    QList<QnAbstractMediaDataPtr> m_lastAudioData;
    int m_numberOfVideoChannels;
    QnRtspTimeHelper m_timeHelper;
    QVector<int> m_gotKeyData;
    bool m_pleaseStop;
    QElapsedTimer m_rtcpReportTimer;
    bool m_gotSomeFrame;
    QnResource::ConnectionRole m_role;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //__MULTI_CODEC_RTP_READER__

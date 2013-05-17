#ifndef __MULTI_CODEC_RTP_READER__
#define __MULTI_CODEC_RTP_READER__

#include "core/resource/resource_consumer.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/network/rtpsession.h"

#include <business/business_logic_common.h>


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

class QnMulticodecRtpReader : public QObject, public QnResourceConsumer
{
    Q_OBJECT
private:
    enum {BLOCK_SIZE = 1460};
public:
    QnMulticodecRtpReader( QnResourcePtr res );
    virtual ~QnMulticodecRtpReader();

    QnAbstractMediaDataPtr getNextData();
    void setRequest(const QString& request);
    /*!
        \return true, if successfully opened stream or stream is already opened. false, if failed. For more detail, call \a getLastResponseCode()
    */
    bool openStream();
    void closeStream() ;
    bool isStreamOpened() const;
    const QnResourceAudioLayout* getAudioLayout() const;
    int getLastResponseCode() const;
    void pleaseStop();

    static void setDefaultTransport( const RtpTransport::Value& defaultTransportToUse );

signals:
    void networkIssue(const QnResourcePtr&, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);
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
private:
    
    RTPSession m_RtpSession;
    RTPIODevice* m_videoIO;
    RTPIODevice* m_audioIO;
    QnRtpVideoStreamParser* m_videoParser;
    QnRtpAudioStreamParser* m_audioParser;

    QString m_request;

    QVector<QnByteArray*> m_demuxedData;
    QnAbstractMediaDataPtr m_lastVideoData;
    QList<QnAbstractMediaDataPtr> m_lastAudioData;
    int m_numberOfVideoChannels;
    QnRtspTimeHelper m_timeHelper;
    QVector<int> m_gotKeyData;
    bool m_pleaseStop;
    QTime m_rtcpReportTimer;
    bool m_gotSomeFrame;
};

#endif //__MULTI_CODEC_RTP_READER__

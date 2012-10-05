#ifndef __MULTI_CODEC_RTP_READER__
#define __MULTI_CODEC_RTP_READER__

#include "core/resource/resource_consumer.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/network/rtpsession.h"



class QnRtpStreamParser;
class QnRtpAudioStreamParser;
class QnRtpVideoStreamParser;
class QnResourceAudioLayout;


class QnMulticodecRtpReader : public QnResourceConsumer
{
private:
    enum {BLOCK_SIZE = 1460};
public:
    QnMulticodecRtpReader(QnResourcePtr res);
    virtual ~QnMulticodecRtpReader();


    QnAbstractMediaDataPtr getNextData();
    void setRequest(const QString& request);
    void openStream();
    void closeStream() ;
    bool isStreamOpened() const;
    const QnResourceAudioLayout* getAudioLayout() const;
    int getLastResponseCode() const;
    void pleaseStop();
private:
    QnRtpStreamParser* createParser(const QString& codecName);
    void initIO(RTPIODevice** ioDevice, QnRtpStreamParser* parser, RTPSession::TrackType mediaType);
    void setNeedKeyData();
    void checkIfNeedKeyData();
    QnAbstractMediaDataPtr getNextDataUDP();
    QnAbstractMediaDataPtr getNextDataTCP();
    void processTcpRtcp(RTPIODevice* ioDevice, quint8* buffer, int bufferSize, int bufferCapacity);
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
};

#endif //__MULTI_CODEC_RTP_READER__

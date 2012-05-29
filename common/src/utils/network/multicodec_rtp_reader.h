#ifndef __MULTI_CODEC_RTP_READER__
#define __MULTI_CODEC_RTP_READER__

#include "core/resource/resource_consumer.h"
#include "core/datapacket/mediadatapacket.h"
#include "utils/network/rtpsession.h"



class QnRtpStreamParser;
class QnRtpAudioStreamParser;
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
    void setDefaultAudioCodecName(const QString& value);
private:
    QnRtpStreamParser* createParser(const QString& codecName);
    void initIO(RTPIODevice** ioDevice, QnRtpStreamParser* parser, const QString& mediaType);
private:
    
    RTPSession m_RtpSession;
    RTPIODevice* m_videoIO;
    RTPIODevice* m_audioIO;
    QnRtpStreamParser* m_videoParser;
    QnRtpAudioStreamParser* m_audioParser;

    QString m_request;

    QList<QnAbstractMediaDataPtr> m_lastVideoData;
    QList<QnAbstractMediaDataPtr> m_lastAudioData;
    int m_numberOfVideoChannels;
};

#endif //__MULTI_CODEC_RTP_READER__

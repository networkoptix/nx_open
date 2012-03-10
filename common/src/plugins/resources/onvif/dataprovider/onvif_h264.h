#ifndef h264_reader_deligate_h_1158
#define h264_reader_deligate_h_1158

#include "core/resource/resource_consumer.h"
#include "core/datapacket/mediadatapacket.h"
#include "utils/network/rtpsession.h"



class CLRtpStreamParser;

class RTPH264StreamreaderDelegate : public QnResourceConsumer
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	RTPH264StreamreaderDelegate(QnResourcePtr res, QString request);
	virtual ~RTPH264StreamreaderDelegate();

protected:
    QnAbstractMediaDataPtr getNextData();
    void openStream();
    void closeStream() ;
    bool isStreamOpened() const;

private:
    
    RTPSession m_RtpSession;
    RTPIODevice* m_rtpIo;
    CLRtpStreamParser* m_streamParser;

    QString m_request;

};

#endif //h264_reader_deligate_h_1158

#ifndef __AAC_RTP_PARSER_H
#define __AAC_RTP_PARSER_H

#include <QByteArray>
#include <QMap>

#include "rtp_stream_parser.h"
#include "../media/nalUnits.h"


class QnAacRtpParser: public QnRtpStreamParser
{
public:
    QnAacRtpParser();
    virtual ~QnAacRtpParser();
    virtual void setSDPInfo(QList<QByteArray> sdpInfo) override;

    virtual bool processData(quint8* rtpBuffer, int readed, QnAbstractMediaDataPtr& result) override;
private:
};

#endif // __AAC_RTP_PARSER_H

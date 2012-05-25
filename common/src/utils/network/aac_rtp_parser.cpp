#include "aac_rtp_parser.h"
#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "utils/common/synctime.h"

QnAacRtpParser::QnAacRtpParser():
        QnRtpStreamParser()
{
}

QnAacRtpParser::~QnAacRtpParser()
{
}

void QnAacRtpParser::setSDPInfo(QList<QByteArray> lines)
{
    for (int i = 0; i < lines.size(); ++ i)
    {
        if (lines[i].startsWith("a=rtpmap"))
        {

        }
        else if (lines[i].startsWith("a=fmtp"))
        {

        }
    }
}

bool QnAacRtpParser::processData(quint8* rtpBuffer, int readed, QnAbstractMediaDataPtr& result)
{
    result.clear();

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    int bytesLeft = readed - RtpHeader::RTP_HEADER_SIZE;

    bool isLastPacket = rtpHeader->marker;
    if (bytesLeft < 2)
        return false;
    int auHeaderLen = (curPtr[0] << 8) + curPtr[1];
    curPtr += 2;
    bytesLeft -=2;

    return true;
}

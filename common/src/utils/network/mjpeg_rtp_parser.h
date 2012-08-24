#ifndef __MJPEG_RTP_PARSER_H
#define __MJPEG_RTP_PARSER_H

#include <QByteArray>
#include <QMap>

#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "plugins/resources/arecontvision/tools/AVJpegHeader.h"


class QnMjpegRtpParser: public QnRtpVideoStreamParser
{
public:
    QnMjpegRtpParser();
    virtual ~QnMjpegRtpParser();
    virtual void setSDPInfo(QList<QByteArray> lines) override;

    virtual bool processData(quint8* rtpBuffer, int readed, const RtspStatistic& statistics, QnAbstractMediaDataPtr& result) override;
private:
    int makeHeaders(quint8 *p, int type, int w, int h, u_char *lqt, u_char *cqt, u_short dri);
    void updateHeaderTables(quint8* lummaTable, quint8* chromaTable);
private:
    QnCompressedVideoDataPtr m_videoData;
    int m_frequency;
    QnMediaContextPtr m_context;
    AVJpeg::Header m_jpegHeader;
    quint8 m_lummaTable[64*2];
    quint8 m_chromaTable[64*2];
    int m_sdpWidth;
    int m_sdpHeight;

    quint8* m_lumaQTPtr;
    quint8* m_chromaQTPtr;
    int m_lastJpegQ;

    int m_hdrQ;
    int m_hdrDri;
    int m_hdrWidth;
    int m_hdrHeight;
    quint8 m_hdrBuffer[1024];
    int m_headerLen;
    quint8* m_lummaTablePos;
    quint8* m_chromaTablePos;

    QnByteArray m_frameData;
};

#endif

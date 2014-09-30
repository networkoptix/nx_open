#ifndef __MJPEG_RTP_PARSER_H
#define __MJPEG_RTP_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QMap>

#include "core/datapacket/video_data_packet.h"
#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "plugins/resource/arecontvision/tools/AVJpegHeader.h"


class QnMjpegRtpParser: public QnRtpVideoStreamParser
{
public:
    QnMjpegRtpParser();
    virtual ~QnMjpegRtpParser();
    virtual void setSDPInfo(QList<QByteArray> lines) override;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics) override;
    virtual QnAbstractMediaDataPtr nextData() override;

private:
    int makeHeaders(quint8 *p, int type, int w, int h, const quint8 *lqt, const quint8 *cqt, u_short dri);
    void updateHeaderTables(const quint8* lummaTable, const quint8* chromaTable);
private:
    int m_frequency;
    QnMediaContextPtr m_context;
    //AVJpeg::Header m_jpegHeader;
    quint8 m_lummaTable[64*2];
    quint8 m_chromaTable[64*2];
    int m_sdpWidth;
    int m_sdpHeight;

    int m_lastJpegQ;

    int m_hdrQ;
    int m_hdrDri;
    int m_hdrWidth;
    int m_hdrHeight;
    quint8 m_hdrBuffer[1024];
    int m_headerLen;
    quint8* m_lummaTablePos;
    quint8* m_chromaTablePos;

    //QnByteArray m_frameData;
    int m_frameSize;
};

#endif

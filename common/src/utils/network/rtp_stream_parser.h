#ifndef __RTP_STREAM_PARSER_H
#define __RTP_STREAM_PARSER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QIODevice>
#include "core/datapacket/media_data_packet.h"
#include "core/resource/resource_media_layout.h"
#include "rtpsession.h"

class RTPIODevice;
class RtspStatistic;

#pragma pack(push, 1)
struct RtpHeader
{
    static const int RTP_HEADER_SIZE = 12;
    static const int RTP_VERSION = 2;
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    unsigned short   version:2;     /* packet type                */
    unsigned short   padding:1;     /* padding flag               */
    unsigned short   extension:1;   /* header extension flag      */
    unsigned short   CSRCCount:4;   /* CSRC count                 */
    unsigned short   marker:1;      /* marker bit                 */
    unsigned short   payloadType:7; /* payload type               */
#else
    unsigned short   CSRCCount:4;   /* CSRC count                 */
    unsigned short   extension:1;   /* header extension flag      */
    unsigned short   padding:1;     /* padding flag               */
    unsigned short   version:2;     /* packet type                */
    unsigned short   payloadType:7; /* payload type               */
    unsigned short   marker:1;      /* marker bit                 */
#endif
    quint16 sequence;               // sequence number
    quint32 timestamp;              // timestamp
    quint32 ssrc;                   // synchronization source
    //quint32 csrc;                 // synchronization source
    //quint32 csrc[1];              // optional CSRC list
};
#pragma pack(pop)

class QnRtpStreamParser: public QObject
{
    Q_OBJECT
public:
    QnRtpStreamParser();
    virtual void setSDPInfo(QList<QByteArray> sdpInfo) = 0;
    
    virtual ~QnRtpStreamParser();

    // used for sync audio/video streams
    void setTimeHelper(QnRtspTimeHelper* timeHelper);
signals:
    void packetLostDetected(quint32 prev, quint32 next);
protected:
    QnRtspTimeHelper* m_timeHelper;
};

class QnRtspAudioLayout: public QnResourceAudioLayout
{
public:
    QnRtspAudioLayout(): QnResourceAudioLayout() {}
    virtual int channelCount() const override { return 1; }
    virtual AudioTrack getAudioTrackInfo(int /*index*/) const override { return m_audioTrack; }
    void setAudioTrackInfo(const AudioTrack& info) { m_audioTrack = info; }
private:
    AudioTrack m_audioTrack;
};

class QnRtpVideoStreamParser: public QnRtpStreamParser
{
public:
    QnRtpVideoStreamParser();

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, QnAbstractMediaDataPtr& result) = 0;
protected:
    struct Chunk
    {
        Chunk(): bufferOffset(0), len(0), nalStart(false) {}
        Chunk(int _bufferOffset, quint16 _len, quint8 _nalStart = false): bufferOffset(_bufferOffset), len(_len), nalStart(_nalStart) {}

        int bufferOffset;
        quint16 len;
        bool nalStart;
    };
    std::vector<Chunk> m_chunks;
};

class QnRtpAudioStreamParser: public QnRtpStreamParser
{
public:
    virtual QnResourceAudioLayoutPtr getAudioLayout() = 0;

    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, QList<QnAbstractMediaDataPtr>& result) = 0;
protected:
    void processIntParam(const QByteArray& checkName, int& setValue, const QByteArray& param);
    void processHexParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param);
    void processStringParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param);
};

#endif // ENABLE_DATA_PROVIDERS

#endif

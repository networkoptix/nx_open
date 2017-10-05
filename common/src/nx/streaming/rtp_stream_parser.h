#ifndef __RTP_STREAM_PARSER_H
#define __RTP_STREAM_PARSER_H

#include <QtCore/QIODevice>
#include <deque>

#include <nx/streaming/media_data_packet.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/rtsp_client.h>

class QnRtspIoDevice;
class QnRtspStatistic;

#pragma pack(push, 1)
struct RtpHeader
{
    static const int RTP_HEADER_SIZE = 12;
    static const int RTP_VERSION = 2;
    static const int CSRC_SIZE = 4;
    static const int EXTENSION_HEADER_SIZE = 4;
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

#pragma pack(push, 1)
struct RtpHeaderExtension
{
    uint16_t definedByProfile;
    uint16_t length;
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

    virtual QnAbstractMediaDataPtr nextData() = 0;
    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, const QnRtspStatistic& statistics, bool& gotData) = 0;

    int logicalChannelNum() const { return m_logicalChannelNum; }
    void setLogicalChannelNum(int value) { m_logicalChannelNum = value; }
    
    void setCodecName(const QString& value) { m_codecName = value; }
    QString codecName() const { return m_codecName;  }
signals:
    void packetLostDetected(quint32 prev, quint32 next);
protected:
    QnRtspTimeHelper* m_timeHelper;
    int m_logicalChannelNum;
    QString m_codecName;
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

    virtual QnAbstractMediaDataPtr nextData() override;
protected:
    QnAbstractMediaDataPtr m_mediaData;
protected:
    struct Chunk
    {
        Chunk(): bufferStart(nullptr), bufferOffset(0), len(0), nalStart(false) {}
        Chunk(int _bufferOffset, quint16 _len, quint8 _nalStart = false): 
            bufferStart(nullptr),
            bufferOffset(_bufferOffset), 
            len(_len), 
            nalStart(_nalStart) {}

        quint8* bufferStart;
        int bufferOffset;
        quint16 len;
        bool nalStart;
    };
    std::vector<Chunk> m_chunks;
};

class QnRtpAudioStreamParser: public QnRtpStreamParser
{
public:
    QnRtpAudioStreamParser() {}

    virtual QnConstResourceAudioLayoutPtr getAudioLayout() = 0;
    virtual QnAbstractMediaDataPtr nextData() override;
protected:
    std::deque<QnAbstractMediaDataPtr> m_audioData;
protected:
    void processIntParam(const QByteArray& checkName, int& setValue, const QByteArray& param);
    void processHexParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param);
    void processStringParam(const QByteArray& checkName, QByteArray& setValue, const QByteArray& param);
};

#endif

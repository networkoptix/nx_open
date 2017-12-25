#pragma once

#include <deque>

#include <nx/streaming/media_data_packet.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/rtsp_client.h>

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
    uint16_t definedByProfile; //< Name from RFC. Actually it is extension type id.
    uint16_t length; //< Length in 4-byte words.
};
#pragma pack(pop)

class QnRtspAudioLayout: public QnResourceAudioLayout
{

public:
    virtual int channelCount() const override;
    virtual AudioTrack getAudioTrackInfo(int index) const override;
    void setAudioTrackInfo(const AudioTrack& info);

private:
    AudioTrack m_audioTrack;
};

class QnRtpStreamParser: public QObject
{
    Q_OBJECT

public:
    virtual ~QnRtpStreamParser() {};

    virtual void setSdpInfo(QList<QByteArray> sdpInfo) = 0;
    virtual QnAbstractMediaDataPtr nextData() = 0;
    virtual bool processData(
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        const QnRtspStatistic& statistics,
        bool& gotData) = 0;

    // used for sync audio/video streams
    void setTimeHelper(QnRtspTimeHelper* timeHelper);

    int logicalChannelNum() const;
    void setLogicalChannelNum(int value);

signals:
    void packetLostDetected(quint32 prev, quint32 next);

protected:
    QnRtspTimeHelper* m_timeHelper = nullptr;
    int m_logicalChannelNum = 0;
};

class QnRtpVideoStreamParser: public QnRtpStreamParser
{

public:
    QnRtpVideoStreamParser();
    virtual QnAbstractMediaDataPtr nextData() override;

protected:
    struct Chunk
    {
        Chunk():
            bufferStart(nullptr),
            bufferOffset(0),
            len(0),
            nalStart(false)
        {
        }

        Chunk(int bufferOffset, quint16 len, quint8 nalStart = false):
            bufferStart(nullptr),
            bufferOffset(bufferOffset),
            len(len),
            nalStart(nalStart)
        {
        }

        quint8* bufferStart = nullptr;
        int bufferOffset = 0;
        quint16 len = 0;
        bool nalStart = false;
    };

protected:
    void backupCurrentData(const quint8* currentBufferBase);

protected:
    QnAbstractMediaDataPtr m_mediaData;
    std::vector<Chunk> m_chunks;
    std::vector<quint8> m_nextFrameChunksBuffer;
};

class QnRtpAudioStreamParser: public QnRtpStreamParser
{

public:
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() = 0;
    virtual QnAbstractMediaDataPtr nextData() override;

protected:
    void processIntParam(const QByteArray& checkName, int& setValue, const QByteArray& param);

    void processHexParam(
        const QByteArray& checkName,
        QByteArray& setValue,
        const QByteArray& param);

    void processStringParam(
        const QByteArray& checkName,
        QByteArray& setValue,
        const QByteArray& param);
protected:
    std::deque<QnAbstractMediaDataPtr> m_audioData;
};

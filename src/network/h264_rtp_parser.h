#include <QByteArray>
#include <QMap>

#include "rtp_stream_parser.h"
#include "base/nalUnits.h"

class CLH264RtpParser: CLRtpStreamParser
{
public:
    CLH264RtpParser(QIODevice* input);
    virtual CLAbstractMediaData* getNextData();
    virtual ~CLH264RtpParser();
    virtual void setSDPInfo(const QByteArray& data);
private:
    //QByteArray m_sliceBuffer;
    int m_readBufferActualSize;
    QMap <int, QByteArray> m_allNonSliceNal;
    QByteArray m_sdpSpsPps;
    CLCompressedVideoData* m_videoData;
    SPSUnit m_sps;
    int m_frequency;
    int m_rtpChannel;
    bool m_builtinSpsFound;
    bool m_builtinPpsFound;
    quint64 m_timeCycles;
    quint64 m_timeCycleValue;
    quint64 m_lastTimeStamp;
private:
    void serializeSpsPps(CLByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
};

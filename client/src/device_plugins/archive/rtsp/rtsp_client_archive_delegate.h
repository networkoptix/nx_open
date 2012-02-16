#ifndef __RTSP_CLIENT_ARCHIVE_DELEGATE_H
#define __RTSP_CLIENT_ARCHIVE_DELEGATE_H


#include "utils/network/rtpsession.h"
#include "core/resource/resource_media_layout.h"
#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "utils/common/util.h"



struct AVFormatContext;
class CLCustomDeviceVideoLayout;

class QnRtspClientArchiveDelegate: public QnAbstractArchiveDelegate, public QnAbstractFilterPlaybackDelegate
{
public:
    QnRtspClientArchiveDelegate();
    virtual ~QnRtspClientArchiveDelegate();

    virtual bool open(QnResourcePtr resource);
    virtual void close();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnVideoResourceLayout* getVideoLayout();
    virtual QnResourceAudioLayout* getAudioLayout();

    virtual void onReverseMode(qint64 displayTime, bool value);

    virtual bool isRealTimeSource() const;
    virtual void beforeClose();
    virtual void setSingleshotMode(bool value);
    
    // filter input data by motion region
    virtual void setMotionRegion(const QRegion& region);

    // Send motion data to client
    virtual void setSendMotion(bool value);

    virtual bool setQuality(MediaQuality quality, bool fastSwitch);

    virtual void beforeSeek(qint64 time);
    virtual void beforeChangeReverseMode(bool reverseMode);
protected:
private:
    QnAbstractDataPacketPtr processFFmpegRtpPayload(const quint8* data, int dataSize);
    void deleteContexts();
    void reopen();
private:
    RTPSession m_rtspSession;
    RTPIODevice* m_rtpData;
    quint8* m_rtpDataBuffer;
    bool m_tcpMode;
    QMap<int, QnMediaContextPtr> m_contextMap;
    //QMap<quint32, quint32> m_timeStampCycles;
    QMap<quint32, quint16> m_prevTimestamp;
    QMap<int, QnAbstractDataPacketPtr> m_nextDataPacket;
    qint64 m_position;
    QnDefaultDeviceVideoLayout m_defaultVideoLayout;
    bool m_opened;
    qint64 m_lastRtspTime;
    QnResourcePtr m_resource;
    //bool m_waitBOF;
    int m_lastPacketFlags;
    bool m_closing;
    bool m_singleShotMode;
    quint8 m_sendedCSec;
    qint64 m_lastSeekTime;
    qint64 m_lastReceivedTime;
    bool m_blockReopening;
    MediaQuality m_quality;
    bool m_qualityFastSwitch;
};

#endif

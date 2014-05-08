#ifndef __RTSP_CLIENT_ARCHIVE_DELEGATE_H
#define __RTSP_CLIENT_ARCHIVE_DELEGATE_H


#include "utils/network/rtpsession.h"
#include "core/resource/resource_media_layout.h"
#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "utils/common/util.h"
#include "recording/time_period.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "utils/network/ffmpeg_rtp_parser.h"

struct AVFormatContext;
class QnCustomResourceVideoLayout;
class QnArchiveStreamReader;
class QnCustomResourceVideoLayout;

class QnRtspClientArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT
public:
    QnRtspClientArchiveDelegate(QnArchiveStreamReader* reader);
    virtual ~QnRtspClientArchiveDelegate();

    void setResource(QnResourcePtr resource);
    void setServer(QnResourcePtr mServer);
    virtual bool open(QnResourcePtr resource);
    virtual void close();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    qint64 seek(qint64 startTime, qint64 endTime);
    virtual QnResourceVideoLayoutPtr getVideoLayout();
    virtual QnResourceAudioLayoutPtr getAudioLayout();

    virtual void onReverseMode(qint64 displayTime, bool value);

    virtual bool isRealTimeSource() const;
    virtual void beforeClose();
    virtual void setSingleshotMode(bool value);
    
    // filter input data by motion region
    virtual void setMotionRegion(const QRegion& region);

    // Send motion data to client
    virtual void setSendMotion(bool value) override;

    virtual bool setQuality(MediaQuality quality, bool fastSwitch);

    virtual void beforeSeek(qint64 time);
    virtual void beforeChangeReverseMode(bool reverseMode);

    void setAdditionalAttribute(const QByteArray& name, const QByteArray& value);
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;

    void setMultiserverAllowed(bool value);

    void setPlayNowModeAllowed(bool value);
signals:
    void dataDropped(QnArchiveStreamReader* reader);
private:
    QnAbstractDataPacketPtr processFFmpegRtpPayload(quint8* data, int dataSize, int channelNum);
    void processMetadata(const quint8* data, int dataSize);
    bool openInternal();
    void reopen();

    // determine camera's video server on specified time
    QnResourcePtr getServerOnTime(qint64 time);
    QnResourcePtr getNextMediaServerFromTime(QnResourcePtr resource, qint64 time);
    QnAbstractMediaDataPtr getNextDataInternal();
    QString getUrl(QnResourcePtr server, QnResourcePtr camera);
    qint64 checkMinTimeFromOtherServer(QnResourcePtr resource);
    void updateRtpParam(QnResourcePtr resource);
    void parseAudioSDP(const QList<QByteArray>& audioSDP);
private:
    QMutex m_mutex;
    RTPSession m_rtspSession;
    RTPIODevice* m_rtpData;
    quint8* m_rtpDataBuffer;
    bool m_tcpMode;
    QMap<quint32, quint16> m_prevTimestamp;
    qint64 m_position;
    QSharedPointer<QnDefaultResourceVideoLayout> m_defaultVideoLayout;
    bool m_opened;
    QnResourcePtr m_resource;
    QnResourcePtr m_server;
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
    qint64 m_lastMinTimeTime;

    QnTimePeriod m_serverTimePeriod;
    qint64 m_globalMinArchiveTime; // min archive time between all servers

    qint64 m_forcedEndTime;
    bool m_isMultiserverAllowed;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    bool m_playNowModeAllowed; // fast open mode without DESCRIBE
    QnArchiveStreamReader* m_reader;
    int m_frameCnt;
    QnCustomResourceVideoLayoutPtr m_customVideoLayout;
    
    QMap<int, QnFfmpegRtpParserPtr> m_parsers;
};

typedef QSharedPointer<QnRtspClientArchiveDelegate> QnRtspClientArchiveDelegatePtr;

#endif

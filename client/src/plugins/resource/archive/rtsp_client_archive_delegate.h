#ifndef __RTSP_CLIENT_ARCHIVE_DELEGATE_H
#define __RTSP_CLIENT_ARCHIVE_DELEGATE_H


#include <utils/network/rtpsession.h>
#include <utils/network/ffmpeg_rtp_parser.h>

#include <core/resource/resource_media_layout.h>

#include <recording/time_period.h>

#include <plugins/resource/archive/abstract_archive_delegate.h>

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

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    void setServer(const QnMediaServerResourcePtr &server);

    virtual bool open(const QnResourcePtr &resource) override;
    virtual void close() override;
    virtual qint64 startTime() override;
    virtual qint64 endTime() override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    qint64 seek(qint64 startTime, qint64 endTime);
    virtual QnResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnResourceAudioLayoutPtr getAudioLayout() override;

    virtual void onReverseMode(qint64 displayTime, bool value) override;

    virtual bool isRealTimeSource() const override;
    virtual void beforeClose() override;
    virtual void setSingleshotMode(bool value) override;
    
    // filter input data by motion region
    virtual void setMotionRegion(const QRegion& region);

    // Send motion data to client
    virtual void setSendMotion(bool value) override;

    virtual bool setQuality(MediaQuality quality, bool fastSwitch) override;

    virtual void beforeSeek(qint64 time) override;
    virtual void beforeChangeReverseMode(bool reverseMode) override;

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
    QnMediaServerResourcePtr getServerOnTime(qint64 time);
    QnMediaServerResourcePtr getNextMediaServerFromTime(const QnVirtualCameraResourcePtr &camera, qint64 time);
    QnAbstractMediaDataPtr getNextDataInternal();
    QString getUrl(const QnVirtualCameraResourcePtr &camera, const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr()) const;
    qint64 checkMinTimeFromOtherServer(const QnVirtualCameraResourcePtr &camera) const;
    void setupRtspSession(const QnVirtualCameraResourcePtr &camera, const QnMediaServerResourcePtr &server, RTPSession* session, bool usePredefinedTracks) const;
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
    QnVirtualCameraResourcePtr m_camera;
    QnMediaServerResourcePtr m_server;
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
    
    struct {
        QString username;
        QString password;
        QUuid videowall;
    } m_auth;
};

typedef QSharedPointer<QnRtspClientArchiveDelegate> QnRtspClientArchiveDelegatePtr;

#endif

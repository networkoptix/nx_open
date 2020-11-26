#ifndef __RTSP_CLIENT_ARCHIVE_DELEGATE_H
#define __RTSP_CLIENT_ARCHIVE_DELEGATE_H

#include <atomic>

#include <QtCore/QElapsedTimer>

#include <nx/utils/uuid.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/streaming/rtp/parsers/nx_rtp_parser.h>
#include <nx/utils/thread/mutex.h>


#include "recording/time_period.h"

struct AVFormatContext;
class QnCustomResourceVideoLayout;
class QnArchiveStreamReader;
class QnCustomResourceVideoLayout;
class QnRtspClient;
class QnRtspIoDevice;
class QnNxRtpParser;

class QnRtspClientArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT
public:
    QnRtspClientArchiveDelegate(QnArchiveStreamReader* reader, const QString& rtpLogTag = QString());
    virtual ~QnRtspClientArchiveDelegate();

    void setCamera(const QnSecurityCamResourcePtr &camera);
    void setFixedServer(const QnMediaServerResourcePtr &server);

    virtual bool open(
        const QnResourcePtr &resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    qint64 seek(qint64 startTime, qint64 endTime);
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    virtual void setSpeed(qint64 displayTime, double value) override;

    virtual bool isRealTimeSource() const override;
    virtual void beforeClose() override;
    virtual void setSingleshotMode(bool value) override;

    // filter input data by motion region
    virtual void setMotionRegion(const QRegion& region);

    // Send motion data to client
    virtual void setStreamDataFilter(nx::vms::api::StreamDataFilters filter) override;
    virtual nx::vms::api::StreamDataFilters streamDataFilter() const override;

    virtual bool setQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution) override;

    virtual void beforeSeek(qint64 time) override;
    virtual void beforeChangeSpeed(double speed) override;

    void setAdditionalAttribute(const QByteArray& name, const QByteArray& value);
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;
    virtual bool hasVideo() const override;

    void setMultiserverAllowed(bool value);

    void setPlayNowModeAllowed(bool value);

    /**
     * Get currently used network protocol. See the predefined values in the nx::network::Protocol
     * namespace (nx/network/abstract_socket.h).
     * @return 0 if connection is not opened yet, IANA protocol number otherwise.
     */
    virtual int protocol() const override;

    virtual int getSequence() const override;

    virtual void pleaseStop() override;
signals:
    void dataDropped(QnArchiveStreamReader* reader);
private:
    QnAbstractDataPacketPtr processFFmpegRtpPayload(quint8* data, int dataSize, int channelNum, qint64* parserPosition);
    void processMetadata(const quint8* data, int dataSize);
    bool openInternal();
    void reopen();

    // determine camera's video server on specified time
    QnMediaServerResourcePtr getServerOnTime(qint64 time);
    QnMediaServerResourcePtr getNextMediaServerFromTime(const QnSecurityCamResourcePtr &camera, qint64 time);
    QnAbstractMediaDataPtr getNextDataInternal();
    QString getUrl(const QnSecurityCamResourcePtr &camera, const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr()) const;
    void checkGlobalTimeAsync(const QnSecurityCamResourcePtr &camera, const QnMediaServerResourcePtr &server, qint64* result);
    void checkMinTimeFromOtherServer(const QnSecurityCamResourcePtr &camera);
    void setupRtspSession(const QnSecurityCamResourcePtr &camera, const QnMediaServerResourcePtr &server, QnRtspClient* session) const;
    void parseAudioSDP(const QStringList& audioSDP);
    void setCustomVideoLayout(const QnCustomResourceVideoLayoutPtr& value);
    bool isConnectionExpired() const;
private:
    QnMutex m_mutex;
    std::unique_ptr<QnRtspClient> m_rtspSession;
    std::unique_ptr<QnRtspIoDevice> m_rtspDevice;
    QnRtspIoDevice* m_rtpData;
    quint8* m_rtpDataBuffer;
    bool m_tcpMode;
    QMap<quint32, quint16> m_prevTimestamp;
    qint64 m_position;
    QnSecurityCamResourcePtr m_camera;
    QnMediaServerResourcePtr m_server;
    QnMediaServerResourcePtr m_fixedServer;
    //bool m_waitBOF;
    int m_lastMediaFlags;
    bool m_closing;
    bool m_singleShotMode;
    quint8 m_sendedCSec;
    qint64 m_lastSeekTime;
    qint64 m_lastReceivedTime;
    bool m_blockReopening;
    MediaQuality m_quality;
    bool m_qualityFastSwitch;
    QSize m_resolution;
    qint64 m_lastMinTimeTime;
    int m_channelCount = 0;

    QnTimePeriod m_serverTimePeriod;
    std::atomic<qint64> m_globalMinArchiveTime; // min archive time between all servers

    qint64 m_forcedEndTime;
    bool m_isMultiserverAllowed;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    bool m_playNowModeAllowed; // fast open mode without DESCRIBE
    QPointer<QnArchiveStreamReader> m_reader;
    int m_frameCnt;
    QnCustomResourceVideoLayoutPtr m_customVideoLayout;

    QMap<int, QSharedPointer<nx::streaming::rtp::QnNxRtpParser>> m_parsers;

    struct {
        QString username;
        QString password;
        QnUuid videowall;
    } m_auth;

    std::atomic_flag m_footageUpToDate;
    std::atomic_flag m_currentServerUpToDate;
    QElapsedTimer m_reopenTimer;
    QElapsedTimer m_sessionTimeout;
    std::chrono::milliseconds m_maxSessionDurationMs;
    nx::vms::api::StreamDataFilters m_streamDataFilter{ nx::vms::api::StreamDataFilter::media};
    QString m_rtpLogTag;
};

typedef QSharedPointer<QnRtspClientArchiveDelegate> QnRtspClientArchiveDelegatePtr;

#endif

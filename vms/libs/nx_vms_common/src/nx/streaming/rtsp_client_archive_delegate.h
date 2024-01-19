// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>

#include <nx/network/http/auth_tools.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/streaming/rtp/parsers/nx_rtp_parser.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/storage_location.h>
#include <recording/time_period.h>

Q_MOC_INCLUDE(<nx/streaming/archive_stream_reader.h>)

struct AVFormatContext;
class QnCustomResourceVideoLayout;
class QnArchiveStreamReader;
class QnCustomResourceVideoLayout;
class QnRtspClient;
class QnRtspIoDevice;
class QnNxRtpParser;

/**
 * Implements QnAbstractArchiveDelegate interface for accessing media data in server archive via
 * RTSP session.
 */
class NX_VMS_COMMON_API QnRtspClientArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT
public:
    QnRtspClientArchiveDelegate(
        QnArchiveStreamReader* reader,
        nx::network::http::Credentials credentials,
        const QString& rtpLogTag = QString(),
        bool sleepIfEmptySocket = false);
    virtual ~QnRtspClientArchiveDelegate();

    // TODO: #sivanov Pass credentials to constructor or using separate setter.
    void setCamera(const QnSecurityCamResourcePtr& camera);
    void updateCredentials(const nx::network::http::Credentials& credentials);

    virtual bool open(
        const QnResourcePtr& resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;

    /**
     * Return data at current position in archive and move current position to next frame.
     * @return Media data at current position. If previous call to getNextData() caused jump over
     *     video data gap, there is no way to detect that analyzing data returned except by
     *     comparison of frames timestamps. Video data just after a gap is not required to be
     *     key frame.
     */
    virtual QnAbstractMediaDataPtr getNextData() override;

    /**
     * Move current position in the archive according to time and findIFrame provided.
     * @param time Desired time to set current position to, microseconds. Actual position will
     *     be set to time near desired time (i.e. not exactly), depending on findIFrame.
     * @param findIFrame If true, current position is set to key frame just before time specified.
     *     If true and archive starts from non-key frame, seek() to time earlier than archive
     *     start sets current position exactly to start of the archive, even if it isn't key frame.
     *     If false, behavior is not strictly defined. In reality, current position is set to
     *     some non-key frame after time desired at arbitrary time distance within the same GOP.
     *     Some using classes assume that if findIFrame is false, current position will be set
     *     exactly to the frame nearest to time specified; that is mistake in fact.
     * @return Simply an echo of time argument. Some using classes assume that return value
     *     depends on real actions performed somehow; that is mistake in fact.
     */
    virtual qint64 seek(qint64 time, bool findIFrame) override;

    qint64 seek(qint64 startTime, qint64 endTime);
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual AudioLayoutConstPtr getAudioLayout() override;

    virtual void setSpeed(qint64 displayTime, double value) override;

    virtual bool isRealTimeSource() const override;
    virtual void beforeClose() override;
    virtual void setSingleshotMode(bool value) override;

    // filter input data by motion region
    virtual void setMotionRegion(const QRegion& region);

    // Send motion data to client
    virtual void setStreamDataFilter(nx::vms::api::StreamDataFilters filter) override;
    virtual nx::vms::api::StreamDataFilters streamDataFilter() const override;

    // Chunks filter
    virtual void setStorageLocationFilter(nx::vms::api::StorageLocation filter) override;

    virtual bool setQuality(
        MediaQuality quality,
        bool fastSwitch,
        const QSize& resolution) override;

    virtual void beforeSeek(qint64 time) override;
    virtual void beforeChangeSpeed(double speed) override;

    void setAdditionalAttribute(const QByteArray& name, const QByteArray& value);
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;
    virtual bool hasVideo() const override;

    void setPlayNowModeAllowed(bool value);

    /**
     * Get currently used network protocol. See the predefined values in the nx::network::Protocol
     * namespace (nx/network/abstract_socket.h).
     * @return 0 if connection is not opened yet, IANA protocol number otherwise.
     */
    virtual int protocol() const override;

    virtual int getSequence() const override;

    virtual CameraDiagnostics::Result lastError() const override;

    virtual void pleaseStop() override;

    void setMediaRole(PlaybackMode mode);
    virtual bool reopen() override;

    void invalidateServer();

signals:
    void dataDropped(QnArchiveStreamReader* reader);
    void needUpdateTimeLine();

private:
    std::pair<QnAbstractDataPacketPtr, bool> processFFmpegRtpPayload(
        quint8* data,
        int dataSize,
        int channelNum,
        qint64* parserPosition);
    void processMetadata(const quint8* data, int dataSize);
    bool openInternal();

    // determine camera's video server on specified time
    QnMediaServerResourcePtr getServerOnTime(qint64 time);
    QnMediaServerResourcePtr getNextMediaServerFromTime(
        const QnSecurityCamResourcePtr& camera, qint64 time);
    QnAbstractMediaDataPtr getNextDataInternal();
    void checkGlobalTimeAsync(
        const QnSecurityCamResourcePtr& camera,
        const QnMediaServerResourcePtr& server,
        qint64* result);
    void checkMinTimeFromOtherServer(const QnSecurityCamResourcePtr& camera);
    void setupRtspSession(
        const QnSecurityCamResourcePtr& camera,
        const QnMediaServerResourcePtr& server,
        QnRtspClient* session) const;
    void parseAudioSDP(const QStringList& audioSDP);
    void setCustomVideoLayout(const QnCustomResourceVideoLayoutPtr& value);
    bool isConnectionExpired() const;

private:
    nx::Mutex m_mutex;
    std::unique_ptr<QnRtspClient> m_rtspSession;
    std::unique_ptr<QnRtspIoDevice> m_rtspDevice;
    QnRtspIoDevice* m_rtpData = nullptr;
    quint8* m_rtpDataBuffer;
    bool m_tcpMode = true;
    QMap<quint32, quint16> m_prevTimestamp;
    qint64 m_position = DATETIME_NOW;
    QnSecurityCamResourcePtr m_camera;
    QnMediaServerResourcePtr m_server;
    int m_lastMediaFlags = -1;
    bool m_closing = false;
    bool m_singleShotMode = false;
    quint8 m_sendedCSec = 0;
    qint64 m_lastSeekTime = DATETIME_INVALID;
    qint64 m_lastReceivedTime = DATETIME_INVALID;
    std::atomic<bool> m_blockReopening = false;
    MediaQuality m_quality = MediaQuality::MEDIA_Quality_High;
    bool m_qualityFastSwitch = true;
    QSize m_resolution;
    qint64 m_lastMinTimeTime = 0;
    int m_channelCount = 0;

    QnTimePeriod m_serverTimePeriod;

    /** Minimal archive time between all servers. */
    std::atomic<qint64> m_globalMinArchiveTime = DATETIME_INVALID;

    qint64 m_forcedEndTime = DATETIME_INVALID;
    AudioLayoutPtr m_audioLayout;

    /** Fast open mode without DESCRIBE. */
    bool m_playNowModeAllowed = true;
    QPointer<QnArchiveStreamReader> m_reader;
    int m_frameCnt = 0;
    QnCustomResourceVideoLayoutPtr m_customVideoLayout;

    QMap<int, QSharedPointer<nx::rtp::QnNxRtpParser>> m_parsers;

    nx::network::http::Credentials m_credentials;

    std::atomic_flag m_footageUpToDate;
    std::atomic_flag m_currentServerUpToDate;
    QElapsedTimer m_reopenTimer;
    QElapsedTimer m_sessionTimeout;
    std::chrono::milliseconds m_maxSessionDurationMs{std::numeric_limits<qint64>::max()};
    nx::vms::api::StreamDataFilters m_streamDataFilter{nx::vms::api::StreamDataFilter::media};
    nx::vms::api::StorageLocation m_storageLocationFilter{};
    QString m_rtpLogTag;
    const bool m_sleepIfEmptySocket;
    CameraDiagnostics::Result m_lastError = CameraDiagnostics::NoErrorResult();
};

typedef QSharedPointer<QnRtspClientArchiveDelegate> QnRtspClientArchiveDelegatePtr;

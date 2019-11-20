#pragma once
#include <nx/streaming/abstract_data_consumer.h>
#include <camera/video_camera.h>
#include <audit/audit_manager.h>
#include <utils/common/adaptive_sleep.h>

#include "cached_output_stream.h"

class ProgressiveDownloadingServer;

class ProgressiveDownloadingConsumer: public QnAbstractDataConsumer
{
public:
    struct Config
    {
        bool standFrameDuration = false;
        bool dropLateFrames = false;
        unsigned int maxFramesToCacheBeforeDrop = 0;
        bool liveMode = false;
        bool continuousTimestamps = false;
        int64_t endTimeUsec = AV_NOPTS_VALUE;
    };

public:
    ProgressiveDownloadingConsumer(ProgressiveDownloadingServer* owner, const Config& config);
    ~ProgressiveDownloadingConsumer();
    void setAuditHandle(const AuditHandle& handle);
    void copyLastGopFromCamera(const QnVideoCameraPtr& camera);

protected:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

private:
    ProgressiveDownloadingServer* m_owner;
    const Config m_config;
    int64_t m_lastMediaTime {AV_NOPTS_VALUE};
    int64_t m_utcShift {0};
    int64_t m_rtStartTime {AV_NOPTS_VALUE};
    int64_t m_lastRtTime {0};
    bool m_needKeyData {false};
    std::unique_ptr<CachedOutputStream> m_dataOutput;
    QnAdaptiveSleep m_adaptiveSleep {MAX_FRAME_DURATION_MS * 1000};
    AuditHandle m_auditHandle;

private:
    void sendFrame(qint64 timestamp, const QnByteArray& result);
    QByteArray toHttpChunk(const char* data, size_t size);
    void doRealtimeDelay(const QnAbstractDataPacketPtr& media);
    void finalizeMediaStream();
};

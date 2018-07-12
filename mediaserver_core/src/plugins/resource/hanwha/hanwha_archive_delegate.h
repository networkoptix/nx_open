#pragma once

#include <nx/streaming/abstract_archive_delegate.h>
#include <recording/time_period_list.h>
#include <core/resource/avi/thumbnails_archive_delegate.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaStreamReader;

class HanwhaArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    HanwhaArchiveDelegate(const QnResourcePtr& res);
    virtual ~HanwhaArchiveDelegate();

    virtual bool open(
        const QnResourcePtr &resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 timeUsec, bool findIFrame) override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    virtual void beforeClose() override;

    virtual void setSpeed(qint64 displayTime, double value);
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;
    virtual void setOverlappedId(nx::core::resource::OverlappedId overlappedId);

    virtual void beforeSeek(qint64 time) override;

    virtual void setPlaybackMode(PlaybackMode value) override;
    virtual void setGroupId(const QByteArray& id) override;

    void setRateControlEnabled(bool enabled);
    virtual bool setQuality(
        MediaQuality /*quality*/,
        bool /*fastSwitch*/,
        const QSize& /*resolution*/) override;

    virtual CameraDiagnostics::Result lastError() const override;
private:
    bool isForwardDirection() const;

    qint64 currentPositionUsec() const;
    void updateCurrentPositionUsec(
        qint64 positionUsec,
        bool isForwardPlayback,
        bool force);

private:
    std::unique_ptr<QnThumbnailsArchiveDelegate> m_thumbnailsDelegate;
    std::shared_ptr<HanwhaStreamReader> m_streamReader;
    bool m_rateControlEnabled = true;
    qint64 m_startTimeUsec = AV_NOPTS_VALUE;
    qint64 m_endTimeUsec = AV_NOPTS_VALUE;
    qint64 m_currentPositionUsec = AV_NOPTS_VALUE;
    PlaybackMode m_playbackMode = PlaybackMode::Archive;
    CameraDiagnostics::Result m_lastOpenResult {CameraDiagnostics::NoErrorResult()};
    bool m_isSeekAlignedByChunkBorder = false;
    SessionContextPtr m_sessionContext;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

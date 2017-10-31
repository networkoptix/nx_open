#pragma once

#include <nx/streaming/abstract_archive_delegate.h>
#include <recording/time_period_list.h>
#include <core/resource/avi/thumbnails_archive_delegate.h>

#if defined(ENABLE_HANWHA)

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaStreamReader;

class HanwhaArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    HanwhaArchiveDelegate(const QnResourcePtr& res);
    virtual ~HanwhaArchiveDelegate();

    virtual bool open(const QnResourcePtr &resource) override;
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

    virtual void beforeSeek(qint64 time) override;

    virtual void setPlaybackMode(PlaybackMode value) override;
private:
    bool isForwardDirection() const;
private:
    std::unique_ptr<QnThumbnailsArchiveDelegate> m_thumbnailsDelegate;
    std::shared_ptr<HanwhaStreamReader> m_streamReader;
    qint64 m_endTimeUsec = AV_NOPTS_VALUE;
    qint64 m_currentPositionUsec = AV_NOPTS_VALUE;
    PlaybackMode m_playbackMode = PlaybackMode::Archive;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)

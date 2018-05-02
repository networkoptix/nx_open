#pragma once

#include <nx/streaming/abstract_archive_delegate.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class ArchiveDelegateWrapper: public QnAbstractArchiveDelegate
{
    using ArchiveDelegatePtr = std::unique_ptr<QnAbstractArchiveDelegate>;

public:
    ArchiveDelegateWrapper(std::unique_ptr<QnAbstractArchiveDelegate> delegate);

    virtual bool open(
        const QnResourcePtr& resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher = nullptr) override;

    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek(qint64 time, bool findIframe) override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;
    virtual bool hasVideo() const override;
    virtual AVCodecContext* setAudioChannel(int num) override;
    virtual void setSpeed(qint64 displayTime, double value) override;
    virtual void setSingleshotMode(bool value) override;
    virtual bool setQuality(
        MediaQuality quality,
        bool fastSwitch,
        const QSize& resolution) override;

    virtual bool isRealTimeSource() const override;
    virtual void beforeClose() override;
    virtual void beforeSeek(qint64 time) override;
    virtual void beforeChangeSpeed(double speed) override;
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;
    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) override;
    virtual void setSendMotion(bool value) override;
    virtual void setMotionRegion(const QnMotionRegion& region) override;
    virtual void setGroupId(const QByteArray& groupId) override;
    virtual QnAbstractArchiveDelegate::ArchiveChunkInfo getLastUsedChunkInfo() const override;
    virtual int getSequence() const override;
    virtual void setPlaybackMode(PlaybackMode value) override;
    virtual void setEndOfPlaybackHandler(std::function<void()> handler) override;
    virtual void setErrorHandler(std::function<void(const QString& errorString)> handler) override;

    QnAbstractArchiveDelegate* delegate() const;

private:
    ArchiveDelegatePtr m_delegate;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

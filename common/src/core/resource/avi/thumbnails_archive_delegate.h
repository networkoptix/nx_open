#pragma once

#include <core/resource/avi/avi_archive_delegate.h>

class QnThumbnailsArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT;
public:
    QnThumbnailsArchiveDelegate(QnAbstractArchiveDelegatePtr baseDelegate);

    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual bool open(
        const QnResourcePtr &resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;
    virtual void setGroupId(const QByteArray& groupId) override;

    virtual ArchiveChunkInfo getLastUsedChunkInfo() const override;

private:
    qint64 m_currentPos;
    qint64 m_rangeStart;
    qint64 m_rangeEnd;
    qint64 m_frameStep;
    qint64 m_lastMediaTime;
    QnAbstractArchiveDelegatePtr m_baseDelegate;
    int m_nextChannelNum;
    int m_channelCount;
};

typedef QSharedPointer<QnThumbnailsArchiveDelegate> QnThumbnailsArchiveDelegatePtr;

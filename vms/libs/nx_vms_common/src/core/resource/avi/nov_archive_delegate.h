// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef __NOV_ARCHIVE_DELEGATE_H
#define __NOV_ARCHIVE_DELEGATE_H

#include <QtCore/QSharedPointer>

#include <recording/time_period_list.h>

#include "avi_archive_delegate.h"

struct AVFormatContext;
class QnCustomResourceVideoLayout;
class QnAviAudioLayout;

class NX_VMS_COMMON_API QnNovArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    QnNovArchiveDelegate(const QnStorageResourcePtr& storage);

    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    virtual bool open(
        const QnResourcePtr& resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher = nullptr) override;
    virtual void setSpeed(qint64 displayTime, double value) override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual AudioLayoutConstPtr getAudioLayout() override;
    virtual void close() override;
    virtual std::optional<QnAviArchiveMetadata> metadata() const override;
private:
    QString getFileName(const QString& url, int64_t timestamp) const;
    void findStartEndTime(const QnResourcePtr& resource);
    bool openFile(const QString& filename);

private:
    QnTimePeriodList m_chunks;
    std::vector<int64_t> m_startTimes;
    QnAviArchiveDelegate m_currentFile;
    std::optional<QnAviArchiveMetadata> m_metadata;
    int m_fileIndex = 0;
    QString m_baseFilename;
    QnResourcePtr m_resource;
    QnStorageResourcePtr m_storage;

    qint64 m_skipFramesBeforeTime;
    qint64 m_startTime = 0;
    qint64 m_endTime = 0;
    bool m_reverseMode;
};

typedef QSharedPointer<QnNovArchiveDelegate> QnNovArchiveDelegatePtr;

#endif // __NOV_ARCHIVE_DELEGATE_H

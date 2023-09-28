// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/streaming/abstract_archive_delegate.h>
#include <utils/media/externaltimesource.h>

class QnAbstractArchiveStreamReader;
class QnArchiveSyncPlayWrapper;
class QnAbstractArchiveDelegate;

class QnSyncPlayArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT

public:
    QnSyncPlayArchiveDelegate(
        QnAbstractArchiveStreamReader* reader,
        QnArchiveSyncPlayWrapper* syncWrapper,
        QnAbstractArchiveDelegate* ownerDelegate);
    virtual ~QnSyncPlayArchiveDelegate() override;

    virtual bool open(
        const QnResourcePtr &resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    virtual void close() override;
    virtual bool reopen() override;
    virtual void beforeClose() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual AudioLayoutConstPtr getAudioLayout() override;
    virtual bool isRealTimeSource() const override;
    virtual void setSpeed(qint64 displayTime, double value) override;
    virtual void setSingleshotMode(bool value) override;

    virtual bool setAudioChannel(unsigned num) override;

    virtual void beforeSeek(qint64 time) override;
    virtual void beforeChangeSpeed(double value) override;
    virtual bool setQuality(MediaQuality quality, bool fastSwitch, const QSize& size) override;
    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) override;
    virtual void setStreamDataFilter(nx::vms::api::StreamDataFilters filter) override;
    virtual nx::vms::api::StreamDataFilters streamDataFilter() const override;

    virtual void setStorageLocationFilter(nx::vms::api::StorageLocation filter) override;

    virtual int protocol() const override;
    virtual ArchiveChunkInfo getLastUsedChunkInfo() const override;
    virtual bool hasVideo() const override;
    virtual void pleaseStop() override;

protected:
    friend class QnArchiveSyncPlayWrapper;
    //void setPrebuffering(bool value);

private:
    //nx::Mutex m_mutex;
    //bool m_usePrebuffer;
    //QnAbstractMediaDataPtr m_nextData;
    QnAbstractArchiveStreamReader* m_reader;
    QPointer<QnArchiveSyncPlayWrapper> m_syncWrapper;
    QnAbstractArchiveDelegate* m_ownerDelegate;
};

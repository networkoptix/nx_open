#pragma once

#include <nx/utils/thread/mutex.h>
#include <QtGui/QRegion>

#include "core/resource/resource_fwd.h"
#include "core/resource/avi/avi_archive_delegate.h"
#include "recorder/device_file_catalog.h"
#include "recorder/storage_manager.h"
#include "utils/media/sse_helper.h"
#include "motion/motion_archive.h"
#include "dualquality_helper.h"

class QnMediaServerModule;

class QnServerArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    QnServerArchiveDelegate(
        QnMediaServerModule* mediaServerModule, MediaQuality quality = MEDIA_Quality_High);
    virtual ~QnServerArchiveDelegate() override;

    //virtual void setSendMotion(bool value) override;

    virtual bool open(
        const QnResourcePtr &resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    bool isOpened() const;
    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual qint64 endTime() const override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    virtual bool setAudioChannel(unsigned num) override;
    virtual void setSpeed(qint64 displayTime, double value) override;

    virtual bool setQuality(MediaQuality quality, bool fastSwitch, const QSize &) override;
    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) override;
    virtual QnAbstractMotionArchiveConnectionPtr getAnalyticsConnection(int channel) override;

    virtual ArchiveChunkInfo getLastUsedChunkInfo() const override;

private:
    bool switchToChunk(const DeviceFileCatalog::TruncableChunk &newChunk, const DeviceFileCatalogPtr& newCatalog);
    qint64 seekInternal(qint64 time, bool findIFrame, bool recursive);
    bool getNextChunk(DeviceFileCatalog::TruncableChunk& chunk, DeviceFileCatalogPtr& chunkCatalog,
                      DeviceFileCatalog::UniqueChunkCont &ignoreChunks);
    bool setQualityInternal(MediaQuality quality, bool fastSwitch, qint64 timeMs, bool recursive);
    void setCatalogs() const;

    DeviceFileCatalog::Chunk findChunk(DeviceFileCatalogPtr catalog, qint64 time, DeviceFileCatalog::FindMethod findMethod);

private:
    typedef std::map<QnServer::StoragePool, DeviceFileCatalogPtr> PoolToCatalogMap;

    QnMediaServerModule* m_mediaServerModule;
    bool m_opened;
    QnResourcePtr m_resource;
    qint64 m_lastPacketTime;

    qint64 m_skipFramesToTime;
    mutable PoolToCatalogMap m_catalogHi;
    mutable PoolToCatalogMap m_catalogLow;
    //QnChunkSequence* m_chunkSequenceHi;
    //QnChunkSequence* m_chunkSequenceLow;
    DeviceFileCatalog::Chunk m_currentChunk;
    PoolToCatalogMap m_currentChunkCatalog;

    QnAviArchiveDelegatePtr m_aviDelegate;
    QnAviResourcePtr m_fileRes;
    bool m_reverseMode;
    unsigned m_selectedAudioChannel = 0;

    QRegion m_motionRegion;

    QnTimePeriod m_lastTimePeriod;
    qint64 m_lastSeekTime;
    bool m_afterSeek;
    //QnMotionArchiveConnectionPtr m_motionConnection[CL_MAX_CHANNELS];
    //QnAbstractMediaDataPtr m_tmpData;

    DeviceFileCatalog::Chunk m_newQualityChunk;
    DeviceFileCatalogPtr m_newQualityCatalog;
    QnAbstractMediaDataPtr m_newQualityTmpData;
    QnAviResourcePtr m_newQualityFileRes;
    QnAviArchiveDelegatePtr m_newQualityAviDelegate;

    //bool m_sendMotion;
    bool m_eof;
    MediaQuality m_quality;
    QnDualQualityHelper m_dialQualityHelper;

    ArchiveChunkInfo m_currentChunkInfo;

    mutable QnMutex m_mutex;
    QnServer::ChunksCatalog m_lastChunkQuality;
    QnServer::StoragePool m_currentChunkStoragePool;
    QnServer::StoragePool m_newQualityChunkStoragePool;

    AbstractArchiveIntegrityWatcher* m_archiveIntegrityWatcher;
private:
    QnMediaServerModule* m_serverModule = nullptr;
};

typedef QSharedPointer<QnServerArchiveDelegate> QnServerArchiveDelegatePtr;

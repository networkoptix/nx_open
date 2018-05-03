#ifndef _SERVER_ARCHIVE_DELEGATE_H__
#define _SERVER_ARCHIVE_DELEGATE_H__

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
    QnServerArchiveDelegate(QnMediaServerModule* mediaServerModule);
    virtual ~QnServerArchiveDelegate();

    //virtual void setSendMotion(bool value) override;

    virtual bool open(
        const QnResourcePtr &resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    bool isOpened() const;
    virtual void close();
    virtual qint64 startTime() const;
    virtual qint64 endTime() const;
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;

    virtual AVCodecContext* setAudioChannel(int num);
    virtual void setSpeed(qint64 displayTime, double value);

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
    int m_selectedAudioChannel;

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
};

typedef QSharedPointer<QnServerArchiveDelegate> QnServerArchiveDelegatePtr;

#endif // _SERVER_ARCHIVE_DELEGATE_H__

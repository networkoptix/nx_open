#ifndef _SERVER_ARCHIVE_DELEGATE_H__
#define _SERVER_ARCHIVE_DELEGATE_H__

#include <QRegion>

#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "plugins/resources/archive/avi_files/avi_device.h"
#include "recorder/device_file_catalog.h"
#include "recorder/storage_manager.h"
#include "utils/media/sse_helper.h"
#include "motion/motion_archive.h"

class QnServerArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    QnServerArchiveDelegate();
    virtual ~QnServerArchiveDelegate();

    void setMotionRegion(const QRegion& region);
    void setSendMotion(bool value);

    virtual bool open(QnResourcePtr resource);
    virtual void close();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnVideoResourceLayout* getVideoLayout();
    virtual QnResourceAudioLayout* getAudioLayout();

    virtual AVCodecContext* setAudioChannel(int num);
    virtual void onReverseMode(qint64 displayTime, bool value);

    virtual bool setQuality(MediaQuality quality, bool fastSwitch);
private:
    bool switchToChunk(const DeviceFileCatalog::Chunk newChunk, DeviceFileCatalogPtr newCatalog);
    qint64 correctTimeByMask(qint64 time, bool useReverseSearch);
    qint64 seekInternal(qint64 time, bool findIFrame, bool recursive);
    void loadPlaybackMask(qint64 msTime, bool useReverseSearch);
    void getNextChunk(DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& chunkCatalog);
private:
    bool m_opened;
    QnResourcePtr m_resource;
    qint64 m_lastPacketTime;
    
    qint64 m_skipFramesToTime;
    DeviceFileCatalogPtr m_catalogHi;
    DeviceFileCatalogPtr m_catalogLow;
    QnChunkSequence* m_chunkSequenceHi;
    QnChunkSequence* m_chunkSequenceLow;
    DeviceFileCatalog::Chunk m_currentChunk;
    DeviceFileCatalogPtr m_currentChunkCatalog;

    QnAviArchiveDelegatePtr m_aviDelegate;
    QnAviResourcePtr m_fileRes;
    bool m_reverseMode;
    int m_selectedAudioChannel;

    QRegion m_motionRegion;
    QnTimePeriodList m_playbackMask;
    qint64 m_playbackMaskStart;
    qint64 m_playbackMaskEnd;

    QnTimePeriod m_lastTimePeriod;
    qint64 m_lastSeekTime;
    bool m_afterSeek;
    QnMotionArchiveConnectionPtr m_motionConnection;
    QnAbstractMediaDataPtr m_tmpData;

    DeviceFileCatalog::Chunk m_newQualityChunk;
    DeviceFileCatalogPtr m_newQualityCatalog;
    QnAbstractMediaDataPtr m_newQualityTmpData;
    QnAviResourcePtr m_newQualityFileRes;
    QnAviArchiveDelegatePtr m_newQualityAviDelegate;
    

    bool m_sendMotion;
    bool m_eof;
    MediaQuality m_quality;
};

#endif // _SERVER_ARCHIVE_DELEGATE_H__

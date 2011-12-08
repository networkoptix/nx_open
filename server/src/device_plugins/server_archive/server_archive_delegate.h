#ifndef _SERVER_ARCHIVE_DELEGATE_H__
#define _SERVER_ARCHIVE_DELEGATE_H__

#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "plugins/resources/archive/avi_files/avi_device.h"
#include "recording/device_file_catalog.h"
#include "recording/storage_manager.h"

class QnServerArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    QnServerArchiveDelegate();
    virtual ~QnServerArchiveDelegate();

    void setPlaybackMask(const QnTimePeriodList& periods);

    virtual bool open(QnResourcePtr resource);
    virtual void close();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time);
    virtual QnVideoResourceLayout* getVideoLayout();
    virtual QnResourceAudioLayout* getAudioLayout();

    virtual AVCodecContext* setAudioChannel(int num);
    virtual void onReverseMode(qint64 displayTime, bool value);
private:
    bool switchToChunk(const DeviceFileCatalog::Chunk newChunk);
    qint64 correctTimeByMask(qint64 time);
    qint64 seekInternal(qint64 time);
private:
    bool m_opened;
    QnResourcePtr m_resource;
    DeviceFileCatalogPtr m_catalog;
    QnChunkSequence* m_chunkSequence;
    DeviceFileCatalog::Chunk m_currentChunk;
    QnAviArchiveDelegate* m_aviDelegate;
    QnAviResourcePtr m_fileRes;
    bool m_reverseMode;
    int m_selectedAudioChannel;

    QnTimePeriodList m_mask;
    QnTimePeriod m_lastTimePeriod;
};

#endif // _SERVER_ARCHIVE_DELEGATE_H__

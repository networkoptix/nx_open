#ifndef _SYNC_PLAY_ARCHIVE_DELEGATE_H__
#define _SYNC_PLAY_ARCHIVE_DELEGATE_H__

#include "abstract_archive_delegate.h"

class QnAbstractArchiveReader;
class QnArchiveSyncPlayWrapper;
class QnAbstractArchiveDelegate;

class QnSyncPlayArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    QnSyncPlayArchiveDelegate(QnAbstractArchiveReader* reader, QnArchiveSyncPlayWrapper* syncWrapper, QnAbstractArchiveDelegate* ownerDelegate);
    virtual ~QnSyncPlayArchiveDelegate();

    virtual bool open(QnResourcePtr resource);
    virtual void close();
    virtual void beforeClose();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time);
    virtual QnVideoResourceLayout* getVideoLayout();
    virtual QnResourceAudioLayout* getAudioLayout();
    virtual bool isRealTimeSource() const;

    void setStartDelay(qint64 startDelay);
    qint64 secondTime() const;
    qint64 jumpToPreviousFrame (qint64 time, bool makeshot);

    virtual AVCodecContext* setAudioChannel(int num);
    void enableSync(bool value);

private:
    QnArchiveSyncPlayWrapper* m_syncWrapper;
    QnAbstractArchiveDelegate* m_ownerDelegate;
    QnAbstractMediaDataPtr m_tmpData;
    qint64 m_seekTime;

    mutable QMutex m_genericMutex;
    qint64 m_startDelay;
    QTime m_initTime;
    QnAbstractArchiveReader* m_reader;
    bool m_enableSync;
    bool m_liveMode;
};

#endif // _SYNC_PLAY_ARCHIVE_DELEGATE_H__

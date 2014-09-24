#ifndef _SYNC_PLAY_ARCHIVE_DELEGATE_H__
#define _SYNC_PLAY_ARCHIVE_DELEGATE_H__

#include <utils/media/externaltimesource.h>

#include <plugins/resource/archive/abstract_archive_delegate.h>

class QnAbstractArchiveReader;
class QnArchiveSyncPlayWrapper;
class QnAbstractArchiveDelegate;

class QnSyncPlayArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT;

public:
    QnSyncPlayArchiveDelegate(QnAbstractArchiveReader* reader, QnArchiveSyncPlayWrapper* syncWrapper, QnAbstractArchiveDelegate* ownerDelegate);
    virtual ~QnSyncPlayArchiveDelegate();

    virtual bool open(const QnResourcePtr &resource);
    virtual void close();
    virtual void beforeClose();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnResourceVideoLayoutPtr getVideoLayout();
    virtual QnResourceAudioLayoutPtr getAudioLayout();
    virtual bool isRealTimeSource() const;
    virtual void onReverseMode(qint64 /*displayTime*/, bool /*value*/);
    virtual void setSingleshotMode(bool value);

    virtual AVCodecContext* setAudioChannel(int num);

    //virtual void setMotionRegion(const QRegion& region);
    //virtual void setSendMotion(bool value);
    virtual void beforeSeek(qint64 time);
    virtual void beforeChangeReverseMode(bool reverseMode);
    virtual bool setQuality(MediaQuality quality, bool fastSwitch) override;
    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) override;
    virtual void setSendMotion(bool value) override;

    virtual ArchiveChunkInfo getLastUsedChunkInfo() const override;

protected:
    friend class QnArchiveSyncPlayWrapper;
    //void setPrebuffering(bool value);
private:
    //QMutex m_mutex;
    //bool m_usePrebuffer;
    //QnAbstractMediaDataPtr m_nextData;
    QnAbstractArchiveReader* m_reader; 
    QnArchiveSyncPlayWrapper* m_syncWrapper;
    QnAbstractArchiveDelegate* m_ownerDelegate;
};

#endif // _SYNC_PLAY_ARCHIVE_DELEGATE_H__

#pragma once

#include <QtCore/QPointer>

#include <utils/media/externaltimesource.h>

#include <nx/streaming/abstract_archive_delegate.h>

class QnAbstractArchiveStreamReader;
class QnArchiveSyncPlayWrapper;
class QnAbstractArchiveDelegate;

class QnSyncPlayArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT;

public:
    QnSyncPlayArchiveDelegate(QnAbstractArchiveStreamReader* reader, QnArchiveSyncPlayWrapper* syncWrapper, QnAbstractArchiveDelegate* ownerDelegate);
    virtual ~QnSyncPlayArchiveDelegate();

    virtual bool open(
        const QnResourcePtr &resource,
        AbstractMetaDataIntegrityChecker* metaDataIntegrityChecker = nullptr) override;
    virtual void close();
    virtual void beforeClose();
    virtual qint64 startTime() const;
    virtual qint64 endTime() const;
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnConstResourceVideoLayoutPtr getVideoLayout();
    virtual QnConstResourceAudioLayoutPtr getAudioLayout();
    virtual bool isRealTimeSource() const;
    virtual void setSpeed(qint64 displayTime, double value);
    virtual void setSingleshotMode(bool value);

    virtual AVCodecContext* setAudioChannel(int num);

    //virtual void setMotionRegion(const QRegion& region);
    //virtual void setSendMotion(bool value);
    virtual void beforeSeek(qint64 time);
    virtual void beforeChangeSpeed(double value);
    virtual bool setQuality(MediaQuality quality, bool fastSwitch, const QSize& size) override;
    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) override;
    virtual void setSendMotion(bool value) override;

    virtual ArchiveChunkInfo getLastUsedChunkInfo() const override;
    virtual bool hasVideo() const override;
protected:
    friend class QnArchiveSyncPlayWrapper;
    //void setPrebuffering(bool value);
private:
    //QnMutex m_mutex;
    //bool m_usePrebuffer;
    //QnAbstractMediaDataPtr m_nextData;
    QnAbstractArchiveStreamReader* m_reader;
    QPointer<QnArchiveSyncPlayWrapper> m_syncWrapper;
    QnAbstractArchiveDelegate* m_ownerDelegate;
};


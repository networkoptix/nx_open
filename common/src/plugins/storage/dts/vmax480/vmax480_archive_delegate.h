#ifndef __VMAX480_ARCHIVE_DELEGATE
#define __VMAX480_ARCHIVE_DELEGATE

#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "vmax480_resource.h"
#include "vmax480_stream_fetcher.h"

class QnVMax480ArchiveDelegate: public QnAbstractArchiveDelegate, public QnVmax480DataConsumer
{
public:
    QnVMax480ArchiveDelegate(QnResourcePtr res);
    virtual ~QnVMax480ArchiveDelegate();


    virtual bool open(QnResourcePtr resource)override;
    virtual void close()override;
    virtual qint64 startTime()override;
    virtual qint64 endTime()override;
    virtual QnAbstractMediaDataPtr getNextData()override;
    virtual qint64 seek (qint64 time, bool findIFrame)override;
    virtual QnResourceVideoLayout* getVideoLayout()override;
    virtual QnResourceAudioLayout* getAudioLayout()override;

    virtual void beforeClose() override;

    virtual void onReverseMode(qint64 displayTime, bool value);
    virtual void setRange(qint64 startTime, qint64 endTime, qint64 frameStep) override;

    virtual int getChannel() const override;

    virtual void setGroupId(const QByteArray& data) override;
    virtual QnTimePeriodList chunks() override;
    virtual void beforeSeek(qint64 time) override;
private:
    void calcSeekPoints(qint64 startTime, qint64 endTime, qint64 frameStep);
    qint64 seekInternal(qint64 time, bool findIFrame);
private:
    VMaxStreamFetcher* m_maxStream;
    QnPlVmax480ResourcePtr m_res;
    bool m_needStop;
    bool m_reverseMode;
    QMap<qint64, bool> m_ThumbnailsSeekPoints; // key - time, value - isRecordingHole detected
    bool m_thumbnailsMode;
    qint64 m_lastSeekPos;
    bool m_isOpened;
    mutable QMutex m_seekMtx;
    QByteArray m_groupId;
    bool m_beforeSeek;
    bool m_ignoreNextSeek;
};

#endif // __VMAX480_ARCHIVE_DELEGATE

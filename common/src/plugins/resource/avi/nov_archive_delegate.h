#ifndef __NOV_ARCHIVE_DELEGATE_H
#define __NOV_ARCHIVE_DELEGATE_H

#include <QSharedPointer>
#include "avi_archive_delegate.h"
#include "recording/time_period_list.h"

struct AVFormatContext;
class QnCustomResourceVideoLayout;
class QnAviAudioLayout;

class QnNovArchiveDelegate: public QnAviArchiveDelegate
{
    Q_OBJECT;
public:
    QnNovArchiveDelegate();

    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    virtual bool open(const QnResourcePtr &resource) override;
    virtual void setSpeed(qint64 displayTime, double value) override;
private:
    QnTimePeriodList m_chunks;
    qint64 m_skipFramesBeforeTime;
    bool m_reverseMode;
};

typedef QSharedPointer<QnNovArchiveDelegate> QnNovArchiveDelegatePtr;

#endif // __NOV_ARCHIVE_DELEGATE_H

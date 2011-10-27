#ifndef __SYNCPLAY_WRAPPER_H__
#define __SYNCPLAY_WRAPPER_H__

#include "base/base.h"
class QnAbstractArchiveReader;
class QnAbstractArchiveDelegate;

class QnArchiveSyncPlayWrapper
{
public:
    QnArchiveSyncPlayWrapper();
    virtual ~QnArchiveSyncPlayWrapper();
    void addArchiveReader(QnAbstractArchiveReader* reader);
    void removeArchiveReader(QnAbstractArchiveReader* reader);
private:
    qint64 minTime() const;
    qint64 endTime() const;
    qint64 seek (qint64 time);
    qint64 secondTime() const;
    void waitIfNeed(QnAbstractArchiveReader* reader, qint64 timestamp);
    void onNewDataReaded();
    void erase(QnAbstractArchiveDelegate* value);
private:
    friend class QnSyncPlayArchiveDelegate;
    QN_DECLARE_PRIVATE(QnArchiveSyncPlayWrapper);
};

#endif // __SYNCPLAY_WRAPPER_H__

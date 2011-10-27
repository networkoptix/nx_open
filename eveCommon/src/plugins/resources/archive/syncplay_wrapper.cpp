#include <QWaitCondition>

#include "syncplay_wrapper.h"
#include "syncplay_archive_delegate.h"
#include "abstract_archive_stream_reader.h"

static const qint64 WAIT_EPS = 0; //100 * 1000;

struct ReaderInfo
{
    ReaderInfo(QnAbstractArchiveReader* _reader, QnAbstractArchiveDelegate* _oldDelegate):
        reader(_reader),
        oldDelegate(_oldDelegate)
    {
    }
    QnAbstractArchiveReader* reader;
    QnAbstractArchiveDelegate* oldDelegate;
};

struct QnArchiveSyncPlayWrapper::QnArchiveSyncPlayWrapperPrivate
{
    QnArchiveSyncPlayWrapperPrivate()
    {
    }

    QList<ReaderInfo> readers;
    QMutex mutex;

    QWaitCondition syncCond;
    QMutex syncMutex;
};

// ------------------- QnArchiveSyncPlayWrapper ----------------------------

QnArchiveSyncPlayWrapper::QnArchiveSyncPlayWrapper():
    d_ptr(new QnArchiveSyncPlayWrapperPrivate())
{
}

QnArchiveSyncPlayWrapper::~QnArchiveSyncPlayWrapper()
{

    delete d_ptr;
}

void QnArchiveSyncPlayWrapper::addArchiveReader(QnAbstractArchiveReader* reader)
{
    Q_D(QnArchiveSyncPlayWrapper);
    d->readers << ReaderInfo(reader, reader->getArchiveDelegate());
    reader->setArchiveDelegate(new QnSyncPlayArchiveDelegate(reader, this, reader->getArchiveDelegate()));
    reader->setCycleMode(false);
}

qint64 QnArchiveSyncPlayWrapper::minTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 result = 0x7fffffffffffffff;
    foreach(ReaderInfo info, d->readers)
        result = qMin(result, info.oldDelegate->startTime());
    return result;
}

qint64 QnArchiveSyncPlayWrapper::endTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 result = 0;
    foreach(ReaderInfo info, d->readers)
        result = qMax(result, info.oldDelegate->endTime());
    return result;
}

qint64 QnArchiveSyncPlayWrapper::seek (qint64 time)
{
    Q_D(QnArchiveSyncPlayWrapper);
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->internalSeek(time);
    }
    d->syncCond.wakeAll();
    return time;
}

qint64 QnArchiveSyncPlayWrapper::secondTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 rez = 0x7fffffffffffffffll;
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        qint64 val = syncDelegate->secondTime();
        if (val != AV_NOPTS_VALUE)
            rez = qMin(rez, val);
    }
    return rez;
}

void QnArchiveSyncPlayWrapper::waitIfNeed(QnAbstractArchiveReader* reader, qint64 timestamp)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->syncMutex);
    while (!reader->needToStop() && timestamp > secondTime()+WAIT_EPS) // time of second(next) frame is less than it time.
    {
        d->syncCond.wait(&d->syncMutex, 10);
    }
}

void QnArchiveSyncPlayWrapper::onNewDataReaded()
{
    Q_D(QnArchiveSyncPlayWrapper);
    d->syncCond.wakeAll();
}

void QnArchiveSyncPlayWrapper::removeArchiveReader(QnAbstractArchiveReader* reader)
{
    erase(reader->getArchiveDelegate());
}

void QnArchiveSyncPlayWrapper::erase(QnAbstractArchiveDelegate* value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->syncMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->reader->getArchiveDelegate() == value)
        {
            d->readers.erase(i);
            break;
        }
    }
}

#ifndef QN_OBJECT_THREAD_PULLER_H
#define QN_OBJECT_THREAD_PULLER_H

#include <cassert>

#include <QtCore/QSet>
#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QSharedPointer>
#include <utils/common/mutex.h>
#include <utils/common/wait_condition.h>
#include <QtCore/QCoreApplication>

#include "warnings.h"
#include "event_loop.h"


namespace detail {
    struct QnObjectThreadPullerData {
        QSet<QObject *> objects;
        QnMutex mutex;
        QnWaitCondition condition;
    };

    typedef QSharedPointer<QnObjectThreadPullerData> QnObjectThreadPullerDataPtr;

    class QnObjectThreadPullWorker: public QObject {
        Q_OBJECT
    public:
        QnObjectThreadPullWorker(QObject *object, QThread *thread, const QnObjectThreadPullerDataPtr &d): d(d), m_object(object), m_thread(thread) {}

        virtual ~QnObjectThreadPullWorker() {
            m_object->moveToThread(m_thread);
            emit pushed(m_object);

            SCOPED_MUTEX_LOCK( locker, &d->mutex);
            d->objects.remove(m_object);
            d->condition.wakeAll();
        }
        
    signals:
        void pushed(QObject *object);

    private:
        QnObjectThreadPullerDataPtr d;
        QObject *m_object;
        QThread *m_thread;
    };

} // namespace detail



/**
 * This class can be used for pulling objects from another thread.
 */
class QnObjectThreadPuller: public QObject {
    Q_OBJECT
public:
    QnObjectThreadPuller(QObject *parent = NULL): QObject(parent), d(new detail::QnObjectThreadPullerData) {}

    virtual ~QnObjectThreadPuller() {}

    bool pull(QObject *object) {
        if(!object) {
            qnNullWarning(object);
            return false;
        }

        if(!qApp) {
            qnWarning("QApplication instance does not exist, cannot pull an object.");
            return false;
        }

        QThread *sourceThread = object->thread();
        QThread *targetThread = QThread::currentThread();
        if(!sourceThread || !targetThread)
            return false; /* Application is being terminated. */

        if(sourceThread == targetThread) {
            QMetaObject::invokeMethod(this, "pulled", Qt::QueuedConnection, Q_ARG(QObject *, object));
            return true; /* Nothing to do. */
        }

        if(!qnHasEventLoop(sourceThread)) {
            qnWarning("Event loop is not running in given object's thread, cannot pull an object.");
            return false;
        }

        SCOPED_MUTEX_LOCK( locker, &d->mutex);
        if(d->objects.contains(object)) {
            qnWarning("Given object is already being moved.");
            return false;
        }
        d->objects.insert(object);
        locker.unlock();

        detail::QnObjectThreadPullWorker *worker = new detail::QnObjectThreadPullWorker(object, targetThread, d);
        connect(worker, SIGNAL(pushed(QObject *)), this, SIGNAL(pulled(QObject *)), Qt::QueuedConnection);
        worker->moveToThread(sourceThread);
        worker->deleteLater();
        return true;
    }

    void wait(QObject *object = NULL) {
        SCOPED_MUTEX_LOCK( locker, &d->mutex);
        while (true) {
            if(object == NULL && d->objects.isEmpty())
                break;

            if(object != NULL && !d->objects.contains(object))
                break;

            d->condition.wait(&d->mutex);
        }
    }

signals:
    void pulled(QObject *object);

private:
    detail::QnObjectThreadPullerDataPtr d;
};


#endif // QN_OBJECT_THREAD_PULLER_H

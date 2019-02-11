#pragma once

#include <QtCore/QSharedPointer>

#include <nx/utils/singleton.h>

#include "thread.h"

class QnLongRunnablePoolPrivate;

class NX_UTILS_API QnLongRunnable:
    public nx::utils::Thread
{
    using base_type = nx::utils::Thread;

    Q_OBJECT

public:

    QnLongRunnable(const char* threadName = nullptr);
    ~QnLongRunnable();

protected:
    virtual void at_started() override;
    virtual void at_finished() override;

private:
    QSharedPointer<QnLongRunnablePoolPrivate> m_pool;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API QnLongRunnablePool:
    public QObject,
    public Singleton<QnLongRunnablePool>
{
    Q_OBJECT

public:
    QnLongRunnablePool(QObject *parent = NULL);
    virtual ~QnLongRunnablePool();

    /**
     * Sends stop request to all threads previously registered with this pool
     * and waits until they are stopped.
     */
    void stopAll();

    /**
     * Blocks current thread until all threads previously registered with this
     * pool are stopped.
     */
    void waitAll();

private:
    friend class QnLongRunnable;
    QSharedPointer<QnLongRunnablePoolPrivate> d;
};

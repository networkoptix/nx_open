// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>

class QnLongRunnable;

/**
 * This helper class stops threads asynchronously.
 * It is single tone with managed life-time, so it guarantee
 * that all nested threads will be stopped before this class destroyed.
 */
class NX_VMS_COMMON_API QnLongRunableCleanup: public QObject
{
public:
    using LongRunnablePtr = std::unique_ptr<QnLongRunnable>;

public:
    QnLongRunableCleanup();
    virtual ~QnLongRunableCleanup() override;

    void cleanupAsync(LongRunnablePtr threadUniquePtr, QObject* context);
    void clearContextAsync(QObject* context);

private:
    struct ThreadData
    {
        LongRunnablePtr thread;
        QObject* context = nullptr;
    };

    struct ContextData
    {
        int refCount = 0;
        bool cleanup = false;
    };

private:
    void tryClearContext(QObject* context);

private:
    mutable nx::Mutex m_mutex;

    std::map<QnLongRunnable*, ThreadData> m_threadsToStop;
    std::map<QObject*, ContextData> m_contextsToStop;
};

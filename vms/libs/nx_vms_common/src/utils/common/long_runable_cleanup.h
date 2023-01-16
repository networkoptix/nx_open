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
    QnLongRunableCleanup();
    virtual ~QnLongRunableCleanup();

    void cleanupAsync(std::unique_ptr<QnLongRunnable> threadUniquePtr);
    void cleanupAsyncShared(std::shared_ptr<QnLongRunnable> threadSharedPtr);

private:
    std::map<QnLongRunnable*, std::shared_ptr<QnLongRunnable>> m_threadsToStop;
    mutable nx::Mutex m_mutex;
};

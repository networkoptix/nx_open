#include "resource_allocator.h"

#if defined(Q_OS_ANDROID)

ResourceAllocator::ResourceAllocator(QQuickWindow *window):
    nx::media::AbstractResourceAllocator(),
    m_window(window),
    m_lambda(nullptr),
    m_opaque(nullptr),
    m_connected(false)
{
}

void ResourceAllocator::at_execLambda()
{
    QMutexLocker lock(&m_mutex);
    if (m_lambda) {
        m_lambda(m_opaque);
        m_lambda = 0;
        m_waitCond.wakeOne();
    }
}

void ResourceAllocator::execAtGlThread(std::function<void (void*)> lambda, void* opaque)
{
    if (!m_connected)
    {
        m_connected = true;
        connect(m_window, &QQuickWindow::beforeSynchronizing, this, &ResourceAllocator::at_execLambda, Qt::DirectConnection);
        connect(m_window, &QQuickWindow::frameSwapped, this, &ResourceAllocator::at_execLambda, Qt::DirectConnection);
    }

    QMutexLocker lock(&m_mutex);
    m_lambda = lambda;
    m_opaque = opaque;
    while (m_lambda)
        m_waitCond.wait(&m_mutex);
}

#endif // #if defined(Q_OS_ANDROID)

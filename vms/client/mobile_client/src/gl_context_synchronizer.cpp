#include "gl_context_synchronizer.h"

#include <ini.h>

using nx::mobile_client::ini;

GlContextSynchronizer::GlContextSynchronizer(QQuickWindow *window):
    nx::media::AbstractRenderContextSynchronizer(),
    m_window(window),
    m_lambda(nullptr),
    m_opaque(nullptr),
    m_connected(false)
{
}

void GlContextSynchronizer::at_execLambda()
{
    QMutexLocker lock(&m_mutex);
    if (m_lambda) {
        m_lambda(m_opaque);
        m_lambda = nullptr;
        m_waitCond.wakeOne();
    }
}

void GlContextSynchronizer::at_execLambdaAsync()
{
    QMutexLocker lock(&m_mutex);
    while (!m_lambdasAsync.empty())
    {
        auto lambda = m_lambdasAsync.front();
        m_lambdasAsync.pop_front();
        lock.unlock();
        lambda();
        lock.relock();
    }
}

void GlContextSynchronizer::execInRenderContext(std::function<void (void*)> lambda, void* opaque)
{
    if (!m_connected)
    {
        m_connected = true;
        connect(m_window, &QQuickWindow::beforeSynchronizing,
            this, &GlContextSynchronizer::at_execLambda, Qt::DirectConnection);
        connect(m_window, &QQuickWindow::frameSwapped,
            this, &GlContextSynchronizer::at_execLambda, Qt::DirectConnection);
    }

    QMutexLocker lock(&m_mutex);
    m_lambda = lambda;
    m_opaque = opaque;
    while (m_lambda)
        m_waitCond.wait(&m_mutex);
}

void GlContextSynchronizer::execInRenderContextAsync(std::function<void()> lambda)
{
    if (!m_connectedAsync)
    {
        m_connectedAsync = true;
        if (ini().execAtGlThreadOnBeforeSynchronizing)
        {
            connect(m_window, &QQuickWindow::beforeSynchronizing,
                this, &GlContextSynchronizer::at_execLambdaAsync, Qt::DirectConnection);
        }
        if (ini().execAtGlThreadOnFrameSwapped)
        {
            connect(m_window, &QQuickWindow::frameSwapped,
                this, &GlContextSynchronizer::at_execLambdaAsync, Qt::DirectConnection);
        }
    }

    QMutexLocker lock(&m_mutex);
    m_lambdasAsync.push_back(lambda);
}

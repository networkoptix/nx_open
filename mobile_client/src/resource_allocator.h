#pragma once

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickWindow>

#include <nx/media/abstract_resource_allocator.h>
#include <memory>
#include <deque>

class GlContextThreadExecutor;

class ResourceAllocator: public QObject, public nx::media::AbstractResourceAllocator
{
public:
    ResourceAllocator(QQuickWindow *window);

    virtual void execAtGlThread(std::function<void (void*)> lambda, void* opaque) override;
    virtual void execAtGlThreadAsync(std::function<void ()> lambda) override;
private slots:
    void at_execLambda();
    void at_execLambdaAsync();
private:
    QQuickWindow *m_window;

    QMutex m_mutex;
    QWaitCondition m_waitCond;
    std::function<void (void*)> m_lambda;
    std::deque<std::function<void()>> m_lambdasAsync;
    void* m_opaque;
    bool m_connected;
};


#pragma once

#if defined(Q_OS_ANDROID)

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickWindow>

#include <nx/media/abstract_resource_allocator.h>
#include <memory>

class GlContextThreadExecutor;

class ResourceAllocator: public QObject, public nx::media::AbstractResourceAllocator
{
public:
    ResourceAllocator(QQuickWindow *window);

    virtual void execAtGlThread(std::function<void (void*)> lambda, void* opaque) override;
private slots:
    void at_execLambda();
private:
    QQuickWindow *m_window;

    QMutex m_mutex;
    QWaitCondition m_waitCond;
    std::function<void (void*)> m_lambda;
    void* m_opaque;
    bool m_connected;
};

#endif // #if defined(Q_OS_ANDROID)

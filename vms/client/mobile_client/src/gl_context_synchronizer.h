// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>

#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickWindow>

#include <nx/media/abstract_render_context_synchronizer.h>

class GlContextThreadExecutor;

class GlContextSynchronizer: public QObject, public nx::media::AbstractRenderContextSynchronizer
{
public:
    GlContextSynchronizer(QQuickWindow* window);

    virtual void execInRenderContext(std::function<void (void*)> lambda, void* opaque) override;
    virtual void execInRenderContextAsync(std::function<void ()> lambda) override;

private:
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
    bool m_connectedAsync;
};

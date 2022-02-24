// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_output_shader_manager.h"

#include <memory>

#include <QtCore/QPointer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QWindow>

#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

struct MediaOutputShaderManager::Private
{
    const QPointer<QOpenGLContext> context;
    QHash<MediaOutputShaderProgram::Key, MediaOutputShaderProgram*> programs;

    void cleanup()
    {
        if (programs.empty())
            return;

        NX_ASSERT(context && context == QOpenGLContext::currentContext());

        for (auto program: programs)
            delete program;

        programs.clear();
    }
};

MediaOutputShaderManager* MediaOutputShaderManager::instance(QOpenGLContext* context)
{
    if (!NX_ASSERT(context))
        return nullptr;

    const auto existing = context->findChild<MediaOutputShaderManager*>();
    if (existing)
        return existing;

    const auto manager = new MediaOutputShaderManager(context);

    connect(context, &QOpenGLContext::aboutToBeDestroyed,
        [context]() { delete context->findChild<MediaOutputShaderManager*>(); });

    return manager;
}

QOpenGLContext* MediaOutputShaderManager::context() const
{
    return d->context;
}

MediaOutputShaderProgram* MediaOutputShaderManager::program(
    const MediaOutputShaderProgram::Key& key)
{
    if (!NX_ASSERT(d->context && d->context == QOpenGLContext::currentContext()))
        return nullptr;

    auto& programRef = d->programs[key];
    if (!programRef)
        programRef = new MediaOutputShaderProgram(key, this);

    return programRef;
}

MediaOutputShaderManager::MediaOutputShaderManager(QOpenGLContext* context):
    base_type(context),
    d(new Private{context})
{
    NX_ASSERT(d->context);
}

MediaOutputShaderManager::~MediaOutputShaderManager()
{
    if (!d->context)
        return;

    const auto currentContext = QOpenGLContext::currentContext();
    if (currentContext == d->context)
    {
        d->cleanup();
        return;
    }

    const auto surface = currentContext ? currentContext->surface() : nullptr;
    const auto restoreContext = nx::utils::makeScopeGuard(
        [context = QPointer<QOpenGLContext>(currentContext), surface]()
        {
            if (context)
                context->makeCurrent(surface);
        });

    const auto dummy = std::make_unique<QWindow>();
    dummy->setSurfaceType(QSurface::OpenGLSurface);
    dummy->setGeometry(0, 0, 1, 1);
    dummy->create();

    NX_ASSERT(d->context->makeCurrent(dummy.get()));
    d->cleanup();
    d->context->doneCurrent();
}

} // namespace nx::vms::client::desktop
